/*
 * $Id$
 *
 * vCard implementation for Citadel/UX
 *
 * Copyright (C) 1999 by Art Cancro
 * This code is freely redistributable under the terms of the GNU General
 * Public License.  All other rights reserved.
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <syslog.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "vcard.h"

/* 
 * Constructor (empty vCard)
 */
struct vCard *new_vcard() {
	struct vCard *v;

	v = (struct vCard *) mallok(sizeof(struct vCard));
	if (v == NULL) return v;

	v->magic = CTDL_VCARD_MAGIC;
	v->numprops = 0;
	v->prop = NULL;

	return v;
}


/*
 * Constructor (supply serialized vCard)
 */
struct vCard *load_vcard(char *vtext) {
	struct vCard *v;
	int valid = 0;
	char *mycopy, *ptr;
	char *namebuf, *valuebuf;
	int i;
	int colonpos, nlpos;

	mycopy = strdoop(vtext);
	if (mycopy == NULL) return NULL;

	/* First, fix this big pile o' vCard to make it more parseable.
	 * To make it easier to parse, we convert CRLF to LF, and unfold any
	 * multi-line fields into single lines.
	 */
	for (i=0; i<strlen(mycopy); ++i) {
		if (!strncmp(&mycopy[i], "\r\n", 2)) {
			strcpy(&mycopy[i], &mycopy[i+1]);
		}
		if ( (mycopy[i]=='\n') && (isspace(mycopy[i+1])) ) {
			strcpy(&mycopy[i], &mycopy[i+1]);
		}
	}

	v = new_vcard();
	if (v == NULL) return v;

	ptr = mycopy;
	while (strlen(ptr)>0) {
		colonpos = (-1);
		nlpos = (-1);
		colonpos = pattern2(ptr, ":");
		nlpos = pattern2(ptr, "\n");

		if (nlpos > colonpos > 0) {
			namebuf = mallok(colonpos + 1);
			valuebuf = mallok(nlpos - colonpos + 1);
			strncpy(namebuf, ptr, colonpos);
			namebuf[colonpos] = 0;
			strncpy(valuebuf, &ptr[colonpos+1], nlpos-colonpos-1);
			valuebuf[nlpos-colonpos-1] = 0;

			if ( (!strcasecmp(namebuf, "begin"))
			   && (!strcasecmp(valuebuf, "vcard")) )  valid = 1;
			if ( (!strcasecmp(namebuf, "end"))
			   && (!strcasecmp(valuebuf, "vcard")) )  valid = 0;

			if (valid) {
				++v->numprops;
				v->prop = reallok(v->prop,
					(v->numprops * sizeof(char *) * 2) );
				v->prop[v->numprops-1].name = namebuf;
				v->prop[v->numprops-1].value = valuebuf;
			}
			else {
				phree(namebuf);
				phree(valuebuf);
			}

		}

		while ( (*ptr != '\n') && (strlen(ptr)>0) ) {
			++ptr;
		}
		if (*ptr == '\n') ++ptr;
	}

	phree(mycopy);
	return v;
}



/*
 * Destructor
 */
void free_vcard(struct vCard *v) {
	int i;

	if (v->magic != CTDL_VCARD_MAGIC) return;	/* Self-check */
	
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		phree(v->prop[i].name);
		phree(v->prop[i].value);
	}

	if (v->prop != NULL) phree(v->prop);
	
	memset(v, 0, sizeof(struct vCard));
}


/*
 * Set a name/value pair in the card
 */
void set_prop(struct vCard *v, char *name, char *value) {
	int i;

	if (v->magic != CTDL_VCARD_MAGIC) return;	/* Self-check */

	/* If this key is already present, replace it */
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if (!strcasecmp(v->prop[i].name, name)) {
			phree(v->prop[i].name);
			phree(v->prop[i].value);
			v->prop[i].name = strdoop(name);
			v->prop[i].value = strdoop(value);
			return;
		}
	}

	/* Otherwise, append it */
	++v->numprops;
	v->prop = reallok(v->prop,
		(v->numprops * sizeof(char *) * 2) );
	v->prop[v->numprops-1].name = strdoop(name);
	v->prop[v->numprops-1].value = (value);
}




/*
 * Serialize a struct vcard into a standard text/x-vcard MIME type.
 *
 */
char *serialize_vcard(struct vCard *v)
{
	char *ser;
	int i;
	size_t len;

	if (v->magic != CTDL_VCARD_MAGIC) return NULL;	/* self check */

	/* Figure out how big a buffer we need to allocate */
	len = 64;	/* for begin, end, and a little padding for safety */
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		len = len +
			strlen(v->prop[i].name) +
			strlen(v->prop[i].value) + 4;
	}

	ser = mallok(len);
	if (ser == NULL) return NULL;

	strcpy(ser, "begin:vcard\r\n");
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		strcat(ser, v->prop[i].name);
		strcat(ser, ":");
		strcat(ser, v->prop[i].name);
		strcat(ser, "\r\n");
	}
	strcat(ser, "end:vcard\r\n");

	return ser;
}
