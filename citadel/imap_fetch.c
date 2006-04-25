/*
 * $Id$
 *
 * Implements the FETCH command in IMAP.
 * This is a good example of the protocol's gratuitous complexity.
 *
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "mime_parser.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "genstamp.h"



/*
 * Individual field functions for imap_do_fetch_msg() ...
 */

void imap_fetch_uid(int seq) {
	cprintf("UID %ld", IMAP->msgids[seq-1]);
}

void imap_fetch_flags(int seq) {
	int num_flags_printed = 0;
	cprintf("FLAGS (");
	if (IMAP->flags[seq] & IMAP_DELETED) {
		if (num_flags_printed > 0) cprintf(" ");
		cprintf("\\Deleted");
		++num_flags_printed;
	}
	if (IMAP->flags[seq] & IMAP_SEEN) {
		if (num_flags_printed > 0) cprintf(" ");
		cprintf("\\Seen");
		++num_flags_printed;
	}
	if (IMAP->flags[seq] & IMAP_ANSWERED) {
		if (num_flags_printed > 0) cprintf(" ");
		cprintf("\\Answered");
		++num_flags_printed;
	}
	if (IMAP->flags[seq] & IMAP_RECENT) {
		if (num_flags_printed > 0) cprintf(" ");
		cprintf("\\Recent");
		++num_flags_printed;
	}
	cprintf(")");
}

void imap_fetch_internaldate(struct CtdlMessage *msg) {
	char buf[SIZ];
	time_t msgdate;

	if (!msg) return;
	if (msg->cm_fields['T'] != NULL) {
		msgdate = atol(msg->cm_fields['T']);
	}
	else {
		msgdate = time(NULL);
	}

	datestring(buf, sizeof buf, msgdate, DATESTRING_IMAP);
	cprintf("INTERNALDATE \"%s\"", buf);
}


/*
 * Fetch RFC822-formatted messages.
 *
 * 'whichfmt' should be set to one of:
 * 	"RFC822"	entire message
 *	"RFC822.HEADER"	headers only (with trailing blank line)
 *	"RFC822.SIZE"	size of translated message
 *	"RFC822.TEXT"	body only (without leading blank line)
 */
void imap_fetch_rfc822(long msgnum, char *whichfmt) {
	char buf[SIZ];
	char *ptr = NULL;
	size_t headers_size, text_size, total_size;
	size_t bytes_to_send = 0;
	struct MetaData smi;
	int need_to_rewrite_metadata = 0;
	int need_body = 0;

	/* Determine whether this particular fetch operation requires
	 * us to fetch the message body from disk.  If not, we can save
	 * on some disk operations...
	 */
	if ( (!strcasecmp(whichfmt, "RFC822"))
	   || (!strcasecmp(whichfmt, "RFC822.TEXT")) ) {
		need_body = 1;
	}

	/* If this is an RFC822.SIZE fetch, first look in the message's
	 * metadata record to see if we've saved that information.
	 */
	if (!strcasecmp(whichfmt, "RFC822.SIZE")) {
		GetMetaData(&smi, msgnum);
		if (smi.meta_rfc822_length > 0L) {
			cprintf("RFC822.SIZE %ld", smi.meta_rfc822_length);
			return;
		}
		need_to_rewrite_metadata = 1;
		need_body = 1;
	}
	
	/* Cache the most recent RFC822 FETCH because some clients like to
	 * fetch in pieces, and we don't want to have to go back to the
	 * message store for each piece.  We also burn the cache if the
	 * client requests something that involves reading the message
	 * body, but we haven't fetched the body yet.
	 */
	if ((IMAP->cached_rfc822_data != NULL)
	   && (IMAP->cached_rfc822_msgnum == msgnum)
	   && (IMAP->cached_rfc822_withbody || (!need_body)) ) {
		/* Good to go! */
	}
	else if (IMAP->cached_rfc822_data != NULL) {
		/* Some other message is cached -- free it */
		free(IMAP->cached_rfc822_data);
		IMAP->cached_rfc822_data = NULL;
		IMAP->cached_rfc822_msgnum = (-1);
		IMAP->cached_rfc822_len = 0;
	}

	/* At this point, we now can fetch and convert the message iff it's not
	 * the one we had cached.
	 */
	if (IMAP->cached_rfc822_data == NULL) {
		/*
		 * Load the message into memory for translation & measurement
		 */
		CC->redirect_buffer = malloc(SIZ);
		CC->redirect_len = 0;
		CC->redirect_alloc = SIZ;
		CtdlOutputMsg(msgnum, MT_RFC822,
			(need_body ? HEADERS_ALL : HEADERS_ONLY),
			0, 1, NULL);
		if (!need_body) cprintf("\r\n");	/* extra trailing newline */
		IMAP->cached_rfc822_data = CC->redirect_buffer;
		IMAP->cached_rfc822_len = CC->redirect_len;
		IMAP->cached_rfc822_msgnum = msgnum;
		IMAP->cached_rfc822_withbody = need_body;
		CC->redirect_buffer = NULL;
		CC->redirect_len = 0;
		CC->redirect_alloc = 0;
		if ( (need_to_rewrite_metadata) && (IMAP->cached_rfc822_len > 0) ) {
			smi.meta_rfc822_length = (long)IMAP->cached_rfc822_len;
			PutMetaData(&smi);
		}
	}

	/*
	 * Now figure out where the headers/text break is.  IMAP considers the
	 * intervening blank line to be part of the headers, not the text.
	 */
	headers_size = 0;
	text_size = 0;
	total_size = 0;

	if (need_body) {
		ptr = IMAP->cached_rfc822_data;
		do {
			ptr = memreadline(ptr, buf, sizeof buf);
			if (*ptr != 0) {
				striplt(buf);
				if (strlen(buf) == 0) {
					headers_size = ptr - IMAP->cached_rfc822_data;
				}
			}
		} while ( (headers_size == 0) && (*ptr != 0) );

		total_size = IMAP->cached_rfc822_len;
		text_size = total_size - headers_size;
	}
	else {
		headers_size = IMAP->cached_rfc822_len;
		total_size = IMAP->cached_rfc822_len;
		text_size = 0;
	}

	lprintf(CTDL_DEBUG, "RFC822: headers=%d, text=%d, total=%d\n",
		headers_size, text_size, total_size);

	if (!strcasecmp(whichfmt, "RFC822.SIZE")) {
		cprintf("RFC822.SIZE %d", total_size);
		return;
	}

	else if (!strcasecmp(whichfmt, "RFC822")) {
		ptr = IMAP->cached_rfc822_data;
		bytes_to_send = total_size;
	}

	else if (!strcasecmp(whichfmt, "RFC822.HEADER")) {
		ptr = IMAP->cached_rfc822_data;
		bytes_to_send = headers_size;
	}

	else if (!strcasecmp(whichfmt, "RFC822.TEXT")) {
		ptr = &IMAP->cached_rfc822_data[headers_size];
		bytes_to_send = text_size;
	}

	cprintf("%s {%d}\r\n", whichfmt, bytes_to_send);
	client_write(ptr, bytes_to_send);
}



