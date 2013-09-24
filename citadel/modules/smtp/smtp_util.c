/*
 * This module is an SMTP and ESMTP implementation for the Citadel system.
 * It is compliant with all of the following:
 *
 * RFC  821 - Simple Mail Transfer Protocol
 * RFC  876 - Survey of SMTP Implementations
 * RFC 1047 - Duplicate messages and SMTP
 * RFC 1652 - 8 bit MIME
 * RFC 1869 - Extended Simple Mail Transfer Protocol
 * RFC 1870 - SMTP Service Extension for Message Size Declaration
 * RFC 2033 - Local Mail Transfer Protocol
 * RFC 2197 - SMTP Service Extension for Command Pipelining
 * RFC 2476 - Message Submission
 * RFC 2487 - SMTP Service Extension for Secure SMTP over TLS
 * RFC 2554 - SMTP Service Extension for Authentication
 * RFC 2821 - Simple Mail Transfer Protocol
 * RFC 2822 - Internet Message Format
 * RFC 2920 - SMTP Service Extension for Command Pipelining
 *  
 * The VRFY and EXPN commands have been removed from this implementation
 * because nobody uses these commands anymore, except for spammers.
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
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
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"

#include "ctdl_module.h"

#include "smtp_util.h"

const char *smtp_get_Recipients(void)
{
	citsmtp *sSMTP = SMTP;

	if (sSMTP == NULL)
		return NULL;
	else return ChrPtr(sSMTP->from);
}


/*
 * smtp_do_bounce() is caled by smtp_do_procmsg() to scan a set of delivery
 * instructions for "5" codes (permanent fatal errors) and produce/deliver
 * a "bounce" message (delivery status notification).
 */
