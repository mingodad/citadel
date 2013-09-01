/*
 * funambol65.c
 * Author: Mathew McBride
 *
 * This module facilitates notifications to a Funambol server
 * for push email
 *
 * Based on bits of the previous serv_funambol
 * Contact: <matt@mcbridematt.dhs.org> / <matt@comalies>
 *
 * Copyright (c) 2008-2010
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <time.h>
#include <libcitadel.h>
#include <errno.h>
#include <unistd.h>
#include <curl/curl.h>

#include "citadel.h"
#include "citserver.h"
#include "citadel_dirs.h"
#include "clientsocket.h"
#include "sysdep.h"
#include "config.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "ctdl_module.h"

#include "event_client.h"
#include "extnotify.h"

eNextState EvaluateResult(AsyncIO *IO);
eNextState ExtNotifyTerminate(AsyncIO *IO);
eNextState ExtNotifyTerminateDB(AsyncIO *IO);
eNextState ExtNotifyShutdownAbort(AsyncIO *IO);

/*
* \brief Sends a message to the Funambol server notifying
* of new mail for a user
* Returns 0 if unsuccessful
*/
int notify_http_server(char *remoteurl,
		       const char* template, long tlen,
		       char *user,
		       char *msgid,
		       long MsgNum,
		       NotifyContext *Ctx)
{
	CURLcode sta;
	char msgnumstr[128];
	char *buf = NULL;
	char *SOAPMessage = NULL;
	char *contenttype = NULL;
	StrBuf *ReplyBuf;
	StrBuf *Buf;
	CURL *chnd;
	AsyncIO *IO;

	IO = (AsyncIO*) malloc(sizeof(AsyncIO));
	memset(IO, 0, sizeof(AsyncIO));

	if (! InitcURLIOStruct(IO,
			       NULL, /* we don't have personal data anymore. */
			       "Citadel ExtNotify",
			       EvaluateResult,
			       ExtNotifyTerminate,
			       ExtNotifyTerminateDB,
			       ExtNotifyShutdownAbort))
	{
		syslog(LOG_ALERT, "Unable to initialize libcurl.\n");
		goto abort;
	}

	snprintf(msgnumstr, 128, "%ld", MsgNum);

	if (tlen > 0) {
		/* Load the template message. Get mallocs done too */
		int fd;
		struct stat statbuf;
		const char *mimetype;
		const char *Err = NULL;

		fd = open(template, O_RDONLY);
		if ((fd < 0) ||
		    (fstat(fd, &statbuf) == -1))
		{
			char buf[SIZ];

			snprintf(buf, SIZ,
				 "Cannot load template file %s [%s] "
				 "won't send notification\r\n",
				 file_funambol_msg,
				 strerror(errno));
			syslog(LOG_ERR, "%s", buf);
			// TODO: once an hour!
			CtdlAideMessage(
				buf,
				"External notifier: "
				"unable to find/stat message template!");
			goto abort;
		}

		Buf = NewStrBufPlain(NULL, statbuf.st_size + 1);
		if (StrBufReadBLOB(Buf, &fd, 1, statbuf.st_size, &Err) < 0) {
			char buf[SIZ];

			close(fd);

			snprintf(buf, SIZ,
				 "Cannot load template file %s [%s] "
				 "won't send notification\r\n",
				 file_funambol_msg,
				 Err);
			syslog(LOG_ERR, "%s", buf);
			// TODO: once an hour!
			CtdlAideMessage(
				buf,
				"External notifier: "
				"unable to load message template!");
			goto abort;
		}
		close(fd);

		mimetype = GuessMimeByFilename(template, tlen);

		SOAPMessage = SmashStrBuf(&Buf);

		// Do substitutions
		help_subst(SOAPMessage, "^notifyuser", user);
		help_subst(SOAPMessage, "^syncsource",
			   config.c_funambol_source);
		help_subst(SOAPMessage, "^msgid", msgid);
		help_subst(SOAPMessage, "^msgnum", msgnumstr);

		/* pass our list of custom made headers */

		contenttype=(char*) malloc(40+strlen(mimetype));
		sprintf(contenttype,
			"Content-Type: %s; charset=utf-8",
			mimetype);

		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			"SOAPAction: \"\"");

		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			contenttype);
		free(contenttype);
		contenttype = NULL;
		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			"Accept: application/soap+xml, "
			"application/mime, multipart/related, text/*");

		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			"Pragma: no-cache");

		/* Now specify the POST binary data */
		IO->HttpReq.PlainPostData = SOAPMessage;
		IO->HttpReq.PlainPostDataLen = strlen(SOAPMessage);
	}
	else {
		help_subst(remoteurl, "^notifyuser", user);
		help_subst(remoteurl, "^syncsource", config.c_funambol_source);
		help_subst(remoteurl, "^msgid", msgid);
		help_subst(remoteurl, "^msgnum", msgnumstr);

		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			"Accept: application/soap+xml, "
			"application/mime, multipart/related, text/*");

		IO->HttpReq.headers = curl_slist_append(
			IO->HttpReq.headers,
			"Pragma: no-cache");
	}

	Buf = NewStrBufPlain (remoteurl, -1);
	ParseURL(&IO->ConnectMe, Buf, 80);
	FreeStrBuf(&Buf); /* TODO: this is uncool... */
	CurlPrepareURL(IO->ConnectMe);

	chnd = IO->HttpReq.chnd;
	OPT(SSL_VERIFYPEER, 0);
	OPT(SSL_VERIFYHOST, 0);

	QueueCurlContext(IO);

	return 0;
