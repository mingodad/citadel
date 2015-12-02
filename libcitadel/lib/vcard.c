/*
 * vCard implementation for Citadel
 *
 * Copyright (C) 1999-2008 by the citadel.org development team.
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <string.h>
#include <libcitadel.h>


/* 
 * Constructor (empty vCard)
 * Returns an empty vcard
 */
struct vCard *vcard_new() {
	struct vCard *v;

	v = (struct vCard *) malloc(sizeof(struct vCard));
	if (v == NULL) return v;

	v->magic = CTDL_VCARD_MAGIC;
	v->numprops = 0;
	v->prop = NULL;

	return v;
}

/*
 * Remove the "charset=" attribute from a vCard property name
 *
 */
void remove_charset_attribute(char *strbuf)
{
	int i, t;
	char compare[256];

	t = num_tokens(strbuf, ';');
	for (i=0; i<t; ++i) {
		extract_token(compare, strbuf, i, ';', sizeof compare);
		striplt(compare);
		if (!strncasecmp(compare, "charset=", 8)) {
			remove_token(strbuf, i, ';');
		}
	}
	if (!IsEmptyStr(strbuf)) {
		if (strbuf[strlen(strbuf)-1] == ';') {
			strbuf[strlen(strbuf)-1] = 0;
		}
	}
}


/*
 * Add a property to a vCard
 *
 * v		vCard structure to which we are adding
 * propname	name of new property
 * propvalue	value of new property
 */
void vcard_add_prop(struct vCard *v, const char *propname, const char *propvalue) {
	++v->numprops;
	v->prop = realloc(v->prop,
		(v->numprops * sizeof(struct vCardProp)) );
	v->prop[v->numprops-1].name = strdup(propname);
	v->prop[v->numprops-1].value = strdup(propvalue);
}

/*
 * Constructor - returns a new struct vcard given a serialized vcard
 */
struct vCard *VCardLoad(StrBuf *vbtext) {
	return vcard_load((char*)ChrPtr(vbtext));
}

/*
 * Constructor - returns a new struct vcard given a serialized vcard
 */
struct vCard *vcard_load(char *vtext) {
	struct vCard *v;
	int valid = 0;
	char *mycopy, *ptr;
	char *namebuf, *valuebuf;
	int i;
	int colonpos, nlpos;

	if (vtext == NULL) return vcard_new();
	mycopy = strdup(vtext);
	if (mycopy == NULL) return NULL;

	/*
	 * First, fix this big pile o' vCard to make it more parseable.
	 * To make it easier to parse, we convert CRLF to LF, and unfold any
	 * multi-line fields into single lines.
	 */
	for (i=0; !IsEmptyStr(&mycopy[i]); ++i) {
		if (!strncmp(&mycopy[i], "\r\n", 2)) {
			strcpy(&mycopy[i], &mycopy[i+1]);
		}
		if ( (mycopy[i]=='\n') && (isspace(mycopy[i+1])) ) {
			strcpy(&mycopy[i], &mycopy[i+1]);
		}
	}

	v = vcard_new();
	if (v == NULL)
	{
		free(mycopy);
		return v;
	}

	ptr = mycopy;
	while (!IsEmptyStr(ptr)) {
		colonpos = pattern2(ptr, ":");
		nlpos = pattern2(ptr, "\n");

		if ((nlpos > colonpos) && (colonpos > 0)) {
			namebuf = malloc(colonpos + 1);
			valuebuf = malloc(nlpos - colonpos + 1);
			memcpy(namebuf, ptr, colonpos);
			namebuf[colonpos] = '\0';
			memcpy(valuebuf, &ptr[colonpos+1], nlpos-colonpos-1);
			valuebuf[nlpos-colonpos-1] = '\0';

			if (!strcasecmp(namebuf, "end")) {
				valid = 0;
			}
			if (	(!strcasecmp(namebuf, "begin"))
				&& (!strcasecmp(valuebuf, "vcard"))
			) {
				valid = 1;
			}

			if ( (valid) && (strcasecmp(namebuf, "begin")) ) {
				remove_charset_attribute(namebuf);
				++v->numprops;
				v->prop = realloc(v->prop,
					(v->numprops * sizeof(struct vCardProp))
				);
				v->prop[v->numprops-1].name = namebuf;
				v->prop[v->numprops-1].value = valuebuf;
			} 
			else {
				free(namebuf);
				free(valuebuf);
			}

		}

		while ( (*ptr != '\n') && (!IsEmptyStr(ptr)) ) {
			++ptr;
		}
		if (*ptr == '\n') ++ptr;
	}

	free(mycopy);
	return v;
}


/*
 * Fetch the value of a particular key.
 * If is_partial is set to 1, a partial match is ok (for example,
 * a key of "tel;home" will satisfy a search for "tel").
 * Set "instance" to a value higher than 0 to return subsequent instances
 * of the same key.
 *
 * Set "get_propname" to nonzero to fetch the property name instead of value.
 * v		vCard to get keyvalue from
 * propname	key to retrieve
 * is_partial
 * instance	if nonzero return a later token of the value
 * get_propname	if nonzero get the real property name???
 *
 * returns the requested value / token / propertyname
 */
char *vcard_get_prop(struct vCard *v, char *propname,
			int is_partial, int instance, int get_propname) {
	int i;
	int found_instance = 0;

	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if ( (!strcasecmp(v->prop[i].name, propname))
		   || (propname[0] == 0)
		   || (  (!strncasecmp(v->prop[i].name,
					propname, strlen(propname)))
			 && (v->prop[i].name[strlen(propname)] == ';')
			 && (is_partial) ) ) {
			if (instance == found_instance++) {
				if (get_propname) {
					return(v->prop[i].name);
				}
				else {
					return(v->prop[i].value);
				}
			}
		}
	}

	return NULL;
}




