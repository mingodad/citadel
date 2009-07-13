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

#include "citadel.h"
#include "citadel_dirs.h"
#include "clientsocket.h"
#include "sysdep.h"
#include "config.h"
#include "sysdep_decls.h"
#include "msgbase.h"
#include "ctdl_module.h"

/*
* \brief Sends a message to the Funambol server notifying 
* of new mail for a user
* Returns 0 if unsuccessful
*/
int notify_funambol_server(char *user) {
	char port[1024];
	int sock = -1;
	char *buf = NULL;
	char *SOAPMessage = NULL;
	char *SOAPHeader = NULL;
	char *funambolCreds = NULL;
	FILE *template = NULL;
	
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
	// Load the template SOAP message. Get mallocs done too
	template = fopen(file_funambol_msg, "r");

	if (template == NULL) {
		char buf[SIZ];

		snprintf(buf, SIZ, 
			 "Cannot load template file %s [%s]won't send notification\r\n", 
			 file_funambol_msg, strerror(errno));
		CtdlLogPrintf(CTDL_ERR, buf);

		aide_message(buf, "External notifier unable to find message template!");
		goto free;
	}


	buf = malloc(SIZ);
	memset(buf, 0, SIZ);
	SOAPMessage = malloc(3072);
	memset(SOAPMessage, 0, 3072);
	
	SOAPHeader  = malloc(SIZ);
	memset(SOAPHeader, 0, SIZ);
	
	funambolCreds = malloc(strlen(config.c_funambol_auth)*2);
	memset(funambolCreds, 0, strlen(config.c_funambol_auth)*2);
	
	while(fgets(buf, SIZ, template) != NULL) {
		strcat(SOAPMessage, buf);
	}
	fclose(template);
	
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
	
	/* Build the HTTP request header */

	
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
	
	

	CtdlEncodeBase64(funambolCreds, config.c_funambol_auth, strlen(config.c_funambol_auth), 0);
	
	
	sprintf(buf, "Authorization: Basic %s\r\n\r\n",
		funambolCreds);
	strcat(SOAPHeader, buf);
	
	sock_write(sock, SOAPHeader, strlen(SOAPHeader));
	sock_write(sock, SOAPMessage, strlen(SOAPMessage));
	sock_shutdown(sock, SHUT_WR);
	
	/* Response */
	CtdlLogPrintf(CTDL_DEBUG, "Awaiting response\n");
        if (sock_getln(sock, buf, SIZ) < 0) {
                goto free;
        }
        CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "HTTP/1.1 200 OK", strlen("HTTP/1.1 200 OK"))) {
		
		goto free;
	}
	CtdlLogPrintf(CTDL_DEBUG, "Funambol notified\n");
free:
	if (funambolCreds != NULL) free(funambolCreds);
	if (SOAPMessage != NULL) free(SOAPMessage);
	if (buf != NULL) free(buf);
	if (SOAPHeader != NULL) free(SOAPHeader);
bail:
	close(sock);
	return 0;
}

