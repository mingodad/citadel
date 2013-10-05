/*
 * Implements the FETCH command in IMAP.
 * This is a good example of the protocol's gratuitous complexity.
 *
 * Copyright (c) 2001-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "genstamp.h"
#include "ctdl_module.h"



/*
 * Individual field functions for imap_do_fetch_msg() ...
 */

void imap_fetch_uid(int seq) {
	IAPrintf("UID %ld", IMAP->msgids[seq-1]);
}

void imap_fetch_flags(int seq) 
{
	citimap *Imap = IMAP;
	int num_flags_printed = 0;
	IAPuts("FLAGS (");
	if (Imap->flags[seq] & IMAP_DELETED) {
		if (num_flags_printed > 0) 
			IAPuts(" ");
		IAPuts("\\Deleted");
		++num_flags_printed;
	}
	if (Imap->flags[seq] & IMAP_SEEN) {
		if (num_flags_printed > 0) 
			IAPuts(" ");
		IAPuts("\\Seen");
		++num_flags_printed;
	}
	if (Imap->flags[seq] & IMAP_ANSWERED) {
		if (num_flags_printed > 0) 
			IAPuts(" ");
		IAPuts("\\Answered");
		++num_flags_printed;
	}
	if (Imap->flags[seq] & IMAP_RECENT) {
		if (num_flags_printed > 0) 
			IAPuts(" ");
		IAPuts("\\Recent");
		++num_flags_printed;
	}
	IAPuts(")");
}


