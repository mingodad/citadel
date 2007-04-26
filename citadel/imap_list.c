/*
 * $Id$ 
 *
 * Implements the LIST and LSUB commands.
 *
 * Copyright (C) 2000-2007 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
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
#include "imap_search.h"
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_misc.h"

#ifdef HAVE_OPENSSL
#include "serv_crypto.h"
#endif

/*
 * Used by LIST and LSUB to show the floors in the listing
 */
void imap_list_floors(char *verb, char *pattern)
{
	int i;
	struct floor *fl;

	for (i = 0; i < MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			if (imap_mailbox_matches_pattern
			    (pattern, fl->f_name)) {
				cprintf("* %s (\\NoSelect) \"/\" ", verb);
				imap_strout(fl->f_name);
				cprintf("\r\n");
			}
		}
	}
}


/*
 * Back end for imap_list()
 *
 * Implementation note: IMAP "subscribed folder" is equivalent to Citadel "known room"
 *
 * The "user data" field is actually an array of pointers; see below for the breakdown
 *
 */
void imap_list_listroom(struct ctdlroom *qrbuf, void *data)
{
	char buf[SIZ];
	int ra;
	int yes_output_this_room;

	char **data_for_callback;
	char *pattern;
	char *verb;
	int subscribed_rooms_only;

	/* Here's how we break down the array of pointers passed to us */
	data_for_callback = data;
	pattern = data_for_callback[0];
	verb = data_for_callback[1];
	subscribed_rooms_only = (int) data_for_callback[2];

	/* Only list rooms to which the user has access!! */
	yes_output_this_room = 0;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, NULL);

	if (subscribed_rooms_only) {
		if (ra & UA_KNOWN) {
			yes_output_this_room = 1;
		}
	}
	else {
		if ((ra & UA_KNOWN) || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
			yes_output_this_room = 1;
		}
	}

	if (yes_output_this_room) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		if (imap_mailbox_matches_pattern(pattern, buf)) {
			cprintf("* %s () \"/\" ", verb);
			imap_strout(buf);
			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LIST and LSUB commands
 */
void imap_list(int num_parms, char *parms[])
{
	char pattern[SIZ];
	int subscribed_rooms_only = 0;
	char verb[16];
	int i, j;

	char *data_for_callback[3];

	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}

	/* parms[1] is the IMAP verb being used (e.g. LIST or LSUB)
	 * This tells us how to behave, and what verb to return back to the caller
	 */
	safestrncpy(verb, parms[1], sizeof verb);
	j = strlen(verb);
	for (i=0; i<j; ++i) {
		verb[i] = toupper(verb[i]);
	}

	if (!strcasecmp(verb, "LSUB")) {
		subscribed_rooms_only = 1;
	}

	snprintf(pattern, sizeof pattern, "%s%s", parms[2], parms[3]);

	data_for_callback[0] = pattern;
	data_for_callback[1] = verb;
	data_for_callback[2] = (char *) subscribed_rooms_only;

	if (strlen(parms[3]) == 0) {
		cprintf("* %s (\\Noselect) \"/\" \"\"\r\n", verb);
	}

	else {
		imap_list_floors(verb, pattern);
		ForEachRoom(imap_list_listroom, data_for_callback);
	}

	cprintf("%s OK %s completed\r\n", parms[0], verb);
}


