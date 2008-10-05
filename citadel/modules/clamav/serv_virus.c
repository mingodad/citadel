/*
 * $Id$
 *
 * This module allows Citadel to use clamd to filter incoming messages
 * arriving via SMTP.  For more information on clamd, visit
 * http://clamav.net (the ClamAV project is not in any way
 * affiliated with the Citadel project).
 */

#define CLAMD_PORT       "3310"

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


#include "ctdl_module.h"



/*
 * Connect to the clamd server and scan a message.
 */
int clamd(struct CtdlMessage *msg) {
	int sock = (-1);
	int streamsock = (-1);
	char clamhosts[SIZ];
	int num_clamhosts;
	char buf[SIZ];
        char hostbuf[SIZ];
        char portbuf[SIZ];
	int is_virus = 0;
	int clamhost;
	char *msgtext;
	size_t msglen;

	/* Don't care if you're logged in.  You can still spread viruses.
	 */
	/* if (CC->logged_in) return(0); */

	/* See if we have any clamd hosts configured */
	num_clamhosts = get_hosts(clamhosts, "clamav");
	if (num_clamhosts < 1) return(0);

	/* Try them one by one until we get a working one */
        for (clamhost=0; clamhost<num_clamhosts; ++clamhost) {
                extract_token(buf, clamhosts, clamhost, '|', sizeof buf);
                CtdlLogPrintf(CTDL_INFO, "Connecting to clamd at <%s>\n", buf);

                /* Assuming a host:port entry */ 
                extract_token(hostbuf, buf, 0, ':', sizeof hostbuf);
                if (extract_token(portbuf, buf, 1, ':', sizeof portbuf)==-1)
                  /* Didn't specify a port so we'll try the psuedo-standard 3310 */
                  sock = sock_connect(hostbuf, CLAMD_PORT, "tcp");
                else
                  /* Port specified lets try connecting to it! */
                  sock = sock_connect(hostbuf, portbuf, "tcp");

                if (sock >= 0) CtdlLogPrintf(CTDL_DEBUG, "Connected!\n");
        }

	if (sock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
	}

	/* Command */
	CtdlLogPrintf(CTDL_DEBUG, "Transmitting STREAM command\n");
	sprintf(buf, "STREAM\r\n");
	sock_write(sock, buf, strlen(buf));

	CtdlLogPrintf(CTDL_DEBUG, "Waiting for PORT number\n");
        if (sock_getln(sock, buf, sizeof buf) < 0) {
                goto bail;
        }

        CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "PORT", 4)!=0) {
	        goto bail;
	}

        /* Should have received a port number to connect to */
	extract_token(portbuf, buf, 1, ' ', sizeof portbuf);

	/* Attempt to establish connection to STREAM socket */
        streamsock = sock_connect(hostbuf, portbuf, "tcp");

	if (streamsock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
        }
	else {
	        CtdlLogPrintf(CTDL_DEBUG, "STREAM socket connected!\n");
	}



	/* Message */
	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1, 0);
	msgtext = CC->redirect_buffer;
	msglen = CC->redirect_len;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	sock_write(streamsock, msgtext, msglen);
	free(msgtext);

	/* Close the streamsocket connection; this tells clamd
	 * that we're done.
	 */
	close(streamsock);
	
	/* Response */
	CtdlLogPrintf(CTDL_DEBUG, "Awaiting response\n");
        if (sock_getln(sock, buf, sizeof buf) < 0) {
                goto bail;
        }
        CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "stream: OK", 10)!=0) {
		is_virus = 1;
	}

	if (is_virus) {
		if (msg->cm_fields['0'] != NULL) {
			free(msg->cm_fields['0']);
		}
		msg->cm_fields['0'] = strdup("message rejected by virus filter");
	}

bail:	close(sock);
	return(is_virus);
}



CTDL_MODULE_INIT(virus)
{
	if (!threading)
	{
		CtdlRegisterMessageHook(clamd, EVT_SMTPSCAN);
	}
	
	/* return our Subversion id for the Log */
        return "$Id$";
}
