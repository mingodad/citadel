/*
 * $Id$
 *
 * Implements the STORE command in IMAP.
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
#include "imap_store.h"
#include "genstamp.h"


/*
 * imap_do_store() calls imap_do_store_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_store_msg(int num, int num_items, char **itemlist) {
	int i;
	int flagbucket = 0;

	/* at this point it should be down to "item (flags)" */
	if (num_items < 2) return;

	/* put together the flag bucket */
	for (i=0; i<strlen(itemlist[1]); ++i) {
		if (!strncasecmp(&itemlist[1][i], "\\Deleted", 8))
			flagbucket |= IMAP_DELETED;
	}

	/*
	 * Figure out what to do and do it.  Brazenly IGnore the ".SILENT"
	 * option, since it is not illegal to output the data anyway.
	 */
	if (!strncasecmp(itemlist[0], "FLAGS", 5)) {
		IMAP->flags[num] &= IMAP_INTERNAL_MASK;
		IMAP->flags[num] |= flagbucket;
	}

	if (!strncasecmp(itemlist[0], "+FLAGS", 6)) {
		IMAP->flags[num] |= flagbucket;
	}

	if (!strncasecmp(itemlist[0], "-FLAGS", 6)) {
		IMAP->flags[num] &= ~flagbucket;
	}

	/*
	 * Tell the client what happen (someone set up us the bomb!)
	 */
	cprintf("* %d FETCH ", num+1);	/* output sequence number */
	imap_output_flags(num);
	cprintf("\r\n");
}



/*
 * imap_store() calls imap_do_store() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_store(int num_items, char **itemlist, int is_uid) {
	int i;

	if (IMAP->num_msgs > 0) {
		for (i = 0; i < IMAP->num_msgs; ++i) {
			if (IMAP->flags[i] && IMAP_SELECTED) {
				imap_do_store_msg(i, num_items, itemlist);
			}
		}
	}
}


/*
 * This function is called by the main command loop.
 */
void imap_store(int num_parms, char *parms[]) {
	char items[1024];
	char *itemlist[256];
	int num_items;
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[2])) {
		imap_pick_range(parms[2], 0);
	}
	else {
		cprintf("%s BAD No message set specified to STORE\r\n",
			parms[0]);
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

	imap_do_store(num_items, itemlist, 0);
	cprintf("%s OK STORE completed\r\n", parms[0]);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidstore(int num_parms, char *parms[]) {
	char items[1024];
	char *itemlist[256];
	int num_items;
	int i;

	if (num_parms < 5) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[3])) {
		imap_pick_range(parms[3], 1);
	}
	else {
		cprintf("%s BAD No message set specified to STORE\r\n",
			parms[0]);
		return;
	}

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

	imap_do_store(num_items, itemlist, 1);
	cprintf("%s OK UID STORE completed\r\n", parms[0]);
}


