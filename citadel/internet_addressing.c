/*
 * $Id$
 *
 * This file contains functions which handle the mapping of Internet addresses
 * to users on the Citadel system.
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

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
#include "citadel.h"
#include "server.h"
#include "dynloader.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "user_ops.h"
#include "room_ops.h"
#include "parsedate.h"


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


struct trynamebuf {
	char buffer1[SIZ];
	char buffer2[SIZ];
};

char *inetcfg = NULL;



/*
 * Return nonzero if the supplied name is an alias for this host.
 */
int CtdlHostAlias(char *fqdn) {
	int config_lines;
	int i;
	char buf[SIZ];
	char host[SIZ], type[SIZ];

	if (!strcasecmp(fqdn, config.c_fqdn)) return(hostalias_localhost);
	if (!strcasecmp(fqdn, config.c_nodename)) return(hostalias_localhost);
	if (inetcfg == NULL) return(hostalias_nomatch);

	config_lines = num_tokens(inetcfg, '\n');
	for (i=0; i<config_lines; ++i) {
		extract_token(buf, inetcfg, i, '\n');
		extract_token(host, buf, 0, '|');
		extract_token(type, buf, 1, '|');

		if ( (!strcasecmp(type, "localhost"))
		   && (!strcasecmp(fqdn, host)))
			return(hostalias_localhost);

		if ( (!strcasecmp(type, "gatewaydomain"))
		   && (!strcasecmp(&fqdn[strlen(fqdn)-strlen(host)], host)))
			return(hostalias_gatewaydomain);

	}

	return(hostalias_nomatch);
}







/*
 * Return 0 if a given string fuzzy-matches a Citadel user account
 *
 * FIXME ... this needs to be updated to handle aliases.
 */
