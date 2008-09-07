  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2004 Wilmer van der Gaast and others                *
  \********************************************************************/

/* User manager (root) commands                                         */

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

#define BITLBEE_CORE
#include "commands.h"
#include "crypting.h"
#include "bitlbee.h"
#include "help.h"

#include <string.h>

void root_command_string( irc_t *irc, user_t *u, char *command, int flags )
{
	char *cmd[IRC_MAX_ARGS];
	char *s;
	int k;
	char q = 0;
	
	memset( cmd, 0, sizeof( cmd ) );
	cmd[0] = command;
	k = 1;
	for( s = command; *s && k < ( IRC_MAX_ARGS - 1 ); s ++ )
		if( *s == ' ' && !q )
		{
			*s = 0;
			while( *++s == ' ' );
			if( *s == '"' || *s == '\'' )
			{
				q = *s;
				s ++;
			}
			if( *s )
			{
				cmd[k++] = s;
				s --;
			}
			else
			{
				break;
			}
		}
		else if( *s == '\\' && ( ( !q && s[1] ) || ( q && q == s[1] ) ) )
		{
			char *cpy;
			
			for( cpy = s; *cpy; cpy ++ )
				cpy[0] = cpy[1];
		}
		else if( *s == q )
		{
			q = *s = 0;
		}
	cmd[k] = NULL;
	
	root_command( irc, cmd );
}

void root_command( irc_t *irc, char *cmd[] )
{	
	int i;
	
	if( !cmd[0] )
		return;
	
	for( i = 0; commands[i].command; i++ )
		if( g_strcasecmp( commands[i].command, cmd[0] ) == 0 )
		{
			if( !cmd[commands[i].required_parameters] )
			{
				irc_usermsg( irc, "Not enough parameters given (need %d)", commands[i].required_parameters );
				return;
			}
			commands[i].execute( irc, cmd );
			return;
		}
	
	irc_usermsg( irc, "Unknown command: %s. Please use \x02help commands\x02 to get a list of available commands.", cmd[0] );
}

static void cmd_help( irc_t *irc, char **cmd )
{
	char param[80];
	int i;
	char *s;
	
	memset( param, 0, sizeof(param) );
	for ( i = 1; (cmd[i] != NULL && ( strlen(param) < (sizeof(param)-1) ) ); i++ ) {
		if ( i != 1 )	// prepend space except for the first parameter
			strcat(param, " ");
		strncat( param, cmd[i], sizeof(param) - strlen(param) - 1 );
	}

	s = help_get( &(global.help), param );
	if( !s ) s = help_get( &(global.help), "" );
	
	if( s )
	{
		irc_usermsg( irc, "%s", s );
		g_free( s );
	}
	else
	{
		irc_usermsg( irc, "Error opening helpfile." );
	}
}

static void cmd_account( irc_t *irc, char **cmd );

static void cmd_identify( irc_t *irc, char **cmd )
{
	storage_status_t status = storage_load( irc, cmd[1] );
	char *account_on[] = { "account", "on", NULL };
	
	switch (status) {
	case STORAGE_INVALID_PASSWORD:
		irc_usermsg( irc, "Incorrect password" );
		break;
	case STORAGE_NO_SUCH_USER:
		irc_usermsg( irc, "The nick is (probably) not registered" );
		break;
	case STORAGE_OK:
		irc_usermsg( irc, "Password accepted, settings and accounts loaded" );
		irc_setpass( irc, cmd[1] );
		irc->status |= USTATUS_IDENTIFIED;
		irc_umode_set( irc, "+R", 1 );
		if( set_getbool( &irc->set, "auto_connect" ) )
			cmd_account( irc, account_on );
		break;
	case STORAGE_OTHER_ERROR:
	default:
		irc_usermsg( irc, "Unknown error while loading configuration" );
		break;
	}
}

static void cmd_register( irc_t *irc, char **cmd )
{
	if( global.conf->authmode == AUTHMODE_REGISTERED )
	{
		irc_usermsg( irc, "This server does not allow registering new accounts" );
		return;
	}

	switch( storage_save( irc, cmd[1], FALSE ) ) {
		case STORAGE_ALREADY_EXISTS:
			irc_usermsg( irc, "Nick is already registered" );
			break;
			
		case STORAGE_OK:
			irc_usermsg( irc, "Account successfully created" );
			irc_setpass( irc, cmd[1] );
			irc->status |= USTATUS_IDENTIFIED;
			irc_umode_set( irc, "+R", 1 );
			break;

		default:
			irc_usermsg( irc, "Error registering" );
			break;
	}
}

