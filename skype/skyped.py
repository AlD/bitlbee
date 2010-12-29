#!/usr/bin/env python2.7
# 
#   skyped.py
#  
#   Copyright (c) 2007, 2008, 2009, 2010 by Miklos Vajna <vmiklos@frugalware.org>
#
#   This program is free software; you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation; either version 2 of the License, or
#   (at your option) any later version.
# 
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#  
#   You should have received a copy of the GNU General Public License
#   along with this program; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, 
#   USA.
#

import sys
import os
import signal
import locale
import time
import socket
import getopt
import Skype4Py
import hashlib
from ConfigParser import ConfigParser, NoOptionError
from traceback import print_exception
import ssl
import select
import threading

__version__ = "0.1.1"

def eh(type, value, tb):
	global options

	if type != KeyboardInterrupt:
		print_exception(type, value, tb)
	if options.conn:
		options.conn.close()
	# shut down client if it's running
	try:
		skype.skype.Client.Shutdown()
	except NameError:
		pass
	sys.exit("Exiting.")

sys.excepthook = eh

def wait_for_lock(lock, timeout_to_print, timeout, msg):
	start = time.time()
	locked = lock.acquire(0)
	while not(locked):
		time.sleep(0.5)
		if timeout_to_print and (time.time() - timeout_to_print > start):
			dprint("%s: Waited %f seconds" % \
					(msg, time.time() - start))
			timeout_to_print = False
		if timeout and (time.time() - timeout > start):
			dprint("%s: Waited %f seconds, giving up" % \
					(msg, time.time() - start))
			return False
		locked = lock.acquire(0)
	return True

def input_handler(fd):
	global options
	global skype
	if options.buf:
		for i in options.buf:
			skype.send(i.strip())
		options.buf = None
		return True
	else:
		close_socket = False
		if wait_for_lock(options.lock, 3, 10, "input_handler"):
			try:
					input = fd.recv(1024)
					options.lock.release()
			except Exception, s:
				dprint("Warning, receiving 1024 bytes failed (%s)." % s)
				fd.close()
				options.conn = False
				options.lock.release()
				return False
			for i in input.split("\n"):
				if i.strip() == "SET USERSTATUS OFFLINE":
					close_socket = True
				skype.send(i.strip())
		return not(close_socket)

def skype_idle_handler(skype):
	try:
		c = skype.skype.Command("PING", Block=True)
		skype.skype.SendCommand(c)
	except Skype4Py.SkypeAPIError, s:
		dprint("Warning, pinging Skype failed (%s)." % (s))
	return True

def send(sock, txt):
	global options
	count = 1
	done = False
	while (not done) and (count < 10) and options.conn:
		if wait_for_lock(options.lock, 3, 10, "socket send"):
			try:
				if options.conn: sock.send(txt)
				options.lock.release()
				done = True
			except Exception, s:
				options.lock.release()
				count += 1
				dprint("Warning, sending '%s' failed (%s). count=%d" % (txt, s, count))
				time.sleep(1)
	if not done:
		if options.conn:
			options.conn.close()
		options.conn = False
	return done

def bitlbee_idle_handler(skype):
	global options
	done = False
	if options.conn:
		try:
			e = "PING"
			done = send(options.conn, "%s\n" % e)
		except Exception, s:
			dprint("Warning, sending '%s' failed (%s)." % (e, s))
			if options.conn: options.conn.close()
			options.conn = False
			done = False
	return done

def server(host, port, skype):
	global options
	sock = socket.socket()
	sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
	sock.bind((host, port))
	sock.listen(1)
	dprint("Waiting for connection...")
	listener(sock, skype)

def listener(sock, skype):
	global options
	if not(wait_for_lock(options.lock, 3, 10, "listener")): return False
	rawsock, addr = sock.accept()
	options.conn = ssl.wrap_socket(rawsock,
		server_side=True,
		certfile=options.config.sslcert,
		keyfile=options.config.sslkey,
		ssl_version=ssl.PROTOCOL_TLSv1)
	if hasattr(options.conn, 'handshake'):
		try:
			options.conn.handshake()
		except Exception:
			options.lock.release()
			dprint("Warning, handshake failed, closing connection.")
			return False
	ret = 0
	try:
		line = options.conn.recv(1024)
		if line.startswith("USERNAME") and line.split(' ')[1].strip() == options.config.username:
			ret += 1
		line = options.conn.recv(1024)
		if line.startswith("PASSWORD") and hashlib.sha1(line.split(' ')[1].strip()).hexdigest() == options.config.password:
			ret += 1
	except Exception, s:
		dprint("Warning, receiving 1024 bytes failed (%s)." % s)
		options.conn.close()
		options.conn = False
		options.lock.release()
		return False
	if ret == 2:
		dprint("Username and password OK.")
		options.conn.send("PASSWORD OK\n")
		options.lock.release()
		serverloop(options, skype)
		return True
	else:
		dprint("Username and/or password WRONG.")
		options.conn.send("PASSWORD KO\n")
		options.conn.close()
		options.conn = False
		options.lock.release()
		return False