/*
 * Load a specific part of a message into the temp file to be output to a
 * client.  FIXME we can handle parts like "2" and "2.1" and even "2.MIME"
 * but we still can't handle "2.HEADER" (which might not be a problem, because
 * we currently don't have the ability to break out nested RFC822's anyway).
 *
 * Note: mime_parser() was called with dont_decode set to 1, so we have the
 * luxury of simply spewing without having to re-encode.
 */
void imap_load_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		    void *cbuserdata)
{
	char mbuf2[SIZ];
	char *desired_section;

	desired_section = (char *)cbuserdata;

	if (!strcasecmp(partnum, desired_section)) {
		client_write(content, length);
	}

	snprintf(mbuf2, sizeof mbuf2, "%s.MIME", partnum);

	if (!strcasecmp(desired_section, mbuf2)) {
		cprintf("Content-type: %s", cbtype);
		if (strlen(cbcharset) > 0)
			cprintf("; charset=\"%s\"", cbcharset);
		if (strlen(name) > 0)
			cprintf("; name=\"%s\"", name);
		cprintf("\r\n");
		if (strlen(encoding) > 0)
			cprintf("Content-Transfer-Encoding: %s\r\n", encoding);
		if (strlen(encoding) > 0) {
			cprintf("Content-Disposition: %s", disp);
			if (strlen(filename) > 0) {
				cprintf("; filename=\"%s\"", filename);
			}
			cprintf("\r\n");
		}
		cprintf("Content-Length: %ld\r\n", (long)length);
		cprintf("\r\n");
	}
			

}


/* 
 * Called by imap_fetch_envelope() to output the "From" field.
 * This is in its own function because its logic is kind of complex.  We
 * really need to make this suck less.
 */
