/*
 * $Id$
 * Copyright (C) 1999-2006 by Art Cancro
 * This code is freely redistributable under the terms of the GNU General
 * Public License.  All other rights reserved.
 */
/**
 * \defgroup VCardMain vCard data type implementation for the Citadel system.
 * \ingroup VCards
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"
#include "vcard.h"

/** 
 * \brief Constructor (empty vCard)
 * \return an empty vcard
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

/**
 * \brief	Remove the "charset=" attribute from a vCard property name
 *
 * \param	strbuf		The property name string to be stripped
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
	if (strlen(strbuf) > 0) {
		if (strbuf[strlen(strbuf)-1] == ';') {
			strbuf[strlen(strbuf)-1] = 0;
		}
	}
}


/*
 * \brief	Add a property to a vCard
 *
 * \param	v		vCard structure to which we are adding
 * \param	propname	name of new property
 * \param	propvalue	value of new property
 */
void vcard_add_prop(struct vCard *v, char *propname, char *propvalue) {
	++v->numprops;
	v->prop = realloc(v->prop,
		(v->numprops * sizeof(struct vCardProp)) );
	v->prop[v->numprops-1].name = strdup(propname);
	v->prop[v->numprops-1].value = strdup(propvalue);
}



/**
 * \brief Constructor (supply serialized vCard)
 * \param vtext the text to parse into the new vcard
 * \return the parsed VCard
 */
struct vCard *vcard_load(char *vtext) {
	struct vCard *v;
	int valid = 0;
	char *mycopy, *ptr;
	char *namebuf, *valuebuf;
	int i;
	int len;
	int colonpos, nlpos;

	if (vtext == NULL) return vcard_new();
	mycopy = strdup(vtext);
	if (mycopy == NULL) return NULL;

	/**
	 * First, fix this big pile o' vCard to make it more parseable.
	 * To make it easier to parse, we convert CRLF to LF, and unfold any
	 * multi-line fields into single lines.
	 */
	len = strlen(mycopy);
	for (i=0; i<len; ++i) {
		if (!strncmp(&mycopy[i], "\r\n", 2)) {
			memmove(&mycopy[i], &mycopy[i+1], len - i);
			len --;
		}
		if ( (mycopy[i]=='\n') && (isspace(mycopy[i+1])) ) {
			memmove(&mycopy[i], &mycopy[i+1], len - i);
			len --;
		}
	}

	v = vcard_new();
	if (v == NULL) return v;

	ptr = mycopy;
	while (*ptr != '\0') {
		colonpos = (-1);
		nlpos = (-1);
		colonpos = pattern2(ptr, ":");
		nlpos = pattern2(ptr, "\n");

		if ((nlpos > colonpos) && (colonpos > 0)) {
			namebuf = malloc(colonpos + 1);
			valuebuf = malloc(nlpos - colonpos + 1);
			strncpy(namebuf, ptr, colonpos);
			namebuf[colonpos] = 0;
			strncpy(valuebuf, &ptr[colonpos+1], nlpos-colonpos-1);
			valuebuf[nlpos-colonpos-1] = 0;

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

		while ( (*ptr != '\n') && (*ptr != '\0') ) {
			++ptr;
		}
		if (*ptr == '\n') ++ptr;
	}

	free(mycopy);
	return v;
}


/**
 * \brief Fetch the value of a particular key.
 * If is_partial is set to 1, a partial match is ok (for example,
 * a key of "tel;home" will satisfy a search for "tel").
 * Set "instance" to a value higher than 0 to return subsequent instances
 * of the same key.
 * Set "get_propname" to nonzero to fetch the property name instead of value.
 * \param v vCard to get keyvalue from
 * \param propname key to retrieve
 * \param is_partial dunno???
 * \param instance if >0 return a later token of the value
 * \param get_propname if nonzero get the real property name???
 * \return the requested value / token / propertyname
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




/**
 * \brief Destructor
 * kill a vCard
 * \param v the vCard to purge from memory
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




/**
 * \brief Set a name/value pair in the card
 * \param v vCard to inspect
 * \param name key to set
 * \param value the value to assign to key
 * \param append should we append the value to an existing one?
 */
void vcard_set_prop(struct vCard *v, char *name, char *value, int append) {
	int i;

	if (v->magic != CTDL_VCARD_MAGIC) return;	/** Self-check */

	/** If this key is already present, replace it */
	if (!append) if (v->numprops) for (i=0; i<(v->numprops); ++i) {
		if (!strcasecmp(v->prop[i].name, name)) {
			free(v->prop[i].name);
			free(v->prop[i].value);
			v->prop[i].name = strdup(name);
			v->prop[i].value = strdup(value);
			return;
		}
	}

	/** Otherwise, append it */
	++v->numprops;
	v->prop = realloc(v->prop,
		(v->numprops * sizeof(struct vCardProp)) );
	v->prop[v->numprops-1].name = strdup(name);
	v->prop[v->numprops-1].value = strdup(value);
}




/**
 * \brief Serialize a struct vcard into its standard format.
 * \param v vCard to serialize
 * \return the serialized vCard
 */
char *vcard_serialize(struct vCard *v)
{
	char *ser;
	int i, j;
	size_t len;
	int is_utf8 = 0;

	if (v->magic != CTDL_VCARD_MAGIC) return NULL;	/** self check */

	/** Figure out how big a buffer we need to allocate */
	len = 64;	/** for begin, end, and a little padding for safety */
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
			for (j=0; j<strlen(v->prop[i].value); ++j) {
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
 * \brief	Convert FN (Friendly Name) into N (Name)
 *
 * \param	vname		Supplied friendly-name
 * \param	n		Target buffer to store Name
 * \param	vname_size	Size of buffer
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






/*@}*/
