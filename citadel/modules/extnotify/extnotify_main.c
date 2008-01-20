/*
 * \file extnotify_main.c
 * @author Mathew McBride
 *
 * This module implements an external pager hook for when notifcation
 * of a new email is wanted.
 * Based on bits of serv_funambol
 * Contact: <matt@mcbridematt.dhs.org> / <matt@comalies>
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
#include <libcitadel.h>
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
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"
#include "extnotify.h"

#include "ctdl_module.h"

/*! \brief Create the notify message queue. We use the exact same room
 *			as the Funambol module.
 *
 *	Run at server startup, creates FNBL_QUEUE_ROOM if it doesn't exist
 *	and sets as system room.
 */
void create_extnotify_queue(void) {
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
/*!
 * \brief Run through the pager room queue
 */
void do_extnotify_queue(void) {
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
    lprintf(CTDL_DEBUG, "serv_extnotify: processing notify queue\n");
    
    if (getroom(&CC->room, FNBL_QUEUE_ROOM) != 0) {
        lprintf(CTDL_ERR, "Cannot find room <%s>\n", FNBL_QUEUE_ROOM);
        return;
    }
    CtdlForEachMessage(MSGS_ALL, 0L, NULL,
            SPOOLMIME, NULL, process_notify, NULL);
    
    lprintf(CTDL_DEBUG, "serv_extnotify: queue run completed\n");
    doing_queue = 0;
}
/*!
 * \brief Process messages in the external notification queue
 */
void process_notify(long msgnum, void *usrdata) {
    struct CtdlMessage *msg;
    msg = CtdlFetchMessage(msgnum, 1);
    if ( msg->cm_fields['W'] == NULL) {
        goto nuke;
    }
    
    long configMsgNum = extNotify_getConfigMessage(msg->cm_fields['W']);
    char configMsg[SIZ];
    
    extNotify_getPrefs(configMsgNum, &configMsg);
    
    /* Check to see if:
     * 1. The user has configured paging / They have and disabled it
     * AND 2. There is an external pager program
     * 3. A Funambol server has been entered
     *
     */
    if (configMsgNum == -1 || strncasecmp(configMsg, "none", 4) == 0 &&
    IsEmptyStr(config.c_pager_program) && IsEmptyStr(config.c_funambol_host)) {
        lprintf(CTDL_DEBUG, "No external notifiers configured on system/user");
        goto nuke;
    }
    // Can Funambol take the message?
    int fnblAllowed = strncasecmp(configMsg, FUNAMBOL_CONFIG_TEXT, strlen(FUNAMBOL_CONFIG_TEXT));
    int extPagerAllowed = strncasecmp(configMsg, PAGER_CONFIG_TEXT, strlen(PAGER_CONFIG_TEXT)); 
    if (fnblAllowed == 0) {
        notify_funambol_server(msg->cm_fields['W']);
    } else if (extPagerAllowed == 0) {
	char *number = strtok(configMsg, "textmessage\n");
	int commandSiz = sizeof(config.c_pager_program) + strlen(number) + strlen(msg->cm_fields['W']) + 5;
	char *command = malloc(commandSiz);
	snprintf(command, commandSiz, "%s %s -u %s", config.c_pager_program, number, msg->cm_fields['W']);
	system(command);
	free(command);
    }
    nuke:
    CtdlFreeMessage(msg);
    memset(configMsg, 0, sizeof(configMsg));
    long todelete[1];
    todelete[0] = msgnum;
    CtdlDeleteMessages(FNBL_QUEUE_ROOM, todelete, 1, "");
}

/*! \brief Checks to see what notification option the user has set
 *
 */
char *extNotify_getPrefs(long configMsgNum, char *configMsg) {
    // Do a simple string search to see if 'funambol' is selected as the
    // type. This string would be at the very top of the message contents.
    if (configMsgNum == -1) {
        lprintf(CTDL_ERR, "extNotify_isAllowedByPrefs was passed a non-existant config message id\n");
        return "none";
    }
    struct CtdlMessage *prefMsg;
    prefMsg = CtdlFetchMessage(configMsgNum, 1);
    strncpy(configMsg, prefMsg->cm_fields['M'], strlen(prefMsg->cm_fields['M']));
    CtdlFreeMessage(prefMsg);
    return configMsg;
}
/*! \brief Get configuration message for pager/funambol system from the
 *			users "My Citadel Config" room
 */
long extNotify_getConfigMessage(char *username) {
    struct ctdlroom qrbuf; // scratch for room
    struct ctdluser user; // ctdl user instance
    char configRoomName[ROOMNAMELEN];
    struct CtdlMessage *msg;
    struct cdbdata *cdbfr;
    long *msglist = NULL;
    int num_msgs = 0;
    long confMsgNum = -1;
    // Get the user
    getuser(&user, username);
    
    MailboxName(configRoomName, sizeof configRoomName, &user, USERCONFIGROOM);
    // Fill qrbuf
    getroom(&qrbuf, configRoomName);
    /* Do something really, really stoopid here. Raid the room on ourselves,
     * loop through the messages manually and find it. I don't want
     * to use a CtdlForEachMessage callback here, as we would be
     * already in one */
    cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));
    if (cdbfr != NULL) {
        msglist = (long *) cdbfr->ptr;
        cdbfr->ptr = NULL;	/* CtdlForEachMessage() now owns this memory */
        num_msgs = cdbfr->len / sizeof(long);
        cdb_free(cdbfr);
    } else {
        lprintf(CTDL_DEBUG, "extNotify_getConfigMessage: No config messages found\n");
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
CTDL_MODULE_INIT(extnotify)
{
	if (!threading)
	{
		create_extnotify_queue();
		CtdlRegisterSessionHook(do_extnotify_queue, EVT_TIMER);
	}
	/* return our Subversion id for the Log */
        return "$Id:  $";
}