void imap_output_envelope_from(struct CtdlMessage *msg) {
	char user[SIZ], node[SIZ], name[SIZ];

	if (!msg) return;

	/* For anonymous messages, it's so easy! */
	if (!is_room_aide() && (msg->cm_anon_type == MES_ANONONLY)) {
		cprintf("((\"----\" NIL \"x\" \"x.org\")) ");
		return;
	}
	if (!is_room_aide() && (msg->cm_anon_type == MES_ANONOPT)) {
		cprintf("((\"anonymous\" NIL \"x\" \"x.org\")) ");
		return;
	}

	/* For everything else, we do stuff. */
	cprintf("((");				/* open double-parens */
	imap_strout(msg->cm_fields['A']);	/* personal name */
	cprintf(" NIL ");			/* source route (not used) */


	if (msg->cm_fields['F'] != NULL) {
		process_rfc822_addr(msg->cm_fields['F'], user, node, name);
		imap_strout(user);		/* mailbox name (user id) */
		cprintf(" ");
		if (!strcasecmp(node, config.c_nodename)) {
			imap_strout(config.c_fqdn);
		}
		else {
			imap_strout(node);		/* host name */
		}
	}
	else {
		imap_strout(msg->cm_fields['A']); /* mailbox name (user id) */
		cprintf(" ");
		imap_strout(msg->cm_fields['N']);	/* host name */
	}
	
	cprintf(")) ");				/* close double-parens */
}



/*
 * Output an envelope address (or set of addresses) in the official,
 * convuluted, braindead format.  (Note that we can't use this for
 * the "From" address because its data may come from a number of different
 * fields.  But we can use it for "To" and possibly others.
 */
void imap_output_envelope_addr(char *addr) {
	char individual_addr[256];
	int num_addrs;
	int i;
	char user[256];
	char node[256];
	char name[256];

	if (addr == NULL) {
		cprintf("NIL ");
		return;
	}

	if (strlen(addr) == 0) {
		cprintf("NIL ");
		return;
	}

	cprintf("(");

	/* How many addresses are listed here? */
	num_addrs = num_tokens(addr, ',');

	/* Output them one by one. */
	for (i=0; i<num_addrs; ++i) {
		extract_token(individual_addr, addr, i, ',', sizeof individual_addr);
		striplt(individual_addr);
		process_rfc822_addr(individual_addr, user, node, name);
		cprintf("(");
		imap_strout(name);
		cprintf(" NIL ");
		imap_strout(user);
		cprintf(" ");
		imap_strout(node);
		cprintf(")");
		if (i < (num_addrs-1)) cprintf(" ");
	}

	cprintf(") ");
}


/*
 * Implements the ENVELOPE fetch item
 * 
 * Note that the imap_strout() function can cleverly output NULL fields as NIL,
 * so we don't have to check for that condition like we do elsewhere.
 */
void imap_fetch_envelope(struct CtdlMessage *msg) {
	char datestringbuf[SIZ];
	time_t msgdate;
	char *fieldptr = NULL;

	if (!msg) return;

	/* Parse the message date into an IMAP-format date string */
	if (msg->cm_fields['T'] != NULL) {
		msgdate = atol(msg->cm_fields['T']);
	}
	else {
		msgdate = time(NULL);
	}
	datestring(datestringbuf, sizeof datestringbuf,
		msgdate, DATESTRING_IMAP);

	/* Now start spewing data fields.  The order is important, as it is
	 * defined by the protocol specification.  Nonexistent fields must
	 * be output as NIL, existent fields must be quoted or literalled.
	 * The imap_strout() function conveniently does all this for us.
	 */
	cprintf("ENVELOPE (");

	/* Date */
	imap_strout(datestringbuf);
	cprintf(" ");

	/* Subject */
	imap_strout(msg->cm_fields['U']);
	cprintf(" ");

	/* From */
	imap_output_envelope_from(msg);

	/* Sender (default to same as 'From' if not present) */
	fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Sender");
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
		free(fieldptr);
	}
	else {
		imap_output_envelope_from(msg);
	}

	/* Reply-to */
	fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Reply-to");
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
		free(fieldptr);
	}
	else {
		imap_output_envelope_from(msg);
	}

	/* To */
	imap_output_envelope_addr(msg->cm_fields['R']);

	/* Cc (we do it this way because there might be a legacy non-Citadel Cc: field present) */
	fieldptr = msg->cm_fields['Y'];
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
	}
	else {
		fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Cc");
		imap_output_envelope_addr(fieldptr);
		if (fieldptr != NULL) free(fieldptr);
	}

	/* Bcc */
	fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Bcc");
	imap_output_envelope_addr(fieldptr);
	if (fieldptr != NULL) free(fieldptr);

	/* In-reply-to */
	fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "In-reply-to");
	imap_strout(fieldptr);
	cprintf(" ");
	if (fieldptr != NULL) free(fieldptr);

	/* message ID */
	imap_strout(msg->cm_fields['I']);

	cprintf(")");
}

/*
 * This function is called only when CC->redirect_buffer contains a set of
 * RFC822 headers with no body attached.  Its job is to strip that set of
 * headers down to *only* the ones we're interested in.
 */
