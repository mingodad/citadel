/*
 * This module implements an external pager hook for when notifcation
 * of a new email is wanted.
 * Based on bits of serv_funambol
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
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"
#include "serv_pager.h"

#include "ctdl_module.h"

#define PAGER_CONFIG_MESSAGE "__ Push email settings __"
#define PAGER_CONFIG_TEXT  "textmessage"

/*
 * Create the notify message queue. We use the exact same room 
 */
void create_pager_queue(void) {
	struct ctdlroom qrbuf;

	create_room(FNBL_QUEUE_ROOM, 3, "", 0, 1, 0, VIEW_MAILBOX);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (lgetroom(&qrbuf, FNBL_QUEUE_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		lputroom(&qrbuf);
	}
}
void do_pager_queue(void) {
	static int doing_queue = 0;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;

	/* 
	 * Go ahead and run the queue
	 */
	lprintf(CTDL_DEBUG, "serv_pager: processing notify queue\n");

	if (getroom(&CC->room, FNBL_QUEUE_ROOM) != 0) {
		lprintf(CTDL_ERR, "Cannot find room <%s>\n", FNBL_QUEUE_ROOM);
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L, NULL,
		SPOOLMIME, NULL, notify_pager, NULL);

	lprintf(CTDL_DEBUG, "serv_pager: queue run completed\n");
	doing_queue = 0;
}

/*
 * Call the external tool
 */
void notify_pager(long msgnum, void *userdata) {
	struct CtdlMessage *msg;
	struct ctdlroom qrbuf;
	
	/* W means 'wireless', which contains the unix name */
	msg = CtdlFetchMessage(msgnum, 1);
	if ( msg->cm_fields['W'] == NULL) {
		goto nuke;
	}
	/* Are we allowed to push? */
	if (IsEmptyStr(config.c_pager_program)) {
		return;
	} else if (IsEmptyStr(config.c_pager_program) && IsEmptyStr(config.c_funambol_host)) {
		goto nuke;
	} else {
		lprintf(CTDL_INFO, "Pager alerter enabled\n");	
	}

	/* Get the configuration. We might be allowed system wide but the user
		may have configured otherwise */
	long configMsgNum = pager_getConfigMessage(msg->cm_fields['W']);
	int allowed = pager_isPagerAllowedByPrefs(configMsgNum);
	if (allowed != 0 && pager_doesUserWant(configMsgNum) == 0) {
		goto nuke;
	} else if (allowed != 0) {
		return;
	}
	char *num = pager_getUserPhoneNumber(configMsgNum);
	char command[SIZ];
	snprintf(command, sizeof command, "%s %s -u %s", config.c_pager_program, num, &msg->cm_fields['W']);
	system(command);
	
	nuke:
	CtdlFreeMessage(msg);
	long todelete[1];
	todelete[0] = msgnum;
	CtdlDeleteMessages(FNBL_QUEUE_ROOM, todelete, 1, "");
}

long pager_getConfigMessage(char *username) {
	struct ctdlroom qrbuf; // scratch for room
	struct ctdluser user; // ctdl user instance
	char configRoomName[ROOMNAMELEN];
	struct CtdlMessage *template;
	struct CtdlMessage *msg;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	long confMsgNum = -1;
	// Get the user
	getuser(&user, username);
	
	MailboxName(configRoomName, sizeof configRoomName, &user, USERCONFIGROOM);
	int prefroom = getroom(&qrbuf, configRoomName);
	
	/* Do something really, really stoopid here. Raid the room on ourselves,
		loop through the messages manually and find it. I don't want 
		to use a CtdlForEachMessage callback here, as we would be 
		already in one */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;	/* CtdlForEachMessage() now owns this memory */
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	} else {
		return -1;	/* No messages at all?  No further action. */
	}
	int a;
	for (a = 0; a < num_msgs; ++a) {
				msg = CtdlFetchMessage(msglist[a], 1);
				if (msg != NULL) {
					if (msg->cm_fields['U'] != NULL && strncasecmp(msg->cm_fields['U'], PAGER_CONFIG_MESSAGE, 
						strlen(PAGER_CONFIG_MESSAGE)) == 0) {
						confMsgNum = msglist[a];
					}
					CtdlFreeMessage(msg);
				}
	}
	return confMsgNum;

}
int pager_isPagerAllowedByPrefs(long configMsgNum) {
	// Do a simple string search to see if 'textmessage' is selected as the 
	// type. This string would be at the very top of the message contents.
	if (configMsgNum == -1) {
		return -1;
	}
	struct CtdlMessage *prefMsg;
	prefMsg = CtdlFetchMessage(configMsgNum, 1);
	char *msgContents = prefMsg->cm_fields['M'];
	return strncasecmp(msgContents, PAGER_CONFIG_TEXT, strlen(PAGER_CONFIG_TEXT));
}
int pager_doesUserWant(long configMsgNum) {
	if (configMsgNum == -1) {
		return -1;
	}
	struct CtdlMessage *prefMsg;
	prefMsg = CtdlFetchMessage(configMsgNum, 1);
	char *msgContents = prefMsg->cm_fields['M'];
	return strncasecmp(msgContents, "none", 4);
}
	/* warning: fetching twice gravely inefficient, will fix some time */
char *pager_getUserPhoneNumber(long configMsgNum) {
	if (configMsgNum == -1) {
		return;
	}
	struct CtdlMessage *prefMsg;
	prefMsg = CtdlFetchMessage(configMsgNum, 1);
	char *msgContents = prefMsg->cm_fields['M'];
	char *lines = strtok(msgContents, "textmessage\n");
	return lines;
}
CTDL_MODULE_INIT(pager)
{
	create_pager_queue();
	CtdlRegisterSessionHook(do_pager_queue, EVT_TIMER);

	/* return our Subversion id for the Log */
        return "$Id: serv_pager.c $";
}
