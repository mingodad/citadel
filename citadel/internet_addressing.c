/*
 * $Id$
 *
 * This file contains functions which handle the mapping of Internet addresses
 * to users on the Citadel system.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "user_ops.h"
#include "room_ops.h"
#include "parsedate.h"
#include "database.h"


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


struct trynamebuf {
	char buffer1[SIZ];
	char buffer2[SIZ];
};

char *inetcfg = NULL;
struct spamstrings_t *spamstrings = NULL;


/*
 * Return nonzero if the supplied name is an alias for this host.
 */
int CtdlHostAlias(char *fqdn) {
	int config_lines;
	int i;
	char buf[256];
	char host[256], type[256];

	if (fqdn == NULL) return(hostalias_nomatch);
	if (IsEmptyStr(fqdn)) return(hostalias_nomatch);
	if (!strcasecmp(fqdn, "localhost")) return(hostalias_localhost);
	if (!strcasecmp(fqdn, config.c_fqdn)) return(hostalias_localhost);
	if (!strcasecmp(fqdn, config.c_nodename)) return(hostalias_localhost);
	if (inetcfg == NULL) return(hostalias_nomatch);

	config_lines = num_tokens(inetcfg, '\n');
	for (i=0; i<config_lines; ++i) {
		extract_token(buf, inetcfg, i, '\n', sizeof buf);
		extract_token(host, buf, 0, '|', sizeof host);
		extract_token(type, buf, 1, '|', sizeof type);

		if ( (!strcasecmp(type, "localhost"))
		   && (!strcasecmp(fqdn, host)))
			return(hostalias_localhost);

		if ( (!strcasecmp(type, "gatewaydomain"))
		   && (!strcasecmp(&fqdn[strlen(fqdn)-strlen(host)], host)))
			return(hostalias_gatewaydomain);

		if ( (!strcasecmp(type, "directory"))
		   && (!strcasecmp(&fqdn[strlen(fqdn)-strlen(host)], host)))
			return(hostalias_directory);

		if ( (!strcasecmp(type, "masqdomain"))
		   && (!strcasecmp(&fqdn[strlen(fqdn)-strlen(host)], host)))
			return(hostalias_masq);

	}

	return(hostalias_nomatch);
}







/*
 * Return 0 if a given string fuzzy-matches a Citadel user account
 *
 * FIXME ... this needs to be updated to handle aliases.
 */
int fuzzy_match(struct ctdluser *us, char *matchstring) {
	int a;
	long len;

	if ( (!strncasecmp(matchstring, "cit", 3)) 
	   && (atol(&matchstring[3]) == us->usernum)) {
		return 0;
	}

	len = strlen(matchstring);
	for (a=0; !IsEmptyStr(&us->fullname[a]); ++a) {
		if (!strncasecmp(&us->fullname[a],
		   matchstring, len)) {
			return 0;
		}
	}
	return -1;
}


/*
 * Unfold a multi-line field into a single line, removing multi-whitespaces
 */
void unfold_rfc822_field(char *field) {
	int i;
	int quote = 0;

	striplt(field);		/* remove leading/trailing whitespace */

	/* convert non-space whitespace to spaces, and remove double blanks */
	for (i=0; i<strlen(field); ++i) {
		if (field[i]=='\"') quote = 1 - quote;
		if (!quote) {
			if (isspace(field[i])) field[i] = ' ';
			while (isspace(field[i]) && isspace(field[i+1])) {
				strcpy(&field[i+1], &field[i+2]);
			}
		}
	}
}



/*
 * Split an RFC822-style address into userid, host, and full name
 *
 */