void imap_fetch_internaldate(struct CtdlMessage *msg) {
	char datebuf[64];
	time_t msgdate;

	if (!msg) return;
	if (!CM_IsEmpty(msg, eTimestamp)) {
		msgdate = atol(msg->cm_fields[eTimestamp]);
	}
	else {
		msgdate = time(NULL);
	}

	datestring(datebuf, sizeof datebuf, msgdate, DATESTRING_IMAP);
	IAPrintf( "INTERNALDATE \"%s\"", datebuf);
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
void imap_fetch_rfc822(long msgnum, const char *whichfmt) {
	CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;
	const char *ptr = NULL;
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
			IAPrintf("RFC822.SIZE %ld", smi.meta_rfc822_length);
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
	if ((Imap->cached_rfc822 != NULL)
	   && (Imap->cached_rfc822_msgnum == msgnum)
	   && (Imap->cached_rfc822_withbody || (!need_body)) ) {
		/* Good to go! */
	}
	else if (Imap->cached_rfc822 != NULL) {
		/* Some other message is cached -- free it */
		FreeStrBuf(&Imap->cached_rfc822);
		Imap->cached_rfc822_msgnum = (-1);
	}

	/* At this point, we now can fetch and convert the message iff it's not
	 * the one we had cached.
	 */
	if (Imap->cached_rfc822 == NULL) {
		/*
		 * Load the message into memory for translation & measurement
		 */
		CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
		CtdlOutputMsg(msgnum, MT_RFC822,
			(need_body ? HEADERS_ALL : HEADERS_FAST),
			0, 1, NULL, SUPPRESS_ENV_TO, NULL, NULL
		);
		if (!need_body) IAPuts("\r\n");	/* extra trailing newline */
		Imap->cached_rfc822 = CCC->redirect_buffer;
		CCC->redirect_buffer = NULL;
		Imap->cached_rfc822_msgnum = msgnum;
		Imap->cached_rfc822_withbody = need_body;
		if ( (need_to_rewrite_metadata) && 
		     (StrLength(Imap->cached_rfc822) > 0) ) {
			smi.meta_rfc822_length = StrLength(Imap->cached_rfc822);
			PutMetaData(&smi);
		}
	}

	/*
	 * Now figure out where the headers/text break is.  IMAP considers the
	 * intervening blank line to be part of the headers, not the text.
	 */
	headers_size = 0;

	if (need_body) {
		StrBuf *Line = NewStrBuf();
		ptr = NULL;
		do {
			StrBufSipLine(Line, Imap->cached_rfc822, &ptr);

			if ((StrLength(Line) != 0)  && (ptr != StrBufNOTNULL))
			{
				StrBufTrim(Line);
				if ((StrLength(Line) != 0) && 
				    (ptr != StrBufNOTNULL)    )
				{
					headers_size = ptr - ChrPtr(Imap->cached_rfc822);
				}
			}
		} while ( (headers_size == 0)    && 
			  (ptr != StrBufNOTNULL) );

		total_size = StrLength(Imap->cached_rfc822);
		text_size = total_size - headers_size;
		FreeStrBuf(&Line);
	}
	else {
		headers_size = 
			total_size = StrLength(Imap->cached_rfc822);
		text_size = 0;
	}

	IMAP_syslog(LOG_DEBUG, 
		    "RFC822: headers=" SIZE_T_FMT 
		    ", text=" SIZE_T_FMT
		    ", total=" SIZE_T_FMT,
		    headers_size, text_size, total_size);

	if (!strcasecmp(whichfmt, "RFC822.SIZE")) {
		IAPrintf("RFC822.SIZE " SIZE_T_FMT, total_size);
		return;
	}

	else if (!strcasecmp(whichfmt, "RFC822")) {
		ptr = ChrPtr(Imap->cached_rfc822);
		bytes_to_send = total_size;
	}

	else if (!strcasecmp(whichfmt, "RFC822.HEADER")) {
		ptr = ChrPtr(Imap->cached_rfc822);
		bytes_to_send = headers_size;
	}

	else if (!strcasecmp(whichfmt, "RFC822.TEXT")) {
		ptr = &ChrPtr(Imap->cached_rfc822)[headers_size];
		bytes_to_send = text_size;
	}

	IAPrintf("%s {" SIZE_T_FMT "}\r\n", whichfmt, bytes_to_send);
	iaputs(ptr, bytes_to_send);
}



/*
 * Load a specific part of a message into the temp file to be output to a
 * client.  FIXME we can handle parts like "2" and "2.1" and even "2.MIME"
 * but we still can't handle "2.HEADER" (which might not be a problem).
 *
 * Note: mime_parser() was called with dont_decode set to 1, so we have the
 * luxury of simply spewing without having to re-encode.
 */
void imap_load_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		    char *cbid, void *cbuserdata)
{
	struct CitContext *CCC = CC;
	char mimebuf2[SIZ];
	StrBuf *desired_section;

	desired_section = (StrBuf *)cbuserdata;
	IMAP_syslog(LOG_DEBUG, "imap_load_part() looking for %s, found %s",
		    ChrPtr(desired_section),
		    partnum
		);

	if (!strcasecmp(partnum, ChrPtr(desired_section))) {
		client_write(content, length);
	}

	snprintf(mimebuf2, sizeof mimebuf2, "%s.MIME", partnum);

	if (!strcasecmp(ChrPtr(desired_section), mimebuf2)) {
		client_write(HKEY("Content-type: "));
	 	client_write(cbtype, strlen(cbtype));
		if (!IsEmptyStr(cbcharset)) {
			client_write(HKEY("; charset=\""));
			client_write(cbcharset, strlen(cbcharset));
			client_write(HKEY("\""));
		}
		if (!IsEmptyStr(name)) {
			client_write(HKEY("; name=\""));
			client_write(name, strlen(name));
			client_write(HKEY("\""));
		}
		client_write(HKEY("\r\n"));
		if (!IsEmptyStr(encoding)) {
			client_write(HKEY("Content-Transfer-Encoding: "));
			client_write(encoding, strlen(encoding));
			client_write(HKEY("\r\n"));
		}
		if (!IsEmptyStr(encoding)) {
			client_write(HKEY("Content-Disposition: "));
			client_write(disp, strlen(disp));
		
			if (!IsEmptyStr(filename)) {
				client_write(HKEY("; filename=\""));
				client_write(filename, strlen(filename));
				client_write(HKEY("\""));
			}
			client_write(HKEY("\r\n"));
		}
		cprintf("Content-Length: %ld\r\n\r\n", (long)length);
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
		IAPuts("((\"----\" NIL \"x\" \"x.org\")) ");
		return;
	}
	if (!is_room_aide() && (msg->cm_anon_type == MES_ANONOPT)) {
		IAPuts("((\"anonymous\" NIL \"x\" \"x.org\")) ");
		return;
	}

	/* For everything else, we do stuff. */
	IAPuts("(("); /* open double-parens */
	IPutMsgField(eAuthor);	/* personal name */
	IAPuts(" NIL ");	/* source route (not used) */


	if (!CM_IsEmpty(msg, erFc822Addr)) {
		process_rfc822_addr(msg->cm_fields[erFc822Addr], user, node, name);
		IPutStr(user, strlen(user));		/* mailbox name (user id) */
		IAPuts(" ");
		if (!strcasecmp(node, config.c_nodename)) {
			IPutStr(CFG_KEY(c_fqdn));
		}
		else {
			IPutStr(node, strlen(node));		/* host name */
		}
	}
	else {
		IPutMsgField(eAuthor); /* mailbox name (user id) */
		IAPuts(" ");
		IPutMsgField(eNodeName);	/* host name */
	}
	
	IAPuts(")) "); /* close double-parens */
}



