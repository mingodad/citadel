/*
 * $Id$
 * 
 * Copyright 2003-2004 Oliver Feiler <kiza@kcore.de>
 *
 * os-support.c
 *
 * Library support functions.
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

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <ctype.h>

#include "main.h"

/******************************************************************************
 * This is a replacement for strsep which is not portable (missing on Solaris).
 *
 * http://www.winehq.com/hypermail/wine-patches/2001/11/0024.html
 *
 * The following function was written by François Gouget.
 */
#ifdef SUN
char* strsep(char** str, const char* delims)
{
    char* token;

    if (*str==NULL) {
        /* No more tokens */
        return NULL;
    }

    token=*str;
    while (**str!='\0') {
        if (strchr(delims,**str)!=NULL) {
            **str='\0';
            (*str)++;
            return token;
        }
        (*str)++;
    }
    /* There is no other token */
    *str=NULL;
   return token;
}
#endif

/* strcasestr stolen from: http://www.unixpapa.com/incnote/string.html */
char *s_strcasestr(char *a, char *b) {
	size_t l;
	char f[3];
	int lena = strlen(a);
	int lenb = strlen(b);
	
	snprintf(f, sizeof(f), "%c%c", tolower(*b), toupper(*b));
	for (l = strcspn(a, f); l != lena; l += strcspn(a + l + 1, f) + 1)
		if (strncasecmp(a + l, b, lenb) == 0)
			return(a + l);
	return(NULL);
}


/* Private malloc wrapper. Aborts program execution if malloc fails. */
void * s_malloc (size_t size) {
	void *newmem;
	
	newmem = malloc (size);
	
	if (newmem == NULL) {
		fprintf(stderr, "Error allocating memory: %s\n", strerror(errno));
		abort();
	}
	
	return newmem;
}
