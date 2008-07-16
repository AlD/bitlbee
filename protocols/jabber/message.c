/***************************************************************************\
*                                                                           *
*  BitlBee - An IRC to IM gateway                                           *
*  Jabber module - Handling of message(s) (tags), etc                       *
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

#include "jabber.h"

xt_status jabber_pkt_message( struct xt_node *node, gpointer data )
{
	struct im_connection *ic = data;
	char *from = xt_find_attr( node, "from" );
	char *type = xt_find_attr( node, "type" );
	struct xt_node *body = xt_find_node( node->children, "body" ), *c;
	struct jabber_buddy *bud = NULL;
	char *s;
	
	if( !from )
		return XT_HANDLED; /* Consider this packet corrupted. */
	
	bud = jabber_buddy_by_jid( ic, from, GET_BUDDY_EXACT );
	
	if( type && strcmp( type, "error" ) == 0 )
	{
		/* Handle type=error packet. */
	}
	else if( type && from && strcmp( type, "groupchat" ) == 0 )
	{
		jabber_chat_pkt_message( ic, bud, node );
	}
	else /* "chat", "normal", "headline", no-type or whatever. Should all be pretty similar. */
	{
		GString *fullmsg = g_string_new( "" );

		for( c = node->children; ( c = xt_find_node( c, "x" ) ); c = c->next )
		{
			char *ns = xt_find_attr( c, "xmlns" ), *room;
			struct xt_node *inv, *reason;
			
			if( strcmp( ns, XMLNS_MUC_USER ) == 0 &&
			    ( inv = xt_find_node( c->children, "invite" ) ) )
			{
				room = from;
				from = xt_find_attr( inv, "from" ) ? : from;

				g_string_append_printf( fullmsg, "<< \002BitlBee\002 - Invitation to chatroom %s >>\n", room );
				if( ( reason = xt_find_node( inv->children, "reason" ) ) && reason->text_len > 0 )
					g_string_append( fullmsg, reason->text );
			}
		}
		
		if( ( s = strchr( from, '/' ) ) )
		{
			if( bud )
			{
				bud->last_act = time( NULL );
				from = bud->ext_jid ? : bud->bare_jid;
			}
			else
				*s = 0; /* We need to generate a bare JID now. */
		}
		
		if( type && strcmp( type, "headline" ) == 0 )
		{
			c = xt_find_node( node->children, "subject" );
			g_string_append_printf( fullmsg, "Headline: %s\n", c && c->text_len > 0 ? c->text : "" );
			
			/* <x xmlns="jabber:x:oob"><url>http://....</url></x> can contain a URL, it seems. */
			for( c = node->children; c; c = c->next )
			{
				struct xt_node *url;
				
				if( ( url = xt_find_node( c->children, "url" ) ) && url->text_len > 0 )
					g_string_append_printf( fullmsg, "URL: %s\n", url->text );
			}
		}
		else if( ( c = xt_find_node( node->children, "subject" ) ) && c->text_len > 0 )
		{
			g_string_append_printf( fullmsg, "<< \002BitlBee\002 - Message with subject: %s >>\n", c->text );
		}
		
		if( body && body->text_len > 0 ) /* Could be just a typing notification. */
			fullmsg = g_string_append( fullmsg, body->text );
		
		if( fullmsg->len > 0 )
			imcb_buddy_msg( ic, from, fullmsg->str,
			                0, jabber_get_timestamp( node ) );
		
		g_string_free( fullmsg, TRUE );
		
		/* Handling of incoming typing notifications. */
		if( bud == NULL )
		{
			/* Can't handle these for unknown buddies. */
		}
		else if( xt_find_node( node->children, "composing" ) )
		{
			bud->flags |= JBFLAG_DOES_XEP85;
			imcb_buddy_typing( ic, from, OPT_TYPING );
		}
		/* No need to send a "stopped typing" signal when there's a message. */
		else if( xt_find_node( node->children, "active" ) && ( body == NULL ) )
		{
			bud->flags |= JBFLAG_DOES_XEP85;
			imcb_buddy_typing( ic, from, 0 );
		}
		else if( xt_find_node( node->children, "paused" ) )
		{
			bud->flags |= JBFLAG_DOES_XEP85;
			imcb_buddy_typing( ic, from, OPT_THINKING );
		}
		
		if( s )
			*s = '/'; /* And convert it back to a full JID. */
	}
	
	return XT_HANDLED;
}