void imap_strip_headers(char *section) {
	char buf[SIZ];
	char *which_fields = NULL;
	int doing_headers = 0;
	int headers_not = 0;
	char *parms[SIZ];
        int num_parms = 0;
	int i;
	char *boiled_headers = NULL;
	int ok = 0;
	int done_headers = 0;
	char *ptr = NULL;

	if (CC->redirect_buffer == NULL) return;

	which_fields = strdup(section);

	if (!strncasecmp(which_fields, "HEADER.FIELDS", 13))
		doing_headers = 1;
	if (!strncasecmp(which_fields, "HEADER.FIELDS.NOT", 17))
		headers_not = 1;

	for (i=0; i<strlen(which_fields); ++i) {
		if (which_fields[i]=='(')
			strcpy(which_fields, &which_fields[i+1]);
	}
	for (i=0; i<strlen(which_fields); ++i) {
		if (which_fields[i]==')')
			which_fields[i] = 0;
	}
	num_parms = imap_parameterize(parms, which_fields);

	boiled_headers = malloc(CC->redirect_alloc);
	strcpy(boiled_headers, "");

	ptr = CC->redirect_buffer;
	ok = 0;
	while ( (done_headers == 0) && (ptr = memreadline(ptr, buf, sizeof buf), *ptr != 0) ) {
		if (!isspace(buf[0])) {
			ok = 0;
			if (doing_headers == 0) ok = 1;
			else {
				if (headers_not) ok = 1;
				else ok = 0;
				for (i=0; i<num_parms; ++i) {
					if ( (!strncasecmp(buf, parms[i],
					   strlen(parms[i]))) &&
					   (buf[strlen(parms[i])]==':') ) {
						if (headers_not) ok = 0;
						else ok = 1;
					}
				}
			}
		}

		if (ok) {
			strcat(boiled_headers, buf);
			strcat(boiled_headers, "\r\n");
		}

		if (strlen(buf) == 0) done_headers = 1;
		if (buf[0]=='\r') done_headers = 1;
		if (buf[0]=='\n') done_headers = 1;
	}

	strcat(boiled_headers, "\r\n");

	/* Now save it back (it'll always be smaller) */
	strcpy(CC->redirect_buffer, boiled_headers);
	CC->redirect_len = strlen(boiled_headers);

	free(which_fields);
	free(boiled_headers);
}


/*
 * Implements the BODY and BODY.PEEK fetch items
 */
void imap_fetch_body(long msgnum, char *item, int is_peek) {
	struct CtdlMessage *msg = NULL;
	char section[SIZ];
	char partial[SIZ];
	int is_partial = 0;
	size_t pstart, pbytes;
	int loading_body_now = 0;
	int need_body = 1;
	int burn_the_cache = 0;

	/* extract section */
	safestrncpy(section, item, sizeof section);
	if (strchr(section, '[') != NULL) {
		stripallbut(section, '[', ']');
	}
	lprintf(CTDL_DEBUG, "Section is: %s%s\n", section, ((strlen(section)==0) ? "(empty)" : "") );
	if (!strncasecmp(section, "HEADER", 6)) {
		need_body = 0;
	}

	/* Burn the cache if we don't have the same section of the 
	 * same message again.
	 */
	if (IMAP->cached_body != NULL) {
		if (IMAP->cached_bodymsgnum != msgnum) {
			burn_the_cache = 1;
		}
		else if ( (!IMAP->cached_body_withbody) && (need_body) ) {
			burn_the_cache = 1;
		}
		else if (strcasecmp(IMAP->cached_bodypart, section)) {
			burn_the_cache = 1;
		}
		if (burn_the_cache) {
			/* Yup, go ahead and burn the cache. */
			free(IMAP->cached_body);
			IMAP->cached_body_len = 0;
			IMAP->cached_body = NULL;
			IMAP->cached_bodymsgnum = (-1);
			strcpy(IMAP->cached_bodypart, "");
		}
	}

	/* extract partial */
	safestrncpy(partial, item, sizeof partial);
	if (strchr(partial, '<') != NULL) {
		stripallbut(partial, '<', '>');
		is_partial = 1;
	}
	if (is_partial == 0) strcpy(partial, "");
	/* if (strlen(partial) > 0) lprintf(CTDL_DEBUG, "Partial is %s\n", partial); */

	if (IMAP->cached_body == NULL) {
		CC->redirect_buffer = malloc(SIZ);
		CC->redirect_len = 0;
		CC->redirect_alloc = SIZ;
		loading_body_now = 1;
		msg = CtdlFetchMessage(msgnum, (need_body ? 1 : 0));
	}

	/* Now figure out what the client wants, and get it */

	if (!loading_body_now) {
		/* What we want is already in memory */
	}

	else if ( (!strcmp(section, "1")) && (msg->cm_format_type != 4) ) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_NONE, 0, 1);
	}

	else if (!strcmp(section, "")) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1);
	}

	/*
	 * If the client asked for just headers, or just particular header
	 * fields, strip it down.
	 */
	else if (!strncasecmp(section, "HEADER", 6)) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ONLY, 0, 1);
		imap_strip_headers(section);
	}

	/*
	 * Strip it down if the client asked for everything _except_ headers.
	 */
	else if (!strncasecmp(section, "TEXT", 4)) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_NONE, 0, 1);
	}

	/*
	 * Anything else must be a part specifier.
	 * (Note value of 1 passed as 'dont_decode' so client gets it encoded)
	 */
	else {
		mime_parser(msg->cm_fields['M'], NULL,
				*imap_load_part, NULL, NULL,
				section,
				1);
	}

	if (loading_body_now) {
		IMAP->cached_body = CC->redirect_buffer;
		IMAP->cached_body_len = CC->redirect_len;
		IMAP->cached_bodymsgnum = msgnum;
		IMAP->cached_body_withbody = need_body;
		strcpy(IMAP->cached_bodypart, section);
		CC->redirect_buffer = NULL;
		CC->redirect_len = 0;
		CC->redirect_alloc = 0;
	}

	if (is_partial == 0) {
		cprintf("BODY[%s] {%d}\r\n", section, IMAP->cached_body_len);
		pstart = 0;
		pbytes = IMAP->cached_body_len;
	}
	else {
		sscanf(partial, "%d.%d", &pstart, &pbytes);
		if (pbytes > (IMAP->cached_body_len - pstart)) {
			pbytes = IMAP->cached_body_len - pstart;
		}
		cprintf("BODY[%s]<%d> {%d}\r\n", section, pstart, pbytes);
	}

	/* Here we go -- output it */
	client_write(&IMAP->cached_body[pstart], pbytes);

	if (msg != NULL) {
		CtdlFreeMessage(msg);
	}

	/* Mark this message as "seen" *unless* this is a "peek" operation */
	if (is_peek == 0) {
		CtdlSetSeen(&msgnum, 1, 1, ctdlsetseen_seen, NULL, NULL);
	}
}