def dprint(msg):
	from time import strftime
	global options

	now = strftime("%Y-%m-%d %H:%M:%S")

	if options.debug:
		print now + ": " + msg
		sys.stdout.flush()
	if options.log:
		sock = open(options.log, "a")
		sock.write("%s: %s\n" % (now, msg))
		sock.close()

class SkypeApi:
	def __init__(self):
		self.skype = Skype4Py.Skype()
		self.skype.OnNotify = self.recv
		self.skype.Client.Start()

	def recv(self, msg_text):
		global options
		if msg_text == "PONG":
			return
		if "\n" in msg_text:
			# crappy skype prefixes only the first line for
			# multiline messages so we need to do so for the other
			# lines, too. this is something like:
			# 'CHATMESSAGE id BODY first line\nsecond line' ->
			# 'CHATMESSAGE id BODY first line\nCHATMESSAGE id BODY second line'
			prefix = " ".join(msg_text.split(" ")[:3])
			msg_text = ["%s %s" % (prefix, i) for i in " ".join(msg_text.split(" ")[3:]).split("\n")]
		else:
			msg_text = [msg_text]
		for i in msg_text:
			# use utf-8 here to solve the following problem:
			# people use env vars like LC_ALL=en_US (latin1) then
			# they complain about why can't they receive latin2
			# messages.. so here it is: always use utf-8 then
			# everybody will be happy
			e = i.encode('UTF-8')
			if options.conn:
				dprint('<< ' + e)
				try:
					# I called the send function really_send
					send(options.conn, e + "\n")
				except Exception, s:
					dprint("Warning, sending '%s' failed (%s)." % (e, s))
					if options.conn: options.conn.close()
					options.conn = False
			else:
				dprint('-- ' + e)

	def send(self, msg_text):
		if not len(msg_text) or msg_text == "PONG":
			if msg_text == "PONG":
				options.last_bitlbee_pong = time.time()
			return
		try:
			encoding = locale.getdefaultlocale()[1]
			if not encoding:
				raise ValueError
			e = msg_text.decode(encoding)
		except ValueError:
			e = msg_text.decode('UTF-8')
		dprint('>> ' + e)
		try:
			c = self.skype.Command(e, Block=True)
			self.skype.SendCommand(c)
			self.recv(c.Reply)
		except Skype4Py.SkypeError:
			pass
		except Skype4Py.SkypeAPIError, s:
			dprint("Warning, sending '%s' failed (%s)." % (e, s))

class Options:
	def __init__(self):
		self.cfgpath = os.path.join(os.environ['HOME'], ".skyped", "skyped.conf")
		# for backwards compatibility
		self.syscfgpath = "/usr/local/etc/skyped/skyped.conf"
		if os.path.exists(self.syscfgpath):
			self.cfgpath = self.syscfgpath
		self.daemon = True
		self.debug = False
		self.help = False
		self.host = "0.0.0.0"
		self.log = None
		self.port = None
		self.version = False
		# well, this is a bit hackish. we store the socket of the last connected client
		# here and notify it. maybe later notify all connected clients?
		self.conn = None
		# this will be read first by the input handler
		self.buf = None


	def usage(self, ret):
		print """Usage: skyped [OPTION]...

skyped is a daemon that acts as a tcp server on top of a Skype instance.

Options:
	-c      --config        path to configuration file (default: %s)
	-d	--debug		enable debug messages
	-h	--help		this help
	-H	--host		set the tcp host (default: %s)
	-l      --log           set the log file in background mode (default: none)
	-n	--nofork	don't run as daemon in the background
	-p	--port		set the tcp port (default: %s)
	-v	--version	display version information""" % (self.cfgpath, self.host, self.port)
		sys.exit(ret)

