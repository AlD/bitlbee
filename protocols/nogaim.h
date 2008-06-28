  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/*
 * nogaim, soon to be known as im_api. Not a separate product (unless
 * someone would be interested in such a thing), just a new name.
 *
 * Gaim without gaim - for BitlBee
 *
 * This file contains functions called by the Gaim IM-modules. It contains
 * some struct and type definitions from Gaim.
 *
 * Copyright (C) 1998-1999, Mark Spencer <markster@marko.net>
 *                          (and possibly other members of the Gaim team)
 * Copyright 2002-2007 Wilmer van der Gaast <wilmer@gaast.net>
 */

/*
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License with
  the Debian GNU/Linux distribution in /usr/share/common-licenses/GPL;
  if not, write to the Free Software Foundation, Inc., 59 Temple Place,
  Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef _NOGAIM_H
#define _NOGAIM_H

#include "bitlbee.h"
#include "account.h"
#include "proxy.h"
#include "query.h"
#include "md5.h"

#define BUDDY_ALIAS_MAXLEN 388   /* because MSN names can be 387 characters */

#define WEBSITE "http://www.bitlbee.org/"
#define GAIM_AWAY_CUSTOM "Custom"

/* Sharing flags between all kinds of things. I just hope I won't hit any
   limits before 32-bit machines become extinct. ;-) */
#define OPT_LOGGED_IN   0x00000001
#define OPT_LOGGING_OUT 0x00000002
#define OPT_AWAY        0x00000004
#define OPT_DOES_HTML   0x00000010
#define OPT_LOCALBUDDY  0x00000020 /* For nicks local to one groupchat */
#define OPT_TYPING      0x00000100 /* Some pieces of code make assumptions */
#define OPT_THINKING    0x00000200 /* about these values... Stupid me! */

/* ok. now the fun begins. first we create a connection structure */
struct im_connection
{
	account_t *acc;
	uint32_t flags;
	
	/* each connection then can have its own protocol-specific data */
	void *proto_data;
	
	/* all connections need an input watcher */
	int inpa;
	guint keepalive;
	
	/* buddy list stuff. there is still a global groups for the buddy list, but
	 * we need to maintain our own set of buddies, and our own permit/deny lists */
	GSList *permit;
	GSList *deny;
	int permdeny;
	
	char displayname[128];
	char *away;
	
	int evil;
	
	/* BitlBee */
	irc_t *irc;
	
	struct groupchat *groupchats;
};

struct groupchat {
	struct im_connection *ic;

	/* stuff used just for chat */
	/* The in_room variable is a list of handles (not nicks!), kind of
	 * "nick list". This is how you can check who is in the group chat
	 * already, for example to avoid adding somebody two times. */
	GList *in_room;
	GList *ignored;
	
	struct groupchat *next;
	char *channel;
	/* The title variable contains the ID you gave when you created the
	 * chat using imcb_chat_new(). */
	char *title;
	/* Use imcb_chat_topic() to change this variable otherwise the user
	 * won't notice the topic change. */
	char *topic;
	char joined;
	/* This is for you, you can add your own structure here to extend this
	 * structure for your protocol's needs. */
	void *data;
};

struct buddy {
	char name[80];
	char show[BUDDY_ALIAS_MAXLEN];
	int present;
	int evil;
	time_t signon;
	time_t idle;
	int uc;
	guint caps; /* woohoo! */
	void *proto_data; /* what a hack */
	struct im_connection *ic; /* the connection it belongs to */
};

struct prpl {
	int options;
	/* You should set this to the name of your protocol.
	 * - The user sees this name ie. when imcb_log() is used. */
	const char *name;

	/* Added this one to be able to add per-account settings, don't think
	 * it should be used for anything else. You are supposed to use the
	 * set_add() function to add new settings. */
	void (* init)		(account_t *);
	/* The typical usage of the login() function:
	 * - Create an im_connection using imcb_new() from the account_t parameter.
	 * - Initialize your myproto_data struct - you should store all your protocol-specific data there.
	 * - Save your custom structure to im_connection->proto_data.
	 * - Use proxy_connect() to connect to the server.
	 */
	void (* login)		(account_t *);
	/* Implementing this function is optional. */
	void (* keepalive)	(struct im_connection *);
	/* In this function you should:
	 * - Tell the server about you are logging out.
	 * - g_free() your myproto_data struct as BitlBee does not know how to
	 *   properly do so.
	 */
	void (* logout)		(struct im_connection *);
	