/*
 * Called immediately before outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_pre(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		void *cbuserdata
		) {

	cprintf("(");
}



/*
 * Called immediately after outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_post(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		void *cbuserdata
		) {

	char subtype[128];

	cprintf(" ");

	/* disposition */
	extract_token(subtype, cbtype, 1, '/', sizeof subtype);
	imap_strout(subtype);

	/* body language */
	cprintf(" NIL");

	cprintf(")");
}



/*
 * Output the info for a MIME part in the format required by BODYSTRUCTURE.
 *
 */
void imap_fetch_bodystructure_part(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		void *cbuserdata
		) {

	int have_cbtype = 0;
	int have_encoding = 0;
	int lines = 0;
	size_t i;
	char cbmaintype[128];
	char cbsubtype[128];

	if (cbtype != NULL) if (strlen(cbtype)>0) have_cbtype = 1;
	if (have_cbtype) {
		extract_token(cbmaintype, cbtype, 0, '/', sizeof cbmaintype);
		extract_token(cbsubtype, cbtype, 1, '/', sizeof cbsubtype);
	}
	else {
		strcpy(cbmaintype, "TEXT");
		strcpy(cbsubtype, "PLAIN");
	}

	cprintf("(");
	imap_strout(cbmaintype);
	cprintf(" ");
	imap_strout(cbsubtype);
	cprintf(" ");

	if (cbcharset == NULL) {
		cprintf("(\"CHARSET\" \"US-ASCII\"");
	}
	else if (strlen(cbcharset) == 0) {
		cprintf("(\"CHARSET\" \"US-ASCII\"");
	}
	else {
		cprintf("(\"CHARSET\" ");
		imap_strout(cbcharset);
	}

	if (name != NULL) if (strlen(name)>0) {
		cprintf(" \"NAME\" ");
		imap_strout(name);
	}

	cprintf(") ");

	cprintf("NIL ");	/* Body ID */
	cprintf("NIL ");	/* Body description */

	if (encoding != NULL) if (strlen(encoding) > 0)  have_encoding = 1;
	if (have_encoding) {
		imap_strout(encoding);
	}
	else {
		imap_strout("7BIT");
	}
	cprintf(" ");

	/* The next field is the size of the part in bytes. */
	cprintf("%ld ", (long)length);	/* bytes */

	/* The next field is the number of lines in the part, if and only
	 * if the part is TEXT.  More gratuitous complexity.
	 */
	if (!strcasecmp(cbmaintype, "TEXT")) {
		if (length) for (i=0; i<length; ++i) {
			if (((char *)content)[i] == '\n') ++lines;
		}
		cprintf("%d ", lines);
	}

	/* More gratuitous complexity */
	if ((!strcasecmp(cbmaintype, "MESSAGE"))
	   && (!strcasecmp(cbsubtype, "RFC822"))) {
		/* FIXME
                     A body type of type MESSAGE and subtype RFC822
                     contains, immediately after the basic fields, the
                     envelope structure, body structure, and size in
                     text lines of the encapsulated message.
		*/
	}

	/* MD5 value of body part; we can get away with NIL'ing this */
	cprintf("NIL ");

	/* Disposition */
	if (disp == NULL) {
		cprintf("NIL");
	}
	else if (strlen(disp) == 0) {
		cprintf("NIL");
	}
	else {
		cprintf("(");
		imap_strout(disp);
		if (filename != NULL) if (strlen(filename)>0) {
			cprintf(" (\"FILENAME\" ");
			imap_strout(filename);
			cprintf(")");
		}
		cprintf(")");
	}

	/* Body language (not defined yet) */
	cprintf(" NIL)");
}



