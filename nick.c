  /********************************************************************\
  * BitlBee -- An IRC to other IM-networks gateway                     *
  *                                                                    *
  * Copyright 2002-2007 Wilmer van der Gaast and others                *
  \********************************************************************/

/* Some stuff to fetch, save and handle nicknames for your buddies      */

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
#include "bitlbee.h"

/* Store handles in lower case and strip spaces, because AIM is braindead. */
static char *clean_handle( const char *orig )
{
	char *new = g_malloc( strlen( orig ) + 1 );
	int i = 0;
	
	do {
		if (*orig != ' ')
			new[i++] = tolower( *orig );
	}
	while (*(orig++));
	
	return new;
}

void nick_set( account_t *acc, const char *handle, const char *nick )
{
	char *store_handle, *store_nick = g_malloc( MAX_NICK_LENGTH + 1 );
	
	store_handle = clean_handle( handle );
	store_nick[MAX_NICK_LENGTH] = 0;
	strncpy( store_nick, nick, MAX_NICK_LENGTH );
	nick_strip( store_nick );
	
	g_hash_table_replace( acc->nicks, store_handle, store_nick );
}

char *nick_get( account_t *acc, const char *handle )
{
	static char nick[MAX_NICK_LENGTH+1];
	char *store_handle, *found_nick;
	
	memset( nick, 0, MAX_NICK_LENGTH + 1 );
	
	store_handle = clean_handle( handle );
	/* Find out if we stored a nick for this person already. If not, try
	   to generate a sane nick automatically. */
	if( ( found_nick = g_hash_table_lookup( acc->nicks, store_handle ) ) )
	{
		strncpy( nick, found_nick, MAX_NICK_LENGTH );
	}
	else
	{
		char *s;
		
		g_snprintf( nick, MAX_NICK_LENGTH, "%s", handle );
		if( ( s = strchr( nick, '@' ) ) )
			while( *s )
				*(s++) = 0;
		
		nick_strip( nick );
		if( set_getbool( &acc->irc->set, "lcnicks" ) )
			nick_lc( nick );
	}
	g_free( store_handle );
	
	/* Make sure the nick doesn't collide with an existing one by adding
	   underscores and that kind of stuff, if necessary. */
	nick_dedupe( acc, handle, nick );
	
	return nick;
}

void nick_dedupe( account_t *acc, const char *handle, char nick[MAX_NICK_LENGTH+1] )
{
	int inf_protection = 256;
	
	/* Now, find out if the nick is already in use at the moment, and make
	   subtle changes to make it unique. */
	while( !nick_ok( nick ) || user_find( acc->irc, nick ) )
	{
		if( strlen( nick ) < ( MAX_NICK_LENGTH - 1 ) )
		{
			nick[strlen(nick)+1] = 0;
			nick[strlen(nick)] = '_';
		}
		else
		{
			nick[0] ++;
		}
		
		if( inf_protection-- == 0 )
		{
			int i;
			
			irc_usermsg( acc->irc, "Warning: Almost had an infinite loop in nick_get()! "
			                       "This used to be a fatal BitlBee bug, but we tried to fix it. "
			                       "This message should *never* appear anymore. "
			                       "If it does, please *do* send us a bug report! "
			                       "Please send all the following lines in your report:" );
			
			irc_usermsg( acc->irc, "Trying to get a sane nick for handle %s", handle );
			for( i = 0; i < MAX_NICK_LENGTH; i ++ )
				irc_usermsg( acc->irc, "Char %d: %c/%d", i, nick[i], nick[i] );
			
			irc_usermsg( acc->irc, "FAILED. Returning an insane nick now. Things might break. "
			                       "Good luck, and please don't forget to paste the lines up here "
			                       "in #bitlbee on OFTC or in a mail to wilmer@gaast.net" );
			
			g_snprintf( nick, MAX_NICK_LENGTH + 1, "xx%x", rand() );
			
			break;
		}
	}
}

/* Just check if there is a nickname set for this buddy or if we'd have to
   generate one. */
int nick_saved( account_t *acc, const char *handle )
{
	char *store_handle, *found;
	
	store_handle = clean_handle( handle );
	found = g_hash_table_lookup( acc->nicks, store_handle );
	g_free( store_handle );
	
	return found != NULL;
}

void nick_del( account_t *acc, const char *handle )
{
	g_hash_table_remove( acc->nicks, handle );
}


/* Character maps, _lc_[x] == _uc_[x] (but uppercase), according to the RFC's.
   With one difference, we allow dashes. */

static char *nick_lc_chars = "0123456789abcdefghijklmnopqrstuvwxyz{}^`-_|";
static char *nick_uc_chars = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ[]~`-_\\";

void nick_strip( char *nick )
{
	int i, j;
	
	for( i = j = 0; nick[i] && j < MAX_NICK_LENGTH; i++ )
	{
		if( strchr( nick_lc_chars, nick[i] ) || 
		    strchr( nick_uc_chars, nick[i] ) )
		{
			nick[j] = nick[i];
			j++;
		}
	}
	if( isdigit( nick[0] ) )
	{
		char *orig;
		
		orig = g_strdup( nick );
		g_snprintf( nick, MAX_NICK_LENGTH, "_%s", orig );
		g_free( orig );
		j ++;
	}
	while( j <= MAX_NICK_LENGTH )
		nick[j++] = '\0';
}

int nick_ok( const char *nick )
{
	const char *s;
	
	/* Empty/long nicks are not allowed, nor numbers at [0] */
	if( !*nick || isdigit( nick[0] ) || strlen( nick ) > MAX_NICK_LENGTH )
		return( 0 );
	
	for( s = nick; *s; s ++ )
		if( !strchr( nick_lc_chars, *s ) && !strchr( nick_uc_chars, *s ) )
			return( 0 );
	
	return( 1 );
}

int nick_lc( char *nick )
{
	static char tab[128] = { 0 };
	int i;
	
	if( tab['A'] == 0 )
		for( i = 0; nick_lc_chars[i]; i ++ )
		{
			tab[(int)nick_uc_chars[i]] = nick_lc_chars[i];
			tab[(int)nick_lc_chars[i]] = nick_lc_chars[i];
		}
	
	for( i = 0; nick[i]; i ++ )
	{
		if( !tab[(int)nick[i]] )
			return( 0 );
		
		nick[i] = tab[(int)nick[i]];
	}
	
	return( 1 );
}

int nick_uc( char *nick )
{
	static char tab[128] = { 0 };
	int i;
	
	if( tab['A'] == 0 )
		for( i = 0; nick_lc_chars[i]; i ++ )
		{
			tab[(int)nick_uc_chars[i]] = nick_uc_chars[i];
			tab[(int)nick_lc_chars[i]] = nick_uc_chars[i];
		}
	
	for( i = 0; nick[i]; i ++ )
	{
		if( !tab[(int)nick[i]] )
			return( 0 );
		
		nick[i] = tab[(int)nick[i]];
	}
	
	return( 1 );
}

int nick_cmp( const char *a, const char *b )
{
	char aa[1024] = "", bb[1024] = "";
	
	strncpy( aa, a, sizeof( aa ) - 1 );
	strncpy( bb, b, sizeof( bb ) - 1 );
	if( nick_lc( aa ) && nick_lc( bb ) )
	{
		return( strcmp( aa, bb ) );
	}
	else
	{
		return( -1 );	/* Hmm... Not a clear answer.. :-/ */
	}
}

char *nick_dup( const char *nick )
{
	return g_strndup( nick, MAX_NICK_LENGTH );
}