int fuzzy_match(struct usersupp *us, char *matchstring) {
	int a;

	if ( (!strncasecmp(matchstring, "cit", 3)) 
	   && (atol(&matchstring[3]) == us->usernum)) {
		return 0;
	}


	for (a=0; a<strlen(us->fullname); ++a) {
		if (!strncasecmp(&us->fullname[a],
		   matchstring, strlen(matchstring))) {
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
void process_rfc822_addr(char *rfc822, char *user, char *node, char *name)
{
	int a;

	strcpy(user, "");
	strcpy(node, config.c_fqdn);
	strcpy(name, "");

	/* extract full name - first, it's From minus <userid> */
	strcpy(name, rfc822);
	stripout(name, '<', '>');

	/* strip anything to the left of a bang */
	while ((strlen(name) > 0) && (haschar(name, '!') > 0))
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
		while ((strlen(name) > 0) && (name[0] != 34)) {
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
	while ((strlen(user) > 0) && (haschar(user, '!') > 0))
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
		while ((strlen(node) > 0) && (haschar(node, '@') > 0))
			strcpy(node, &node[1]);
	
		/* strip anything to the left of a % */
		while ((strlen(node) > 0) && (haschar(node, '%') > 0))
			strcpy(node, &node[1]);
	
		/* reduce multiple system bang paths to node!user */
		while ((strlen(node) > 0) && (haschar(node, '!') > 1))
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
}



/*
 * Back end for convert_internet_address()
 * (Compares an internet name [buffer1] and stores in [buffer2] if found)
 */
void try_name(struct usersupp *us, void *data) {
	struct passwd *pw;
	struct trynamebuf *tnb;
	tnb = (struct trynamebuf *)data;

	if (!strncasecmp(tnb->buffer1, "cit", 3))
		if (atol(&tnb->buffer1[3]) == us->usernum)
			strcpy(tnb->buffer2, us->fullname);

	if (!collapsed_strcmp(tnb->buffer1, us->fullname)) 
			strcpy(tnb->buffer2, us->fullname);

	if (us->uid != BBSUID) {
		pw = getpwuid(us->uid);
		if (pw != NULL) {
			if (!strcasecmp(tnb->buffer1, pw->pw_name)) {
				strcpy(tnb->buffer2, us->fullname);
			}
		}
	}
}


/*
 * Convert an Internet email address to a Citadel user/host combination
 */
int convert_internet_address(char *destuser, char *desthost, char *source)
{
	char user[SIZ];
	char node[SIZ];
	char name[SIZ];
	struct quickroom qrbuf;
	int i;
	int hostalias;
	struct trynamebuf tnb;
	char buf[SIZ];
	int passes = 0;
	char sourcealias[1024];

	safestrncpy(sourcealias, source, sizeof(sourcealias) );
	alias(sourcealias);

REALIAS:
	/* Split it up */
	process_rfc822_addr(sourcealias, user, node, name);
	lprintf(9, "process_rfc822_addr() converted to <%s@%s> (%s)\n",
		user, node, name);

	/* Map the FQDN to a Citadel node name
	 */
	hostalias =  CtdlHostAlias(node);
	switch(hostalias) {
		case hostalias_localhost:
			strcpy(node, config.c_nodename);
			break;

		case hostalias_gatewaydomain:
			extract_token(buf, node, 0, '.');
			safestrncpy(node, buf, sizeof buf);
	}

	/* Now try to resolve the name
	 * FIXME ... do the multiple-addresses thing
	 */
	if (!strcasecmp(node, config.c_nodename)) {


		/* First, see if we hit an alias.  Don't do this more than
		 * a few times, in case we accidentally hit an alias loop
		 */
		strcpy(sourcealias, user);
		alias(user);
		if ( (strcasecmp(user, sourcealias)) && (++passes < 3) )
			goto REALIAS;

		/* Try all local rooms */
		if (!strncasecmp(user, "room_", 5)) {
			strcpy(name, &user[5]);
			for (i=0; i<strlen(name); ++i) 
				if (name[i]=='_') name[i]=' ';
			if (getroom(&qrbuf, name) == 0) {
				strcpy(destuser, qrbuf.QRname);
				strcpy(desthost, config.c_nodename);
				return rfc822_room_delivery;
			}
		}

		/* Try all local users */
		strcpy(destuser, user);
		strcpy(desthost, config.c_nodename);
		strcpy(tnb.buffer1, user);
		strcpy(tnb.buffer2, "");
		ForEachUser(try_name, &tnb);
		if (strlen(tnb.buffer2) == 0) return(rfc822_no_such_user);
		strcpy(destuser, tnb.buffer2);
		return(rfc822_address_locally_validated);
	}

	strcpy(destuser, user);
	strcpy(desthost, node);
	if (hostalias == hostalias_gatewaydomain)
		return(rfc822_address_on_citadel_network);
	return(rfc822_address_nonlocal);
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

	key = mallok((end - beg) + 2);
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
			msg->cm_fields['T'] = strdoop(buf);
		processed = 1;
	}

	else if (!strcasecmp(key, "From")) {
		process_rfc822_addr(value, user, node, name);
		lprintf(9, "Converted to <%s@%s> (%s)\n", user, node, name);
		snprintf(addr, sizeof addr, "%s@%s", user, node);
		if (msg->cm_fields['A'] == NULL)
			msg->cm_fields['A'] = strdoop(name);
		processed = 1;
		if (msg->cm_fields['F'] == NULL)
			msg->cm_fields['F'] = strdoop(addr);
		processed = 1;
	}

	else if (!strcasecmp(key, "Subject")) {
		if (msg->cm_fields['U'] == NULL)
			msg->cm_fields['U'] = strdoop(value);
		processed = 1;
	}

	else if (!strcasecmp(key, "Message-ID")) {
		if (msg->cm_fields['I'] != NULL) {
			lprintf(5, "duplicate message id\n");
		}

		if (msg->cm_fields['I'] == NULL) {
			msg->cm_fields['I'] = strdoop(value);

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

	/* Clean up and move on. */
	phree(key);	/* Don't free 'value', it's actually the same buffer */
	return(processed);
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

	msg = mallok(sizeof(struct CtdlMessage));
	if (msg == NULL) return msg;

	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;	/* self check */
	msg->cm_anon_type = 0;			/* never anonymous */
	msg->cm_format_type = FMT_RFC822;	/* internet message */
	msg->cm_fields['M'] = rfc822;

	lprintf(9, "Unconverted RFC822 message length = %ld\n", strlen(rfc822));
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
			if (   ((rfc822[pos]=='\n')
			      ||(rfc822[pos]=='\r') )
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
		msg->cm_fields['T'] = strdoop(buf);
	}

	lprintf(9, "RFC822 length remaining after conversion = %ld\n",
		strlen(rfc822));
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
	int pos = 0;
	int beg, end;
	int done = 0;
	int colonpos, i;
	char *fieldbuf = NULL;

	/* Should never happen, but sometimes we get stupid */
	if (rfc822 == NULL) return(NULL);
	if (fieldname == NULL) return(NULL);

	while (!done) {

		/* Locate beginning and end of field, keeping in mind that
		 * some fields might be multiline
		 */
		beg = pos;
		end = (-1);
		for (pos=beg; ((pos<=strlen(rfc822))&&(end<0)); ++pos) {
			if ((rfc822[pos]=='\n')
			   && (!isspace(rfc822[pos+1]))) {
				end = pos;
			}
			if ( (rfc822[pos]=='\n')	/* done w. headers? */
			   && ( (rfc822[pos+1]=='\n')
			      ||(rfc822[pos+1]=='\r'))) {
				end = pos;
				done = 1;
			}

		}

		/* At this point we have a field.  Is it The One? */
		if (end > beg) {
			fieldbuf = mallok((end-beg)+3);
			if (fieldbuf == NULL) return(NULL);
			safestrncpy(fieldbuf, &rfc822[beg], (end-beg)+1);
			unfold_rfc822_field(fieldbuf);
			colonpos = (-1);
			for (i = strlen(fieldbuf); i >= 0; --i) {
				if (fieldbuf[i] == ':') colonpos = i;
			}
			if (colonpos > 0) {
				fieldbuf[colonpos] = 0;
				if (!strcasecmp(fieldbuf, fieldname)) {
					strcpy(fieldbuf, &fieldbuf[colonpos+1]);
					striplt(fieldbuf);
					return(fieldbuf);
				}
			}
			phree(fieldbuf);
		}

		/* If we've hit the end of the message, bail out */
		if (pos > strlen(rfc822)) done = 1;
	}
	return(NULL);
}