def serverloop(options, skype):
	timeout = 1; # in seconds
	skype_ping_period = 5
	bitlbee_ping_period = 10
	bitlbee_pong_timeout = 30
	now = time.time()
	skype_ping_start_time = now
	bitlbee_ping_start_time = now
	options.last_bitlbee_pong = now
	in_error = []
	handler_ok = True
	while (len(in_error) == 0) and handler_ok and options.conn:
		ready_to_read, ready_to_write, in_error = \
			select.select([options.conn], [], [options.conn], \
				timeout)
		now = time.time()
		handler_ok = len(in_error) == 0
		if (len(ready_to_read) == 1) and handler_ok:
			handler_ok = input_handler(ready_to_read.pop())
			# don't ping bitlbee/skype if they already received data
			now = time.time() # allow for the input_handler to take some time
			bitlbee_ping_start_time = now
			skype_ping_start_time = now
			options.last_bitlbee_pong = now
		if (now - skype_ping_period > skype_ping_start_time) and handler_ok:
			handler_ok = skype_idle_handler(skype)
			skype_ping_start_time = now
		if now - bitlbee_ping_period > bitlbee_ping_start_time:
			handler_ok = bitlbee_idle_handler(skype)
			bitlbee_ping_start_time = now
			if options.last_bitlbee_pong:
				if (now - options.last_bitlbee_pong) > bitlbee_pong_timeout:
					dprint("Bitlbee pong timeout")
					# TODO is following line necessary? Should there be a options.conn.unwrap() somewhere?
					# options.conn.shutdown()
					if options.conn:
						options.conn.close()
					options.conn = False
			else:
				options.last_bitlbee_pong = now

if __name__=='__main__':
	options = Options()
	try:
		opts, args = getopt.getopt(sys.argv[1:], "c:dhH:l:np:v", ["config=", "debug", "help", "host=", "log=", "nofork", "port=", "version"])
	except getopt.GetoptError:
		options.usage(1)
	for opt, arg in opts:
		if opt in ("-c", "--config"):
			options.cfgpath = arg
		elif opt in ("-d", "--debug"):
			options.debug = True
		elif opt in ("-h", "--help"):
			options.help = True
		elif opt in ("-H", "--host"):
			options.host = arg
		elif opt in ("-l", "--log"):
			options.log = arg
		elif opt in ("-n", "--nofork"):
			options.daemon = False
		elif opt in ("-p", "--port"):
			options.port = int(arg)
		elif opt in ("-v", "--version"):
			options.version = True
	if options.help:
		options.usage(0)
	elif options.version:
		print "skyped %s" % __version__
		sys.exit(0)
	# parse our config
	if not os.path.exists(options.cfgpath):
		print "Can't find configuration file at '%s'." % options.cfgpath
		print "Use the -c option to specify an alternate one."
		sys.exit(1)
	options.config = ConfigParser()
	options.config.read(options.cfgpath)
	options.config.username = options.config.get('skyped', 'username').split('#')[0]
	options.config.password = options.config.get('skyped', 'password').split('#')[0]
	options.config.sslkey = os.path.expanduser(options.config.get('skyped', 'key').split('#')[0])
	options.config.sslcert = os.path.expanduser(options.config.get('skyped', 'cert').split('#')[0])
	# hack: we have to parse the parameters first to locate the
	# config file but the -p option should overwrite the value from
	# the config file
	try:
		options.config.port = int(options.config.get('skyped', 'port').split('#')[0])
		if not options.port:
			options.port = options.config.port
	except NoOptionError:
		pass
	if not options.port:
		options.port = 2727
	dprint("Parsing config file '%s' done, username is '%s'." % (options.cfgpath, options.config.username))
	if options.daemon:
		pid = os.fork()
		if pid == 0:
			nullin = file(os.devnull, 'r')
			nullout = file(os.devnull, 'w')
			os.dup2(nullin.fileno(), sys.stdin.fileno())
			os.dup2(nullout.fileno(), sys.stdout.fileno())
			os.dup2(nullout.fileno(), sys.stderr.fileno())
		else:
			print 'skyped is started on port %s, pid: %d' % (options.port, pid)
			sys.exit(0)
	else:
		dprint('skyped is started on port %s' % options.port)
	try:
		skype = SkypeApi()
	except Skype4Py.SkypeAPIError, s:
		sys.exit("%s. Are you sure you have started Skype?" % s)
	while 1:
		options.conn = False
		options.lock = threading.Lock()
		server(options.host, options.port, skype)