/*
 * Destructor
 */
void vcard_free(struct vCard *v) {
	int i;
	
	if (v->magic != CTDL_VCARD_MAGIC) return;	/* Self-check */
	
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		free(v->prop[i].name);
		free(v->prop[i].value);
	}

	if (v->prop != NULL) free(v->prop);
	
	memset(v, 0, sizeof(struct vCard));
	free(v);
}




/*
 * Set a name/value pair in the card
 * v		vCard to manipulate
 * name		key to set
 * value	the value to assign to key
 * append	if nonzero, append rather than replace if this key already exists.
 */
void vcard_set_prop(struct vCard *v, char *name, char *value, int append) {
	int i;

	if (v->magic != CTDL_VCARD_MAGIC) return;	/* Self-check */

	/* If this key is already present, replace it */
	if (!append) if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if (!strcasecmp(v->prop[i].name, name)) {
			free(v->prop[i].name);
			free(v->prop[i].value);
			v->prop[i].name = strdup(name);
			v->prop[i].value = strdup(value);
			return;
		}
	}

	/* Otherwise, append it */
	++v->numprops;
	v->prop = realloc(v->prop,
		(v->numprops * sizeof(struct vCardProp)) );
	v->prop[v->numprops-1].name = strdup(name);
	v->prop[v->numprops-1].value = strdup(value);
}




/*
 * Serialize a 'struct vcard' into an actual vcard.
 */
char *vcard_serialize(struct vCard *v)
{
	char *ser;
	int i, j;
	size_t len;
	int is_utf8 = 0;

	if (v == NULL) return NULL;			/* self check */
	if (v->magic != CTDL_VCARD_MAGIC) return NULL;	/* self check */

	/* Set the vCard version number to 2.1 at this time. */
	vcard_set_prop(v, "VERSION", "2.1", 0);

	/* Figure out how big a buffer we need to allocate */
	len = 64;	/* for begin, end, and a little padding for safety */
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		len = len +
			strlen(v->prop[i].name) +
			strlen(v->prop[i].value) + 16;
	}

	ser = malloc(len);
	if (ser == NULL) return NULL;

	safestrncpy(ser, "begin:vcard\r\n", len);
	if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if ( (strcasecmp(v->prop[i].name, "end")) && (v->prop[i].value != NULL) ) {
			is_utf8 = 0;
			for (j=0; !IsEmptyStr(&v->prop[i].value[j]); ++j) {
				if ( (v->prop[i].value[j] < 32) || (v->prop[i].value[j] > 126) ) {
					is_utf8 = 1;
				}
			}
			strcat(ser, v->prop[i].name);
			if (is_utf8) {
				strcat(ser, ";charset=UTF-8");
			}
			strcat(ser, ":");
			strcat(ser, v->prop[i].value);
			strcat(ser, "\r\n");
		}
	}
	strcat(ser, "end:vcard\r\n");

	return ser;
}



/*
 * Convert FN (Friendly Name) into N (Name)
 *
 * vname	Supplied friendly-name
 * n		Target buffer to store Name
 * vname_size	Size of buffer
 */
void vcard_fn_to_n(char *vname, char *n, size_t vname_size) {
	char lastname[256];
	char firstname[256];
	char middlename[256];
	char honorific_prefixes[256];
	char honorific_suffixes[256];
	char buf[256];

	safestrncpy(buf, n, sizeof buf);

	/* Try to intelligently convert the screen name to a
	 * fully expanded vCard name based on the number of
	 * words in the name
	 */
	safestrncpy(lastname, "", sizeof lastname);
	safestrncpy(firstname, "", sizeof firstname);
	safestrncpy(middlename, "", sizeof middlename);
	safestrncpy(honorific_prefixes, "", sizeof honorific_prefixes);
	safestrncpy(honorific_suffixes, "", sizeof honorific_suffixes);

	/* Honorific suffixes */
	if (num_tokens(buf, ',') > 1) {
		extract_token(honorific_suffixes, buf, (num_tokens(buf, ' ') - 1), ',',
			sizeof honorific_suffixes);
		remove_token(buf, (num_tokens(buf, ',') - 1), ',');
	}

	/* Find a last name */
	extract_token(lastname, buf, (num_tokens(buf, ' ') - 1), ' ', sizeof lastname);
	remove_token(buf, (num_tokens(buf, ' ') - 1), ' ');

	/* Find honorific prefixes */
	if (num_tokens(buf, ' ') > 2) {
		extract_token(honorific_prefixes, buf, 0, ' ', sizeof honorific_prefixes);
		remove_token(buf, 0, ' ');
	}

	/* Find a middle name */
	if (num_tokens(buf, ' ') > 1) {
		extract_token(middlename, buf, (num_tokens(buf, ' ') - 1), ' ', sizeof middlename);
		remove_token(buf, (num_tokens(buf, ' ') - 1), ' ');
	}

	/* Anything left is probably the first name */
	safestrncpy(firstname, buf, sizeof firstname);
	striplt(firstname);

	/* Compose the structured name */
	snprintf(vname, vname_size, "%s;%s;%s;%s;%s", lastname, firstname, middlename,
		honorific_prefixes, honorific_suffixes);
}




