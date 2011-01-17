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
 * This program is free software; you can redistribute it and/or modify
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "citadel_dirs.h"
#include "clientsocket.h"
#include "sysdep.h"
#include "config.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "ctdl_module.h"

#include "extnotify.h"

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
	char curl_errbuf[CURL_ERROR_SIZE];
	char *pchs, *pche;
	char userpass[SIZ];
	char msgnumstr[128];
	char *buf = NULL;
	CURL *curl;
	CURLcode res;
	struct curl_slist * headers=NULL;
	char errmsg[1024] = "";
	char *SOAPMessage = NULL;
	char *contenttype = NULL;
	StrBuf *ReplyBuf;

	curl = curl_easy_init();
	if (!curl) {
		syslog(LOG_ALERT, "Unable to initialize libcurl.\n");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	ReplyBuf = NewStrBuf();
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, ReplyBuf);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlFillStrBuf_callback);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, curl_errbuf);

	pchs = strchr(remoteurl, ':');
	pche = strchr(remoteurl, '@');
	if ((pche != NULL) && 
	    (pchs != NULL) && 
	    (pchs < pche) && ((pche - pchs) < SIZ)) {
		memcpy(userpass, pchs + 3, pche - pchs - 3);
		
		userpass[pche - pchs - 3] = '\0';
		curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
		curl_easy_setopt(curl, CURLOPT_USERPWD, userpass);

	}
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	curl_easy_setopt(curl, CURLOPT_HTTP_CONTENT_DECODING, 1);
	curl_easy_setopt(curl, CURLOPT_ENCODING, "");
#endif
	curl_easy_setopt(curl, CURLOPT_USERAGENT, CITADEL);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 180);		/* die after 180 seconds */
	if (
		(!IsEmptyStr(config.c_ip_addr))
		&& (strcmp(config.c_ip_addr, "*"))
		&& (strcmp(config.c_ip_addr, "::"))
		&& (strcmp(config.c_ip_addr, "0.0.0.0"))
	) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	headers = curl_slist_append(headers,"Accept: application/soap+xml, application/mime, multipart/related, text/*");
	headers = curl_slist_append(headers,"Pragma: no-cache");

	if (tlen > 0) {
		/* Load the template message. Get mallocs done too */
		FILE *Ftemplate = NULL;
		const char *mimetype;

		Ftemplate = fopen(template, "r");
		if (Ftemplate == NULL) {
			char buf[SIZ];

			snprintf(buf, SIZ, 
				 "Cannot load template file %s [%s]won't send notification\r\n", 
				 file_funambol_msg, strerror(errno));
			syslog(LOG_ERR, buf);

			CtdlAideMessage(buf, "External notifier unable to find message template!");
			goto free;
		}
		mimetype = GuessMimeByFilename(template, tlen);

		snprintf(msgnumstr, 128, "%ld", MsgNum);

		buf = malloc(SIZ);
		memset(buf, 0, SIZ);
		SOAPMessage = malloc(3072);
		memset(SOAPMessage, 0, 3072);
	
		while(fgets(buf, SIZ, Ftemplate) != NULL) {
			strcat(SOAPMessage, buf);
		}
		fclose(Ftemplate);
	
		if (strlen(SOAPMessage) < 0) {
			char buf[SIZ];

			snprintf(buf, SIZ, 
				 "Cannot load template file %s; won't send notification\r\n", 
				 file_funambol_msg);
			syslog(LOG_ERR, buf);

			CtdlAideMessage(buf, "External notifier unable to load message template!");
			goto free;
		}
		// Do substitutions
		help_subst(SOAPMessage, "^notifyuser", user);
		help_subst(SOAPMessage, "^syncsource", config.c_funambol_source);
		help_subst(SOAPMessage, "^msgid", msgid);
		help_subst(SOAPMessage, "^msgnum", msgnumstr);

		curl_easy_setopt(curl, CURLOPT_URL, remoteurl);

		/* pass our list of custom made headers */

		contenttype=(char*) malloc(40+strlen(mimetype));
		sprintf(contenttype,"Content-Type: %s; charset=utf-8", mimetype);

		headers = curl_slist_append(headers, "SOAPAction: \"\"");
		headers = curl_slist_append(headers, contenttype);

		/* Now specify the POST binary data */

		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, SOAPMessage);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, strlen(SOAPMessage));
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}
	else {
		help_subst(remoteurl, "^notifyuser", user);
		help_subst(remoteurl, "^syncsource", config.c_funambol_source);
		help_subst(remoteurl, "^msgid", msgid);
		help_subst(remoteurl, "^msgnum", msgnumstr);
		curl_easy_setopt(curl, CURLOPT_URL, remoteurl);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	}

	res = curl_easy_perform(curl);
	if (res) {
		StrBuf *ErrMsg;

		syslog(LOG_ALERT, "libcurl error %d: %s\n", res, errmsg);
		ErrMsg = NewStrBufPlain(HKEY("Error sending your Notification\n"));
		StrBufAppendPrintf(ErrMsg, "\nlibcurl error %d: %s\n", res, errmsg);
		StrBufAppendBufPlain(ErrMsg, curl_errbuf, -1, 0);
		StrBufAppendBufPlain(ErrMsg, HKEY("\nWas Trying to send: \n"), 0);
		StrBufAppendBufPlain(ErrMsg, remoteurl, -1, 0);
		if (tlen > 0) {
			StrBufAppendBufPlain(ErrMsg, HKEY("\nThe Post document was: \n"), 0);
			StrBufAppendBufPlain(ErrMsg, SOAPMessage, -1, 0);
			StrBufAppendBufPlain(ErrMsg, HKEY("\n\n"), 0);			
		}
		if (StrLength(ReplyBuf) > 0) {			
			StrBufAppendBufPlain(ErrMsg, HKEY("\n\nThe Serverreply was: \n\n"), 0);
			StrBufAppendBuf(ErrMsg, ReplyBuf, 0);
		}
		else 
			StrBufAppendBufPlain(ErrMsg, HKEY("\n\nThere was no Serverreply.\n\n"), 0);
		ExtNotify_PutErrorMessage(Ctx, ErrMsg);
	}

	syslog(LOG_DEBUG, "Funambol notified\n");
free:
	curl_slist_free_all (headers);
	curl_easy_cleanup(curl);
	if (contenttype) free(contenttype);
	if (SOAPMessage != NULL) free(SOAPMessage);
	if (buf != NULL) free(buf);
	FreeStrBuf (&ReplyBuf);
	return 0;
}
