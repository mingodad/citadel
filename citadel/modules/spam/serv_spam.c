/*
 * This module allows Citadel to use SpamAssassin to filter incoming messages
 * arriving via SMTP.  For more information on SpamAssassin, visit
 * http://www.spamassassin.org (the SpamAssassin project is not in any way
 * affiliated with the Citadel project).
 *
 * Copyright (c) 1998-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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


#include "ctdl_module.h"



/*
 * Connect to the SpamAssassin server and scan a message.
 */
int spam_assassin(struct CtdlMessage *msg, recptypes *recp) {
	int sock = (-1);
	char sahosts[SIZ];
	int num_sahosts;
	char buf[SIZ];
	int is_spam = 0;
	int sa;
	StrBuf *msgtext;
	CitContext *CCC=CC;

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
                syslog(LOG_INFO, "Connecting to SpamAssassin at <%s>\n", buf);
                sock = sock_connect(buf, SPAMASSASSIN_PORT);
                if (sock >= 0) syslog(LOG_DEBUG, "Connected!\n");
        }

	if (sock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
	}

	CCC->SBuf.Buf = NewStrBuf();
	CCC->sMigrateBuf = NewStrBuf();
	CCC->SBuf.ReadWritePointer = NULL;

	/* Command */
	syslog(LOG_DEBUG, "Transmitting command\n");
	sprintf(buf, "CHECK SPAMC/1.2\r\n\r\n");
	sock_write(&sock, buf, strlen(buf));

	/* Message */
	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1, 0);
	msgtext = CC->redirect_buffer;
	CC->redirect_buffer = NULL;

	sock_write(&sock, SKEY(msgtext));
	FreeStrBuf(&msgtext);

	/* Close one end of the socket connection; this tells SpamAssassin
	 * that we're done.
	 */
	if (sock != -1)
		sock_shutdown(sock, SHUT_WR);
	
	/* Response */
	syslog(LOG_DEBUG, "Awaiting response\n");
        if (sock_getln(&sock, buf, sizeof buf) < 0) {
                goto bail;
        }
        syslog(LOG_DEBUG, "<%s\n", buf);
	if (strncasecmp(buf, "SPAMD", 5)) {
		goto bail;
	}
        if (sock_getln(&sock, buf, sizeof buf) < 0) {
                goto bail;
        }
        syslog(LOG_DEBUG, "<%s\n", buf);
        syslog(LOG_DEBUG, "c_spam_flag_only setting %d\n", CtdlGetConfigInt("c_spam_flag_only"));
        if (CtdlGetConfigInt("c_spam_flag_only")) {
		int headerlen;
		char *cur;
		char sastatus[10];
		char sascore[10];
		char saoutof[10];
		int numscore;

                syslog(LOG_DEBUG, "flag spam code used");

                extract_token(sastatus, buf, 1, ' ', sizeof sastatus);
                extract_token(sascore, buf, 3, ' ', sizeof sascore);
                extract_token(saoutof, buf, 5, ' ', sizeof saoutof);

		memcpy(buf, HKEY("X-Spam-Level: "));
		cur = buf + 14;
		for (numscore = atoi(sascore); numscore>0; numscore--)
			*(cur++) = '*';
		*cur = '\0';

		headerlen  = cur - buf;
		headerlen += snprintf(cur, (sizeof(buf) - headerlen), 
				     "\r\nX-Spam-Status: %s, score=%s required=%s\r\n",
				     sastatus, sascore, saoutof);

		CM_PrependToField(msg, eMesageText, buf, headerlen);

	} else {
                syslog(LOG_DEBUG, "reject spam code used");
		if (!strncasecmp(buf, "Spam: True", 10)) {
			is_spam = 1;
		}

		if (is_spam) {
			CM_SetField(msg, eErrorMsg, HKEY("message rejected by spam filter"));
		}
	}

bail:	close(sock);
	FreeStrBuf(&CCC->SBuf.Buf);
	FreeStrBuf(&CCC->sMigrateBuf);
	return(is_spam);
}



CTDL_MODULE_INIT(spam)
{
	if (!threading)
	{
		CtdlRegisterMessageHook(spam_assassin, EVT_SMTPSCAN);
	}
	
	/* return our module name for the log */
        return "spam";
}
