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


/*
 * Poll for express messages (yeah, we can do this in IMAP ... I think)
 */
void imap_print_express_messages(void) {
	struct ExpressMessage *ptr, *holdptr;
	char *dumpomatic = NULL;
	int i;

	if (CC->FirstExpressMessage == NULL) {
		return;
	}
	begin_critical_section(S_SESSION_TABLE);
	ptr = CC->FirstExpressMessage;
	CC->FirstExpressMessage = NULL;
	end_critical_section(S_SESSION_TABLE);

	while (ptr != NULL) {
		dumpomatic = mallok(strlen(ptr->text) + SIZ);
		strcpy(dumpomatic, "");
		if (ptr->flags && EM_BROADCAST)
			strcat(dumpomatic, "Broadcast message ");
		else if (ptr->flags && EM_CHAT)
			strcat(dumpomatic, "Chat request ");
		else if (ptr->flags && EM_GO_AWAY)
			strcat(dumpomatic, "Please logoff now, as requested ");
		else
			strcat(dumpomatic, "Message ");
		sprintf(&dumpomatic[strlen(dumpomatic)],
			"from %s:\n", ptr->sender);
		if (ptr->text != NULL)
			strcat(dumpomatic, ptr->text);

		holdptr = ptr->next;
		if (ptr->text != NULL) phree(ptr->text);
		phree(ptr);
		ptr = holdptr;

		for (i=0; i<strlen(dumpomatic); ++i) {
			if (!isprint(dumpomatic[i])) dumpomatic[i] = ' ';
			if (dumpomatic[i]=='\\') dumpomatic[i]='/';
			if (dumpomatic[i]=='\"') dumpomatic[i]='\'';
		}

		cprintf("* OK [ALERT] %s\r\n", dumpomatic);
		phree(dumpomatic);
	}
	cprintf("000\n");
}



/*
 * This function is called by the main command loop.
 */
void imap_append(int num_parms, char *parms[]) {
	size_t literal_length;
	int ret;

	if (num_parms < 4) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	if ( (parms[num_parms-1][0] != '{')
	   || (parms[num_parms-1][strlen(parms[num_parms-1])-1] != '}') )  {
		cprintf("%s BAD no message literal supplied\r\n", parms[0]);
		return;
	}

	literal_length = (size_t) atol(&parms[num_parms-1][1]);
	if (literal_length < 1) {
		cprintf("%s BAD Message length must be at least 1.\r\n",
			parms[0]);
		return;
	}

	imap_free_transmitted_message();	/* just in case. */
	IMAP->transmitted_message = mallok(literal_length);
	if (IMAP->transmitted_message == NULL) {
		cprintf("%s NO Cannot allocate memory.\r\n", parms[0]);
		return;
	}
	IMAP->transmitted_length = literal_length;

	cprintf("+ Transmit message now.\r\n");
	ret = client_read(IMAP->transmitted_message, literal_length);
	if (ret != 1) {
		cprintf("%s NO Read failed.\r\n", parms[0]);
		return;
	}

	cprintf("%s NO not implemented yet ** FIXME ** \r\n", parms[0]);
}
