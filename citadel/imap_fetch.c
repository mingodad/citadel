/*
 * $Id$
 *
 * Implements the FETCH command in IMAP.
 * This command is way too convoluted.  Marc Crispin is a fscking idiot.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
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



struct imap_fetch_part {
	char desired_section[SIZ];
	FILE *output_fp;
};

/*
 * Individual field functions for imap_do_fetch_msg() ...
 */



void imap_fetch_uid(int seq) {
	cprintf("UID %ld", IMAP->msgids[seq-1]);
}

void imap_fetch_flags(int seq) {
	cprintf("FLAGS (");
	if (IMAP->flags[seq] & IMAP_DELETED) cprintf("\\Deleted ");
	cprintf(")");
}

void imap_fetch_internaldate(struct CtdlMessage *msg) {
	char buf[SIZ];
	time_t msgdate;

	if (msg->cm_fields['T'] != NULL) {
		msgdate = atol(msg->cm_fields['T']);
	}
	else {
		msgdate = time(NULL);
	}

	datestring(buf, msgdate, DATESTRING_IMAP);
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
void imap_fetch_rfc822(int msgnum, char *whichfmt, struct CtdlMessage *msg) {
	FILE *tmp;
	char buf[1024];
	char *ptr;
	long headers_size, text_size, total_size;
	long bytes_remaining = 0;
	long blocksize;

	tmp = tmpfile();
	if (tmp == NULL) {
		lprintf(1, "Cannot open temp file: %s\n", strerror(errno));
		return;
	}

	/*
	 * Load the message into a temp file for translation and measurement
	 */ 
	CtdlRedirectOutput(tmp, -1);
	CtdlOutputPreLoadedMsg(msg, msgnum, MT_RFC822, 0, 0, 1);
	CtdlRedirectOutput(NULL, -1);
	if (!is_valid_message(msg)) {
		lprintf(1, "WARNING: output clobbered the message!\n");
	}

	/*
	 * Now figure out where the headers/text break is.  IMAP considers the
	 * intervening blank line to be part of the headers, not the text.
	 */
	rewind(tmp);
	headers_size = 0L;
	do {
		ptr = fgets(buf, sizeof buf, tmp);
		if (ptr != NULL) {
			striplt(buf);
			if (strlen(buf) == 0) headers_size = ftell(tmp);
		}
	} while ( (headers_size == 0L) && (ptr != NULL) );
	fseek(tmp, 0L, SEEK_END);
	total_size = ftell(tmp);
	text_size = total_size - headers_size;

	if (!strcasecmp(whichfmt, "RFC822.SIZE")) {
		cprintf("RFC822.SIZE %ld", total_size);
		fclose(tmp);
		return;
	}

	else if (!strcasecmp(whichfmt, "RFC822")) {
		bytes_remaining = total_size;
		rewind(tmp);
	}

	else if (!strcasecmp(whichfmt, "RFC822.HEADER")) {
		bytes_remaining = headers_size;
		rewind(tmp);
	}

	else if (!strcasecmp(whichfmt, "RFC822.TEXT")) {
		bytes_remaining = text_size;
		fseek(tmp, headers_size, SEEK_SET);
	}

	cprintf("%s {%ld}\r\n", whichfmt, bytes_remaining);
	blocksize = sizeof(buf);
	while (bytes_remaining > 0L) {
		if (blocksize > bytes_remaining) blocksize = bytes_remaining;
		fread(buf, blocksize, 1, tmp);
		client_write(buf, blocksize);
		bytes_remaining = bytes_remaining - blocksize;
	}

	fclose(tmp);
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
		    void *content, char *cbtype, size_t length, char *encoding,
		    void *cbuserdata)
{
	struct imap_fetch_part *imfp;
	char mbuf2[1024];

	imfp = (struct imap_fetch_part *)cbuserdata;

	if (!strcasecmp(partnum, imfp->desired_section)) {
		fwrite(content, length, 1, imfp->output_fp);
	}

	sprintf(mbuf2, "%s.MIME", partnum);

	if (!strcasecmp(imfp->desired_section, mbuf2)) {
		fprintf(imfp->output_fp, "Content-type: %s", cbtype);
		if (strlen(name) > 0)
			fprintf(imfp->output_fp, "; name=\"%s\"", name);
		fprintf(imfp->output_fp, "\r\n");
		if (strlen(encoding) > 0)
			fprintf(imfp->output_fp,
				"Content-Transfer-Encoding: %s\r\n", encoding);
		if (strlen(encoding) > 0) {
			fprintf(imfp->output_fp, "Content-Disposition: %s",
					disp);
			if (strlen(filename) > 0) {
				fprintf(imfp->output_fp, "; filename=\"%s\"",
					filename);
			}
			fprintf(imfp->output_fp, "\r\n");
		}
		fprintf(imfp->output_fp, "Content-Length: %d\r\n", length);
		fprintf(imfp->output_fp, "\r\n");
	}
			

}


/* 
 * Called by imap_fetch_envelope() to output the "From" field.
 * This is in its own function because its logic is kind of complex.  We
 * really need to make this suck less.
 */
void imap_output_envelope_from(struct CtdlMessage *msg) {
	char user[1024], node[1024], name[1024];

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
 * Implements the ENVELOPE fetch item
 * 
 * FIXME ... we only output some of the fields right now.  Definitely need
 *           to do all of them.  Accurately, too.
 *
 * Note that the imap_strout() function can cleverly output NULL fields as NIL,
 * so we don't have to check for that condition like we do elsewhere.
 */
void imap_fetch_envelope(long msgnum, struct CtdlMessage *msg) {
	char datestringbuf[SIZ];
	time_t msgdate;
	char *fieldptr = NULL;

	/* Parse the message date into an IMAP-format date string */
	if (msg->cm_fields['T'] != NULL) {
		msgdate = atol(msg->cm_fields['T']);
	}
	else {
		msgdate = time(NULL);
	}
	datestring(datestringbuf, msgdate, DATESTRING_IMAP);

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

	/* Sender */
	if (0) {
		/* FIXME ... check for a *real* Sender: field */
	}
	else {
		imap_output_envelope_from(msg);
	}

	/* Reply-to */
	if (0) {
		/* FIXME ... check for a *real* Reply-to: field */
	}
	else {
		imap_output_envelope_from(msg);
	}

	cprintf("NIL ");	/* to */

	cprintf("NIL ");	/* cc */

	cprintf("NIL ");	/* bcc */

	/* In-reply-to */
	fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "In-reply-to");
	imap_strout(fieldptr);
	cprintf(" ");
	if (fieldptr != NULL) phree(fieldptr);

	/* message ID */
	imap_strout(msg->cm_fields['I']);

	cprintf(") ");
}


/*
 * Strip any non header information out of a chunk of RFC822 data on disk,
 * then boil it down to just the fields we want.
 */
void imap_strip_headers(FILE *fp, char *section) {
	char buf[1024];
	char *which_fields = NULL;
	int doing_headers = 0;
	int headers_not = 0;
	char *parms[SIZ];
        int num_parms = 0;
	int i;
	char *boiled_headers = NULL;
	int ok = 0;
	int done_headers = 0;

	which_fields = strdoop(section);

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

	fseek(fp, 0L, SEEK_END);
	boiled_headers = mallok((size_t)(ftell(fp) + 256L));
	strcpy(boiled_headers, "");

	rewind(fp);
	ok = 0;
	while ( (done_headers == 0) && (fgets(buf, sizeof buf, fp) != NULL) ) {
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
		}

		if (strlen(buf) == 0) done_headers = 1;
		if (buf[0]=='\r') done_headers = 1;
		if (buf[0]=='\n') done_headers = 1;
	}

	/* Now write it back */
	rewind(fp);
	fwrite(boiled_headers, strlen(boiled_headers), 1, fp);
	fflush(fp);
	ftruncate(fileno(fp), ftell(fp));
	fflush(fp);
	fprintf(fp, "\r\n");	/* add the trailing newline */
	rewind(fp);
	phree(which_fields);
	phree(boiled_headers);
}