/*
 * Output an envelope address (or set of addresses) in the official,
 * convoluted, braindead format.  (Note that we can't use this for
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
		IAPuts("NIL ");
		return;
	}

	if (IsEmptyStr(addr)) {
		IAPuts("NIL ");
		return;
	}

	IAPuts("(");

	/* How many addresses are listed here? */
	num_addrs = num_tokens(addr, ',');

	/* Output them one by one. */
	for (i=0; i<num_addrs; ++i) {
		extract_token(individual_addr, addr, i, ',', sizeof individual_addr);
		striplt(individual_addr);
		process_rfc822_addr(individual_addr, user, node, name);
		IAPuts("(");
		IPutStr(name, strlen(name));
		IAPuts(" NIL ");
		IPutStr(user, strlen(user));
		IAPuts(" ");
		IPutStr(node, strlen(node));
		IAPuts(")");
		if (i < (num_addrs-1)) 
			IAPuts(" ");
	}

	IAPuts(") ");
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
	long len;

	if (!msg) return;

	/* Parse the message date into an IMAP-format date string */
	if (!CM_IsEmpty(msg, eTimestamp)) {
		msgdate = atol(msg->cm_fields[eTimestamp]);
	}
	else {
		msgdate = time(NULL);
	}
	len = datestring(datestringbuf, sizeof datestringbuf,
			 msgdate, DATESTRING_IMAP);

	/* Now start spewing data fields.  The order is important, as it is
	 * defined by the protocol specification.  Nonexistent fields must
	 * be output as NIL, existent fields must be quoted or literalled.
	 * The imap_strout() function conveniently does all this for us.
	 */
	IAPuts("ENVELOPE (");

	/* Date */
	IPutStr(datestringbuf, len);
	IAPuts(" ");

	/* Subject */
	IPutMsgField(eMsgSubject);
	IAPuts(" ");

	/* From */
	imap_output_envelope_from(msg);

	/* Sender (default to same as 'From' if not present) */
	fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Sender");
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
		free(fieldptr);
	}
	else {
		imap_output_envelope_from(msg);
	}

	/* Reply-to */
	fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Reply-to");
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
		free(fieldptr);
	}
	else {
		imap_output_envelope_from(msg);
	}

	/* To */
	imap_output_envelope_addr(msg->cm_fields[eRecipient]);

	/* Cc (we do it this way because there might be a legacy non-Citadel Cc: field present) */
	fieldptr = msg->cm_fields[eCarbonCopY];
	if (fieldptr != NULL) {
		imap_output_envelope_addr(fieldptr);
	}
	else {
		fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Cc");
		imap_output_envelope_addr(fieldptr);
		if (fieldptr != NULL) free(fieldptr);
	}

	/* Bcc */
	fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Bcc");
	imap_output_envelope_addr(fieldptr);
	if (fieldptr != NULL) free(fieldptr);

	/* In-reply-to */
	fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "In-reply-to");
	IPutStr(fieldptr, (fieldptr)?strlen(fieldptr):0);
	IAPuts(" ");
	if (fieldptr != NULL) free(fieldptr);

	/* message ID */
	len = msg->cm_lengths[emessageId];
	
	if ((len == 0) || (
		    (msg->cm_fields[emessageId][0] == '<') && 
		    (msg->cm_fields[emessageId][len - 1] == '>'))
		)
	{
		IPutMsgField(emessageId);
	}
	else 
	{
		char *Buf = malloc(len + 3);
		long pos = 0;
		
		if (msg->cm_fields[emessageId][0] != '<')
		{
			Buf[pos] = '<';
			pos ++;
		}
		memcpy(&Buf[pos], msg->cm_fields[emessageId], len);
		pos += len;
		if (msg->cm_fields[emessageId][len] != '>')
		{
			Buf[pos] = '>';
			pos++;
		}
		Buf[pos] = '\0';
		IPutStr(Buf, pos);
		free(Buf);
	}
	IAPuts(")");
}

/*
 * This function is called only when CC->redirect_buffer contains a set of
 * RFC822 headers with no body attached.  Its job is to strip that set of
 * headers down to *only* the ones we're interested in.
 */
void imap_strip_headers(StrBuf *section) {
	citimap_command Cmd;
	StrBuf *which_fields = NULL;
	int doing_headers = 0;
	int headers_not = 0;
	int num_parms = 0;
	int i;
	StrBuf *boiled_headers = NULL;
	StrBuf *Line;
	int ok = 0;
	int done_headers = 0;
	const char *Ptr = NULL;
	CitContext *CCC = CC;

	if (CCC->redirect_buffer == NULL) return;

	which_fields = NewStrBufDup(section);

	if (!strncasecmp(ChrPtr(which_fields), "HEADER.FIELDS", 13))
		doing_headers = 1;
	if (doing_headers && 
	    !strncasecmp(ChrPtr(which_fields), "HEADER.FIELDS.NOT", 17))
		headers_not = 1;

	for (i=0; i < StrLength(which_fields); ++i) {
		if (ChrPtr(which_fields)[i]=='(')
			StrBufReplaceToken(which_fields, i, 1, HKEY(""));
	}
	for (i=0; i < StrLength(which_fields); ++i) {
		if (ChrPtr(which_fields)[i]==')') {
			StrBufCutAt(which_fields, i, NULL);
			break;
		}
	}
	memset(&Cmd, 0, sizeof(citimap_command));
	Cmd.CmdBuf = which_fields;
	num_parms = imap_parameterize(&Cmd);

	boiled_headers = NewStrBufPlain(NULL, StrLength(CCC->redirect_buffer));
	Line = NewStrBufPlain(NULL, SIZ);
	Ptr = NULL;
	ok = 0;
	do {
		StrBufSipLine(Line, CCC->redirect_buffer, &Ptr);

		if (!isspace(ChrPtr(Line)[0])) {

			if (doing_headers == 0) ok = 1;
			else {
				/* we're supposed to print all headers that are not matching the filter list */
				if (headers_not) for (i=0, ok = 1; (i < num_parms) && (ok == 1); ++i) {
						if ( (!strncasecmp(ChrPtr(Line), 
								   Cmd.Params[i].Key,
								   Cmd.Params[i].len)) &&
						     (ChrPtr(Line)[Cmd.Params[i].len]==':') ) {
							ok = 0;
						}
				}
				/* we're supposed to print all headers matching the filterlist */
				else for (i=0, ok = 0; ((i < num_parms) && (ok == 0)); ++i) {
						if ( (!strncasecmp(ChrPtr(Line), 
								   Cmd.Params[i].Key,
								   Cmd.Params[i].len)) &&
						     (ChrPtr(Line)[Cmd.Params[i].len]==':') ) {
							ok = 1;
					}
				}
			}
		}

		if (ok) {
			StrBufAppendBuf(boiled_headers, Line, 0);
			StrBufAppendBufPlain(boiled_headers, HKEY("\r\n"), 0);
		}

		if ((Ptr == StrBufNOTNULL)  ||
		    (StrLength(Line) == 0)  ||
		    (ChrPtr(Line)[0]=='\r') ||
		    (ChrPtr(Line)[0]=='\n')   ) done_headers = 1;
	} while (!done_headers);

	StrBufAppendBufPlain(boiled_headers, HKEY("\r\n"), 0);

	/* Now save it back (it'll always be smaller) */
	FreeStrBuf(&CCC->redirect_buffer);
	CCC->redirect_buffer = boiled_headers;

	free(Cmd.Params);
	FreeStrBuf(&which_fields);
	FreeStrBuf(&Line);
}