/*
 * Spew the BODYSTRUCTURE data for a message.
 *
 */
void imap_fetch_bodystructure (long msgnum, char *item,
		struct CtdlMessage *msg) {
	char *rfc822 = NULL;
	char *rfc822_body = NULL;
	size_t rfc822_len;
	size_t rfc822_headers_len;
	size_t rfc822_body_len;
	char *ptr = NULL;
	char buf[SIZ];
	int lines = 0;

	/* Handle NULL message gracefully */
	if (msg == NULL) {
		cprintf("BODYSTRUCTURE (\"TEXT\" \"PLAIN\" "
			"(\"CHARSET\" \"US-ASCII\") NIL NIL "
			"\"7BIT\" 0 0)");
		return;
	}

	/* For non-RFC822 (ordinary Citadel) messages, this is short and
	 * sweet...
	 */
	if (msg->cm_format_type != FMT_RFC822) {

		/* *sigh* We have to RFC822-format the message just to be able
		 * to measure it.  FIXME use smi cached fields if possible
		 */

		CC->redirect_buffer = malloc(SIZ);
		CC->redirect_len = 0;
		CC->redirect_alloc = SIZ;
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, 0, 0, 1);
		rfc822 = CC->redirect_buffer;
		rfc822_len = CC->redirect_len;
		CC->redirect_buffer = NULL;
		CC->redirect_len = 0;
		CC->redirect_alloc = 0;

		ptr = rfc822;
		while (ptr = memreadline(ptr, buf, sizeof buf), *ptr != 0) {
			++lines;
			if ((strlen(buf) == 0) && (rfc822_body == NULL)) {
				rfc822_body = ptr;
			}
		}

		rfc822_headers_len = rfc822_body - rfc822;
		rfc822_body_len = rfc822_len - rfc822_headers_len;
		free(rfc822);

		cprintf("BODYSTRUCTURE (\"TEXT\" \"PLAIN\" "
			"(\"CHARSET\" \"US-ASCII\") NIL NIL "
			"\"7BIT\" %d %d)", rfc822_body_len, lines);

		return;
	}

	/* For messages already stored in RFC822 format, we have to parse. */
	cprintf("BODYSTRUCTURE ");
	mime_parser(msg->cm_fields['M'],
			NULL,
			*imap_fetch_bodystructure_part,	/* part */
			*imap_fetch_bodystructure_pre,	/* pre-multi */
			*imap_fetch_bodystructure_post,	/* post-multi */
			NULL,
			1);	/* don't decode -- we want it as-is */
}


/*
 * imap_do_fetch() calls imap_do_fetch_msg() to output the data of an
 * individual message, once it has been selected for output.
 */
void imap_do_fetch_msg(int seq, int num_items, char **itemlist) {
	int i;
	struct CtdlMessage *msg = NULL;
	int body_loaded = 0;

	/* Don't attempt to fetch bogus messages or UID's */
	if (seq < 1) return;
	if (IMAP->msgids[seq-1] < 1L) return;

	buffer_output();
	cprintf("* %d FETCH (", seq);

	for (i=0; i<num_items; ++i) {

		/* Fetchable without going to the message store at all */
		if (!strcasecmp(itemlist[i], "UID")) {
			imap_fetch_uid(seq);
		}
		else if (!strcasecmp(itemlist[i], "FLAGS")) {
			imap_fetch_flags(seq-1);
		}

		/* Potentially fetchable from cache, if the client requests
		 * stuff from the same message several times in a row.
		 */
		else if (!strcasecmp(itemlist[i], "RFC822")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i]);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.HEADER")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i]);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.SIZE")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i]);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.TEXT")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i]);
		}

		/* BODY fetches do their own fetching and caching too. */
		else if (!strncasecmp(itemlist[i], "BODY[", 5)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i], 0);
		}
		else if (!strncasecmp(itemlist[i], "BODY.PEEK[", 10)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i], 1);
		}

		/* Otherwise, load the message into memory.
		 */
		else if (!strcasecmp(itemlist[i], "BODYSTRUCTURE")) {
			if ((msg != NULL) && (!body_loaded)) {
				CtdlFreeMessage(msg);	/* need the whole thing */
				msg = NULL;
			}
			if (msg == NULL) {
				msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
				body_loaded = 1;
			}
			imap_fetch_bodystructure(IMAP->msgids[seq-1],
					itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "ENVELOPE")) {
			if (msg == NULL) {
				msg = CtdlFetchMessage(IMAP->msgids[seq-1], 0);
				body_loaded = 0;
			}
			imap_fetch_envelope(msg);
		}
		else if (!strcasecmp(itemlist[i], "INTERNALDATE")) {
			if (msg == NULL) {
				msg = CtdlFetchMessage(IMAP->msgids[seq-1], 0);
				body_loaded = 0;
			}
			imap_fetch_internaldate(msg);
		}

		if (i != num_items-1) cprintf(" ");
	}

	cprintf(")\r\n");
	unbuffer_output();
	if (msg != NULL) {
		CtdlFreeMessage(msg);
	}
}