/*
 * Implements the BODY and BODY.PEEK fetch items
 */
void imap_fetch_body(long msgnum, char *item, int is_peek,
		struct CtdlMessage *msg) {
	char section[1024];
	char partial[1024];
	int is_partial = 0;
	char buf[1024];
	int i;
	FILE *tmp;
	long bytes_remaining = 0;
	long blocksize;
	long pstart, pbytes;
	struct imap_fetch_part imfp;

	/* extract section */
	strcpy(section, item);
	for (i=0; i<strlen(section); ++i) {
		if (section[i]=='[') strcpy(section, &section[i+1]);
	}
	for (i=0; i<strlen(section); ++i) {
		if (section[i]==']') section[i] = 0;
	}
	lprintf(9, "Section is %s\n", section);

	/* extract partial */
	strcpy(partial, item);
	for (i=0; i<strlen(partial); ++i) {
		if (partial[i]=='<') {
			strcpy(partial, &partial[i+1]);
			is_partial = 1;
		}
	}
	for (i=0; i<strlen(partial); ++i) {
		if (partial[i]=='>') partial[i] = 0;
	}
	if (is_partial == 0) strcpy(partial, "");
	if (strlen(partial) > 0) lprintf(9, "Partial is %s\n", partial);

	tmp = tmpfile();
	if (tmp == NULL) {
		lprintf(1, "Cannot open temp file: %s\n", strerror(errno));
		return;
	}

	/* Now figure out what the client wants, and get it */

	if (!strcmp(section, "")) {		/* the whole thing */
		CtdlRedirectOutput(tmp, -1);
		CtdlOutputPreLoadedMsg(msg, msgnum, MT_RFC822, 0, 0, 1);
		CtdlRedirectOutput(NULL, -1);
	}

	/*
	 * If the client asked for just headers, or just particular header
	 * fields, strip it down.
	 */
	else if (!strncasecmp(section, "HEADER", 6)) {
		CtdlRedirectOutput(tmp, -1);
		CtdlOutputPreLoadedMsg(msg, msgnum, MT_RFC822, 1, 0, 1);
		CtdlRedirectOutput(NULL, -1);
		imap_strip_headers(tmp, section);
	}

	/*
	 * Anything else must be a part specifier.
	 * (Note value of 1 passed as 'dont_decode' so client gets it encoded)
	 */
	else {
		safestrncpy(imfp.desired_section, section,
				sizeof(imfp.desired_section));
		imfp.output_fp = tmp;

		mime_parser(msg->cm_fields['M'], NULL,
				*imap_load_part, NULL, NULL,
				(void *)&imfp,
				1);
	}


	fseek(tmp, 0L, SEEK_END);
	bytes_remaining = ftell(tmp);

	if (is_partial == 0) {
		rewind(tmp);
		cprintf("BODY[%s] {%ld}\r\n", section, bytes_remaining);
	}
	else {
		sscanf(partial, "%ld.%ld", &pstart, &pbytes);
		if ((bytes_remaining - pstart) < pbytes) {
			pbytes = bytes_remaining - pstart;
		}
		fseek(tmp, pstart, SEEK_SET);
		bytes_remaining = pbytes;
		cprintf("BODY[%s] {%ld}<%ld>\r\n",
			section, bytes_remaining, pstart);
	}

	blocksize = sizeof(buf);
	while (bytes_remaining > 0L) {
		if (blocksize > bytes_remaining) blocksize = bytes_remaining;
		fread(buf, blocksize, 1, tmp);
		client_write(buf, blocksize);
		bytes_remaining = bytes_remaining - blocksize;
	}

	fclose(tmp);

	if (is_peek) {
		/* FIXME set the last read pointer or something */
	}
}