/*
 * Implements the BODY and BODY.PEEK fetch items
 */
void imap_fetch_body(long msgnum, ConstStr item, int is_peek) {
	struct CtdlMessage *msg = NULL;
	StrBuf *section;
	StrBuf *partial;
	int is_partial = 0;
	size_t pstart, pbytes;
	int loading_body_now = 0;
	int need_body = 1;
	int burn_the_cache = 0;
	CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;

	/* extract section */
	section = NewStrBufPlain(CKEY(item));
	
	if (strchr(ChrPtr(section), '[') != NULL) {
		StrBufStripAllBut(section, '[', ']');
	}
	IMAP_syslog(LOG_DEBUG, "Section is: [%s]", 
		    (StrLength(section) == 0) ? "(empty)" : ChrPtr(section)
	);

	/* Burn the cache if we don't have the same section of the 
	 * same message again.
	 */
	if (Imap->cached_body != NULL) {
		if (Imap->cached_bodymsgnum != msgnum) {
			burn_the_cache = 1;
		}
		else if ( (!Imap->cached_body_withbody) && (need_body) ) {
			burn_the_cache = 1;
		}
		else if (strcasecmp(Imap->cached_bodypart, ChrPtr(section))) {
			burn_the_cache = 1;
		}
		if (burn_the_cache) {
			/* Yup, go ahead and burn the cache. */
			free(Imap->cached_body);
			Imap->cached_body_len = 0;
			Imap->cached_body = NULL;
			Imap->cached_bodymsgnum = (-1);
			strcpy(Imap->cached_bodypart, "");
		}
	}

	/* extract partial */
	partial = NewStrBufPlain(CKEY(item));
	if (strchr(ChrPtr(partial), '<') != NULL) {
		StrBufStripAllBut(partial, '<', '>');
		is_partial = 1;
	}
	if ( (is_partial == 1) && (StrLength(partial) > 0) ) {
		IMAP_syslog(LOG_DEBUG, "Partial is <%s>", ChrPtr(partial));
	}

	if (Imap->cached_body == NULL) {
		CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
		loading_body_now = 1;
		msg = CtdlFetchMessage(msgnum, (need_body ? 1 : 0));
	}

	/* Now figure out what the client wants, and get it */

	if (!loading_body_now) {
		/* What we want is already in memory */
	}

	else if ( (!strcmp(ChrPtr(section), "1")) && (msg->cm_format_type != 4) ) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_NONE, 0, 1, SUPPRESS_ENV_TO);
	}

	else if (StrLength(section) == 0) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1, SUPPRESS_ENV_TO);
	}

	/*
	 * If the client asked for just headers, or just particular header
	 * fields, strip it down.
	 */
	else if (!strncasecmp(ChrPtr(section), "HEADER", 6)) {
		/* This used to work with HEADERS_FAST, but then Apple got stupid with their
		 * IMAP library and this broke Mail.App and iPhone Mail, so we had to change it
		 * to HEADERS_ONLY so the trendy hipsters with their iPhones can read mail.
		 */
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ONLY, 0, 1, SUPPRESS_ENV_TO);
		imap_strip_headers(section);
	}

	/*
	 * Strip it down if the client asked for everything _except_ headers.
	 */
	else if (!strncasecmp(ChrPtr(section), "TEXT", 4)) {
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_NONE, 0, 1, SUPPRESS_ENV_TO);
	}

	/*
	 * Anything else must be a part specifier.
	 * (Note value of 1 passed as 'dont_decode' so client gets it encoded)
	 */
	else {
		mime_parser(CM_RANGE(msg, eMesageText),
			    *imap_load_part, NULL, NULL,
			    section,
			    1
			);
	}

	if (loading_body_now) {
		Imap->cached_body_len = StrLength(CCC->redirect_buffer);
		Imap->cached_body = SmashStrBuf(&CCC->redirect_buffer);
		Imap->cached_bodymsgnum = msgnum;
		Imap->cached_body_withbody = need_body;
		strcpy(Imap->cached_bodypart, ChrPtr(section));
	}

	if (is_partial == 0) {
		IAPuts("BODY[");
		iaputs(SKEY(section));
		IAPrintf("] {" SIZE_T_FMT "}\r\n", Imap->cached_body_len);
		pstart = 0;
		pbytes = Imap->cached_body_len;
	}
	else {
		sscanf(ChrPtr(partial), SIZE_T_FMT "." SIZE_T_FMT, &pstart, &pbytes);
		if (pbytes > (Imap->cached_body_len - pstart)) {
			pbytes = Imap->cached_body_len - pstart;
		}
		IAPuts("BODY[");
		iaputs(SKEY(section));
		IAPrintf("]<" SIZE_T_FMT "> {" SIZE_T_FMT "}\r\n", pstart, pbytes);
	}

	FreeStrBuf(&partial);

	/* Here we go -- output it */
	iaputs(&Imap->cached_body[pstart], pbytes);

	if (msg != NULL) {
		CM_Free(msg);
	}

	/* Mark this message as "seen" *unless* this is a "peek" operation */
	if (is_peek == 0) {
		CtdlSetSeen(&msgnum, 1, 1, ctdlsetseen_seen, NULL, NULL);
	}
	FreeStrBuf(&section);
}

