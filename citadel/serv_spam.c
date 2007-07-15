/*
 * $Id$
 *
 * This module allows Citadel to use SpamAssassin to filter incoming messages
 * arriving via SMTP.  For more information on SpamAssassin, visit
 * http://www.spamassassin.org (the SpamAssassin project is not in any way
 * affiliated with the Citadel project).
 */

#define SPAMASSASSIN_PORT       "783"

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
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "domain.h"
#include "clientsocket.h"



/*
 * Connect to the SpamAssassin server and scan a message.
 */
int spam_assassin(struct CtdlMessage *msg) {
	int sock = (-1);
	char sahosts[SIZ];
	int num_sahosts;
	char buf[SIZ];
	int is_spam = 0;
	int sa;
	char *msgtext;
	size_t msglen;

	/* For users who have authenticated to this server we never want to
	 * apply spam filtering, because presumably they're trustworthy.
	 */
	if (CC->logged_in) return(0);

	/* See if we have any SpamAssassin hosts configured */
	num_sahosts = get_hosts(sahosts, "spamassassin");
	if (num_sahosts < 1) return(0);

	/* Try them one by one until we get a working one */
        for (sa=0; sa<num_sahosts; ++sa) {
                extract_token(buf, sahosts, sa, '|', sizeof buf);
                lprintf(CTDL_INFO, "Connecting to SpamAssassin at <%s>\n", buf);
                sock = sock_connect(buf, SPAMASSASSIN_PORT, "tcp");
                if (sock >= 0) lprintf(CTDL_DEBUG, "Connected!\n");
        }

	if (sock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
	}

	/* Command */
	lprintf(CTDL_DEBUG, "Transmitting command\n");
	sprintf(buf, "CHECK SPAMC/1.2\r\n\r\n");
	sock_write(sock, buf, strlen(buf));

	/* Message */
	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1);
	msgtext = CC->redirect_buffer;
	msglen = CC->redirect_len;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	sock_write(sock, msgtext, msglen);
	free(msgtext);

	/* Close one end of the socket connection; this tells SpamAssassin
	 * that we're done.
	 */
	sock_shutdown(sock, SHUT_WR);
	
	/* Response */
	lprintf(CTDL_DEBUG, "Awaiting response\n");
        if (sock_gets(sock, buf) < 0) {
                goto bail;
        }
        lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "SPAMD", 5)) {
		goto bail;
	}
        if (sock_gets(sock, buf) < 0) {
                goto bail;
        }
        lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (!strncasecmp(buf, "Spam: True", 10)) {
		is_spam = 1;
	}

	if (is_spam) {
		if (msg->cm_fields['0'] != NULL) {
			free(msg->cm_fields['0']);
		}
		msg->cm_fields['0'] = strdup("5.7.1 message rejected by spam filter");
	}

bail:	close(sock);
	return(is_spam);
}



char *serv_spam_init(void)
{
	CtdlRegisterMessageHook(spam_assassin, EVT_SMTPSCAN);

	/* return our Subversion id for the Log */
        return "$Id$";
}
