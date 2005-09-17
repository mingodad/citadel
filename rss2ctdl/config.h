/*
 * $Id$
 * 
 * Copyright 2003 Oliver Feiler <kiza@kcore.de>
 *
 * config.h
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#ifndef CONFIG_H
#define CONFIG_H

#include <time.h>
#include "netio.h"

/* Set your charset here. ISO-8859-1 is default. */
#ifndef TARGET_CHARSET
#define TARGET_CHARSET "ISO-8859-1"
#endif

struct feed {
	char *feedurl;							/* Non hashified URL */
	char *feed;							/* Raw XML */
	int content_length;
	char *title;
	char *link;
	char *description;
	char *lastmodified;						/* Content of header as sent by the server. */
	int lasthttpstatus;
	char *content_type;
	netio_error_type netio_error;					/* See netio.h */
	int connectresult;						/* Socket errno */
	char *cookies;							/* Login cookies for this feed. */
	char *authinfo;							/* HTTP authinfo string. */
	char *servauth;							/* Server supplied authorization header. */
	struct newsitem *items;
	int problem;							/* Set if there was a problem 
									 * downloading the feed. */
	char *original;							/* Original feed title. */
};

struct newsitem {
	struct newsdata *data;
	struct newsitem *next_ptr, *prev_ptr;	/* Pointer to next/prev item in double linked list */
};

struct newsdata {
	struct feed *parent;
	int readstatus;							/* 0: unread, 1: read */
	char *title;
	char *link;
	char *guid;							/* Not always present */
	char *description;
	time_t date;							/* not always present */
};

extern struct feed *first_ptr;

#ifdef LOCALEPATH
#	include <libintl.h>
#	include <locale.h>
#endif

#ifdef LOCALEPATH
#	define _(String) gettext (String)
#else
#	define _(String) (String)s
# 	define ngettext(Singular, Plural, n) (Plural)
#endif

#endif
