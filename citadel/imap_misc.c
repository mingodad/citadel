/*
 * $Id$
 *
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
#include "imap_misc.h"
#include "genstamp.h"






/*
 * imap_copy() calls imap_do_copy() to do its actual work, once it's
 * validated and boiled down the request a bit.  (returns 0 on success)
 */
int imap_do_copy(char *destination_folder) {
	int i;
	char roomname[ROOMNAMELEN];

	i = imap_grabroom(roomname, destination_folder);
	if (i != 0) return(i);

	if (IMAP->num_msgs > 0) {
		for (i = 0; i < IMAP->num_msgs; ++i) {
			if (IMAP->flags[i] && IMAP_SELECTED) {
				CtdlCopyMsgToRoom(
					IMAP->msgids[i], roomname);
			}
		}
	}

	return(0);
}


/*
 * This function is called by the main command loop.
 */
void imap_copy(int num_parms, char *parms[]) {
	int ret;

	if (num_parms != 4) {
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

	ret = imap_do_copy(parms[3]);
	if (!ret) {
		cprintf("%s OK COPY completed\r\n", parms[0]);
	}
	else {
		cprintf("%s NO COPY failed (error %d)\r\n", parms[0], ret);
	}
}

/*
 * This function is called by the main command loop.
 */
void imap_uidcopy(int num_parms, char *parms[]) {

	if (num_parms != 5) {
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

	if (imap_do_copy(parms[4]) == 0) {
		cprintf("%s OK UID COPY completed\r\n", parms[0]);
	}
	else {
		cprintf("%s NO UID COPY failed\r\n", parms[0]);
	}
}