/*
 * Called immediately before outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_pre(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata
		) {

	IAPuts("(");
}



/*
 * Called immediately after outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_post(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata
		) {
	long len;
	char subtype[128];

	IAPuts(" ");

	/* disposition */
	len = extract_token(subtype, cbtype, 1, '/', sizeof subtype);
	IPutStr(subtype, len);

	/* body language */
	/* IAPuts(" NIL"); We thought we needed this at one point, but maybe we don't... */

	IAPuts(")");
}



/*
 * Output the info for a MIME part in the format required by BODYSTRUCTURE.
 *
 */
void imap_fetch_bodystructure_part(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata
		) {

	int have_cbtype = 0;
	int have_encoding = 0;
	int lines = 0;
	size_t i;
	char cbmaintype[128];
	char cbsubtype[128];
	long cbmaintype_len;
	long cbsubtype_len;

	if (cbtype != NULL) if (!IsEmptyStr(cbtype)) have_cbtype = 1;
	if (have_cbtype) {
		cbmaintype_len = extract_token(cbmaintype, cbtype, 0, '/', sizeof cbmaintype);
		cbsubtype_len = extract_token(cbsubtype, cbtype, 1, '/', sizeof cbsubtype);
	}
	else {
		strcpy(cbmaintype, "TEXT");
		cbmaintype_len = 4;
		strcpy(cbsubtype, "PLAIN");
		cbsubtype_len = 5;
	}

	IAPuts("(");
	IPutStr(cbmaintype, cbmaintype_len);			/* body type */
	IAPuts(" ");
	IPutStr(cbsubtype, cbsubtype_len);			/* body subtype */
	IAPuts(" ");

	IAPuts("(");						/* begin body parameter list */

	/* "NAME" must appear as the first parameter.  This is not required by IMAP,
	 * but the Asterisk voicemail application blindly assumes that NAME will be in
	 * the first position.  If it isn't, it rejects the message.
	 */
	if ((name != NULL) && (!IsEmptyStr(name))) {
		IAPuts("\"NAME\" ");
		IPutStr(name, strlen(name));
		IAPuts(" ");
	}

	IAPuts("\"CHARSET\" ");
	if ((cbcharset == NULL) || (cbcharset[0] == 0)){
		IPutStr(HKEY("US-ASCII"));
	}
	else {
		IPutStr(cbcharset, strlen(cbcharset));
	}
	IAPuts(") ");						/* end body parameter list */

	IAPuts("NIL ");						/* Body ID */
	IAPuts("NIL ");						/* Body description */

	if ((encoding != NULL) && (encoding[0] != 0))  have_encoding = 1;
	if (have_encoding) {
		IPutStr(encoding, strlen(encoding));
	}
	else {
		IPutStr(HKEY("7BIT"));
	}
	IAPuts(" ");

	/* The next field is the size of the part in bytes. */
	IAPrintf("%ld ", (long)length);	/* bytes */

	/* The next field is the number of lines in the part, if and only
	 * if the part is TEXT.  More gratuitous complexity.
	 */
	if (!strcasecmp(cbmaintype, "TEXT")) {
		if (length) for (i=0; i<length; ++i) {
			if (((char *)content)[i] == '\n') ++lines;
		}
		IAPrintf("%d ", lines);
	}

	/* More gratuitous complexity */
	if ((!strcasecmp(cbmaintype, "MESSAGE"))
	   && (!strcasecmp(cbsubtype, "RFC822"))) {
		/* FIXME: message/rfc822 also needs to output the envelope structure,
		 * body structure, and line count of the encapsulated message.  Fortunately
		 * there are not yet any clients depending on this, so we can get away
		 * with not implementing it for now.
		 */
	}

	/* MD5 value of body part; we can get away with NIL'ing this */
	IAPuts("NIL ");

	/* Disposition */
	if ((disp == NULL) || IsEmptyStr(disp)) {
		IAPuts("NIL");
	}
	else {
		IAPuts("(");
		IPutStr(disp, strlen(disp));
		if ((filename != NULL) && (!IsEmptyStr(filename))) {
			IAPuts(" (\"FILENAME\" ");
			IPutStr(filename, strlen(filename));
			IAPuts(")");
		}
		IAPuts(")");
	}

	/* Body language (not defined yet) */
	IAPuts(" NIL)");
}



