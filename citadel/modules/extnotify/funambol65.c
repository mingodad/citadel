/* 
* \file funambol65.c
* @author Mathew McBride
* 
* This module facilitates notifications to a Funambol server
* for push email
*
* Based on bits of the previous serv_funambol
* Contact: <matt@mcbridematt.dhs.org> / <matt@comalies>
*/
#include "extnotify.h"

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




size_t extnotify_callback(void *ptr, size_t size, size_t nmemb, void *stream)
{
	return size * nmemb; /* don't care, just discard it so it doesn't end up on the console... */
}
struct fh_data {
	char *buf;
	int total_bytes_received;
	int maxbytes;
};

/*
* \brief Sends a message to the Funambol server notifying 
* of new mail for a user
* Returns 0 if unsuccessful
*/
int notify_http_server(char *remoteurl, 
		       char* template, long tlen, 
		       char *user,
		       char *msgid, 
		       long MsgNum) 
{
	char *pchs, *pche;
	char userpass[SIZ];
	char retbuf[SIZ];
	char msgnumstr[128];
	char *buf = NULL;
	CURL *curl;
	CURLcode res;
	struct curl_slist * headers=NULL;
	char errmsg[1024] = "";
	char *SOAPMessage = NULL;
	char *contenttype;
        struct fh_data fh = {
                retbuf,
                0,
                SIZ
        };
		
	curl = curl_easy_init();
	if (!curl) {
		CtdlLogPrintf(CTDL_ALERT, "Unable to initialize libcurl.\n");
		return 1;
	}

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0);
	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, fh);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, extnotify_callback); /* don't care..*/
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errmsg);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

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
	if (!IsEmptyStr(config.c_ip_addr)) {
		curl_easy_setopt(curl, CURLOPT_INTERFACE, config.c_ip_addr);
	}

	headers = curl_slist_append(headers,"Accept: application/soap+xml, application/dime, multipart/related, text/*");
	headers = curl_slist_append(headers,"Pragma: no-cache");

	if (tlen > 0) {
		/* Load the template message. Get mallocs done too */
		FILE *Ftemplate = NULL;
		const char *mimetype;

		Ftemplate = fopen(template, "r");
		mimetype = GuessMimeByFilename(template, tlen);
		if (template == NULL) {
			char buf[SIZ];

			snprintf(buf, SIZ, 
				 "Cannot load template file %s [%s]won't send notification\r\n", 
				 file_funambol_msg, strerror(errno));
			CtdlLogPrintf(CTDL_ERR, buf);

			aide_message(buf, "External notifier unable to find message template!");
			goto free;
		}
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
			CtdlLogPrintf(CTDL_ERR, buf);

			aide_message(buf, "External notifier unable to load message template!");
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
		sprintf(contenttype,"Content-type: %s; charset=utf-8", mimetype);

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
		CtdlLogPrintf(CTDL_ALERT, "libcurl error %d: %s\n", res, errmsg);
	}

	CtdlLogPrintf(CTDL_DEBUG, "Funambol notified\n");
free:
	curl_slist_free_all (headers);
	curl_easy_cleanup(curl);
	if (contenttype) free(contenttype);
	if (SOAPMessage != NULL) free(SOAPMessage);
	if (buf != NULL) free(buf);
	return 0;
}


/*	
	sprintf(port, "%d", config.c_funambol_port);
	sock = sock_connect(config.c_funambol_host, port, "tcp");
	if (sock >= 0) 
		CtdlLogPrintf(CTDL_DEBUG, "Connected to Funambol!\n");
	else {
		char buf[SIZ];

		snprintf(buf, SIZ, 
			 "Unable to connect to %s:%d [%s]; won't send notification\r\n", 
			 config.c_funambol_host, 
			 config.c_funambol_port, 
			 strerror(errno));
		CtdlLogPrintf(CTDL_ERR, buf);

		aide_message(buf, "External notifier unable to connect remote host!");
		goto bail;
	}
*/
//	if (funambolCreds != NULL) free(funambolCreds);
	//if (SOAPHeader != NULL) free(SOAPHeader);
	///close(sock);

	/* Build the HTTP request header */

	
/*
	sprintf(SOAPHeader, "POST %s HTTP/1.0\r\nContent-type: text/xml; charset=utf-8\r\n",
		FUNAMBOL_WS);
	strcat(SOAPHeader,"Accept: application/soap+xml, application/dime, multipart/related, text/*\r\n");
	sprintf(buf, "User-Agent: %s/%d\r\nHost: %s:%d\r\nCache-control: no-cache\r\n",
		"Citadel",
		REV_LEVEL,
		config.c_funambol_host,
		config.c_funambol_port
		);
	strcat(SOAPHeader,buf);
	strcat(SOAPHeader,"Pragma: no-cache\r\nSOAPAction: \"\"\r\n");
	sprintf(buf, "Content-Length: %d \r\n",
		strlen(SOAPMessage));
	strcat(SOAPHeader, buf);
*/
	
/*	funambolCreds = malloc(strlen(config.c_funambol_auth)*2);
	memset(funambolCreds, 0, strlen(config.c_funambol_auth)*2);
	CtdlEncodeBase64(funambolCreds, config.c_funambol_auth, strlen(config.c_funambol_auth), 0);	
	sprintf(buf, "Authorization: Basic %s\r\n\r\n",
		funambolCreds);
	strcat(SOAPHeader, buf);
	
	sock_write(sock, SOAPHeader, strlen(SOAPHeader));
	sock_write(sock, SOAPMessage, strlen(SOAPMessage));
	sock_shutdown(sock, SHUT_WR);
	
	/ * Response * /
	CtdlLogPrintf(CTDL_DEBUG, "Awaiting response\n");
        if (sock_getln(sock, buf, SIZ) < 0) {
                goto free;
        }
        CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK"))) {
		
		goto free;
	}
*/
