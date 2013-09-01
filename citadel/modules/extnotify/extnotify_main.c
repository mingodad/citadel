/*
 * extnotify_main.c
 * Mathew McBride
 *
 * This module implements an external pager hook for when notifcation
 * of a new email is wanted.
 *
 * Based on bits of serv_funambol
 * Contact: <matt@mcbridematt.dhs.org> / <matt@comalies>
 *
 * Copyright (c) 2008-2011
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
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
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"
#include "event_client.h"
#include "extnotify.h"
#include "ctdl_module.h"

struct CitContext extnotify_queue_CC;

void ExtNotify_PutErrorMessage(NotifyContext *Ctx, StrBuf *ErrMsg)
{
	int nNext;
	if (Ctx->NotifyErrors == NULL)
		Ctx->NotifyErrors = NewHash(1, Flathash);

	nNext = GetCount(Ctx->NotifyErrors) + 1;
	Put(Ctx->NotifyErrors,
	    (char*)&nNext,
	    sizeof(int),
	    ErrMsg,
	    HFreeStrBuf);
}

StrBuf* GetNHBuf(int i, int allocit, StrBuf **NotifyHostList)
{
	if ((NotifyHostList[i] == NULL) && (allocit != 0))
		NotifyHostList[i] = NewStrBuf();
	return NotifyHostList[i];
}


int GetNotifyHosts(NotifyContext *Ctx)
{
	char NotifyHostsBuf[SIZ];
	StrBuf *Host;
	StrBuf *File;
	StrBuf *NotifyBuf;
	int notify;
	const char *pchs, *pche;
	const char *NextHost = NULL;

	/* See if we have any Notification Hosts configured */
	Ctx->nNotifyHosts = get_hosts(NotifyHostsBuf, "notify");
	if (Ctx->nNotifyHosts < 1)
		return 0;

	Ctx->NotifyHostList = malloc(sizeof(StrBuf*) *
				     2 *
				     (Ctx->nNotifyHosts + 1));
	memset(Ctx->NotifyHostList, 0,
	       sizeof(StrBuf*) * 2 * (Ctx->nNotifyHosts + 1));

	NotifyBuf = NewStrBufPlain(NotifyHostsBuf, -1);
	/* get all configured notifiers's */
	for (notify=0; notify<Ctx->nNotifyHosts; notify++) {

		Host = GetNHBuf(notify * 2, 1, Ctx->NotifyHostList);
		StrBufExtract_NextToken(Host, NotifyBuf, &NextHost, '|');
		pchs = ChrPtr(Host);
		pche = strchr(pchs, ':');
		if (pche == NULL) {
			syslog(LOG_ERR,
			       "extnotify: filename of notification "
			       "template not found in %s.\n",
			       pchs);
			continue;
		}
		File = GetNHBuf(notify * 2 + 1, 1, Ctx->NotifyHostList);
		StrBufPlain(File, pchs, pche - pchs);
		StrBufCutLeft(Host, pche - pchs + 1);
	}
	FreeStrBuf(&NotifyBuf);
	return Ctx->nNotifyHosts;
}



/*! \brief Get configuration message for pager/funambol system from the
 *			users "My Citadel Config" room
 */