void process_rfc822_addr(const char *rfc822, char *user, char *node, char *name)
{
	int a;

	strcpy(user, "");
	strcpy(node, config.c_fqdn);
	strcpy(name, "");

	if (rfc822 == NULL) return;

	/* extract full name - first, it's From minus <userid> */
	strcpy(name, rfc822);
	stripout(name, '<', '>');

	/* strip anything to the left of a bang */
	while ((!IsEmptyStr(name)) && (haschar(name, '!') > 0))
		strcpy(name, &name[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(name); ++a) {
		if (name[a] == '@')
			name[a] = 0;
		if (name[a] == '%')
			name[a] = 0;
	}

	/* but if there are parentheses, that changes the rules... */
	if ((haschar(rfc822, '(') == 1) && (haschar(rfc822, ')') == 1)) {
		strcpy(name, rfc822);
		stripallbut(name, '(', ')');
	}

	/* but if there are a set of quotes, that supersedes everything */
	if (haschar(rfc822, 34) == 2) {
		strcpy(name, rfc822);
		while ((!IsEmptyStr(name)) && (name[0] != 34)) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == 34)
				name[a] = 0;
	}
	/* extract user id */
	strcpy(user, rfc822);

	/* first get rid of anything in parens */
	stripout(user, '(', ')');

	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(user, '<') == 1) && (haschar(user, '>') == 1)) {
		stripallbut(user, '<', '>');
	}

	/* strip anything to the left of a bang */
	while ((!IsEmptyStr(user)) && (haschar(user, '!') > 0))
		strcpy(user, &user[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(user); ++a) {
		if (user[a] == '@')
			user[a] = 0;
		if (user[a] == '%')
			user[a] = 0;
	}


	/* extract node name */
	strcpy(node, rfc822);

	/* first get rid of anything in parens */
	stripout(node, '(', ')');

	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(node, '<') == 1) && (haschar(node, '>') == 1)) {
		stripallbut(node, '<', '>');
	}

	/* If no node specified, tack ours on instead */
	if (
		(haschar(node, '@')==0)
		&& (haschar(node, '%')==0)
		&& (haschar(node, '!')==0)
	) {
		strcpy(node, config.c_nodename);
	}

	else {

		/* strip anything to the left of a @ */
		while ((!IsEmptyStr(node)) && (haschar(node, '@') > 0))
			strcpy(node, &node[1]);
	
		/* strip anything to the left of a % */
		while ((!IsEmptyStr(node)) && (haschar(node, '%') > 0))
			strcpy(node, &node[1]);
	
		/* reduce multiple system bang paths to node!user */
		while ((!IsEmptyStr(node)) && (haschar(node, '!') > 1))
			strcpy(node, &node[1]);
	
		/* now get rid of the user portion of a node!user string */
		for (a = 0; a < strlen(node); ++a)
			if (node[a] == '!')
				node[a] = 0;
	}

	/* strip leading and trailing spaces in all strings */
	striplt(user);
	striplt(node);
	striplt(name);

	/* If we processed a string that had the address in angle brackets
	 * but no name outside the brackets, we now have an empty name.  In
	 * this case, use the user portion of the address as the name.
	 */
	if ((IsEmptyStr(name)) && (!IsEmptyStr(user))) {
		strcpy(name, user);
	}
}



/*
 * convert_field() is a helper function for convert_internet_message().
 * Given start/end positions for an rfc822 field, it converts it to a Citadel
 * field if it wants to, and unfolds it if necessary.
 *
 * Returns 1 if the field was converted and inserted into the Citadel message
 * structure, implying that the source field should be removed from the
 * message text.
 */
