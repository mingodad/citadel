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
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_store.h"
#include "genstamp.h"






/*
 * imap_do_store() calls imap_do_store_msg() to tweak the settings of
 * an individual message.
 *
 * We also implement the ".SILENT" protocol option here.  :(
 */
void imap_do_store_msg(int seq, char *oper, unsigned int bits_to_twiddle) {
	int silent = 0;

	if (!strncasecmp(oper, "FLAGS", 5)) {
		IMAP->flags[seq] &= IMAP_MASK_SYSTEM;
		IMAP->flags[seq] |= bits_to_twiddle;
		silent = strncasecmp(&oper[5], ".SILENT", 7);
	}
	else if (!strncasecmp(oper, "+FLAGS", 6)) {
		IMAP->flags[seq] |= bits_to_twiddle;
		silent = strncasecmp(&oper[6], ".SILENT", 7);
	}
	else if (!strncasecmp(oper, "-FLAGS", 6)) {
		IMAP->flags[seq] &= (~bits_to_twiddle);
		silent = strncasecmp(&oper[6], ".SILENT", 7);
	}

	if (bits_to_twiddle & IMAP_SEEN) {
		CtdlSetSeen(IMAP->msgids[seq],
				((IMAP->flags[seq] & IMAP_SEEN) ? 1 : 0),
				ctdlsetseen_seen
		);
	}
	if (bits_to_twiddle & IMAP_ANSWERED) {
		CtdlSetSeen(IMAP->msgids[seq],
				((IMAP->flags[seq] & IMAP_ANSWERED) ? 1 : 0),
				ctdlsetseen_answered
		);
	}

	/* 'silent' is actually the value returned from a strncasecmp() so
	 * we want that option only if its value is zero.  Seems backwards
	 * but that's the way it's supposed to be.
	 */
	if (silent) {
		cprintf("* %d FETCH (", seq+1);
		imap_fetch_flags(seq);
		cprintf(")\r\n");
	}
}



/*
 * imap_store() calls imap_do_store() to perform the actual bit twiddling
 * on the flags.
 */
void imap_do_store(int num_items, char **itemlist) {
	int i, j;
	unsigned int bits_to_twiddle = 0;
	char *oper;
	char flag[SIZ];
	char whichflags[SIZ];
	char num_flags;

	if (num_items < 2) return;
	oper = itemlist[0];

	for (i=1; i<num_items; ++i) {
		strcpy(whichflags, itemlist[i]);
		if (whichflags[0]=='(') strcpy(whichflags, &whichflags[1]);
		if (whichflags[strlen(whichflags)-1]==')') {
			whichflags[strlen(whichflags)-1]=0;
		}
		striplt(whichflags);

		/* A client might twiddle more than one bit at a time.
		 * Note that we check for the flag names without the leading
		 * backslash because imap_parameterize() strips them out.
		 */
		num_flags = num_tokens(whichflags, ' ');
		for (j=0; j<num_flags; ++j) {
			extract_token(flag, whichflags, j, ' ');

			if ((!strcasecmp(flag, "\\Deleted"))
			   || (!strcasecmp(flag, "Deleted"))) {
				if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom()) {
					bits_to_twiddle |= IMAP_DELETED;
				}
			}
			if ((!strcasecmp(flag, "\\Seen"))
			   || (!strcasecmp(flag, "Seen"))) {
				bits_to_twiddle |= IMAP_SEEN;
			}
			if ((!strcasecmp(flag, "\\Answered")) 
			   || (!strcasecmp(flag, "\\Answered"))) {
				bits_to_twiddle |= IMAP_ANSWERED;
			}
		}
	}

	if (IMAP->num_msgs > 0) {
		for (i = 0; i < IMAP->num_msgs; ++i) {
			if (IMAP->flags[i] & IMAP_SELECTED) {
				imap_do_store_msg(i, oper, bits_to_twiddle);
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

	if (num_parms < 3) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[2])) {
		imap_pick_range(parms[2], 0);
	}
	else {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
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

	imap_do_store(num_items, itemlist);
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

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[3])) {
		imap_pick_range(parms[3], 1);
	}
	else {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	strcpy(items, "");
	for (i=4; i<num_parms; ++i) {
		lprintf(9, "item %d: %s\n", i, parms[i]);
		strcat(items, parms[i]);
		if (i < (num_parms-1)) strcat(items, " ");
	}

	num_items = imap_extract_data_items(itemlist, items);
	if (num_items < 1) {
		cprintf("%s BAD invalid data item list\r\n", parms[0]);
		return;
	}

	imap_do_store(num_items, itemlist);
	cprintf("%s OK UID STORE completed\r\n", parms[0]);
}