eNotifyType extNotify_getConfigMessage(char *username,
				       char **PagerNumber,
				       char **FreeMe)
{
	struct ctdlroom qrbuf; // scratch for room
	struct ctdluser user; // ctdl user instance
	char configRoomName[ROOMNAMELEN];
	struct CtdlMessage *msg = NULL;
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	int a;
	char *configMsg;
	long clen;
	char *pch;

	// Get the user
	CtdlGetUser(&user, username);

	CtdlMailboxName(configRoomName,
			sizeof(configRoomName),
			&user,
			USERCONFIGROOM);
	// Fill qrbuf
	CtdlGetRoom(&qrbuf, configRoomName);
	/* Do something really, really stoopid here. Raid the room on ourselves,
	 * loop through the messages manually and find it. I don't want
	 * to use a CtdlForEachMessage callback here, as we would be
	 * already in one */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;
			/* CtdlForEachMessage() now owns this memory */
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	} else {
		syslog(LOG_DEBUG,
		       "extNotify_getConfigMessage: "
		       "No config messages found\n");
		return eNone;	/* No messages at all?  No further action. */
	}
	for (a = 0; a < num_msgs; ++a) {
		msg = CtdlFetchMessage(msglist[a], 1);
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eMsgSubject) &&
			    (strncasecmp(msg->cm_fields[eMsgSubject],
					 PAGER_CONFIG_MESSAGE,
					 strlen(PAGER_CONFIG_MESSAGE)) == 0))
			{
				break;
			}
			CM_Free(msg);
			msg = NULL;
		}
	}

	free(msglist);
	if (msg == NULL)
		return eNone;

	// Do a simple string search to see if 'funambol' is selected as the
	// type. This string would be at the very top of the message contents.

	CM_GetAsField(msg, eMesageText, &configMsg, &clen);
	CM_Free(msg);

	/* here we would find the pager number... */
	pch = strchr(configMsg, '\n');
	if (pch != NULL)
	{
		*pch = '\0';
		pch ++;
	}

	/* Check to see if:
	 * 1. The user has configured paging / They have and disabled it
	 * AND 2. There is an external pager program
	 * 3. A Funambol server has been entered
	 *
	 */
	if (!strncasecmp(configMsg, "none", 4))
	{
		free(configMsg);
		return eNone;
	}

	if (!strncasecmp(configMsg, HKEY(PAGER_CONFIG_HTTP)))
	{
		free(configMsg);
		return eHttpMessages;
	}
	if (!strncasecmp(configMsg, HKEY(FUNAMBOL_CONFIG_TEXT)))
	{
		free(configMsg);
		return eFunambol;
	}
	else if (!strncasecmp(configMsg, HKEY(PAGER_CONFIG_SYSTEM)))
	{
		// whats the pager number?
		if (!pch || (*pch == '\0'))
		{
			free(configMsg);

			return eNone;
		}
		while (isspace(*pch))
			pch ++;
		*PagerNumber = pch;
		while (isdigit(*pch) || (*pch == '+'))
			pch++;
		*pch = '\0';
		*FreeMe = configMsg;
		return eTextMessage;
	}

	free(configMsg);
	return eNone;
}


/*
 * Process messages in the external notification queue
 */
void process_notify(long NotifyMsgnum, void *usrdata)
{
	NotifyContext *Ctx;
	long msgnum = 0;
	long todelete[1];
	char *pch;
	struct CtdlMessage *msg;
	eNotifyType Type;
	char remoteurl[SIZ];
	char *FreeMe = NULL;
	char *PagerNo;

	Ctx = (NotifyContext*) usrdata;

	msg = CtdlFetchMessage(NotifyMsgnum, 1);
	if (!CM_IsEmpty(msg, eExtnotify))
	{
		Type = extNotify_getConfigMessage(
			msg->cm_fields[eExtnotify],
			&PagerNo,
			&FreeMe);

		pch = strstr(msg->cm_fields[eMesageText], "msgid|");
		if (pch != NULL)
			msgnum = atol(pch + sizeof("msgid"));

		switch (Type)
		{
		case eFunambol:
			snprintf(remoteurl, SIZ, "http://%s@%s:%d/%s",
				 config.c_funambol_auth,
				 config.c_funambol_host,
				 config.c_funambol_port,
				 FUNAMBOL_WS);

			notify_http_server(remoteurl,
					   file_funambol_msg,
					   strlen(file_funambol_msg),/*GNA*/
					   msg->cm_fields[eExtnotify],
					   msg->cm_fields[emessageId],
					   msgnum,
					   NULL);
			break;
		case eHttpMessages:
		{
			int i = 0;
			StrBuf *URL;
			char URLBuf[SIZ];
			StrBuf *File;
			StrBuf *FileBuf = NewStrBuf();

			for (i = 0; i < Ctx->nNotifyHosts; i++)
			{

				URL = GetNHBuf(i*2, 0, Ctx->NotifyHostList);
				if (URL==NULL) break;
				File = GetNHBuf(i*2 + 1, 0,
						Ctx->NotifyHostList);
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
						   msg->cm_fields[eExtnotify],
						   msg->cm_fields[emessageId],
						   msgnum,
						   NULL);
			}
			FreeStrBuf(&FileBuf);
		}
		break;
		case eTextMessage:
		{
			int commandSiz;
			char *command;

			commandSiz = sizeof(config.c_pager_program) +
				strlen(PagerNo) +
				strlen(msg->cm_fields[eExtnotify]) + 5;

			command = malloc(commandSiz);

			snprintf(command,
				 commandSiz,
				 "%s %s -u %s",
				 config.c_pager_program,
				 PagerNo,
				 msg->cm_fields[eExtnotify]);

			system(command);
			free(command);
		}
		break;
		case eNone:
			break;
		}
	}
	if (FreeMe != NULL)
		free(FreeMe);
	CM_Free(msg);
	todelete[0] = NotifyMsgnum;
	CtdlDeleteMessages(FNBL_QUEUE_ROOM, todelete, 1, "");
}