/*
 * Called immediately before outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_pre(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata
		) {

	cprintf("(");
}



/*
 * Called immediately after outputting a multipart bodystructure
 */
void imap_fetch_bodystructure_post(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata
		) {

	char subtype[SIZ];

	extract_token(subtype, cbtype, 1, '/');
	imap_strout(subtype);
	cprintf(")");
}



/*
 * Output the info for a MIME part in the format required by BODYSTRUCTURE.
 *
 */
void imap_fetch_bodystructure_part(
		char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata
		) {

	char buf[SIZ];
	int have_cbtype = 0;
	int have_encoding = 0;

	cprintf("(");

	if (cbtype != NULL) if (strlen(cbtype)>0) have_cbtype = 1;

	if (have_cbtype) {
		extract_token(buf, cbtype, 0, '/');
		imap_strout(buf);
		cprintf(" ");
		extract_token(buf, cbtype, 1, '/');
		imap_strout(buf);
		cprintf(" ");
	}
	else {
		cprintf("\"TEXT\" \"PLAIN\" ");
	}

	cprintf("(\"CHARSET\" \"US-ASCII\"");

	if (name != NULL) if (strlen(name)>0) {
		cprintf(" \"NAME\" ");
		imap_strout(name);
	}

	if (filename != NULL) if (strlen(filename)>0) {
		cprintf(" \"FILENAME\" ");
		imap_strout(name);
	}

	cprintf(") ");

	cprintf("NIL NIL ");

	if (encoding != NULL) if (strlen(encoding) > 0)  have_encoding = 1;

	if (have_encoding) {
		imap_strout(encoding);
	}
	else {
		imap_strout("7BIT");
	}
	cprintf(" ");

	cprintf("%ld ", length);	/* bytes */
	cprintf("NIL) ");		/* lines */
}



/*
 * Spew the BODYSTRUCTURE data for a message.  (Do you need a silencer if
 * you're going to shoot a MIME?  Do you need a reason to shoot Mark Crispin?
 * No, and no.)
 *
 */
