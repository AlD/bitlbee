/********************************************************************\
* BitlBee -- An IRC to other IM-networks gateway                     *
*                                                                    *
* Copyright 2006 Marijn Kruisselbrink and others                     *
\********************************************************************/

/* Generic file transfer header                                     */

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

#ifndef _FT_H
#define _FT_H

typedef enum {
	FT_STATUS_LISTENING	= 1,
	FT_STATUS_TRANSFERRING	= 2,
	FT_STATUS_FINISHED	= 4,
	FT_STATUS_CANCELED	= 8,
	FT_STATUS_CONNECTING	= 16
} file_status_t;

/*
 * This structure holds all irc specific information regarding an incoming (from the point of view of
 * the irc client) file transfer. New instances of this struct should only be created by calling the
 * imcb_file_send_start() method, which will initialize most of the fields. The data field and the various
 * methods are zero-initialized. Instances will automatically be deleted once the transfer is completed,
 * canceled, or the connection to the irc client has been lost (note that also if only the irc connection
 * and not the dcc connection is lost, the file transfer will still be canceled and freed).
 *
 * The following (poor ascii-art) diagram illustrates what methods are called for which status-changes:
 *
 *	                        /-----------\                    /----------\
 *	               -------> | LISTENING | -----------------> | CANCELED |
 *	                        \-----------/  [canceled,]free   \----------/
 *	                              |
 *	                              | accept
 *	                              V
 *	               /------ /-------------\                    /------------------------\
 *	   out_of_data |       | TRANSFERING | -----------------> | TRANSFERING | CANCELED |
 *	               \-----> \-------------/  [canceled,]free   \------------------------/
 *	                              |
 *	                              | finished,free
 *	                              V
 *	                 /------------------------\
 *	                 | TRANSFERING | FINISHED |
 *	                 \------------------------/
 */
typedef struct file_transfer {

	/* Are we sending something? */
	int sending;

	/*
	 * The current status of this file transfer.
	 */ 
	file_status_t status;
	
	/*
	 * file size
	 */
	size_t file_size;
	
	/*
	 * Number of bytes that have been successfully transferred.
	 */
	size_t bytes_transferred;

	/*
	 * Time started. Used to calculate kb/s.
	 */
	time_t started;

	/*
	 * file name
	 */
	char *file_name;

	/*
	 * A unique local ID for this file transfer.
	 */
	unsigned int local_id;

	/*
	 * IM-protocol specific data associated with this file transfer.
	 */
	gpointer data;
	
	/*
	 * Private data.
	 */
	gpointer priv;
	
	/*
	 * If set, called after succesful connection setup.
	 */
	void (*accept) ( struct file_transfer *file );
	
	/*
	 * If set, called when the transfer is canceled or finished.
	 * Subsequently, this structure will be freed.
	 *
	 */
	void (*free) ( struct file_transfer *file );
	
	/*
	 * If set, called when the transfer is finished and successful.
	 */
	void (*finished) ( struct file_transfer *file );
	
	/*
	 * If set, called when the transfer is canceled.
	 * ( canceled either by the transfer implementation or by
	 *  a call to imcb_file_canceled )
	 */
	void (*canceled) ( struct file_transfer *file, char *reason );
	
	/*
	 * If set, called when the transfer queue is running empty and
	 * more data can be added.
	 */
	void (*out_of_data) ( struct file_transfer *file );

	/*
	 * When sending files, protocols register this function to receive data.
	 */
	gboolean (*write) (struct file_transfer *file, char *buffer, int len );

} file_transfer_t;

/*
 * This starts a file transfer from bitlbee to the user (currently via DCC).
 */
file_transfer_t *imcb_file_send_start( struct im_connection *ic, char *user_nick, char *file_name, size_t file_size );

/*
 * This should be called by a protocol when the transfer is canceled. Note that
 * the canceled() and free() callbacks given in file will be called by this function.
 */
void imcb_file_canceled( file_transfer_t *file, char *reason );

/*
 * The given buffer is queued for transfer and MUST NOT be freed by the caller.
 * When the method returns false the caller should not invoke this method again
 * until out_of_data has been called.
 */
gboolean imcb_file_write( file_transfer_t *file, gpointer data, size_t data_size );

gboolean imcb_file_recv_start( file_transfer_t *ft );
#endif
