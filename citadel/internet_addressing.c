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

#include "ctdl_module.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


#ifdef HAVE_ICONV
#include <iconv.h>

#if 0
/* This is the non-define version in case of s.b. needing to debug */
inline void FindNextEnd (char *bptr, char *end)
{
	/* Find the next ?Q? */
	end = strchr(bptr + 2, '?');
	if (end == NULL) return NULL;
	if (((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && 
	    (*(end + 2) == '?')) {
		/* skip on to the end of the cluster, the next ?= */
		end = strstr(end + 3, "?=");
	}
	else
		/* sort of half valid encoding, try to find an end. */
		end = strstr(bptr, "?=");
}
#endif

#define FindNextEnd(bptr, end) { \
	end = strchr(bptr + 2, '?'); \
	if (end != NULL) { \
		if (((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && (*(end + 2) == '?')) { \
			end = strstr(end + 3, "?="); \
		} else end = strstr(bptr, "?="); \
	} \
}

/*
 * Handle subjects with RFC2047 encoding such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 */
void utf8ify_rfc822_string(char *buf) {
	char *start, *end, *next, *nextend, *ptr;
	char newbuf[1024];
	char charset[128];
	char encoding[16];
	char istr[1024];
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;			/**< Buffer of characters to be converted */
	char *obuf;			/**< Buffer for converted characters */
	size_t ibuflen;			/**< Length of input buffer */
	size_t obuflen;			/**< Length of output buffer */
	char *isav;			/**< Saved pointer to input buffer */
	char *osav;			/**< Saved pointer to output buffer */
	int passes = 0;
	int i, len, delta;
	int illegal_non_rfc2047_encoding = 0;

	/* Sometimes, badly formed messages contain strings which were simply
	 *  written out directly in some foreign character set instead of
	 *  using RFC2047 encoding.  This is illegal but we will attempt to
	 *  handle it anyway by converting from a user-specified default
	 *  charset to UTF-8 if we see any nonprintable characters.
	 */
	len = strlen(buf);
	for (i=0; i<len; ++i) {
		if ((buf[i] < 32) || (buf[i] > 126)) {
			illegal_non_rfc2047_encoding = 1;
			i = len; ///< take a shortcut, it won't be more than one.
		}
	}
	if (illegal_non_rfc2047_encoding) {
		const char *default_header_charset = "iso-8859-1";
		if ( (strcasecmp(default_header_charset, "UTF-8")) && (strcasecmp(default_header_charset, "us-ascii")) ) {
			ctdl_iconv_open("UTF-8", default_header_charset, &ic);
			if (ic != (iconv_t)(-1) ) {
				ibuf = malloc(1024);
				isav = ibuf;
				safestrncpy(ibuf, buf, 1024);
				ibuflen = strlen(ibuf);
				obuflen = 1024;
				obuf = (char *) malloc(obuflen);
				osav = obuf;
				iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
				osav[1024-obuflen] = 0;
				strcpy(buf, osav);
				free(osav);
				iconv_close(ic);
				free(isav);
			}
		}
	}

	/* pre evaluate the first pair */
	nextend = end = NULL;
	len = strlen(buf);
	start = strstr(buf, "=?");
	if (start != NULL) 
		FindNextEnd (start, end);

	while ((start != NULL) && (end != NULL))
	{
		next = strstr(end, "=?");
		if (next != NULL)
			FindNextEnd(next, nextend);
		if (nextend == NULL)
			next = NULL;

		/* did we find two partitions */
		if ((next != NULL) && 
		    ((next - end) > 2))
		{
			ptr = end + 2;
			while ((ptr < next) && 
			       (isspace(*ptr) ||
				(*ptr == '\r') ||
				(*ptr == '\n') || 
				(*ptr == '\t')))
				ptr ++;
			/* did we find a gab just filled with blanks? */
			if (ptr == next)
			{
				memmove (end + 2,
					 next,
					 len - (next - start));

				/* now terminate the gab at the end */
				delta = (next - end) - 2;
				len -= delta;
				buf[len] = '\0';

				/* move next to its new location. */
				next -= delta;
				nextend -= delta;
			}
		}
		/* our next-pair is our new first pair now. */
		start = next;
		end = nextend;
	}

	/* Now we handle foreign character sets properly encoded
	 * in RFC2047 format.
	 */
	start = strstr(buf, "=?");
	FindNextEnd((start != NULL)? start : buf, end);
	while (start != NULL && end != NULL && end > start)
	{
		extract_token(charset, start, 1, '?', sizeof charset);
		extract_token(encoding, start, 2, '?', sizeof encoding);
		extract_token(istr, start, 3, '?', sizeof istr);

		ibuf = malloc(1024);
		isav = ibuf;
		if (!strcasecmp(encoding, "B")) {	/**< base64 */
			ibuflen = CtdlDecodeBase64(ibuf, istr, strlen(istr));
		}
		else if (!strcasecmp(encoding, "Q")) {	/**< quoted-printable */
			size_t len;
			long pos;
			
			len = strlen(istr);
			pos = 0;
			while (pos < len)
			{
				if (istr[pos] == '_') istr[pos] = ' ';
				pos++;
			}

			ibuflen = CtdlDecodeQuotedPrintable(ibuf, istr, len);
		}
		else {
			strcpy(ibuf, istr);		/**< unknown encoding */
			ibuflen = strlen(istr);
		}

		ctdl_iconv_open("UTF-8", charset, &ic);
		if (ic != (iconv_t)(-1) ) {
			obuflen = 1024;
			obuf = (char *) malloc(obuflen);
			osav = obuf;
			iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
			osav[1024-obuflen] = 0;

			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s%s%s", buf, osav, end);
			strcpy(buf, newbuf);
			free(osav);
			iconv_close(ic);
		}
		else {
			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s(unreadable)%s", buf, end);
			strcpy(buf, newbuf);
		}

		free(isav);

		/*
		 * Since spammers will go to all sorts of absurd lengths to get their
		 * messages through, there are LOTS of corrupt headers out there.
		 * So, prevent a really badly formed RFC2047 header from throwing
		 * this function into an infinite loop.
		 */
		++passes;
		if (passes > 20) return;

		start = strstr(buf, "=?");
		FindNextEnd((start != NULL)? start : buf, end);
	}

}
#else
inline void utf8ify_rfc822_string(char *a){};

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
	int found = 0;

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

		found = 0;

		/* Process these in a specific order, in case there are multiple matches.
		 * We want directory to override masq, for example.
		 */

		if ( (!strcasecmp(type, "masqdomain")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_masq;
		}
		if ( (!strcasecmp(type, "localhost")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_localhost;
		}
		if ( (!strcasecmp(type, "directory")) && (!strcasecmp(fqdn, host))) {
			found = hostalias_directory;
		}

		if (found) return(found);
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

	else if (!strcasecmp(key, "List-ID")) {
		if (msg->cm_fields['L'] == NULL)
			msg->cm_fields['L'] = strdup(value);
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

	if (IsDirectory(internet_addr, 0) == 0) return;
	CtdlLogPrintf(CTDL_DEBUG, "Create directory entry: %s --> %s\n", internet_addr, citadel_addr);
	directory_key(key, internet_addr);
	cdb_store(CDB_DIRECTORY, key, strlen(key), citadel_addr, strlen(citadel_addr)+1 );
}


/*
 * Delete an Internet e-mail address from the directory.
 *
 * (NOTE: we don't actually use or need the citadel_addr variable; it's merely
 * here because the callback API expects to be able to send it.)
 */
void CtdlDirectoryDelUser(char *internet_addr, char *citadel_addr) {
	char key[SIZ];

	CtdlLogPrintf(CTDL_DEBUG, "Delete directory entry: %s --> %s\n", internet_addr, citadel_addr);
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
				if (strstr(addr, "=?") != NULL)
					utf8ify_rfc822_string(addr);
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
