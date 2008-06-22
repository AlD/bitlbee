/***************************************************************************\
*                                                                           *
*  BitlBee - An IRC to IM gateway                                           *
*  Jabber module - Main file                                                *
*                                                                           *
*  Copyright 2006 Wilmer van der Gaast <wilmer@gaast.net>                   *
*                                                                           *
*  This program is free software; you can redistribute it and/or modify     *
*  it under the terms of the GNU General Public License as published by     *
*  the Free Software Foundation; either version 2 of the License, or        *
*  (at your option) any later version.                                      *
*                                                                           *
*  This program is distributed in the hope that it will be useful,          *
*  but WITHOUT ANY WARRANTY; without even the implied warranty of           *
*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            *
*  GNU General Public License for more details.                             *
*                                                                           *
*  You should have received a copy of the GNU General Public License along  *
*  with this program; if not, write to the Free Software Foundation, Inc.,  *
*  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.              *
*                                                                           *
\***************************************************************************/

#ifndef _JABBER_H
#define _JABBER_H

#include <glib.h>

#include "xmltree.h"
#include "bitlbee.h"

extern GSList *jabber_connections;

typedef enum
{
	JFLAG_STREAM_STARTED = 1,       /* Set when we detected the beginning of the stream
	                                   and want to do auth. */
	JFLAG_AUTHENTICATED = 2,        /* Set when we're successfully authenticatd. */
	JFLAG_STREAM_RESTART = 4,       /* Set when we want to restart the stream (after
	                                   SASL or TLS). */
	JFLAG_WAIT_SESSION = 8,	        /* Set if we sent a <session> tag and need a reply
	                                   before we continue. */
	JFLAG_WAIT_BIND = 16,           /* ... for <bind> tag. */
	JFLAG_WANT_TYPING = 32,         /* Set if we ever sent a typing notification, this
	                                   activates all XEP-85 related code. */
	JFLAG_XMLCONSOLE = 64,          /* If the user added an xmlconsole buddy. */
} jabber_flags_t;

typedef enum
{
	JBFLAG_PROBED_XEP85 = 1,        /* Set this when we sent our probe packet to make
	                                   sure it gets sent only once. */
	JBFLAG_DOES_XEP85 = 2,          /* Set this when the resource seems to support
	                                   XEP85 (typing notification shite). */
	JBFLAG_IS_CHATROOM = 4,         /* It's convenient to use this JID thingy for
	                                   groupchat state info too. */
	JBFLAG_IS_ANONYMOUS = 8,        /* For anonymous chatrooms, when we don't have
	                                   have a real JID. */
} jabber_buddy_flags_t;

typedef enum
{
	JCFLAG_MESSAGE_SENT = 1,        /* Set this after sending the first message, so
	                                   we can detect echoes/backlogs. */
} jabber_chat_flags_t;

struct jabber_data
{
	struct im_connection *ic;
	
	int fd;
	void *ssl;
	char *txq;
	int tx_len;
	int r_inpa, w_inpa;
	
	struct xt_parser *xt;
	jabber_flags_t flags;
	
	char *username;		/* USERNAME@server */
	char *server;		/* username@SERVER -=> server/domain, not hostname */
	
	/* After changing one of these two (or the priority setting), call
	   presence_send_update() to inform the server about the changes. */
	struct jabber_away_state *away_state;
	char *away_message;
	
	md5_state_t cached_id_prefix;
	GHashTable *node_cache;
	GHashTable *buddies;
};

struct jabber_away_state
{
	char code[5];
	char *full_name;
};

typedef xt_status (*jabber_cache_event) ( struct im_connection *ic, struct xt_node *node, struct xt_node *orig );

struct jabber_cache_entry
{
	time_t saved_at;
	struct xt_node *node;
	jabber_cache_event func;
};

struct jabber_buddy
{
	char *bare_jid;
	char *full_jid;
	char *resource;
	
	char *ext_jid; /* The JID to use in BitlBee. The real JID if possible, */
	               /* otherwise something similar to the conference JID. */
	
	int priority;
	struct jabber_away_state *away_state;
	char *away_message;
	
	time_t last_act;
	jabber_buddy_flags_t flags;
	
	struct jabber_buddy *next;
};

struct jabber_chat
{
	int flags;
	char *name;
	char *my_full_jid; /* Separate copy because of case sensitivity. */
	struct jabber_buddy *me;
};

#define JABBER_XMLCONSOLE_HANDLE "xmlconsole"

/* Prefixes to use for packet IDs (mainly for IQ packets ATM). Usually the
   first one should be used, but when storing a packet in the cache, a
   "special" kind of ID is assigned to make it easier later to figure out
   if we have to do call an event handler for the response packet. Also
   we'll append a hash to make sure we won't trigger on cached packets from
   other BitlBee users. :-) */
#define JABBER_PACKET_ID "BeeP"
#define JABBER_CACHED_ID "BeeC"

/* The number of seconds to keep cached packets before garbage collecting
   them. This gc is done on every keepalive (every minute). */
#define JABBER_CACHE_MAX_AGE 600

/* RFC 392[01] stuff */
#define XMLNS_TLS          "urn:ietf:params:xml:ns:xmpp-tls"
#define XMLNS_SASL         "urn:ietf:params:xml:ns:xmpp-sasl"
#define XMLNS_BIND         "urn:ietf:params:xml:ns:xmpp-bind"
#define XMLNS_SESSION      "urn:ietf:params:xml:ns:xmpp-session"
#define XMLNS_STANZA_ERROR "urn:ietf:params:xml:ns:xmpp-stanzas"
#define XMLNS_STREAM_ERROR "urn:ietf:params:xml:ns:xmpp-streams"
#define XMLNS_ROSTER       "jabber:iq:roster"

