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
	size_t headers_size, text_size, total_size;
	size_t bytes_remaining = 0;
	size_t blocksize;

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
 * imap_do_fetch() calls imap_do_fetch_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_fetch_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist) {
	int i;

	cprintf("* %d FETCH (", seq);

	for (i=0; i<num_items; ++i) {

		if (!strncasecmp(itemlist[i], "BODY[", 5)) {
			/* FIXME do something here */
		}
		else if (!strncasecmp(itemlist[i], "BODY.PEEK[", 10)) {
			/* FIXME do something here */
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
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, char *parms[]) {
	int lo = 0;
	int hi = 0;
	char lostr[1024], histr[1024], items[1024];
	char *itemlist[256];
	int num_items;
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	extract_token(lostr, parms[2], 0, ':');
	lo = atoi(lostr);
	extract_token(histr, parms[2], 1, ':');
	if (!strcmp(histr, "*")) {
		hi = IMAP->num_msgs;
	}
	else {
		hi = atoi(histr);
	}

	/* Clear out the IMAP_FETCHED flags and then set them for the messages
	 * we're interested in.
	 */
	for (i = 1; i <= IMAP->num_msgs; ++i) {
		IMAP->flags[i-1] = IMAP->flags[i-1] & ~IMAP_FETCHED;
	}

	for (i = 1; i <= IMAP->num_msgs; ++i) {
		if ( (i>=lo) && (i<=hi) ) {
			IMAP->flags[i-1] = IMAP->flags[i-1] | IMAP_FETCHED;
		}
	}

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






