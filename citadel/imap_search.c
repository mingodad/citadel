/*
 * $Id$
 *
 * Implements the SEARCH command in IMAP.
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
#include "imap_search.h"
#include "genstamp.h"






/*
 * imap_do_search() calls imap_do_search_msg() to output the deta of an
 * individual message, once it has been successfully loaded from disk.
 */
void imap_do_search_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist, int is_uid) {

	int is_valid = 0;

	/*** FIXME ***
	 * We haven't written the search command logic yet.  Instead we
	 * simply return every message as valid.  This will probably surprise
	 * some users.  Could someone out there please be the hero of the
	 * century and write this function?
	 */
	is_valid = 1;

	/*
	 * If the message meets the specified search criteria, output its
	 * sequence number *or* UID, depending on what the client wants.
	 */
	if (is_valid) {
		if (is_uid)	cprintf("%ld ", IMAP->msgids[seq-1]);
		else		cprintf("%d ", seq);
	}

}



/*
 * imap_search() calls imap_do_search() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_search(int num_items, char **itemlist, int is_uid) {
	int i;
	struct CtdlMessage *msg;

	cprintf("* SEARCH ");
	if (IMAP->num_msgs > 0)
	 for (i = 0; i < IMAP->num_msgs; ++i)
	  if (IMAP->flags[i] && IMAP_SELECTED) {
		msg = CtdlFetchMessage(IMAP->msgids[i]);
		if (msg != NULL) {
			imap_do_search_msg(i+1, msg, num_items,
					itemlist, is_uid);
			CtdlFreeMessage(msg);
		}
		else {
			lprintf(1, "SEARCH internal error\n");
		}
	}
	cprintf("\r\n");
}


/*
 * This function is called by the main command loop.
 */
void imap_search(int num_parms, char *parms[]) {
	char items[1024];
	char *itemlist[256];
	int num_items;
	int i;

	if (num_parms < 3) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	for (i=1; i<num_parms; ++i) {
		if (imap_is_message_set(parms[i])) {
			imap_pick_range(parms[i], 0);
		}
	}

	strcpy(items, "");
	for (i=2; i<num_parms; ++i) {
		strcat(items, parms[i]);
		if (i < (num_parms-1)) strcat(items, " ");
	}

	num_items = imap_extract_data_items(itemlist, items);
	if (num_items < 1) {
		cprintf("%s BAD invalid data item list\r\n", parms[0]);
		return;
	}

	imap_do_search(num_items, itemlist, 0);
	cprintf("%s OK SEARCH completed\r\n", parms[0]);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidsearch(int num_parms, char *parms[]) {
	char items[1024];
	char *itemlist[256];
	int num_items;
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	for (i=1; i<num_parms; ++i) {
		if (imap_is_message_set(parms[i])) {
			imap_pick_range(parms[i], 1);
		}
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

	imap_do_search(num_items, itemlist, 1);
	cprintf("%s OK UID SEARCH completed\r\n", parms[0]);
}