/* Some supported extensions/legacy stuff */
#define XMLNS_AUTH         "jabber:iq:auth"                     /* XEP-0078 */
#define XMLNS_VERSION      "jabber:iq:version"                  /* XEP-0092 */
#define XMLNS_TIME         "jabber:iq:time"                     /* XEP-0090 */
#define XMLNS_PING         "urn:xmpp:ping"                      /* XEP-0199 */
#define XMLNS_VCARD        "vcard-temp"                         /* XEP-0054 */
#define XMLNS_DELAY        "jabber:x:delay"                     /* XEP-0091 */
#define XMLNS_CHATSTATES   "http://jabber.org/protocol/chatstates"  /* 0085 */
#define XMLNS_DISCOVER     "http://jabber.org/protocol/disco#info"  /* 0030 */
#define XMLNS_MUC          "http://jabber.org/protocol/muc"     /* XEP-0045 */
#define XMLNS_MUC_USER     "http://jabber.org/protocol/muc#user"/* XEP-0045 */
#define XMLNS_CAPS         "http://jabber.org/protocol/caps"    /* XEP-0115 */

/* iq.c */
xt_status jabber_pkt_iq( struct xt_node *node, gpointer data );
int jabber_init_iq_auth( struct im_connection *ic );
xt_status jabber_pkt_bind_sess( struct im_connection *ic, struct xt_node *node, struct xt_node *orig );
int jabber_get_roster( struct im_connection *ic );
int jabber_get_vcard( struct im_connection *ic, char *bare_jid );
int jabber_add_to_roster( struct im_connection *ic, char *handle, char *name );
int jabber_remove_from_roster( struct im_connection *ic, char *handle );

/* message.c */
xt_status jabber_pkt_message( struct xt_node *node, gpointer data );

/* presence.c */
xt_status jabber_pkt_presence( struct xt_node *node, gpointer data );
int presence_send_update( struct im_connection *ic );
int presence_send_request( struct im_connection *ic, char *handle, char *request );

/* jabber_util.c */
char *set_eval_priority( set_t *set, char *value );
char *set_eval_tls( set_t *set, char *value );
struct xt_node *jabber_make_packet( char *name, char *type, char *to, struct xt_node *children );
struct xt_node *jabber_make_error_packet( struct xt_node *orig, char *err_cond, char *err_type );
void jabber_cache_add( struct im_connection *ic, struct xt_node *node, jabber_cache_event func );
struct xt_node *jabber_cache_get( struct im_connection *ic, char *id );
void jabber_cache_entry_free( gpointer entry );
void jabber_cache_clean( struct im_connection *ic );
xt_status jabber_cache_handle_packet( struct im_connection *ic, struct xt_node *node );
const struct jabber_away_state *jabber_away_state_by_code( char *code );
const struct jabber_away_state *jabber_away_state_by_name( char *name );
void jabber_buddy_ask( struct im_connection *ic, char *handle );
char *jabber_normalize( const char *orig );

typedef enum
{
	GET_BUDDY_CREAT = 1,	/* Try to create it, if necessary. */
	GET_BUDDY_EXACT = 2,	/* Get an exact match (only makes sense with bare JIDs). */
	GET_BUDDY_FIRST = 4,	/* No selection, simply get the first resource for this JID. */
} get_buddy_flags_t;

struct jabber_error
{
	char *code, *text, *type;
};

struct jabber_buddy *jabber_buddy_add( struct im_connection *ic, char *full_jid );
struct jabber_buddy *jabber_buddy_by_jid( struct im_connection *ic, char *jid, get_buddy_flags_t flags );
struct jabber_buddy *jabber_buddy_by_ext_jid( struct im_connection *ic, char *jid, get_buddy_flags_t flags );
int jabber_buddy_remove( struct im_connection *ic, char *full_jid );
int jabber_buddy_remove_bare( struct im_connection *ic, char *bare_jid );
time_t jabber_get_timestamp( struct xt_node *xt );
struct jabber_error *jabber_error_parse( struct xt_node *node, char *xmlns );
void jabber_error_free( struct jabber_error *err );

extern const struct jabber_away_state jabber_away_state_list[];

/* io.c */
int jabber_write_packet( struct im_connection *ic, struct xt_node *node );
int jabber_write( struct im_connection *ic, char *buf, int len );
gboolean jabber_connected_plain( gpointer data, gint source, b_input_condition cond );
gboolean jabber_connected_ssl( gpointer data, void *source, b_input_condition cond );
gboolean jabber_start_stream( struct im_connection *ic );
void jabber_end_stream( struct im_connection *ic );

/* sasl.c */
xt_status sasl_pkt_mechanisms( struct xt_node *node, gpointer data );
xt_status sasl_pkt_challenge( struct xt_node *node, gpointer data );
xt_status sasl_pkt_result( struct xt_node *node, gpointer data );
gboolean sasl_supported( struct im_connection *ic );

/* conference.c */
struct groupchat *jabber_chat_join( struct im_connection *ic, char *room, char *nick, char *password );
struct groupchat *jabber_chat_by_jid( struct im_connection *ic, const char *name );
void jabber_chat_free( struct groupchat *c );
int jabber_chat_msg( struct groupchat *ic, char *message, int flags );
int jabber_chat_topic( struct groupchat *c, char *topic );
int jabber_chat_leave( struct groupchat *c, const char *reason );
void jabber_chat_pkt_presence( struct im_connection *ic, struct jabber_buddy *bud, struct xt_node *node );
void jabber_chat_pkt_message( struct im_connection *ic, struct jabber_buddy *bud, struct xt_node *node );
void jabber_chat_invite( struct groupchat *c, char *who, char *message );

#endif