int convert_field(struct CtdlMessage *msg, int beg, int end) {
	char *rfc822;
	char *key, *value;
	int i;
	int colonpos = (-1);
	int processed = 0;
	char buf[SIZ];
	char user[1024];
	char node[1024];
	char name[1024];
	char addr[1024];
	time_t parsed_date;

	rfc822 = msg->cm_fields['M'];	/* M field contains rfc822 text */
	for (i = end; i >= beg; --i) {
		if (rfc822[i] == ':') colonpos = i;
	}

	if (colonpos < 0) return(0);	/* no colon? not a valid header line */

	key = malloc((end - beg) + 2);
	safestrncpy(key, &rfc822[beg], (end-beg)+1);
	key[colonpos - beg] = 0;
	value = &key[(colonpos - beg) + 1];
	unfold_rfc822_field(value);

	/*
	 * Here's the big rfc822-to-citadel loop.
	 */

	/* Date/time is converted into a unix timestamp.  If the conversion
	 * fails, we replace it with the time the message arrived locally.
	 */
	if (!strcasecmp(key, "Date")) {
		parsed_date = parsedate(value);
		if (parsed_date < 0L) parsed_date = time(NULL);
		snprintf(buf, sizeof buf, "%ld", (long)parsed_date );
		if (msg->cm_fields['T'] == NULL)
			msg->cm_fields['T'] = strdup(buf);
		processed = 1;
	}

	else if (!strcasecmp(key, "From")) {
		process_rfc822_addr(value, user, node, name);
		CtdlLogPrintf(CTDL_DEBUG, "Converted to <%s@%s> (%s)\n", user, node, name);
		snprintf(addr, sizeof addr, "%s@%s", user, node);
		if (msg->cm_fields['A'] == NULL)
			msg->cm_fields['A'] = strdup(name);
		processed = 1;
		if (msg->cm_fields['F'] == NULL)
			msg->cm_fields['F'] = strdup(addr);
		processed = 1;
	}

	else if (!strcasecmp(key, "Subject")) {
		if (msg->cm_fields['U'] == NULL)
			msg->cm_fields['U'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "To")) {
		if (msg->cm_fields['R'] == NULL)
			msg->cm_fields['R'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "CC")) {
		if (msg->cm_fields['Y'] == NULL)
			msg->cm_fields['Y'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "Message-ID")) {
		if (msg->cm_fields['I'] != NULL) {
			CtdlLogPrintf(CTDL_WARNING, "duplicate message id\n");
		}

		if (msg->cm_fields['I'] == NULL) {
			msg->cm_fields['I'] = strdup(value);

			/* Strip angle brackets */
			while (haschar(msg->cm_fields['I'], '<') > 0) {
				strcpy(&msg->cm_fields['I'][0],
					&msg->cm_fields['I'][1]);
			}
			for (i = 0; i<strlen(msg->cm_fields['I']); ++i)
				if (msg->cm_fields['I'][i] == '>')
					msg->cm_fields['I'][i] = 0;
		}

		processed = 1;
	}

	else if (!strcasecmp(key, "Return-Path")) {
		if (msg->cm_fields['P'] == NULL)
			msg->cm_fields['P'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "Envelope-To")) {
		if (msg->cm_fields['V'] == NULL)
			msg->cm_fields['V'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "References")) {
		if (msg->cm_fields['W'] != NULL) {
			free(msg->cm_fields['W']);
		}
		msg->cm_fields['W'] = strdup(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "In-reply-to")) {
		if (msg->cm_fields['W'] == NULL) {		/* References: supersedes In-reply-to: */
			msg->cm_fields['W'] = strdup(value);
		}
		processed = 1;
	}



	/* Clean up and move on. */
	free(key);	/* Don't free 'value', it's actually the same buffer */
	return(processed);
}


/*
 * Convert RFC822 references format (References) to Citadel references format (Weferences)
 */
void convert_references_to_wefewences(char *str) {
	int bracket_nesting = 0;
	char *ptr = str;
	char *moveptr = NULL;
	char ch;

	while(*ptr) {
		ch = *ptr;
		if (ch == '>') {
			--bracket_nesting;
			if (bracket_nesting < 0) bracket_nesting = 0;
		}
		if ((ch == '>') && (bracket_nesting == 0) && (*(ptr+1)) && (ptr>str) ) {
			*ptr = '|';
			++ptr;
		}
		else if (bracket_nesting > 0) {
			++ptr;
		}
		else {
			moveptr = ptr;
			while (*moveptr) {
				*moveptr = *(moveptr+1);
				++moveptr;
			}
		}
		if (ch == '<') ++bracket_nesting;
	}

}


/*
 * Convert an RFC822 message (headers + body) to a CtdlMessage structure.
 * NOTE: the supplied buffer becomes part of the CtdlMessage structure, and
 * will be deallocated when CtdlFreeMessage() is called.  Therefore, the
 * supplied buffer should be DEREFERENCED.  It should not be freed or used
 * again.
 */
struct CtdlMessage *convert_internet_message(char *rfc822) {

	struct CtdlMessage *msg;
	int pos, beg, end, msglen;
	int done;
	char buf[SIZ];
	int converted;

	msg = malloc(sizeof(struct CtdlMessage));
	if (msg == NULL) return msg;

	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;	/* self check */
	msg->cm_anon_type = 0;			/* never anonymous */
	msg->cm_format_type = FMT_RFC822;	/* internet message */
	msg->cm_fields['M'] = rfc822;

	pos = 0;
	done = 0;