/*
 * imap_fetch() calls imap_do_fetch() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_fetch(int num_items, char **itemlist) {
	int i;

	if (IMAP->num_msgs > 0) {
		for (i = 0; i < IMAP->num_msgs; ++i) {

			/* Abort the fetch loop if the session breaks.
			 * This is important for users who keep mailboxes
			 * that are too big *and* are too impatient to
			 * let them finish loading.  :)
			 */
			if (CC->kill_me) return;

			/* Get any message marked for fetch. */
			if (IMAP->flags[i] & IMAP_SELECTED) {
				imap_do_fetch_msg(i+1, num_items, itemlist);
			}
		}
	}
}



/*
 * Back end for imap_handle_macros()
 * Note that this function *only* looks at the beginning of the string.  It
 * is not a generic search-and-replace function.
 */
void imap_macro_replace(char *str, char *find, char *replace) {
	char holdbuf[SIZ];

	if (!strncasecmp(str, find, strlen(find))) {
		if (str[strlen(find)]==' ') {
			strcpy(holdbuf, &str[strlen(find)+1]);
			strcpy(str, replace);
			strcat(str, " ");
			strcat(str, holdbuf);
		}
		if (str[strlen(find)]==0) {
			strcpy(holdbuf, &str[strlen(find)+1]);
			strcpy(str, replace);
		}
	}
}



/*
 * Handle macros embedded in FETCH data items.
 * (What the heck are macros doing in a wire protocol?  Are we trying to save
 * the computer at the other end the trouble of typing a lot of characters?)
 */
void imap_handle_macros(char *str) {
	int i;
	int nest = 0;

	for (i=0; i<strlen(str); ++i) {
		if (str[i]=='(') ++nest;
		if (str[i]=='[') ++nest;
		if (str[i]=='<') ++nest;
		if (str[i]=='{') ++nest;
		if (str[i]==')') --nest;
		if (str[i]==']') --nest;
		if (str[i]=='>') --nest;
		if (str[i]=='}') --nest;

		if (nest <= 0) {
			imap_macro_replace(&str[i],
				"ALL",
				"FLAGS INTERNALDATE RFC822.SIZE ENVELOPE"
			);
			imap_macro_replace(&str[i],
				"BODY",
				"BODYSTRUCTURE"
			);
			imap_macro_replace(&str[i],
				"FAST",
				"FLAGS INTERNALDATE RFC822.SIZE"
			);
			imap_macro_replace(&str[i],
				"FULL",
				"FLAGS INTERNALDATE RFC822.SIZE ENVELOPE BODY"
			);
		}
	}
}


/*
 * Break out the data items requested, possibly a parenthesized list.
 * Returns the number of data items, or -1 if the list is invalid.
 * NOTE: this function alters the string it is fed, and uses it as a buffer
 * to hold the data for the pointers it returns.
 */
int imap_extract_data_items(char **argv, char *items) {
	int num_items = 0;
	int nest = 0;
	int i, initial_len;
	char *start;

	/* Convert all whitespace to ordinary space characters. */
	for (i=0; i<strlen(items); ++i) {
		if (isspace(items[i])) items[i]=' ';
	}

	/* Strip leading and trailing whitespace, then strip leading and
	 * trailing parentheses if it's a list
	 */
	striplt(items);
	if ( (items[0]=='(') && (items[strlen(items)-1]==')') ) {
		items[strlen(items)-1] = 0;
		strcpy(items, &items[1]);
		striplt(items);
	}

	/* Parse any macro data items */
	imap_handle_macros(items);

	/*
	 * Now break out the data items.  We throw in one trailing space in
	 * order to avoid having to break out the last one manually.
	 */
	strcat(items, " ");
	start = items;
	initial_len = strlen(items);
	for (i=0; i<initial_len; ++i) {
		if (items[i]=='(') ++nest;
		if (items[i]=='[') ++nest;
		if (items[i]=='<') ++nest;
		if (items[i]=='{') ++nest;
		if (items[i]==')') --nest;
		if (items[i]==']') --nest;
		if (items[i]=='>') --nest;
		if (items[i]=='}') --nest;

		if (nest <= 0) if (items[i]==' ') {
			items[i] = 0;
			argv[num_items++] = start;
			start = &items[i+1];
		}
	}

	return(num_items);

}