static void cmd_drop( irc_t *irc, char **cmd )
{
	storage_status_t status;
	
	status = storage_remove (irc->nick, cmd[1]);
	switch (status) {
	case STORAGE_NO_SUCH_USER:
		irc_usermsg( irc, "That account does not exist" );
		break;
	case STORAGE_INVALID_PASSWORD:
		irc_usermsg( irc, "Password invalid" );
		break;
	case STORAGE_OK:
		irc_setpass( irc, NULL );
		irc->status &= ~USTATUS_IDENTIFIED;
		irc_umode_set( irc, "-R", 1 );
		irc_usermsg( irc, "Account `%s' removed", irc->nick );
		break;
	default:
		irc_usermsg( irc, "Error: `%d'", status );
		break;
	}
}

struct cmd_account_del_data
{
	account_t *a;
	irc_t *irc;
};

void cmd_account_del_yes( void *data )
{
	struct cmd_account_del_data *cad = data;
	account_t *a;
	
	for( a = cad->irc->accounts; a && a != cad->a; a = a->next );
	
	if( a == NULL )
	{
		irc_usermsg( cad->irc, "Account already deleted" );
	}
	else if( a->ic )
	{
		irc_usermsg( cad->irc, "Account is still logged in, can't delete" );
	}
	else
	{
		account_del( cad->irc, a );
		irc_usermsg( cad->irc, "Account deleted" );
	}
	g_free( data );
}

void cmd_account_del_no( void *data )
{
	g_free( data );
}

static void cmd_showset( irc_t *irc, set_t **head, char *key )
{
	char *val;
	
	if( ( val = set_getstr( head, key ) ) )
		irc_usermsg( irc, "%s = `%s'", key, val );
	else
		irc_usermsg( irc, "%s is empty", key );
}