	while (!done) {

		/* Locate beginning and end of field, keeping in mind that
		 * some fields might be multiline
		 */
		beg = pos;
		end = (-1);

		msglen = strlen(rfc822);	
		while ( (end < 0) && (done == 0) ) {

			if ((rfc822[pos]=='\n')
			   && (!isspace(rfc822[pos+1]))) {
				end = pos;
			}

			/* done with headers? */
			if (   (rfc822[pos]=='\n')
			   && ( (rfc822[pos+1]=='\n')
			      ||(rfc822[pos+1]=='\r')) ) {
				end = pos;
				done = 1;
			}

			if (pos >= (msglen-1) ) {
				end = pos;
				done = 1;
			}

			++pos;

		}

		/* At this point we have a field.  Are we interested in it? */
		converted = convert_field(msg, beg, end);

		/* Strip the field out of the RFC822 header if we used it */
		if (converted) {
			strcpy(&rfc822[beg], &rfc822[pos]);
			pos = beg;
		}

		/* If we've hit the end of the message, bail out */
		if (pos > strlen(rfc822)) done = 1;
	}

	/* Follow-up sanity checks... */

	/* If there's no timestamp on this message, set it to now. */
	if (msg->cm_fields['T'] == NULL) {
		snprintf(buf, sizeof buf, "%ld", (long)time(NULL));
		msg->cm_fields['T'] = strdup(buf);
	}

	/* If a W (references, or rather, Wefewences) field is present, we
	 * have to convert it from RFC822 format to Citadel format.
	 */
	if (msg->cm_fields['W'] != NULL) {
		convert_references_to_wefewences(msg->cm_fields['W']);
	}

	return msg;
}



/*
 * Look for a particular header field in an RFC822 message text.  If the
 * requested field is found, it is unfolded (if necessary) and returned to
 * the caller.  The field name is stripped out, leaving only its contents.
 * The caller is responsible for freeing the returned buffer.  If the requested
 * field is not present, or anything else goes wrong, it returns NULL.
 */
char *rfc822_fetch_field(char *rfc822, char *fieldname) {
	char *fieldbuf = NULL;
	char *end_of_headers;
	char *field_start;
	char *ptr;
	char *cont;
	char fieldhdr[SIZ];

	/* Should never happen, but sometimes we get stupid */
	if (rfc822 == NULL) return(NULL);
	if (fieldname == NULL) return(NULL);

	snprintf(fieldhdr, sizeof fieldhdr, "%s:", fieldname);

	/* Locate the end of the headers, so we don't run past that point */
	end_of_headers = bmstrcasestr(rfc822, "\n\r\n");
	if (end_of_headers == NULL) {
		end_of_headers = bmstrcasestr(rfc822, "\n\n");
	}
	if (end_of_headers == NULL) return (NULL);

	field_start = bmstrcasestr(rfc822, fieldhdr);
	if (field_start == NULL) return(NULL);
	if (field_start > end_of_headers) return(NULL);

	fieldbuf = malloc(SIZ);
	strcpy(fieldbuf, "");

	ptr = field_start;
	ptr = memreadline(ptr, fieldbuf, SIZ-strlen(fieldbuf) );
	while ( (isspace(ptr[0])) && (ptr < end_of_headers) ) {
		strcat(fieldbuf, " ");
		cont = &fieldbuf[strlen(fieldbuf)];
		ptr = memreadline(ptr, cont, SIZ-strlen(fieldbuf) );
		striplt(cont);
	}

	strcpy(fieldbuf, &fieldbuf[strlen(fieldhdr)]);
	striplt(fieldbuf);

	return(fieldbuf);
}



/*****************************************************************************
 *                      DIRECTORY MANAGEMENT FUNCTIONS                       *
 *****************************************************************************/

/*
 * Generate the index key for an Internet e-mail address to be looked up
 * in the database.
 */
void directory_key(char *key, char *addr) {
	int i;
	int keylen = 0;

	for (i=0; !IsEmptyStr(&addr[i]); ++i) {
		if (!isspace(addr[i])) {
			key[keylen++] = tolower(addr[i]);
		}
	}
	key[keylen++] = 0;

	CtdlLogPrintf(CTDL_DEBUG, "Directory key is <%s>\n", key);
}



/* Return nonzero if the supplied address is in a domain we keep in
 * the directory
 */
int IsDirectory(char *addr, int allow_masq_domains) {
	char domain[256];
	int h;

	extract_token(domain, addr, 1, '@', sizeof domain);
	striplt(domain);

	h = CtdlHostAlias(domain);

	if ( (h == hostalias_masq) && allow_masq_domains)
		return(1);
	
	if ( (h == hostalias_localhost) || (h == hostalias_directory) ) {
		return(1);
	}
	else {
		return(0);
	}
}