/*
 * Spew the BODYSTRUCTURE data for a message.
 *
 */
void imap_fetch_bodystructure (long msgnum, const char *item,
		struct CtdlMessage *msg) {
	const char *rfc822 = NULL;
	const char *rfc822_body = NULL;
	size_t rfc822_len;
	size_t rfc822_headers_len;
	size_t rfc822_body_len;
	const char *ptr = NULL;
	char *pch;
	char buf[SIZ];
	int lines = 0;

	/* Handle NULL message gracefully */
	if (msg == NULL) {
		IAPuts("BODYSTRUCTURE (\"TEXT\" \"PLAIN\" "
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

		CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, 0, 0, 1, SUPPRESS_ENV_TO);
		rfc822_len = StrLength(CC->redirect_buffer);
		rfc822 = pch = SmashStrBuf(&CC->redirect_buffer);

		ptr = rfc822;
		do {
			ptr = cmemreadline(ptr, buf, sizeof buf);
			++lines;
			if ((IsEmptyStr(buf)) && (rfc822_body == NULL)) {
				rfc822_body = ptr;
			}
		} while (*ptr != 0);

		rfc822_headers_len = rfc822_body - rfc822;
		rfc822_body_len = rfc822_len - rfc822_headers_len;
		free(pch);

		IAPuts("BODYSTRUCTURE (\"TEXT\" \"PLAIN\" "
		       "(\"CHARSET\" \"US-ASCII\") NIL NIL "
		       "\"7BIT\" ");
		IAPrintf(SIZE_T_FMT " %d)", rfc822_body_len, lines);

		return;
	}

	/* For messages already stored in RFC822 format, we have to parse. */
	IAPuts("BODYSTRUCTURE ");
	mime_parser(CM_RANGE(msg, eMesageText),
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
void imap_do_fetch_msg(int seq, citimap_command *Cmd) {
	int i;
	citimap *Imap = IMAP;
	struct CtdlMessage *msg = NULL;
	int body_loaded = 0;

	/* Don't attempt to fetch bogus messages or UID's */
	if (seq < 1) return;
	if (Imap->msgids[seq-1] < 1L) return;

	buffer_output();
	IAPrintf("* %d FETCH (", seq);

	for (i=0; i<Cmd->num_parms; ++i) {

		/* Fetchable without going to the message store at all */
		if (!strcasecmp(Cmd->Params[i].Key, "UID")) {
			imap_fetch_uid(seq);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "FLAGS")) {
			imap_fetch_flags(seq-1);
		}

		/* Potentially fetchable from cache, if the client requests
		 * stuff from the same message several times in a row.
		 */
		else if (!strcasecmp(Cmd->Params[i].Key, "RFC822")) {
			imap_fetch_rfc822(Imap->msgids[seq-1], Cmd->Params[i].Key);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "RFC822.HEADER")) {
			imap_fetch_rfc822(Imap->msgids[seq-1], Cmd->Params[i].Key);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "RFC822.SIZE")) {
			imap_fetch_rfc822(Imap->msgids[seq-1], Cmd->Params[i].Key);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "RFC822.TEXT")) {
			imap_fetch_rfc822(Imap->msgids[seq-1], Cmd->Params[i].Key);
		}

		/* BODY fetches do their own fetching and caching too. */
		else if (!strncasecmp(Cmd->Params[i].Key, "BODY[", 5)) {
			imap_fetch_body(Imap->msgids[seq-1], Cmd->Params[i], 0);
		}
		else if (!strncasecmp(Cmd->Params[i].Key, "BODY.PEEK[", 10)) {
			imap_fetch_body(Imap->msgids[seq-1], Cmd->Params[i], 1);
		}

		/* Otherwise, load the message into memory.
		 */
		else if (!strcasecmp(Cmd->Params[i].Key, "BODYSTRUCTURE")) {
			if ((msg != NULL) && (!body_loaded)) {
				CM_Free(msg);	/* need the whole thing */
				msg = NULL;
			}
			if (msg == NULL) {
				msg = CtdlFetchMessage(Imap->msgids[seq-1], 1);
				body_loaded = 1;
			}
			imap_fetch_bodystructure(Imap->msgids[seq-1],
					Cmd->Params[i].Key, msg);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "ENVELOPE")) {
			if (msg == NULL) {
				msg = CtdlFetchMessage(Imap->msgids[seq-1], 0);
				body_loaded = 0;
			}
			imap_fetch_envelope(msg);
		}
		else if (!strcasecmp(Cmd->Params[i].Key, "INTERNALDATE")) {
			if (msg == NULL) {
				msg = CtdlFetchMessage(Imap->msgids[seq-1], 0);
				body_loaded = 0;
			}
			imap_fetch_internaldate(msg);
		}

		if (i != Cmd->num_parms-1) IAPuts(" ");
	}

	IAPuts(")\r\n");
	unbuffer_output();
	if (msg != NULL) {
		CM_Free(msg);
	}
}