static void cmd_account( irc_t *irc, char **cmd )
{
	account_t *a;
	
	if( global.conf->authmode == AUTHMODE_REGISTERED && !( irc->status & USTATUS_IDENTIFIED ) )
	{
		irc_usermsg( irc, "This server only accepts registered users" );
		return;
	}
	
	if( g_strcasecmp( cmd[1], "add" ) == 0 )
	{
		struct prpl *prpl;
		
		if( cmd[2] == NULL || cmd[3] == NULL || cmd[4] == NULL )
		{
			irc_usermsg( irc, "Not enough parameters" );
			return;
		}
		
		prpl = find_protocol(cmd[2]);
		
		if( prpl == NULL )
		{
			irc_usermsg( irc, "Unknown protocol" );
			return;
		}

		a = account_add( irc, prpl, cmd[3], cmd[4] );
		if( cmd[5] )
		{
			irc_usermsg( irc, "Warning: Passing a servername/other flags to `account add' "
			                  "is now deprecated. Use `account set' instead." );
			set_setstr( &a->set, "server", cmd[5] );
		}
		
		irc_usermsg( irc, "Account successfully added" );
	}
	else if( g_strcasecmp( cmd[1], "del" ) == 0 )
	{
		if( !cmd[2] )
		{
			irc_usermsg( irc, "Not enough parameters given (need %d)", 2 );
		}
		else if( !( a = account_get( irc, cmd[2] ) ) )
		{
			irc_usermsg( irc, "Invalid account" );
		}
		else if( a->ic )
		{
			irc_usermsg( irc, "Account is still logged in, can't delete" );
		}
		else
		{
			struct cmd_account_del_data *cad;
			char *msg;
			
			cad = g_malloc( sizeof( struct cmd_account_del_data ) );
			cad->a = a;
			cad->irc = irc;
			
			msg = g_strdup_printf( "If you remove this account (%s(%s)), BitlBee will "
			                       "also forget all your saved nicknames. If you want "
			                       "to change your username/password, use the `account "
			                       "set' command. Are you sure you want to delete this "
			                       "account?", a->prpl->name, a->user );
			query_add( irc, NULL, msg, cmd_account_del_yes, cmd_account_del_no, cad );
			g_free( msg );
		}
	}
	else if( g_strcasecmp( cmd[1], "list" ) == 0 )
	{
		int i = 0;
		
		if( strchr( irc->umode, 'b' ) )
			irc_usermsg( irc, "Account list:" );
		
		for( a = irc->accounts; a; a = a->next )
		{
			char *con;
			
			if( a->ic && ( a->ic->flags & OPT_LOGGED_IN ) )
				con = " (connected)";
			else if( a->ic )
				con = " (connecting)";
			else if( a->reconnect )
				con = " (awaiting reconnect)";
			else
				con = "";
			
			irc_usermsg( irc, "%2d. %s, %s%s", i, a->prpl->name, a->user, con );
			
			i ++;
		}
		irc_usermsg( irc, "End of account list" );
	}
	else if( g_strcasecmp( cmd[1], "on" ) == 0 )
	{
		if( cmd[2] )
		{
			if( ( a = account_get( irc, cmd[2] ) ) )
			{
				if( a->ic )
				{
					irc_usermsg( irc, "Account already online" );
					return;
				}
				else
				{
					account_on( irc, a );
				}
			}
			else
			{
				irc_usermsg( irc, "Invalid account" );
				return;
			}
		}
		else
		{
			if ( irc->accounts ) {
				irc_usermsg( irc, "Trying to get all accounts connected..." );
			
				for( a = irc->accounts; a; a = a->next )
					if( !a->ic && a->auto_connect )
						account_on( irc, a );
			} 
			else
			{
				irc_usermsg( irc, "No accounts known. Use `account add' to add one." );
			}
		}
	}
	else if( g_strcasecmp( cmd[1], "off" ) == 0 )
	{
		if( !cmd[2] )
		{
			irc_usermsg( irc, "Deactivating all active (re)connections..." );
			
			for( a = irc->accounts; a; a = a->next )
			{
				if( a->ic )
					account_off( irc, a );
				else if( a->reconnect )
					cancel_auto_reconnect( a );
			}
		}
		else if( ( a = account_get( irc, cmd[2] ) ) )
		{
			if( a->ic )
			{
				account_off( irc, a );
			}
			else if( a->reconnect )
			{
				cancel_auto_reconnect( a );
				irc_usermsg( irc, "Reconnect cancelled" );
			}
			else
			{
				irc_usermsg( irc, "Account already offline" );
				return;
			}
		}
		else
		{
			irc_usermsg( irc, "Invalid account" );
			return;
		}
	}
	else if( g_strcasecmp( cmd[1], "set" ) == 0 )
	{
		char *acc_handle, *set_name = NULL, *tmp;
		
		if( !cmd[2] )
		{
			irc_usermsg( irc, "Not enough parameters given (need %d)", 2 );
			return;
		}
		
		if( g_strncasecmp( cmd[2], "-del", 4 ) == 0 )
			acc_handle = g_strdup( cmd[3] );
		else
			acc_handle = g_strdup( cmd[2] );
		
		if( !acc_handle )
		{
			irc_usermsg( irc, "Not enough parameters given (need %d)", 3 );
			return;
		}
		
		if( ( tmp = strchr( acc_handle, '/' ) ) )
		{
			*tmp = 0;
			set_name = tmp + 1;
		}
		
		if( ( a = account_get( irc, acc_handle ) ) == NULL )
		{
			g_free( acc_handle );
			irc_usermsg( irc, "Invalid account" );
			return;
		}
		
		if( cmd[3] && set_name )
		{
			set_t *s = set_find( &a->set, set_name );
			int st;
			
			if( a->ic && s && s->flags & ACC_SET_OFFLINE_ONLY )
			{
				g_free( acc_handle );
				irc_usermsg( irc, "This setting can only be changed when the account is %s-line", "off" );
				return;
			}
			else if( !a->ic && s && s->flags & ACC_SET_ONLINE_ONLY )
			{
				g_free( acc_handle );
				irc_usermsg( irc, "This setting can only be changed when the account is %s-line", "on" );
				return;
			}
			
			if( g_strncasecmp( cmd[2], "-del", 4 ) == 0 )
				st = set_reset( &a->set, set_name );
			else
				st = set_setstr( &a->set, set_name, cmd[3] );
			
			if( set_getstr( &a->set, set_name ) == NULL )
			{
				if( st )
					irc_usermsg( irc, "Setting changed successfully" );
				else
					irc_usermsg( irc, "Failed to change setting" );
			}
			else
			{
				cmd_showset( irc, &a->set, set_name );
			}
		}
		else if( set_name )
		{
			cmd_showset( irc, &a->set, set_name );
		}
		else
		{
			set_t *s = a->set;
			while( s )
			{
				cmd_showset( irc, &s, s->key );
				s = s->next;
			}
		}
		
		g_free( acc_handle );
	}
	else
	{
		irc_usermsg( irc, "Unknown command: account %s. Please use \x02help commands\x02 to get a list of available commands.", cmd[1] );
	}
}

