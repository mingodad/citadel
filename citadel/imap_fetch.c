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



void imap_do_fetch(int lo, int hi, char *items) {
}




/*
 * This function is called by the main command loop.
 */
void imap_fetch(int num_parms, char *parms[]) {
	int lo = 0;
	int hi = 0;
	char lostr[1024], histr[1024], items[1024];
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

	imap_do_fetch(lo, hi, items);
	cprintf("%s OK FETCH completed\r\n", parms[0]);
}