	/* This function is called when the user wants to send a message to a handle.
	 * - 'to' is a handle, not a nick
	 * - 'flags' may be ignored
	 */
	int  (* buddy_msg)	(struct im_connection *, char *to, char *message, int flags);
	/* This function is called then the user uses the /away IRC command.
	 * - 'state' contains the away reason.
	 * - 'message' may be ignored if your protocol does not support it.
	 */
	void (* set_away)	(struct im_connection *, char *state, char *message);
	/* Implementing this function is optional. */
	void (* get_away)       (struct im_connection *, char *who);
	/* Implementing this function is optional. */
	int  (* send_typing)	(struct im_connection *, char *who, int flags);
	
	/* 'name' is a handle to add/remove. For now BitlBee doesn't really
	 * handle groups, just set it to NULL, so you can ignore that
	 * parameter. */
	void (* add_buddy)	(struct im_connection *, char *name, char *group);
	void (* remove_buddy)	(struct im_connection *, char *name, char *group);
	
	/* Block list stuff. Implementing these are optional. */
	void (* add_permit)	(struct im_connection *, char *who);
	void (* add_deny)	(struct im_connection *, char *who);
	void (* rem_permit)	(struct im_connection *, char *who);
	void (* rem_deny)	(struct im_connection *, char *who);
	/* Doesn't actually have UI hooks. */
	void (* set_permit_deny)(struct im_connection *);
	
	/* Request profile info. Free-formatted stuff, the IM module gives back
	   this info via imcb_log(). Implementing these are optional. */
	void (* get_info)	(struct im_connection *, char *who);
	void (* set_my_name)	(struct im_connection *, char *name);
	void (* set_name)	(struct im_connection *, char *who, char *name);
	
	/* Group chat stuff. */
	/* This is called when the user uses the /invite IRC command.
	 * - 'who' may be ignored
	 * - 'message' is a handle to invite
	 */
	void (* chat_invite)	(struct groupchat *, char *who, char *message);
	/* This is called when the user uses the /part IRC command in a group
	 * chat. You just should tell the user about it, nothing more. */
	void (* chat_leave)	(struct groupchat *);
	/* This is called when the user sends a message to the groupchat.
	 * 'flags' may be ignored. */
	void (* chat_msg)	(struct groupchat *, char *message, int flags);
	/* This is called when the user uses the /join #nick IRC command.
	 * - 'who' is the handle of the nick
	 */
	struct groupchat *
	     (* chat_with)	(struct im_connection *, char *who);
	/* This is used when the user uses the /join #channel IRC command.  If
	 * your protocol does not support publicly named group chats, then do
	 * not implement this. */
	struct groupchat *
	     (* chat_join)	(struct im_connection *, char *room, char *nick, char *password);
	/* Change the topic, if supported. Note that BitlBee expects the IM
	   server to confirm the topic change with a regular topic change
	   event. If it doesn't do that, you have to fake it to make it
	   visible to the user. */
	void (* chat_topic)	(struct groupchat *, char *topic);
	
	/* You can tell what away states your protocol supports, so that
	 * BitlBee will try to map the IRC away reasons to them, or use
	 * GAIM_AWAY_CUSTOM when calling skype_set_away(). */
	GList *(* away_states)(struct im_connection *ic);
	
	/* Mainly for AOL, since they think "Bung hole" == "Bu ngho le". *sigh*
	 * - Most protocols will just want to set this to g_strcasecmp().*/
	int (* handle_cmp) (const char *who1, const char *who2);
};

/* im_api core stuff. */
void nogaim_init();
G_MODULE_EXPORT GSList *get_connections();
G_MODULE_EXPORT struct prpl *find_protocol( const char *name );
/* When registering a new protocol, you should allocate space for a new prpl
 * struct, initialize it (set the function pointers to point to your
 * functions), finally call this function. */
G_MODULE_EXPORT void register_protocol( struct prpl * );

/* Connection management. */
/* You will need this function in prpl->login() to get an im_connection from
 * the account_t parameter. */
G_MODULE_EXPORT struct im_connection *imcb_new( account_t *acc );
G_MODULE_EXPORT void imcb_free( struct im_connection *ic );
/* Once you're connected, you should call this function, so that the user will
 * see the success. */
G_MODULE_EXPORT void imcb_connected( struct im_connection *ic );
/* This can be used to disconnect when something went wrong (ie. read error
 * from the server). You probably want to set the second parameter to TRUE. */
G_MODULE_EXPORT void imc_logout( struct im_connection *ic, int allow_reconnect );