void smtp_do_bounce(char *instr, StrBuf *OMsgTxt)
{
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	char bounceto[1024];
	StrBuf *boundary;
	int num_bounces = 0;
	int bounce_this = 0;
	time_t submitted = 0L;
	struct CtdlMessage *bmsg = NULL;
	int give_up = 0;
	recptypes *valid;
	int successful_bounce = 0;
	static int seq = 0;
	StrBuf *BounceMB;
	long omsgid = (-1);

	syslog(LOG_DEBUG, "smtp_do_bounce() called\n");
	strcpy(bounceto, "");
	boundary = NewStrBufPlain(HKEY("=_Citadel_Multipart_"));

	StrBufAppendPrintf(boundary,
			   "%s_%04x%04x",
			   config.c_fqdn,
			   getpid(),
			   ++seq);

	lines = num_tokens(instr, '\n');

	/* See if it's time to give up on delivery of this message */
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		if (!strcasecmp(key, "submitted")) {
			submitted = atol(addr);
		}
	}

	if ( (time(NULL) - submitted) > SMTP_GIVE_UP ) {
		give_up = 1;
	}

	/* Start building our bounce message */

	bmsg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (bmsg == NULL) return;
	memset(bmsg, 0, sizeof(struct CtdlMessage));
	BounceMB = NewStrBufPlain(NULL, 1024);

	bmsg->cm_magic = CTDLMESSAGE_MAGIC;
	bmsg->cm_anon_type = MES_NORMAL;
	bmsg->cm_format_type = FMT_RFC822;
	CM_SetField(bmsg, eAuthor, HKEY("Citadel"));
	CM_SetField(bmsg, eOriginalRoom, HKEY(MAILROOM));
	CM_SetField(bmsg, eNodeName, config.c_nodename, strlen(config.c_nodename));
	CM_SetField(bmsg, eMsgSubject, HKEY("Delivery Status Notification (Failure)"));
	StrBufAppendBufPlain(
		BounceMB,
		HKEY("Content-type: multipart/mixed; boundary=\""), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\"\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("MIME-Version: 1.0\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("X-Mailer: " CITADEL "\r\n"), 0);

	StrBufAppendBufPlain(
		BounceMB,
		HKEY("\r\nThis is a multipart message in MIME format."
		     "\r\n\r\n"), 0);

	StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	StrBufAppendBufPlain(BounceMB,
			     HKEY("Content-type: text/plain\r\n\r\n"), 0);

	if (give_up)
	{
		StrBufAppendBufPlain(
			BounceMB,
			HKEY("A message you sent could not be delivered "
			     "to some or all of its recipients\ndue to "
			     "prolonged unavailability of its destination(s).\n"
			     "Giving up on the following addresses:\n\n"), 0);
	}
	else
	{
		StrBufAppendBufPlain(
			BounceMB,
			HKEY("A message you sent could not be delivered "
			     "to some or all of its recipients.\n"
			     "The following addresses were undeliverable:\n\n"
				), 0);
	}

	/*
	 * Now go through the instructions checking for stuff.
	 */
	for (i=0; i<lines; ++i) {
		long addrlen;
		long dsnlen;
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		addrlen = extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		dsnlen = extract_token(dsn, buf, 3, '|', sizeof dsn);
		bounce_this = 0;

		syslog(LOG_DEBUG, "key=<%s> addr=<%s> status=%d dsn=<%s>\n",
		       key, addr, status, dsn);

		if (!strcasecmp(key, "bounceto")) {
			strcpy(bounceto, addr);
		}

		if (!strcasecmp(key, "msgid")) {
			omsgid = atol(addr);
		}

		if (!strcasecmp(key, "remote")) {
			if (status == 5) bounce_this = 1;
			if (give_up) bounce_this = 1;
		}

		if (bounce_this) {
			++num_bounces;

			StrBufAppendBufPlain(BounceMB, addr, addrlen, 0);
			StrBufAppendBufPlain(BounceMB, HKEY(": "), 0);
			StrBufAppendBufPlain(BounceMB, dsn, dsnlen, 0);
			StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);

			remove_token(instr, i, '\n');
			--i;
			--lines;
		}
	}

	/* Attach the original message */
	if (omsgid >= 0) {
		StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
		StrBufAppendBuf(BounceMB, boundary, 0);
		StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);

		StrBufAppendBufPlain(
			BounceMB,
			HKEY("Content-type: message/rfc822\r\n"), 0);

		StrBufAppendBufPlain(
			BounceMB,
			HKEY("Content-Transfer-Encoding: 7bit\r\n"), 0);

		StrBufAppendBufPlain(
			BounceMB,
			HKEY("Content-Disposition: inline\r\n"), 0);

		StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);

		if (OMsgTxt == NULL) {
			CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
			CtdlOutputMsg(omsgid,
				      MT_RFC822,
				      HEADERS_ALL,
				      0, 1, NULL, 0,
				      NULL, NULL);

			StrBufAppendBuf(BounceMB, CC->redirect_buffer, 0);
			FreeStrBuf(&CC->redirect_buffer);
		}
		else {
			StrBufAppendBuf(BounceMB, OMsgTxt, 0);
		}
	}

	/* Close the multipart MIME scope */
	StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("--\r\n"), 0);
	CM_SetAsFieldSB(bmsg, eMesageText, &BounceMB);

	/* Deliver the bounce if there's anything worth mentioning */
	syslog(LOG_DEBUG, "num_bounces = %d\n", num_bounces);
	if (num_bounces > 0) {

		/* First try the user who sent the message */
		if (IsEmptyStr(bounceto))
			syslog(LOG_ERR, "No bounce address specified\n");
		else
			syslog(LOG_DEBUG, "bounce to user <%s>\n", bounceto);
		/* Can we deliver the bounce to the original sender? */
		valid = validate_recipients(bounceto,
					    smtp_get_Recipients (),
					    0);
		if (valid != NULL) {
			if (valid->num_error == 0) {
				CtdlSubmitMsg(bmsg, valid, "", QP_EADDR);
				successful_bounce = 1;
			}
		}

		/* If not, post it in the Aide> room */
		if (successful_bounce == 0) {
			CtdlSubmitMsg(bmsg, NULL, config.c_aideroom, QP_EADDR);
		}

		/* Free up the memory we used */
		if (valid != NULL) {
			free_recipients(valid);
		}
	}
	FreeStrBuf(&boundary);
	CM_Free(bmsg);
	syslog(LOG_DEBUG, "Done processing bounces\n");
}