/*
 * Initialize the directory database (erasing anything already there)
 */
void CtdlDirectoryInit(void) {
	cdb_trunc(CDB_DIRECTORY);
}


/*
 * Add an Internet e-mail address to the directory for a user
 */
void CtdlDirectoryAddUser(char *internet_addr, char *citadel_addr) {
	char key[SIZ];

	CtdlLogPrintf(CTDL_DEBUG, "Dir: %s --> %s\n",
		internet_addr, citadel_addr);
	if (IsDirectory(internet_addr, 0) == 0) return;

	directory_key(key, internet_addr);

	cdb_store(CDB_DIRECTORY, key, strlen(key),
		citadel_addr, strlen(citadel_addr)+1 );
}


/*
 * Delete an Internet e-mail address from the directory.
 *
 * (NOTE: we don't actually use or need the citadel_addr variable; it's merely
 * here because the callback API expects to be able to send it.)
 */
void CtdlDirectoryDelUser(char *internet_addr, char *citadel_addr) {
	char key[SIZ];

	directory_key(key, internet_addr);
	cdb_delete(CDB_DIRECTORY, key, strlen(key) );
}


/*
 * Look up an Internet e-mail address in the directory.
 * On success: returns 0, and Citadel address stored in 'target'
 * On failure: returns nonzero
 */
int CtdlDirectoryLookup(char *target, char *internet_addr, size_t targbuflen) {
	struct cdbdata *cdbrec;
	char key[SIZ];

	/* Dump it in there unchanged, just for kicks */
	safestrncpy(target, internet_addr, targbuflen);

	/* Only do lookups for addresses with hostnames in them */
	if (num_tokens(internet_addr, '@') != 2) return(-1);

	/* Only do lookups for domains in the directory */
	if (IsDirectory(internet_addr, 0) == 0) return(-1);

	directory_key(key, internet_addr);
	cdbrec = cdb_fetch(CDB_DIRECTORY, key, strlen(key) );
	if (cdbrec != NULL) {
		safestrncpy(target, cdbrec->ptr, targbuflen);
		cdb_free(cdbrec);
		return(0);
	}

	return(-1);
}


/*
 * Harvest any email addresses that someone might want to have in their
 * "collected addresses" book.
 */
char *harvest_collected_addresses(struct CtdlMessage *msg) {
	char *coll = NULL;
	char addr[256];
	char user[256], node[256], name[256];
	int is_harvestable;
	int i, j, h;
	int field = 0;

	if (msg == NULL) return(NULL);

	is_harvestable = 1;
	strcpy(addr, "");	
	if (msg->cm_fields['A'] != NULL) {
		strcat(addr, msg->cm_fields['A']);
	}
	if (msg->cm_fields['F'] != NULL) {
		strcat(addr, " <");
		strcat(addr, msg->cm_fields['F']);
		strcat(addr, ">");
		if (IsDirectory(msg->cm_fields['F'], 0)) {
			is_harvestable = 0;
		}
	}

	if (is_harvestable) {
		coll = strdup(addr);
	}
	else {
		coll = strdup("");
	}

	if (coll == NULL) return(NULL);

	/* Scan both the R (To) and Y (CC) fields */
	for (i = 0; i < 2; ++i) {
		if (i == 0) field = 'R' ;
		if (i == 1) field = 'Y' ;

		if (msg->cm_fields[field] != NULL) {
			for (j=0; j<num_tokens(msg->cm_fields[field], ','); ++j) {
				extract_token(addr, msg->cm_fields[field], j, ',', sizeof addr);
				process_rfc822_addr(addr, user, node, name);
				h = CtdlHostAlias(node);
				if ( (h != hostalias_localhost) && (h != hostalias_directory) ) {
					coll = realloc(coll, strlen(coll) + strlen(addr) + 4);
					if (coll == NULL) return(NULL);
					if (!IsEmptyStr(coll)) {
						strcat(coll, ",");
					}
					striplt(addr);
					strcat(coll, addr);
				}
			}
		}
	}

	if (IsEmptyStr(coll)) {
		free(coll);
		return(NULL);
	}
	return(coll);
}
