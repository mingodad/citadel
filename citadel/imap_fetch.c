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
	char desired_section[256];
	FILE *output_fp;
};

/*
 * Individual field functions for imap_do_fetch_msg() ...
 */



void imap_fetch_uid(int seq) {
	cprintf("UID %ld", IMAP->msgids[seq-1]);
}

void imap_fetch_flags(struct CtdlMessage *msg) {
	cprintf("FLAGS ()");	/* FIXME do something here */
}

void imap_fetch_internaldate(struct CtdlMessage *msg) {
	char buf[256];
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
void imap_fetch_rfc822(int msgnum, char *whichfmt) {
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
	CtdlOutputMsg(msgnum, MT_RFC822, 0, 0, 1);
	CtdlRedirectOutput(NULL, -1);

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
 * FIXME this is TOTALLY BROKEN!!!
 */
void imap_fetch_part(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, size_t length,
		    void *cbuserdata)
{
	struct imap_fetch_part *imfp;

	imfp = (struct imap_fetch_part *)cbuserdata;

	if (!strcasecmp(partnum, imfp->desired_section)) {
		cprintf("part=%s|%s|%s|%s|%s|%d\r\n",
			name, filename, partnum, disp, cbtype, length, NULL);
	}
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
	lprintf(9, "Partial is %s\n", partial);

	tmp = tmpfile();
	if (tmp == NULL) {
		lprintf(1, "Cannot open temp file: %s\n", strerror(errno));
		return;
	}

	/* Now figure out what the client wants, and get it */

	if (!strcmp(section, "")) {		/* the whole thing */
		CtdlRedirectOutput(tmp, -1);
		CtdlOutputMsg(msgnum, MT_RFC822, 0, 0, 1);
		CtdlRedirectOutput(NULL, -1);
	}

	/*
	 * Be obnoxious and send the entire header, even if the client only
	 * asks for certain fields.  FIXME this shortcut later.
	 */
	else if (!strncasecmp(section, "HEADER", 6)) {
		CtdlRedirectOutput(tmp, -1);
		CtdlOutputMsg(msgnum, MT_RFC822, 1, 0, 1);
		CtdlRedirectOutput(NULL, -1);
		fprintf(tmp, "\r\n");	/* add the trailing newline */
	}

	/*
	 * Anything else must be a part specifier.
	 */
	else {
		safestrncpy(imfp.desired_section, section,
				sizeof(imfp.desired_section));
		imfp.output_fp = tmp;

		mime_parser(msg->cm_fields['M'], NULL,
				*imap_fetch_part,
				(void *)&imfp);
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
 * imap_do_fetch() calls imap_do_fetch_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_fetch_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist) {
	int i;

	cprintf("* %d FETCH (", seq);

	for (i=0; i<num_items; ++i) {

		if (!strncasecmp(itemlist[i], "BODY[", 5)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i], 0, msg);
		}
		else if (!strncasecmp(itemlist[i], "BODY.PEEK[", 10)) {
			imap_fetch_body(IMAP->msgids[seq-1], itemlist[i], 1, msg);
		}
		else if (!strcasecmp(itemlist[i], "BODYSTRUCTURE")) {
			/* FIXME do something here */
		}
		else if (!strcasecmp(itemlist[i], "ENVELOPE")) {
			/* FIXME do something here */
		}
		else if (!strcasecmp(itemlist[i], "FLAGS")) {
			imap_fetch_flags(msg);
		}
		else if (!strcasecmp(itemlist[i], "INTERNALDATE")) {
			imap_fetch_internaldate(msg);
		}
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
	  if (IMAP->flags[i] && IMAP_FETCHED) {
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
 * handles this by setting the IMAP_FETCHED flag for each message specified in
 * the ranges/sets, then looping through the message array, outputting messages
 * with the flag set.  We don't bother returning an error if an out-of-range
 * number is specified (we just return quietly) because any client braindead
 * enough to request a bogus message number isn't going to notice the
 * difference anyway.
 *
 * This function clears out the IMAP_FETCHED bits, then sets that bit for each
 * message included in the specified range.
 *
 * Set is_uid to 1 to fetch by UID instead of sequence number.
 */
void imap_pick_range(char *range, int is_uid) {
	int i;
	int num_sets;
	int s;
	char setstr[1024], lostr[1024], histr[1024];
	int lo, hi;

	/*
	 * Clear out the IMAP_FETCHED flags for all messages.
	 */
	for (i = 1; i <= IMAP->num_msgs; ++i) {
		IMAP->flags[i-1] = IMAP->flags[i-1] & ~IMAP_FETCHED;
	}

	/*
	 * Now set it for all specified messages.
	 */
	num_sets = num_tokens(range, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, range, s, ',');

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
						IMAP->flags[i-1] | IMAP_FETCHED;
				}
			}
			else {		/* fetch by uid */
				if ( (i>=lo) && (i<=hi)) {
					IMAP->flags[i-1] =
						IMAP->flags[i-1] | IMAP_FETCHED;
				}
			}
		}
	}
}



/*
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, char *parms[]) {
	char items[1024];
	char *itemlist[256];
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
	char items[1024];
	char *itemlist[256];
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


