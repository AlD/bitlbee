/***************************************************************************\
*                                                                           *
*  BitlBee - An IRC to IM gateway                                           *
*  Simple module to facilitate twitter functionality.                       *
*                                                                           *
*  Copyright 2009 Geert Mulders <g.c.w.m.mulders@gmail.com>                 *
*                                                                           *
*  This library is free software; you can redistribute it and/or            *
*  modify it under the terms of the GNU Lesser General Public               *
*  License as published by the Free Software Foundation, version            *
*  2.1.                                                                     *
*                                                                           *
*  This library is distributed in the hope that it will be useful,          *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU        *
*  Lesser General Public License for more details.                          *
*                                                                           *
*  You should have received a copy of the GNU Lesser General Public License *
*  along with this library; if not, write to the Free Software Foundation,  *
*  Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA           *
*                                                                           *
****************************************************************************/

#include "nogaim.h"

#ifndef _TWITTER_H
#define _TWITTER_H

#ifdef DEBUG_TWITTER
#define debug( text... ) imcb_log( ic, text );
#else
#define debug( text... )
#endif

typedef enum
{
	TWITTER_HAVE_FRIENDS = 1,
} twitter_flags_t;

struct twitter_log_data;

struct twitter_data
{
	char* user;
	struct oauth_info *oauth_info;
	GSList *follow_ids;
	
	guint64 home_timeline_id;
	guint64 last_status_id; /* For undo */
	gint main_loop_id;
	struct groupchat *home_timeline_gc;
	gint http_fails;
	twitter_flags_t flags;
	
	/* set base_url */
	gboolean url_ssl;
	int url_port;
	char *url_host;
	char *url_path;

	char *prefix; /* Used to generate contact + channel name. */
	
	/* set show_ids */
	struct twitter_log_data *log;
	int log_id;
};

struct twitter_user_data
{
	guint64 last_id;
	time_t last_time;
};

#define TWITTER_LOG_LENGTH 100
struct twitter_log_data
{
	guint64 id;
	struct bee_user *bu; /* DANGER: can be a dead pointer. Check it first. */
};

/**
 * This has the same function as the msn_connections GSList. We use this to 
 * make sure the connection is still alive in callbacks before we do anything
 * else.
 */
extern GSList *twitter_connections;

void twitter_login_finish( struct im_connection *ic );

struct http_request;
char *twitter_parse_error( struct http_request *req );

#endif //_TWITTER_H
