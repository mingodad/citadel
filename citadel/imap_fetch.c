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


/*
 * imap_do_fetch() calls imap_do_fetch_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_fetch_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist) {

	cprintf("* %d FETCH ", seq);
	/* FIXME obviously we must do something here */
	cprintf("\r\n");
}



/*
 * imap_fetch() calls imap_do_fetch() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_fetch(int lo, int hi, int num_items, char **itemlist) {
	int i;
	struct CtdlMessage *msg;

	for (i=0; i<num_items; ++i) {
		lprintf(9, "* item[%d] = <%s>\r\n", i, itemlist[i]);
	}

	for (i = lo; i <= hi; ++i) {
		msg = CtdlFetchMessage(IMAP->msgids[i-1]);
		if (msg != NULL) {
			imap_do_fetch_msg(i, msg, num_items, itemlist);
			CtdlFreeMessage(msg);
		}
		else {
			cprintf("* %d FETCH <internal error>\r\n", i);
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
			lprintf(9, "WAS: %s\n", str);
			strcpy(holdbuf, &str[strlen(find)+1]);
			strcpy(str, replace);
			strcat(str, " ");
			strcat(str, holdbuf);
			lprintf(9, "NOW: %s\n", str);
		}
	}
}



/*
 * Handle macros embedded in FETCH data items.
 * (What the heck are macros doing in a wire protocol?  Are we trying to save
 * the computer at the other end the trouble of typing a lot of characters?)
 */
void imap_handle_macros(char *str) {
	imap_macro_replace(str, "meta", "foo bar baz");
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

	/*
	 * Now break out the data items.  We throw in one trailing space in
	 * order to avoid having to break out the last one manually.
	 */
	strcat(items, " ");
	start = items;
	initial_len = strlen(items);
	imap_handle_macros(start);
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
			imap_handle_macros(start);
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
	hi = atoi(histr);

	if ( (lo < 1) || (hi < 1) || (lo > hi) || (hi > IMAP->num_msgs) ) {
		cprintf("%s BAD invalid sequence numbers %d:%d\r\n",
			parms[0], lo, hi);
		return;
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

	imap_do_fetch(lo, hi, num_items, itemlist);
	cprintf("%s OK FETCH completed\r\n", parms[0]);
}