/*
 * imap_fetch() calls imap_do_fetch() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_fetch(citimap_command *Cmd) {
	citimap *Imap = IMAP;
	int i;
#if 0
/* debug output the parsed vector */
	{
		int i;
		IMAP_syslog(LOG_DEBUG, "----- %ld params", Cmd->num_parms);

	for (i=0; i < Cmd->num_parms; i++) {
		if (Cmd->Params[i].len != strlen(Cmd->Params[i].Key))
			IMAP_syslog(LOG_DEBUG, "*********** %ld != %ld : %s",
				    Cmd->Params[i].len, 
				    strlen(Cmd->Params[i].Key),
				    Cmd->Params[i].Key);
		else
			IMAP_syslog(LOG_DEBUG, "%ld : %s",
				    Cmd->Params[i].len, 
				    Cmd->Params[i].Key);
	}}

#endif

	if (Imap->num_msgs > 0) {
		for (i = 0; i < Imap->num_msgs; ++i) {

			/* Abort the fetch loop if the session breaks.
			 * This is important for users who keep mailboxes
			 * that are too big *and* are too impatient to
			 * let them finish loading.  :)
			 */
			if (CC->kill_me) return;

			/* Get any message marked for fetch. */
			if (Imap->flags[i] & IMAP_SELECTED) {
				imap_do_fetch_msg(i+1, Cmd);
			}
		}
	}
}



/*
 * Back end for imap_handle_macros()
 * Note that this function *only* looks at the beginning of the string.  It
 * is not a generic search-and-replace function.
 */
void imap_macro_replace(StrBuf *Buf, long where, 
			StrBuf *TmpBuf,
			char *find, long findlen, 
			char *replace, long replacelen) 
{

	if (StrLength(Buf) - where > findlen)
		return;

	if (!strncasecmp(ChrPtr(Buf) + where, find, findlen)) {
		if (ChrPtr(Buf)[where + findlen] == ' ') {
			StrBufPlain(TmpBuf, replace, replacelen);
			StrBufAppendBufPlain(TmpBuf, HKEY(" "), 0);
			StrBufReplaceToken(Buf, where, findlen, 
					   SKEY(TmpBuf));
		}
		if (where + findlen == StrLength(Buf)) {
			StrBufReplaceToken(Buf, where, findlen, 
					   replace, replacelen);
		}
	}
}



/*
 * Handle macros embedded in FETCH data items.
 * (What the heck are macros doing in a wire protocol?  Are we trying to save
 * the computer at the other end the trouble of typing a lot of characters?)
 */
void imap_handle_macros(citimap_command *Cmd) {
	long i;
	int nest = 0;
	StrBuf *Tmp = NewStrBuf();

	for (i=0; i < StrLength(Cmd->CmdBuf); ++i) {
		char ch = ChrPtr(Cmd->CmdBuf)[i];
		if ((ch=='(') ||
		    (ch=='[') ||
		    (ch=='<') ||
		    (ch=='{')) ++nest;
		else if ((ch==')') ||
			 (ch==']') ||
			 (ch=='>') ||
			 (ch=='}')) --nest;

		if (nest <= 0) {
			imap_macro_replace(Cmd->CmdBuf, i,
					   Tmp, 
					   HKEY("ALL"),
					   HKEY("FLAGS INTERNALDATE RFC822.SIZE ENVELOPE")
			);
			imap_macro_replace(Cmd->CmdBuf, i,
					   Tmp, 
					   HKEY("BODY"),
					   HKEY("BODYSTRUCTURE")
			);
			imap_macro_replace(Cmd->CmdBuf, i,
					   Tmp, 
					   HKEY("FAST"),
					   HKEY("FLAGS INTERNALDATE RFC822.SIZE")
			);
			imap_macro_replace(Cmd->CmdBuf, i,
					   Tmp, 
					   HKEY("FULL"),
					   HKEY("FLAGS INTERNALDATE RFC822.SIZE ENVELOPE BODY")
			);
		}
	}
	FreeStrBuf(&Tmp);
}


/*
 * Break out the data items requested, possibly a parenthesized list.
 * Returns the number of data items, or -1 if the list is invalid.
 * NOTE: this function alters the string it is fed, and uses it as a buffer
 * to hold the data for the pointers it returns.
 */