abort:

	if (contenttype) free(contenttype);
	if (SOAPMessage != NULL) free(SOAPMessage);
	if (buf != NULL) free(buf);
	FreeStrBuf (&ReplyBuf);
	return 1;
}


eNextState EvaluateResult(AsyncIO *IO)
{

	if (IO->HttpReq.httpcode != 200) {
		StrBuf *ErrMsg;

		syslog(LOG_ALERT, "libcurl error %ld: %s\n",
			      IO->HttpReq.httpcode,
			      IO->HttpReq.errdesc);

		ErrMsg = NewStrBufPlain(
			HKEY("Error sending your Notification\n"));
		StrBufAppendPrintf(ErrMsg, "\nlibcurl error %ld: \n\t\t%s\n",
				   IO->HttpReq.httpcode,
				   IO->HttpReq.errdesc);

		StrBufAppendBufPlain(ErrMsg,
				     HKEY("\nWas Trying to send: \n"),
				     0);

		StrBufAppendBufPlain(ErrMsg, IO->ConnectMe->PlainUrl, -1, 0);
		if (IO->HttpReq.PlainPostDataLen > 0) {
			StrBufAppendBufPlain(
				ErrMsg,
				HKEY("\nThe Post document was: \n"),
				0);
			StrBufAppendBufPlain(ErrMsg,
					     IO->HttpReq.PlainPostData,
					     IO->HttpReq.PlainPostDataLen, 0);
			StrBufAppendBufPlain(ErrMsg, HKEY("\n\n"), 0);
		}
		if (StrLength(IO->HttpReq.ReplyData) > 0) {
			StrBufAppendBufPlain(
				ErrMsg,
				HKEY("\n\nThe Serverreply was: \n\n"),
				0);
			StrBufAppendBuf(ErrMsg, IO->HttpReq.ReplyData, 0);
		}
		else
			StrBufAppendBufPlain(
				ErrMsg,
				HKEY("\n\nThere was no Serverreply.\n\n"),
				0);
		///ExtNotify_PutErrorMessage(Ctx, ErrMsg);
		CtdlAideMessage(ChrPtr(ErrMsg),
				"External notifier: "
				"unable to contact notification host!");
	}

	syslog(LOG_DEBUG, "Funambol notified\n");
/*
	while ((Ctx.NotifyHostList != NULL) && (Ctx.NotifyHostList[i] != NULL))
		FreeStrBuf(&Ctx.NotifyHostList[i]);

	if (Ctx.NotifyErrors != NULL)
	{
		long len;
		const char *Key;
		HashPos *It;
		void *vErr;
		StrBuf *ErrMsg;

		It = GetNewHashPos(Ctx.NotifyErrors, 0);
		while (GetNextHashPos(Ctx.NotifyErrors,
		It, &len, &Key, &vErr) &&
		       (vErr != NULL)) {
			ErrMsg = (StrBuf*) vErr;
			quickie_message("Citadel", NULL, NULL,
			AIDEROOM, ChrPtr(ErrMsg), FMT_FIXED,
			"Failed to notify external service about inbound mail");
		}

		DeleteHashPos(&It);
		DeleteHash(&Ctx.NotifyErrors);
	}
*/

////	curl_slist_free_all (headers);
///	curl_easy_cleanup(curl);
	///if (contenttype) free(contenttype);
	///if (SOAPMessage != NULL) free(SOAPMessage);
	///if (buf != NULL) free(buf);
	///FreeStrBuf (&ReplyBuf);
	return 0;
}

eNextState ExtNotifyTerminateDB(AsyncIO *IO)
{
	free(IO);
	return eAbort;
}
eNextState ExtNotifyTerminate(AsyncIO *IO)
{
	free(IO);
	return eAbort;
}
eNextState ExtNotifyShutdownAbort(AsyncIO *IO)
{
	free(IO);
	return eAbort;
}