static void cmd_add( irc_t *irc, char **cmd )
{
	account_t *a;
	int add_on_server = 1;
	
	if( g_strcasecmp( cmd[1], "-tmp" ) == 0 )
	{
		add_on_server = 0;
		cmd ++;
	}
	
	if( !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account" );
		return;
	}
	else if( !( a->ic && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
		return;
	}
	
	if( cmd[3] )
	{
		if( !nick_ok( cmd[3] ) )
		{
			irc_usermsg( irc, "The requested nick `%s' is invalid", cmd[3] );
			return;
		}
		else if( user_find( irc, cmd[3] ) )
		{
			irc_usermsg( irc, "The requested nick `%s' already exists", cmd[3] );
			return;
		}
		else
		{
			nick_set( a, cmd[2], cmd[3] );
		}
	}
	
	if( add_on_server )
		a->ic->acc->prpl->add_buddy( a->ic, cmd[2], NULL );
	else
		/* Yeah, officially this is a call-*back*... So if we just
		   called add_buddy, we'll wait for the IM server to respond
		   before we do this. */
		imcb_add_buddy( a->ic, cmd[2], NULL );
	
	irc_usermsg( irc, "Adding `%s' to your contact list", cmd[2]  );
}

static void cmd_info( irc_t *irc, char **cmd )
{
	struct im_connection *ic;
	account_t *a;
	
	if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->ic )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return;
		}
		ic = u->ic;
		cmd[2] = u->handle;
	}
	else if( !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account" );
		return;
	}
	else if( !( ( ic = a->ic ) && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
		return;
	}
	
	if( !ic->acc->prpl->get_info )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
	}
	else
	{
		ic->acc->prpl->get_info( ic, cmd[2] );
	}
}

static void cmd_rename( irc_t *irc, char **cmd )
{
	user_t *u;
	
	if( g_strcasecmp( cmd[1], irc->nick ) == 0 )
	{
		irc_usermsg( irc, "Nick `%s' can't be changed", cmd[1] );
	}
	else if( user_find( irc, cmd[2] ) && ( nick_cmp( cmd[1], cmd[2] ) != 0 ) )
	{
		irc_usermsg( irc, "Nick `%s' already exists", cmd[2] );
	}
	else if( !nick_ok( cmd[2] ) )
	{
		irc_usermsg( irc, "Nick `%s' is invalid", cmd[2] );
	}
	else if( !( u = user_find( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
	}
	else
	{
		user_rename( irc, cmd[1], cmd[2] );
		irc_write( irc, ":%s!%s@%s NICK %s", cmd[1], u->user, u->host, cmd[2] );
		if( g_strcasecmp( cmd[1], irc->mynick ) == 0 )
		{
			g_free( irc->mynick );
			irc->mynick = g_strdup( cmd[2] );
			
			/* If we're called internally (user did "set root_nick"),
			   let's not go O(INF). :-) */
			if( strcmp( cmd[0], "set_rename" ) != 0 )
				set_setstr( &irc->set, "root_nick", cmd[2] );
		}
		else if( u->send_handler == buddy_send_handler )
		{
			nick_set( u->ic->acc, u->handle, cmd[2] );
		}
		
		irc_usermsg( irc, "Nick successfully changed" );
	}
}

char *set_eval_root_nick( set_t *set, char *new_nick )
{
	irc_t *irc = set->data;
	
	if( strcmp( irc->mynick, new_nick ) != 0 )
	{
		char *cmd[] = { "set_rename", irc->mynick, new_nick, NULL };
		
		cmd_rename( irc, cmd );
	}
	
	return strcmp( irc->mynick, new_nick ) == 0 ? new_nick : SET_INVALID;
}

static void cmd_remove( irc_t *irc, char **cmd )
{
	user_t *u;
	char *s;
	
	if( !( u = user_find( irc, cmd[1] ) ) || !u->ic )
	{
		irc_usermsg( irc, "Buddy `%s' not found", cmd[1] );
		return;
	}
	s = g_strdup( u->handle );
	
	u->ic->acc->prpl->remove_buddy( u->ic, u->handle, NULL );
	nick_del( u->ic->acc, u->handle );
	user_del( irc, cmd[1] );
	
	irc_usermsg( irc, "Buddy `%s' (nick %s) removed from contact list", s, cmd[1] );
	g_free( s );
	
	return;
}

static void cmd_block( irc_t *irc, char **cmd )
{
	struct im_connection *ic;
	account_t *a;
	
	if( !cmd[2] && ( a = account_get( irc, cmd[1] ) ) && a->ic )
	{
		char *format;
		GSList *l;
		
		if( strchr( irc->umode, 'b' ) != NULL )
			format = "%s\t%s";
		else
			format = "%-32.32s  %-16.16s";
		
		irc_usermsg( irc, format, "Handle", "Nickname" );
		for( l = a->ic->deny; l; l = l->next )
		{
			user_t *u = user_findhandle( a->ic, l->data );
			irc_usermsg( irc, format, l->data, u ? u->nick : "(none)" );
		}
		irc_usermsg( irc, "End of list." );
		
		return;
	}
	else if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->ic )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return;
		}
		ic = u->ic;
		cmd[2] = u->handle;
	}
	else if( !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account" );
		return;
	}
	else if( !( ( ic = a->ic ) && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
		return;
	}
	
	if( !ic->acc->prpl->add_deny || !ic->acc->prpl->rem_permit )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
	}
	else
	{
		imc_rem_allow( ic, cmd[2] );
		imc_add_block( ic, cmd[2] );
		irc_usermsg( irc, "Buddy `%s' moved from your allow- to your block-list", cmd[2] );
	}
}