/*
 * One particularly hideous aspect of IMAP is that we have to allow the client
 * to specify arbitrary ranges and/or sets of messages to fetch.  Citadel IMAP
 * handles this by setting the IMAP_SELECTED flag for each message specified in
 * the ranges/sets, then looping through the message array, outputting messages
 * with the flag set.  We don't bother returning an error if an out-of-range
 * number is specified (we just return quietly) because any client braindead
 * enough to request a bogus message number isn't going to notice the
 * difference anyway.
 *
 * This function clears out the IMAP_SELECTED bits, then sets that bit for each
 * message included in the specified range.
 *
 * Set is_uid to 1 to fetch by UID instead of sequence number.
 */
void imap_pick_range(char *supplied_range, int is_uid) {
	int i;
	int num_sets;
	int s;
	char setstr[SIZ], lostr[SIZ], histr[SIZ];
	long lo, hi;
	char actual_range[SIZ];

	/* 
	 * Handle the "ALL" macro
	 */
	if (!strcasecmp(supplied_range, "ALL")) {
		safestrncpy(actual_range, "1:*", sizeof actual_range);
	}
	else {
		safestrncpy(actual_range, supplied_range, sizeof actual_range);
	}

	/*
	 * Clear out the IMAP_SELECTED flags for all messages.
	 */
	for (i = 0; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] = IMAP->flags[i] & ~IMAP_SELECTED;
	}

	/*
	 * Now set it for all specified messages.
	 */
	num_sets = num_tokens(actual_range, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, actual_range, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) snprintf(histr, sizeof histr, "%ld", LONG_MAX);
		} 
		else {
			safestrncpy(histr, lostr, sizeof histr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		/* Loop through the array, flipping bits where appropriate */
		for (i = 1; i <= IMAP->num_msgs; ++i) {
			if (is_uid) {	/* fetch by sequence number */
				if ( (IMAP->msgids[i-1]>=lo)
				   && (IMAP->msgids[i-1]<=hi)) {
					IMAP->flags[i-1] |= IMAP_SELECTED;
				}
			}
			else {		/* fetch by uid */
				if ( (i>=lo) && (i<=hi)) {
					IMAP->flags[i-1] |= IMAP_SELECTED;
				}
			}
		}
	}

}



/*
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, char *parms[]) {
	char items[SIZ];
	char *itemlist[512];
	int num_items;
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	imap_pick_range(parms[2], 0);

	strcpy(items, "");
	for (i=3; i<num_parms; ++i) {
		strcat(items, parms[i]);
		if (i < (num_parms-1)) strcat(items, " ");
	}

	num_items = imap_extract_data_items(itemlist, items);
	if (num_items < 1) {
		cprintf("%s BAD invalid data item list\r\n", parms[0]);
		return;
	}

	imap_do_fetch(num_items, itemlist);
	cprintf("%s OK FETCH completed\r\n", parms[0]);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidfetch(int num_parms, char *parms[]) {
	char items[SIZ];
	char *itemlist[512];
	int num_items;
	int i;
	int have_uid_item = 0;

	if (num_parms < 5) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	imap_pick_range(parms[3], 1);

	strcpy(items, "");
	for (i=4; i<num_parms; ++i) {
		strcat(items, parms[i]);
		if (i < (num_parms-1)) strcat(items, " ");
	}

	num_items = imap_extract_data_items(itemlist, items);
	if (num_items < 1) {
		cprintf("%s BAD invalid data item list\r\n", parms[0]);
		return;
	}

	/* If the "UID" item was not included, we include it implicitly
	 * (at the beginning) because this is a UID FETCH command
	 */
	for (i=0; i<num_items; ++i) {
		if (!strcasecmp(itemlist[i], "UID")) ++have_uid_item;
	}
	if (have_uid_item == 0) {
		memmove(&itemlist[1], &itemlist[0], (sizeof(itemlist[0]) * num_items));
		++num_items;
		itemlist[0] = "UID";
	}

	imap_do_fetch(num_items, itemlist);
	cprintf("%s OK UID FETCH completed\r\n", parms[0]);
}