void imap_fetch_bodystructure (long msgnum, char *item,
		struct CtdlMessage *msg) {
	FILE *tmp;
	char buf[1024];
	long lines = 0L;
	long bytes = 0L;

	/* For non-RFC822 (ordinary Citadel) messages, this is short and
	 * sweet...
	 */
	if (msg->cm_format_type != FMT_RFC822) {

		/* *sigh* We have to RFC822-format the message just to be able
		 * to measure it.
		 */
		tmp = tmpfile();
		if (tmp == NULL) return;
		CtdlRedirectOutput(tmp, -1);
		CtdlOutputPreLoadedMsg(msg, msgnum, MT_RFC822, 0, 0, 1);
		CtdlRedirectOutput(NULL, -1);

		rewind(tmp);
		while (fgets(buf, sizeof buf, tmp) != NULL) ++lines;
		bytes = ftell(tmp);
		fclose(tmp);

		cprintf("BODYSTRUCTURE (\"TEXT\" \"PLAIN\" "
			"(\"CHARSET\" \"US-ASCII\") NIL NIL "
			"\"7BIT\" %ld %ld)", bytes, lines);

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
			0);
}






/*
 * imap_do_fetch() calls imap_do_fetch_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_fetch_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist) {
	int i;

	cprintf("* %d FETCH (", seq);

	for (i=0; i<num_items; ++i) {

		if (!strncasecmp(itemlist[i], "BODY[", 5)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i],
					0, msg);
		}
		else if (!strncasecmp(itemlist[i], "BODY.PEEK[", 10)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i],
					1, msg);
		}
		else if (!strcasecmp(itemlist[i], "BODYSTRUCTURE")) {
			imap_fetch_bodystructure(IMAP->msgids[seq-1],
					itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "ENVELOPE")) {
			imap_fetch_envelope(IMAP->msgids[seq-1], msg);
		}
		else if (!strcasecmp(itemlist[i], "FLAGS")) {
			imap_fetch_flags(seq-1);
		}
		else if (!strcasecmp(itemlist[i], "INTERNALDATE")) {
			imap_fetch_internaldate(msg);
		}
		else if (!strcasecmp(itemlist[i], "RFC822")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.HEADER")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.SIZE")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "RFC822.TEXT")) {
			imap_fetch_rfc822(IMAP->msgids[seq-1], itemlist[i], msg);
		}
		else if (!strcasecmp(itemlist[i], "UID")) {
			imap_fetch_uid(seq);
		}

		if (i != num_items-1) cprintf(" ");
	}

	cprintf(")\r\n");
}



/*
 * imap_fetch() calls imap_do_fetch() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_fetch(int num_items, char **itemlist) {
	int i;
	struct CtdlMessage *msg;

	if (IMAP->num_msgs > 0)
	 for (i = 0; i < IMAP->num_msgs; ++i)
	  if (IMAP->flags[i] & IMAP_SELECTED) {
		msg = CtdlFetchMessage(IMAP->msgids[i]);
		if (msg != NULL) {
			imap_do_fetch_msg(i+1, msg, num_items, itemlist);
			CtdlFreeMessage(msg);
		}
		else {
			cprintf("* %d FETCH <internal error>\r\n", i+1);
		}
	}
}



/*
 * Back end for imap_handle_macros()
 * Note that this function *only* looks at the beginning of the string.  It
 * is not a generic search-and-replace function.
 */
void imap_macro_replace(char *str, char *find, char *replace) {
	char holdbuf[1024];

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
	char setstr[SIZ], lostr[SIZ], histr[SIZ];	/* was 1024 */
	int lo, hi;
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
		extract_token(setstr, actual_range, s, ',');

		extract_token(lostr, setstr, 0, ':');
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':');
			if (!strcmp(histr, "*")) sprintf(histr, "%d", INT_MAX);
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atoi(lostr);
		hi = atoi(histr);

		/* Loop through the array, flipping bits where appropriate */
		for (i = 1; i <= IMAP->num_msgs; ++i) {
			if (is_uid) {	/* fetch by sequence number */
				if ( (IMAP->msgids[i-1]>=lo)
				   && (IMAP->msgids[i-1]<=hi)) {
					IMAP->flags[i-1] =
						IMAP->flags[i-1] | IMAP_SELECTED;
				}
			}
			else {		/* fetch by uid */
				if ( (i>=lo) && (i<=hi)) {
					IMAP->flags[i-1] =
						IMAP->flags[i-1] | IMAP_SELECTED;
				}
			}
		}
	}

}



/*
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, char *parms[]) {
	char items[SIZ];	/* was 1024 */
	char *itemlist[SIZ];
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
	char items[SIZ];	/* was 1024 */
	char *itemlist[SIZ];
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
	 * because this is a UID FETCH command
	 */
	for (i=0; i<num_items; ++i) {
		if (!strcasecmp(itemlist[i], "UID")) ++have_uid_item;
	}
	if (have_uid_item == 0) itemlist[num_items++] = "UID";

	imap_do_fetch(num_items, itemlist);
	cprintf("%s OK UID FETCH completed\r\n", parms[0]);
}