static void cmd_allow( irc_t *irc, char **cmd )
{
	struct im_connection *ic;
	account_t *a;
	
	if( !cmd[2] && ( a = account_get( irc, cmd[1] ) ) && a->ic )
	{
		char *format;
		GSList *l;
		
		if( strchr( irc->umode, 'b' ) != NULL )
			format = "%s\t%s";
		else
			format = "%-32.32s  %-16.16s";
		
		irc_usermsg( irc, format, "Handle", "Nickname" );
		for( l = a->ic->permit; l; l = l->next )
		{
			user_t *u = user_findhandle( a->ic, l->data );
			irc_usermsg( irc, format, l->data, u ? u->nick : "(none)" );
		}
		irc_usermsg( irc, "End of list." );
		
		return;
	}
	else if( !cmd[2] )
	{
		user_t *u = user_find( irc, cmd[1] );
		if( !u || !u->ic )
		{
			irc_usermsg( irc, "Nick `%s' does not exist", cmd[1] );
			return;
		}
		ic = u->ic;
		cmd[2] = u->handle;
	}
	else if( !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account" );
		return;
	}
	else if( !( ( ic = a->ic ) && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
		return;
	}
	
	if( !ic->acc->prpl->rem_deny || !ic->acc->prpl->add_permit )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
	}
	else
	{
		imc_rem_block( ic, cmd[2] );
		imc_add_allow( ic, cmd[2] );
		
		irc_usermsg( irc, "Buddy `%s' moved from your block- to your allow-list", cmd[2] );
	}
}

static void cmd_yesno( irc_t *irc, char **cmd )
{
	query_t *q = NULL;
	int numq = 0;
	
	if( irc->queries == NULL )
	{
		irc_usermsg( irc, "Did I ask you something?" );
		return;
	}
	
	/* If there's an argument, the user seems to want to answer another question than the
	   first/last (depending on the query_order setting) one. */
	if( cmd[1] )
	{
		if( sscanf( cmd[1], "%d", &numq ) != 1 )
		{
			irc_usermsg( irc, "Invalid query number" );
			return;
		}
		
		for( q = irc->queries; q; q = q->next, numq -- )
			if( numq == 0 )
				break;
		
		if( !q )
		{
			irc_usermsg( irc, "Uhm, I never asked you something like that..." );
			return;
		}
	}
	
	if( g_strcasecmp( cmd[0], "yes" ) == 0 )
		query_answer( irc, q, 1 );
	else if( g_strcasecmp( cmd[0], "no" ) == 0 )
		query_answer( irc, q, 0 );
}