/*!
 * \brief Run through the pager room queue
 * Checks to see what notification option the user has set
 */
void do_extnotify_queue(void)
{
	NotifyContext Ctx;
	static int doing_queue = 0;
	int i = 0;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (IsEmptyStr(config.c_pager_program) &&
	    IsEmptyStr(config.c_funambol_host))
	{
		syslog(LOG_ERR,
		       "No external notifiers configured on system/user\n");
		return;
	}

	if (doing_queue)
		return;

	doing_queue = 1;

	become_session(&extnotify_queue_CC);

	pthread_setspecific(MyConKey, (void *)&extnotify_queue_CC);

	/*
	 * Go ahead and run the queue
	 */
	syslog(LOG_DEBUG, "serv_extnotify: processing notify queue\n");

	memset(&Ctx, 0, sizeof(NotifyContext));
	if ((GetNotifyHosts(&Ctx) > 0) &&
	    (CtdlGetRoom(&CC->room, FNBL_QUEUE_ROOM) != 0))
	{
		syslog(LOG_ERR, "Cannot find room <%s>\n", FNBL_QUEUE_ROOM);
		if (Ctx.nNotifyHosts > 0)
		{
			for (i = 0; i < Ctx.nNotifyHosts * 2; i++)
				FreeStrBuf(&Ctx.NotifyHostList[i]);
			free(Ctx.NotifyHostList);
		}
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L, NULL,
			   SPOOLMIME, NULL, process_notify, &Ctx);
	syslog(LOG_DEBUG, "serv_extnotify: queue run completed\n");
	doing_queue = 0;
	if (Ctx.nNotifyHosts > 0)
	{
		for (i = 0; i < Ctx.nNotifyHosts * 2; i++)
			FreeStrBuf(&Ctx.NotifyHostList[i]);
		free(Ctx.NotifyHostList);
	}
}



/* Create the notify message queue. We use the exact same room
 * as the Funambol module.
 *
 * Run at server startup, creates FNBL_QUEUE_ROOM if it doesn't exist
 * and sets as system room.
 */
void create_extnotify_queue(void) {
	struct ctdlroom qrbuf;

	CtdlCreateRoom(FNBL_QUEUE_ROOM, 3, "", 0, 1, 0, VIEW_QUEUE);

	CtdlFillSystemContext(&extnotify_queue_CC, "Extnotify");

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (CtdlGetRoomLock(&qrbuf, FNBL_QUEUE_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		CtdlPutRoomLock(&qrbuf);
	}
}


CTDL_MODULE_INIT(extnotify)
{
	if (!threading)
	{
		create_extnotify_queue();
		CtdlRegisterSessionHook(do_extnotify_queue, EVT_TIMER, PRIO_SEND + 10);
	}
	/* return our module name for the log */
	return "extnotify";
}