int imap_extract_data_items(citimap_command *Cmd) 
{
	int nArgs;
	int nest = 0;
	const char *pch, *end;

	/* Convert all whitespace to ordinary space characters. */
	pch = ChrPtr(Cmd->CmdBuf);
	end = pch + StrLength(Cmd->CmdBuf);

	while (pch < end)
	{
		if (isspace(*pch)) 
			StrBufPeek(Cmd->CmdBuf, pch, 0, ' ');
		pch++;
	}

	/* Strip leading and trailing whitespace, then strip leading and
	 * trailing parentheses if it's a list
	 */
	StrBufTrim(Cmd->CmdBuf);
	pch = ChrPtr(Cmd->CmdBuf);
	if ( (pch[0]=='(') && 
	     (pch[StrLength(Cmd->CmdBuf)-1]==')') ) 
	{
		StrBufCutRight(Cmd->CmdBuf, 1);
		StrBufCutLeft(Cmd->CmdBuf, 1);
		StrBufTrim(Cmd->CmdBuf);
	}

	/* Parse any macro data items */
	imap_handle_macros(Cmd);

	/*
	 * Now break out the data items.  We throw in one trailing space in
	 * order to avoid having to break out the last one manually.
	 */
	nArgs = StrLength(Cmd->CmdBuf) / 10 + 10;
	nArgs = CmdAdjust(Cmd, nArgs, 0);
	Cmd->num_parms = 0;
	Cmd->Params[Cmd->num_parms].Key = pch = ChrPtr(Cmd->CmdBuf);
	end = Cmd->Params[Cmd->num_parms].Key + StrLength(Cmd->CmdBuf);

	while (pch < end) 
	{
		if ((*pch=='(') ||
		    (*pch=='[') ||
		    (*pch=='<') ||
		    (*pch=='{'))
			++nest;

		else if ((*pch==')') ||
			 (*pch==']') ||
			 (*pch=='>') ||
			 (*pch=='}'))
			--nest;

		if ((nest <= 0) && (*pch==' '))	{
			StrBufPeek(Cmd->CmdBuf, pch, 0, '\0');
			Cmd->Params[Cmd->num_parms].len = 
				pch - Cmd->Params[Cmd->num_parms].Key;

			if (Cmd->num_parms + 1 >= Cmd->avail_parms) {
				nArgs = CmdAdjust(Cmd, nArgs * 2, 1);
			}
			Cmd->num_parms++;			
			Cmd->Params[Cmd->num_parms].Key = ++pch;
		}
		else if (pch + 1 == end) {
			Cmd->Params[Cmd->num_parms].len = 
				pch - Cmd->Params[Cmd->num_parms].Key + 1;

			Cmd->num_parms++;			
		}
		pch ++;
	}
	return Cmd->num_parms;

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
void imap_pick_range(const char *supplied_range, int is_uid) {
	citimap *Imap = IMAP;
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
	for (i = 0; i < Imap->num_msgs; ++i) {
		Imap->flags[i] = Imap->flags[i] & ~IMAP_SELECTED;
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
		for (i = 1; i <= Imap->num_msgs; ++i) {
			if (is_uid) {	/* fetch by sequence number */
				if ( (Imap->msgids[i-1]>=lo)
				   && (Imap->msgids[i-1]<=hi)) {
					Imap->flags[i-1] |= IMAP_SELECTED;
				}
			}
			else {		/* fetch by uid */
				if ( (i>=lo) && (i<=hi)) {
					Imap->flags[i-1] |= IMAP_SELECTED;
				}
			}
		}
	}
}



/*
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, ConstStr *Params) {
	citimap_command Cmd;
	int num_items;
	
	if (num_parms < 4) {
		IReply("BAD invalid parameters");
		return;
	}

	imap_pick_range(Params[2].Key, 0);

	memset(&Cmd, 0, sizeof(citimap_command));
	Cmd.CmdBuf = NewStrBufPlain(NULL, StrLength(IMAP->Cmd.CmdBuf));
	MakeStringOf(Cmd.CmdBuf, 3);

	num_items = imap_extract_data_items(&Cmd);
	if (num_items < 1) {
		IReply("BAD invalid data item list");
		FreeStrBuf(&Cmd.CmdBuf);
		free(Cmd.Params);
		return;
	}

	imap_do_fetch(&Cmd);
	IReply("OK FETCH completed");
	FreeStrBuf(&Cmd.CmdBuf);
	free(Cmd.Params);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidfetch(int num_parms, ConstStr *Params) {
	citimap_command Cmd;
	int num_items;
	int i;
	int have_uid_item = 0;

	if (num_parms < 5) {
		IReply("BAD invalid parameters");
		return;
	}

	imap_pick_range(Params[3].Key, 1);

	memset(&Cmd, 0, sizeof(citimap_command));
	Cmd.CmdBuf = NewStrBufPlain(NULL, StrLength(IMAP->Cmd.CmdBuf));

	MakeStringOf(Cmd.CmdBuf, 4);
#if 0
	IMAP_syslog(LOG_DEBUG, "-------%s--------", ChrPtr(Cmd.CmdBuf));
#endif
	num_items = imap_extract_data_items(&Cmd);
	if (num_items < 1) {
		IReply("BAD invalid data item list");
		FreeStrBuf(&Cmd.CmdBuf);
		free(Cmd.Params);
		return;
	}

	/* If the "UID" item was not included, we include it implicitly
	 * (at the beginning) because this is a UID FETCH command
	 */
	for (i=0; i<num_items; ++i) {
		if (!strcasecmp(Cmd.Params[i].Key, "UID")) ++have_uid_item;
	}
	if (have_uid_item == 0) {
		if (Cmd.num_parms + 1 >= Cmd.avail_parms)
			CmdAdjust(&Cmd, Cmd.avail_parms + 1, 1);
		memmove(&Cmd.Params[1], 
			&Cmd.Params[0], 
			sizeof(ConstStr) * Cmd.num_parms);

		Cmd.num_parms++;
		Cmd.Params[0] = (ConstStr){HKEY("UID")};
	}

	imap_do_fetch(&Cmd);
	IReply("OK UID FETCH completed");
	FreeStrBuf(&Cmd.CmdBuf);
	free(Cmd.Params);
}