static void cmd_set( irc_t *irc, char **cmd )
{
	char *set_name = cmd[1];
	
	if( cmd[1] && cmd[2] )
	{
		int st;
		
		if( g_strncasecmp( cmd[1], "-del", 4 ) == 0 )
		{
			st = set_reset( &irc->set, cmd[2] );
			set_name = cmd[2];
		}
		else
		{
			st = set_setstr( &irc->set, cmd[1], cmd[2] );
		}
		
		/* Normally we just show the variable's new/unchanged
		   value as feedback to the user, but this has always
		   caused confusion when changing the password. Give
		   other feedback instead: */
		if( set_getstr( &irc->set, set_name ) == NULL )
		{
			if( st )
				irc_usermsg( irc, "Setting changed successfully" );
			else
				irc_usermsg( irc, "Failed to change setting" );
		}
		else
		{
			cmd_showset( irc, &irc->set, set_name );
		}
	}
	else if( set_name )
	{
		cmd_showset( irc, &irc->set, set_name );

		if( strchr( set_name, '/' ) )
			irc_usermsg( irc, "Warning: / found in setting name, you're probably looking for the `account set' command." );
	}
	else
	{
		set_t *s = irc->set;
		while( s )
		{
			cmd_showset( irc, &s, s->key );
			s = s->next;
		}
	}
}

static void cmd_save( irc_t *irc, char **cmd )
{
	if( ( irc->status & USTATUS_IDENTIFIED ) == 0 )
		irc_usermsg( irc, "Please create an account first" );
	else if( storage_save( irc, NULL, TRUE ) == STORAGE_OK )
		irc_usermsg( irc, "Configuration saved" );
	else
		irc_usermsg( irc, "Configuration could not be saved!" );
}

static void cmd_blist( irc_t *irc, char **cmd )
{
	int online = 0, away = 0, offline = 0;
	user_t *u;
	char s[256];
	char *format;
	int n_online = 0, n_away = 0, n_offline = 0;
	
	if( cmd[1] && g_strcasecmp( cmd[1], "all" ) == 0 )
		online = offline = away = 1;
	else if( cmd[1] && g_strcasecmp( cmd[1], "offline" ) == 0 )
		offline = 1;
	else if( cmd[1] && g_strcasecmp( cmd[1], "away" ) == 0 )
		away = 1;
	else if( cmd[1] && g_strcasecmp( cmd[1], "online" ) == 0 )
		online = 1;
	else
		online =  away = 1;
	
	if( strchr( irc->umode, 'b' ) != NULL )
		format = "%s\t%s\t%s";
	else
		format = "%-16.16s  %-40.40s  %s";
	
	irc_usermsg( irc, format, "Nick", "User/Host/Network", "Status" );
	
	for( u = irc->users; u; u = u->next ) if( u->ic && u->online && !u->away )
	{
		if( online == 1 )
		{
			g_snprintf( s, sizeof( s ) - 1, "%s@%s %s(%s)", u->user, u->host, u->ic->acc->prpl->name, u->ic->acc->user );
			irc_usermsg( irc, format, u->nick, s, "Online" );
		}
		
		n_online ++;
	}

	for( u = irc->users; u; u = u->next ) if( u->ic && u->online && u->away )
	{
		if( away == 1 )
		{
			g_snprintf( s, sizeof( s ) - 1, "%s@%s %s(%s)", u->user, u->host, u->ic->acc->prpl->name, u->ic->acc->user );
			irc_usermsg( irc, format, u->nick, s, u->away );
		}
		n_away ++;
	}
	
	for( u = irc->users; u; u = u->next ) if( u->ic && !u->online )
	{
		if( offline == 1 )
		{
			g_snprintf( s, sizeof( s ) - 1, "%s@%s %s(%s)", u->user, u->host, u->ic->acc->prpl->name, u->ic->acc->user );
			irc_usermsg( irc, format, u->nick, s, "Offline" );
		}
		n_offline ++;
	}
	
	irc_usermsg( irc, "%d buddies (%d available, %d away, %d offline)", n_online + n_away + n_offline, n_online, n_away, n_offline );
}

