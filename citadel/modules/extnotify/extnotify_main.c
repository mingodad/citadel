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

StrBuf* GetNHBuf(int i, int allocit, StrBuf **NotifyHostList)
{
	if ((NotifyHostList[i] == NULL) && (allocit != 0))
		NotifyHostList[i] = NewStrBuf();
	return NotifyHostList[i];
}


StrBuf** GetNotifyHosts(void)
{
	char NotifyHostsBuf[SIZ];
	StrBuf *Host;
	StrBuf *File;
	StrBuf *NotifyBuf;
	int notify;
	const char *pchs, *pche;
	const char *NextHost = NULL;
	StrBuf **NotifyHostList;
	int num_notify;

	/* See if we have any Notification Hosts configured */
	num_notify = get_hosts(NotifyHostsBuf, "notify");
	if (num_notify < 1)
		return(NULL);

	NotifyHostList = malloc(sizeof(StrBuf*) * 2 * (num_notify + 1));
	memset(NotifyHostList, 0, sizeof(StrBuf*) * 2 * (num_notify + 1));
	
	NotifyBuf = NewStrBufPlain(NotifyHostsBuf, -1);
	/* get all configured notifiers's */
        for (notify=0; notify<num_notify; notify++) {
		
		Host = GetNHBuf(notify * 2, 1, NotifyHostList);
		StrBufExtract_NextToken(Host, NotifyBuf, &NextHost, '|');
		pchs = ChrPtr(Host);
		pche = strchr(pchs, ':');
		if (pche == NULL) {
			CtdlLogPrintf(CTDL_ERR, 
				      __FILE__": filename not found in %s.\n", 
				      pchs);
			continue;
		}
		File = GetNHBuf(notify * 2 + 1, 1, NotifyHostList);
		StrBufPlain(File, pchs, pche - pchs);
		StrBufCutLeft(Host, pche - pchs + 1);
	}
	return NotifyHostList;
}


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
	StrBuf **NotifyHosts;
	int i = 0;
    
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
	CtdlLogPrintf(CTDL_DEBUG, "serv_extnotify: processing notify queue\n");
    
	NotifyHosts = GetNotifyHosts();
	if (getroom(&CC->room, FNBL_QUEUE_ROOM) != 0) {
		CtdlLogPrintf(CTDL_ERR, "Cannot find room <%s>\n", FNBL_QUEUE_ROOM);
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L, NULL,
			   SPOOLMIME, NULL, process_notify, NotifyHosts);

    while ((NotifyHosts != NULL) && (NotifyHosts[i] != NULL))
	    FreeStrBuf(&NotifyHosts[i]);

    CtdlLogPrintf(CTDL_DEBUG, "serv_extnotify: queue run completed\n");
    doing_queue = 0;
}
/*!
 * \brief Process messages in the external notification queue
 */