/* Communicating with the user. */
/* A printf()-like function to tell the user anything you want. */
G_MODULE_EXPORT void imcb_log( struct im_connection *ic, char *format, ... ) G_GNUC_PRINTF( 2, 3 );
/* To tell the user an error, ie. before logging out when an error occurs. */
G_MODULE_EXPORT void imcb_error( struct im_connection *ic, char *format, ... ) G_GNUC_PRINTF( 2, 3 );
/* To ask a your about something.
 * - 'msg' is the question.
 * - 'data' can be your custom struct - it will be passed to the callbacks.
 * - 'doit' or 'dont' will be called depending of the answer of the user.
 */
G_MODULE_EXPORT void imcb_ask( struct im_connection *ic, char *msg, void *data, query_callback doit, query_callback dont );
G_MODULE_EXPORT void imcb_ask_add( struct im_connection *ic, char *handle, const char *realname );

/* Buddy management */
/* This function should be called for each handle which are visible to the
 * user, usually after a login, or if the user added a buddy and the IM
 * server confirms that the add was successful. Don't forget to do this! */
G_MODULE_EXPORT void imcb_add_buddy( struct im_connection *ic, char *handle, char *group );
G_MODULE_EXPORT void imcb_remove_buddy( struct im_connection *ic, char *handle, char *group );
G_MODULE_EXPORT struct buddy *imcb_find_buddy( struct im_connection *ic, char *handle );
G_MODULE_EXPORT void imcb_rename_buddy( struct im_connection *ic, char *handle, char *realname );
G_MODULE_EXPORT void imcb_buddy_nick_hint( struct im_connection *ic, char *handle, char *nick );

/* Buddy activity */
/* To manipulate the status of a handle.
 * - flags can be |='d with OPT_* constants. You will need at least:
 *   OPT_LOGGED_IN and OPT_AWAY.
 * - 'state' and 'message' can be NULL */
G_MODULE_EXPORT void imcb_buddy_status( struct im_connection *ic, const char *handle, int flags, const char *state, const char *message );
/* Not implemented yet! */ G_MODULE_EXPORT void imcb_buddy_times( struct im_connection *ic, const char *handle, time_t login, time_t idle );
/* Call when a handle says something. 'flags' and 'sent_at may be just 0. */
G_MODULE_EXPORT void imcb_buddy_msg( struct im_connection *ic, char *handle, char *msg, uint32_t flags, time_t sent_at );
G_MODULE_EXPORT void imcb_buddy_typing( struct im_connection *ic, char *handle, uint32_t flags );
G_MODULE_EXPORT void imcb_clean_handle( struct im_connection *ic, char *handle );

/* Groupchats */
G_MODULE_EXPORT void imcb_chat_invited( struct im_connection *ic, char *handle, char *who, char *msg, GList *data );
/* These two functions are to create a group chat.
 * - imcb_chat_new(): the 'handle' parameter identifies the chat, like the
 *   channel name on IRC.
 * - After you have a groupchat pointer, you should add the handles, finally
 *   the user her/himself. At that point the group chat will be visible to the
 *   user, too. */
G_MODULE_EXPORT struct groupchat *imcb_chat_new( struct im_connection *ic, char *handle );
G_MODULE_EXPORT void imcb_chat_add_buddy( struct groupchat *b, char *handle );
/* To remove a handle from a group chat. Reason can be NULL. */
G_MODULE_EXPORT void imcb_chat_remove_buddy( struct groupchat *b, char *handle, char *reason );
/* To tell BitlBee 'who' said 'msg' in 'c'. 'flags' and 'sent_at' can be 0. */
G_MODULE_EXPORT void imcb_chat_msg( struct groupchat *c, char *who, char *msg, uint32_t flags, time_t sent_at );
/* System messages specific to a groupchat, so they can be displayed in the right context. */
G_MODULE_EXPORT void imcb_chat_log( struct groupchat *c, char *format, ... ) G_GNUC_PRINTF( 2, 3 );
/* To tell BitlBee 'who' changed the topic of 'c' to 'topic'. */
G_MODULE_EXPORT void imcb_chat_topic( struct groupchat *c, char *who, char *topic, time_t set_at );
G_MODULE_EXPORT void imcb_chat_free( struct groupchat *c );

/* Actions, or whatever. */
int imc_set_away( struct im_connection *ic, char *away );
int imc_buddy_msg( struct im_connection *ic, char *handle, char *msg, int flags );
int imc_chat_msg( struct groupchat *c, char *msg, int flags );

void imc_add_allow( struct im_connection *ic, char *handle );
void imc_rem_allow( struct im_connection *ic, char *handle );
void imc_add_block( struct im_connection *ic, char *handle );
void imc_rem_block( struct im_connection *ic, char *handle );

/* Misc. stuff */
char *set_eval_away_devoice( set_t *set, char *value );
gboolean auto_reconnect( gpointer data, gint fd, b_input_condition cond );
void cancel_auto_reconnect( struct account *a );

#endif