static void cmd_nick( irc_t *irc, char **cmd ) 
{
	account_t *a;

	if( !cmd[1] || !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account");
	}
	else if( !( a->ic && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
	}
	else if ( !cmd[2] ) 
	{
		irc_usermsg( irc, "Your name is `%s'" , a->ic->displayname ? a->ic->displayname : "NULL" );
	}
	else if ( !a->prpl->set_my_name ) 
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
	}
	else
	{
		irc_usermsg( irc, "Setting your name to `%s'", cmd[2] );
		
		a->prpl->set_my_name( a->ic, cmd[2] );
	}
}

static void cmd_qlist( irc_t *irc, char **cmd )
{
	query_t *q = irc->queries;
	int num;
	
	if( !q )
	{
		irc_usermsg( irc, "There are no pending questions." );
		return;
	}
	
	irc_usermsg( irc, "Pending queries:" );
	
	for( num = 0; q; q = q->next, num ++ )
		if( q->ic ) /* Not necessary yet, but it might come later */
			irc_usermsg( irc, "%d, %s(%s): %s", num, q->ic->acc->prpl->name, q->ic->acc->user, q->question );
		else
			irc_usermsg( irc, "%d, BitlBee: %s", num, q->question );
}

static void cmd_join_chat( irc_t *irc, char **cmd )
{
	account_t *a;
	struct im_connection *ic;
	char *chat, *channel, *nick = NULL, *password = NULL;
	struct groupchat *c;
	
	if( !( a = account_get( irc, cmd[1] ) ) )
	{
		irc_usermsg( irc, "Invalid account" );
		return;
	}
	else if( !( a->ic && ( a->ic->flags & OPT_LOGGED_IN ) ) )
	{
		irc_usermsg( irc, "That account is not on-line" );
		return;
	}
	else if( a->prpl->chat_join == NULL )
	{
		irc_usermsg( irc, "Command `%s' not supported by this protocol", cmd[0] );
		return;
	}
	ic = a->ic;
	
	chat = cmd[2];
	if( cmd[3] )
	{
		if( cmd[3][0] != '#' && cmd[3][0] != '&' )
			channel = g_strdup_printf( "&%s", cmd[3] );
		else
			channel = g_strdup( cmd[3] );
	}
	else
	{
		char *s;
		
		channel = g_strdup_printf( "&%s", chat );
		if( ( s = strchr( channel, '@' ) ) )
			*s = 0;
	}
	if( cmd[3] && cmd[4] )
		nick = cmd[4];
	else
		nick = irc->nick;
	if( cmd[3] && cmd[4] && cmd[5] )
		password = cmd[5];
	
	if( !nick_ok( channel + 1 ) )
	{
		irc_usermsg( irc, "Invalid channel name: %s", channel );
		g_free( channel );
		return;
	}
	else if( g_strcasecmp( channel, irc->channel ) == 0 || irc_chat_by_channel( irc, channel ) )
	{
		irc_usermsg( irc, "Channel already exists: %s", channel );
		g_free( channel );
		return;
	}
	
	if( ( c = a->prpl->chat_join( ic, chat, nick, password ) ) )
	{
		g_free( c->channel );
		c->channel = channel;
	}
	else
	{
		irc_usermsg( irc, "Tried to join chat, not sure if this was successful" );
		g_free( channel );
	}
}

const command_t commands[] = {
	{ "help",           0, cmd_help,           0 }, 
	{ "identify",       1, cmd_identify,       0 },
	{ "register",       1, cmd_register,       0 },
	{ "drop",           1, cmd_drop,           0 },
	{ "account",        1, cmd_account,        0 },
	{ "add",            2, cmd_add,            0 },
	{ "info",           1, cmd_info,           0 },
	{ "rename",         2, cmd_rename,         0 },
	{ "remove",         1, cmd_remove,         0 },
	{ "block",          1, cmd_block,          0 },
	{ "allow",          1, cmd_allow,          0 },
	{ "save",           0, cmd_save,           0 },
	{ "set",            0, cmd_set,            0 },
	{ "yes",            0, cmd_yesno,          0 },
	{ "no",             0, cmd_yesno,          0 },
	{ "blist",          0, cmd_blist,          0 },
	{ "nick",           1, cmd_nick,           0 },
	{ "qlist",          0, cmd_qlist,          0 },
	{ "join_chat",      2, cmd_join_chat,      0 },
	{ NULL }
};