void process_notify(long msgnum, void *usrdata) 
{
	long todelete[1];
	int fnblAllowed;
	int extPagerAllowedHttp;
	int extPagerAllowedSystem;
	char *pch;
	long configMsgNum;
	char configMsg[SIZ];
	StrBuf **NotifyHostList;	
	struct CtdlMessage *msg;


	NotifyHostList = (StrBuf**) usrdata;
	msg = CtdlFetchMessage(msgnum, 1);
	if ( msg->cm_fields['W'] == NULL) {
		goto nuke;
	}
    
	configMsgNum = extNotify_getConfigMessage(msg->cm_fields['W']);
    
	extNotify_getPrefs(configMsgNum, &configMsg[0]);
	
	/* Check to see if:
	 * 1. The user has configured paging / They have and disabled it
	 * AND 2. There is an external pager program
	 * 3. A Funambol server has been entered
	 *
	 */
	if ((configMsgNum == -1) || 
	    ((strncasecmp(configMsg, "none", 4) == 0) &&
	     IsEmptyStr(config.c_pager_program) && 
	     IsEmptyStr(config.c_funambol_host))) {
		CtdlLogPrintf(CTDL_DEBUG, "No external notifiers configured on system/user");
		goto nuke;
	}

	// Can Funambol take the message?
	pch = strchr(configMsg, '\n');
	if (*pch == '\n')
	    *pch = '\0';
	fnblAllowed = strncasecmp(configMsg, HKEY(FUNAMBOL_CONFIG_TEXT));
	extPagerAllowedHttp = strncasecmp(configMsg, HKEY(PAGER_CONFIG_HTTP)); 
	extPagerAllowedSystem = strncasecmp(configMsg, HKEY(PAGER_CONFIG_SYSTEM));

	if (fnblAllowed == 0) {
		char remoteurl[SIZ];
		snprintf(remoteurl, SIZ, "http://%s@%s:%d/%s",
			 config.c_funambol_auth,
			 config.c_funambol_host,
			 config.c_funambol_port,
			 FUNAMBOL_WS);
		notify_http_server(remoteurl, 
				   file_funambol_msg,
				   strlen(file_funambol_msg),/*GNA*/
				   msg->cm_fields['W'], 
				   msg->cm_fields['I'],
				   msgnum);
	} else if (extPagerAllowedHttp == 0) {
		int i = 0;
		StrBuf *URL;
		char URLBuf[SIZ];
		StrBuf *File;
		StrBuf *FileBuf = NewStrBuf();
		
		while(1)
		{

			URL = GetNHBuf(i*2, 0, NotifyHostList);
			if (URL==NULL) break;
			File = GetNHBuf(i*2 + 1, 0, NotifyHostList);
			if (File==NULL) break;

			if (StrLength(File)>0)
				StrBufPrintf(FileBuf, "%s/%s", 
					     ctdl_shared_dir, 
					     ChrPtr(File));
			else
				FlushStrBuf(FileBuf);
			memcpy(URLBuf, ChrPtr(URL), StrLength(URL) + 1);

			notify_http_server(URLBuf, 
					   ChrPtr(FileBuf),
					   StrLength(FileBuf),
					   msg->cm_fields['W'], 
					   msg->cm_fields['I'],
					   msgnum);
			i++;
		}
		FreeStrBuf(&FileBuf);
	} 
	else if (extPagerAllowedSystem == 0) {
		char *number;
		int commandSiz;
		char *command;

		number = strtok(configMsg, "textmessage\n");
		commandSiz = sizeof(config.c_pager_program) + strlen(number) + strlen(msg->cm_fields['W']) + 5;
		command = malloc(commandSiz);
		snprintf(command, commandSiz, "%s %s -u %s", config.c_pager_program, number, msg->cm_fields['W']);
		system(command);
		free(command);
	}
nuke:
	CtdlFreeMessage(msg);
	memset(configMsg, 0, sizeof(configMsg));
	todelete[0] = msgnum;
	CtdlDeleteMessages(FNBL_QUEUE_ROOM, todelete, 1, "");
}

/*! \brief Checks to see what notification option the user has set
 *
 */
void extNotify_getPrefs(long configMsgNum, char *configMsg) 
{
	struct CtdlMessage *prefMsg;
	// Do a simple string search to see if 'funambol' is selected as the
	// type. This string would be at the very top of the message contents.
	if (configMsgNum == -1) {
		CtdlLogPrintf(CTDL_ERR, "extNotify_isAllowedByPrefs was passed a non-existant config message id\n");
		return;
	}
	prefMsg = CtdlFetchMessage(configMsgNum, 1);
	strncpy(configMsg, prefMsg->cm_fields['M'], strlen(prefMsg->cm_fields['M']));
	CtdlFreeMessage(prefMsg);
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
	int a;

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
		CtdlLogPrintf(CTDL_DEBUG, "extNotify_getConfigMessage: No config messages found\n");
		return -1;	/* No messages at all?  No further action. */
	}
	for (a = 0; a < num_msgs; ++a) {
		msg = CtdlFetchMessage(msglist[a], 1);
		if (msg != NULL) {
			if ((msg->cm_fields['U'] != NULL) && 
			    (strncasecmp(msg->cm_fields['U'], PAGER_CONFIG_MESSAGE,
					 strlen(PAGER_CONFIG_MESSAGE)) == 0)) {
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
        return "$Id$";
}
