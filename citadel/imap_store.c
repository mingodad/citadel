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
void imap_do_store_msg(int seq, char *oper, unsigned int bits_to_twiddle) {
	
	if (!strncasecmp(oper, "FLAGS", 5)) {
		IMAP->flags[seq] &= IMAP_MASK_SYSTEM;
		IMAP->flags[seq] |= bits_to_twiddle;
	}
	else if (!strncasecmp(oper, "+FLAGS", 6)) {
		IMAP->flags[seq] |= bits_to_twiddle;
	}
	else if (!strncasecmp(oper, "-FLAGS", 6)) {
		IMAP->flags[seq] &= (~bits_to_twiddle);
	}

	cprintf("* %d FETCH (", seq+1);
	imap_fetch_flags(seq);
	cprintf(")\r\n");
}



/*
 * imap_store() calls imap_do_store() to perform the actual bit twiddling
 * on flags.  We brazenly ignore the ".silent" protocol option because it's not
 * harmful to send the data anyway.  Fix it yourself if you don't like that.
 */
void imap_do_store(int num_items, char **itemlist) {
	int i;
	unsigned int bits_to_twiddle = 0;
	char *oper;
	char flag[SIZ];

	if (num_items < 2) return;
	oper = itemlist[0];

	for (i=1; i<num_items; ++i) {
		strcpy(flag, itemlist[i]);
		if (flag[0]=='(') strcpy(flag, &flag[1]);
		if (flag[strlen(flag)-1]==')') flag[strlen(flag)-1]=0;
		striplt(flag);

		if (!strcasecmp(flag, "\\Deleted")) {
		  if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom()) {
			bits_to_twiddle |= IMAP_DELETED;
		  }
		}
	}
	
	if (IMAP->num_msgs > 0) {
		for (i = 0; i < IMAP->num_msgs; ++i) {
			if (IMAP->flags[i] && IMAP_SELECTED) {
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


