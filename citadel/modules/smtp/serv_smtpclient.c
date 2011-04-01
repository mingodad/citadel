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
 * Copyright (c) 1998-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#ifndef EXPERIMENTAL_SMTP_EVENT_CLIENT

int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */

/*****************************************************************************/
/*               SMTP CLIENT (OUTBOUND PROCESSING) STUFF                     */
/*****************************************************************************/



/*
 * smtp_try()
 *
 * Called by smtp_do_procmsg() to attempt delivery to one SMTP host
 *
 */
void smtp_try(const char *key, const char *addr, int *status,
	      char *dsn, size_t n, long msgnum, char *envelope_from)
{
	int sock = (-1);
	char mxhosts[1024];
	int num_mxhosts;
	int mx;
	int i;
	char user[1024], node[1024], name[1024];
	char buf[1024];
	char mailfrom[1024];
	char mx_user[256];
	char mx_pass[256];
	char mx_host[256];
	char mx_port[256];
	int lp, rp;
	char *msgtext;
	const char *ptr;
	size_t msg_size;
	int scan_done;
	CitContext *CCC=CC;
	
	
	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(addr, user, node, name);

	syslog(LOG_DEBUG, "SMTP client: Attempting delivery to <%s> @ <%s> (%s)\n",
		user, node, name);

	/* Load the message out of the database */
	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputMsg(msgnum, MT_RFC822, HEADERS_ALL, 0, 1, NULL, (ESC_DOT|SUPPRESS_ENV_TO) );
	msg_size = StrLength(CC->redirect_buffer);
	msgtext = SmashStrBuf(&CC->redirect_buffer);

	/* If no envelope_from is supplied, extract one from the message */
	if ( (envelope_from == NULL) || (IsEmptyStr(envelope_from)) ) {
		strcpy(mailfrom, "");
		scan_done = 0;
		ptr = msgtext;
		do {
			if (ptr = cmemreadline(ptr, buf, sizeof buf), *ptr == 0) {
				scan_done = 1;
			}
			if (!strncasecmp(buf, "From:", 5)) {
				safestrncpy(mailfrom, &buf[5], sizeof mailfrom);
				striplt(mailfrom);
				for (i=0; mailfrom[i]; ++i) {
					if (!isprint(mailfrom[i])) {
						strcpy(&mailfrom[i], &mailfrom[i+1]);
						i=0;
					}
				}
	
				/* Strip out parenthesized names */
				lp = (-1);
				rp = (-1);
				for (i=0; mailfrom[i]; ++i) {
					if (mailfrom[i] == '(') lp = i;
					if (mailfrom[i] == ')') rp = i;
				}
				if ((lp>0)&&(rp>lp)) {
					strcpy(&mailfrom[lp-1], &mailfrom[rp+1]);
				}
	
				/* Prefer brokketized names */
				lp = (-1);
				rp = (-1);
				for (i=0; mailfrom[i]; ++i) {
					if (mailfrom[i] == '<') lp = i;
					if (mailfrom[i] == '>') rp = i;
				}
				if ( (lp>=0) && (rp>lp) ) {
					mailfrom[rp] = 0;
					strcpy(mailfrom, &mailfrom[lp + 1]);
				}
	
				scan_done = 1;
			}
		} while (scan_done == 0);
		if (IsEmptyStr(mailfrom)) {
			char badmail_filename[128];
			snprintf(badmail_filename, sizeof badmail_filename, "/tmp/badmail.%d.%ld",
				getpid, time(NULL)
			);
			FILE *badmail_fp = fopen(badmail_filename, "w");
			fwrite(msgtext, msg_size, 1, badmail_fp);
			fclose(badmail_fp);
		}
		stripallbut(mailfrom, '<', '>');
		envelope_from = mailfrom;
	}

	/* Figure out what mail exchanger host we have to connect to */
	num_mxhosts = getmx(mxhosts, node);
	syslog(LOG_DEBUG, "Number of MX hosts for <%s> is %d [%s]\n", node, num_mxhosts, mxhosts);
	if (num_mxhosts < 1) {
		*status = 5;
		snprintf(dsn, SIZ, "No MX hosts found for <%s>", node);
		return;
	}

	sock = (-1);
	for (mx=0; (mx<num_mxhosts && sock < 0); ++mx) {
		char *endpart;
		extract_token(buf, mxhosts, mx, '|', sizeof buf);
		strcpy(mx_user, "");
		strcpy(mx_pass, "");
		if (num_tokens(buf, '@') > 1) {
			strcpy (mx_user, buf);
			endpart = strrchr(mx_user, '@');
			*endpart = '\0';
			strcpy (mx_host, endpart + 1);
			endpart = strrchr(mx_user, ':');
			if (endpart != NULL) {
				strcpy(mx_pass, endpart+1);
				*endpart = '\0';
			}
		}
		else
			strcpy (mx_host, buf);
		endpart = strrchr(mx_host, ':');
		if (endpart != 0){
			*endpart = '\0';
			strcpy(mx_port, endpart + 1);
		}		
		else {
			strcpy(mx_port, "25");
		}
		syslog(LOG_DEBUG, "SMTP client: connecting to %s : %s ...\n", mx_host, mx_port);
		sock = sock_connect(mx_host, mx_port);
		snprintf(dsn, SIZ, "Could not connect: %s", strerror(errno));
		if (sock >= 0) 
		{
			syslog(LOG_DEBUG, "SMTP client: connected!\n");
				int fdflags; 
				fdflags = fcntl(sock, F_GETFL);
				if (fdflags < 0)
					syslog(LOG_DEBUG,
						      "unable to get SMTP-Client socket flags! %s \n",
						      strerror(errno));
				fdflags = fdflags | O_NONBLOCK;
				if (fcntl(sock, F_SETFL, fdflags) < 0)
					syslog(LOG_DEBUG,
						      "unable to set SMTP-Client socket nonblocking flags! %s \n",
						      strerror(errno));
		}
		if (sock < 0) {
			if (errno > 0) {
				snprintf(dsn, SIZ, "%s", strerror(errno));
			}
			else {
				snprintf(dsn, SIZ, "Unable to connect to %s : %s\n", mx_host, mx_port);
			}
		}
	}

	if (sock < 0) {
		*status = 4;	/* dsn is already filled in */
		return;
	}

	CCC->sReadBuf = NewStrBuf();
	CCC->sMigrateBuf = NewStrBuf();
	CCC->sPos = NULL;

	/* Process the SMTP greeting from the server */
	if (ml_sock_gets(&sock, buf, 90) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		goto bail;
	}
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* At this point we know we are talking to a real SMTP server */

	/* Do a EHLO command.  If it fails, try the HELO command. */
	snprintf(buf, sizeof buf, "EHLO %s\r\n", config.c_fqdn);
	syslog(LOG_DEBUG, ">%s", buf);
	sock_write(&sock, buf, strlen(buf));
	if (ml_sock_gets(&sock, buf, 30) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP HELO");
		goto bail;
	}
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		snprintf(buf, sizeof buf, "HELO %s\r\n", config.c_fqdn);
		syslog(LOG_DEBUG, ">%s", buf);
		sock_write(&sock, buf, strlen(buf));
		if (ml_sock_gets(&sock, buf, 30) < 0) {
			*status = 4;
			strcpy(dsn, "Connection broken during SMTP HELO");
			goto bail;
		}
	}
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* Do an AUTH command if necessary */
	if (!IsEmptyStr(mx_user)) {
		char encoded[1024];
		sprintf(buf, "%s%c%s%c%s", mx_user, '\0', mx_user, '\0', mx_pass);
		CtdlEncodeBase64(encoded, buf, strlen(mx_user) + strlen(mx_user) + strlen(mx_pass) + 2, 0);
		snprintf(buf, sizeof buf, "AUTH PLAIN %s\r\n", encoded);
		syslog(LOG_DEBUG, ">%s", buf);
		sock_write(&sock, buf, strlen(buf));
		if (ml_sock_gets(&sock, buf, 30) < 0) {
			*status = 4;
			strcpy(dsn, "Connection broken during SMTP AUTH");
			goto bail;
		}
		syslog(LOG_DEBUG, "<%s\n", buf);
		if (buf[0] != '2') {
			if (buf[0] == '4') {
				*status = 4;
				safestrncpy(dsn, &buf[4], 1023);
				goto bail;
			}
			else {
				*status = 5;
				safestrncpy(dsn, &buf[4], 1023);
				goto bail;
			}
		}
	}

	/* previous command succeeded, now try the MAIL FROM: command */
	snprintf(buf, sizeof buf, "MAIL FROM:<%s>\r\n", envelope_from);
	syslog(LOG_DEBUG, ">%s", buf);
	sock_write(&sock, buf, strlen(buf));
	if (ml_sock_gets(&sock, buf, 30) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP MAIL");
		goto bail;
	}
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* MAIL succeeded, now try the RCPT To: command */
	snprintf(buf, sizeof buf, "RCPT TO:<%s@%s>\r\n", user, node);
	syslog(LOG_DEBUG, ">%s", buf);
	sock_write(&sock, buf, strlen(buf));
	if (ml_sock_gets(&sock, buf, 30) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP RCPT");
		goto bail;
	}
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* RCPT succeeded, now try the DATA command */
	syslog(LOG_DEBUG, ">DATA\n");
	sock_write(&sock, "DATA\r\n", 6);
	if (ml_sock_gets(&sock, buf, 30) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP DATA");
		goto bail;
	}
	syslog(LOG_DEBUG, "<%s\n", buf);
	if (buf[0] != '3') {
		if (buf[0] == '4') {
			*status = 3;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* If we reach this point, the server is expecting data.*/
	sock_write_timeout(&sock, 
			   msgtext, 
			   msg_size, 
			   (msg_size / 128) + 50);
	if (msgtext[msg_size-1] != 10) {
		syslog(LOG_WARNING, "Possible problem: message did not "
			"correctly terminate. (expecting 0x10, got 0x%02x)\n",
				buf[msg_size-1]);
		sock_write(&sock, "\r\n", 2);
	}

	sock_write(&sock, ".\r\n", 3);
	tcdrain(sock);
	if (ml_sock_gets(&sock, buf, 90) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP message transmit");
		goto bail;
	}
	syslog(LOG_DEBUG, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* We did it! */
	safestrncpy(dsn, &buf[4], 1023);
	*status = 2;

	syslog(LOG_DEBUG, ">QUIT\n");
	sock_write(&sock, "QUIT\r\n", 6);
	ml_sock_gets(&sock, buf, 30);
	syslog(LOG_DEBUG, "<%s\n", buf);
	syslog(LOG_INFO, "SMTP client: delivery to <%s> @ <%s> (%s) succeeded\n",
		user, node, name);

bail:	free(msgtext);
	FreeStrBuf(&CCC->sReadBuf);
	FreeStrBuf(&CCC->sMigrateBuf);
	if (sock != -1)
		sock_close(sock);

	/* Write something to the syslog(which may or may not be where the
	 * rest of the Citadel logs are going; some sysadmins want LOG_MAIL).
	 */
	syslog((LOG_MAIL | LOG_INFO),
		"%ld: to=<%s>, relay=%s, stat=%s",
		msgnum,
		addr,
		mx_host,
		dsn
	);

	return;
}



/*
 * smtp_do_bounce() is caled by smtp_do_procmsg() to scan a set of delivery
 * instructions for "5" codes (permanent fatal errors) and produce/deliver
 * a "bounce" message (delivery status notification).
 */
void smtp_do_bounce(char *instr) {
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
	long bounce_msgid = (-1);
	time_t submitted = 0L;
	struct CtdlMessage *bmsg = NULL;
	int give_up = 0;
	struct recptypes *valid;
	int successful_bounce = 0;
	static int seq = 0;
	StrBuf *BounceMB;
	long omsgid = (-1);

	syslog(LOG_DEBUG, "smtp_do_bounce() called\n");
	strcpy(bounceto, "");
	boundary = NewStrBufPlain(HKEY("=_Citadel_Multipart_"));
	StrBufAppendPrintf(boundary, "%s_%04x%04x", config.c_fqdn, getpid(), ++seq);
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
        bmsg->cm_fields['A'] = strdup("Citadel");
        bmsg->cm_fields['O'] = strdup(MAILROOM);
        bmsg->cm_fields['N'] = strdup(config.c_nodename);
        bmsg->cm_fields['U'] = strdup("Delivery Status Notification (Failure)");
	StrBufAppendBufPlain(BounceMB, HKEY("Content-type: multipart/mixed; boundary=\""), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
        StrBufAppendBufPlain(BounceMB, HKEY("\"\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("MIME-Version: 1.0\r\n"), 0);
	StrBufAppendBufPlain(BounceMB, HKEY("X-Mailer: " CITADEL "\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("\r\nThis is a multipart message in MIME format.\r\n\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
        StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
        StrBufAppendBufPlain(BounceMB, HKEY("Content-type: text/plain\r\n\r\n"), 0);

	if (give_up) StrBufAppendBufPlain(BounceMB, HKEY(
"A message you sent could not be delivered to some or all of its recipients\n"
"due to prolonged unavailability of its destination(s).\n"
"Giving up on the following addresses:\n\n"
						  ), 0);

        else StrBufAppendBufPlain(BounceMB, HKEY(
"A message you sent could not be delivered to some or all of its recipients.\n"
"The following addresses were undeliverable:\n\n"
					  ), 0);

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
		StrBufAppendBufPlain(BounceMB, HKEY("Content-type: message/rfc822\r\n"), 0);
        	StrBufAppendBufPlain(BounceMB, HKEY("Content-Transfer-Encoding: 7bit\r\n"), 0);
        	StrBufAppendBufPlain(BounceMB, HKEY("Content-Disposition: inline\r\n"), 0);
        	StrBufAppendBufPlain(BounceMB, HKEY("\r\n"), 0);
	
		CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
		CtdlOutputMsg(omsgid, MT_RFC822, HEADERS_ALL, 0, 1, NULL, 0);
		StrBufAppendBuf(BounceMB, CC->redirect_buffer, 0);
		FreeStrBuf(&CC->redirect_buffer);
	}

	/* Close the multipart MIME scope */
        StrBufAppendBufPlain(BounceMB, HKEY("--"), 0);
	StrBufAppendBuf(BounceMB, boundary, 0);
	StrBufAppendBufPlain(BounceMB, HKEY("--\r\n"), 0);
	if (bmsg->cm_fields['A'] != NULL)
		free(bmsg->cm_fields['A']);
	bmsg->cm_fields['A'] = SmashStrBuf(&BounceMB);
	/* Deliver the bounce if there's anything worth mentioning */
	syslog(LOG_DEBUG, "num_bounces = %d\n", num_bounces);
	if (num_bounces > 0) {

		/* First try the user who sent the message */
		syslog(LOG_DEBUG, "bounce to user? <%s>\n", bounceto);
		if (IsEmptyStr(bounceto)) {
			syslog(LOG_ERR, "No bounce address specified\n");
			bounce_msgid = (-1L);
		}

		/* Can we deliver the bounce to the original sender? */
		valid = validate_recipients(bounceto, smtp_get_Recipients (), 0);
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
	CtdlFreeMessage(bmsg);
	syslog(LOG_DEBUG, "Done processing bounces\n");
}


/*
 * smtp_purge_completed_deliveries() is caled by smtp_do_procmsg() to scan a
 * set of delivery instructions for completed deliveries and remove them.
 *
 * It returns the number of incomplete deliveries remaining.
 */
int smtp_purge_completed_deliveries(char *instr) {
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	int completed;
	int incomplete = 0;

	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		extract_token(dsn, buf, 3, '|', sizeof dsn);

		completed = 0;

		if (!strcasecmp(key, "remote")) {
			if (status == 2) completed = 1;
			else ++incomplete;
		}

		if (completed) {
			remove_token(instr, i, '\n');
			--i;
			--lines;
		}
	}

	return(incomplete);
}


/*
 * smtp_do_procmsg()
 *
 * Called by smtp_do_queue() to handle an individual message.
 */
void smtp_do_procmsg(long msgnum, void *userdata) {
	struct CtdlMessage *msg = NULL;
	char *instr = NULL;
	char *results = NULL;
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	char envelope_from[1024];
	long text_msgid = (-1);
	int incomplete_deliveries_remaining;
	time_t attempted = 0L;
	time_t last_attempted = 0L;
	time_t retry = SMTP_RETRY_INTERVAL;

	syslog(LOG_DEBUG, "SMTP client: smtp_do_procmsg(%ld)\n", msgnum);
	strcpy(envelope_from, "");

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		syslog(LOG_ERR, "SMTP client: tried %ld but no such message!\n", msgnum);
		return;
	}

	instr = strdup(msg->cm_fields['M']);
	CtdlFreeMessage(msg);

	/* Strip out the headers amd any other non-instruction line */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		if (num_tokens(buf, '|') < 2) {
			remove_token(instr, i, '\n');
			--lines;
			--i;
		}
	}

	/* Learn the message ID and find out about recent delivery attempts */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		if (!strcasecmp(key, "msgid")) {
			text_msgid = extract_long(buf, 1);
		}
		if (!strcasecmp(key, "envelope_from")) {
			extract_token(envelope_from, buf, 1, '|', sizeof envelope_from);
		}
		if (!strcasecmp(key, "retry")) {
			/* double the retry interval after each attempt */
			retry = extract_long(buf, 1) * 2L;
			if (retry > SMTP_RETRY_MAX) {
				retry = SMTP_RETRY_MAX;
			}
			remove_token(instr, i, '\n');
		}
		if (!strcasecmp(key, "attempted")) {
			attempted = extract_long(buf, 1);
			if (attempted > last_attempted)
				last_attempted = attempted;
		}
	}

	/*
	 * Postpone delivery if we've already tried recently.
	 */
	if (((time(NULL) - last_attempted) < retry) && (run_queue_now == 0)) {
		syslog(LOG_DEBUG, "SMTP client: Retry time not yet reached.\n");
		free(instr);
		return;
	}


	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (text_msgid < 0L) {
		syslog(LOG_ERR, "SMTP client: no 'msgid' directive found!\n");
		free(instr);
		return;
	}

	/* Plow through the instructions looking for 'remote' directives and
	 * a status of 0 (no delivery yet attempted) or 3/4 (transient errors
	 * were experienced and it's time to try again)
	 */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		extract_token(dsn, buf, 3, '|', sizeof dsn);
		if ( (!strcasecmp(key, "remote"))
		   && ((status==0)||(status==3)||(status==4)) ) {

			/* Remove this "remote" instruction from the set,
			 * but replace the set's final newline if
			 * remove_token() stripped it.  It has to be there.
			 */
			remove_token(instr, i, '\n');
			if (instr[strlen(instr)-1] != '\n') {
				strcat(instr, "\n");
			}

			--i;
			--lines;
			syslog(LOG_DEBUG, "SMTP client: Trying <%s>\n", addr);
			smtp_try(key, addr, &status, dsn, sizeof dsn, text_msgid, envelope_from);
			if (status != 2) {
				if (results == NULL) {
					results = malloc(1024);
					memset(results, 0, 1024);
				}
				else {
					results = realloc(results, strlen(results) + 1024);
				}
				snprintf(&results[strlen(results)], 1024,
					"%s|%s|%d|%s\n",
					key, addr, status, dsn);
			}
		}
	}

	if (results != NULL) {
		instr = realloc(instr, strlen(instr) + strlen(results) + 2);
		strcat(instr, results);
		free(results);
	}


	/* Generate 'bounce' messages */
	smtp_do_bounce(instr);

	/* Go through the delivery list, deleting completed deliveries */
	incomplete_deliveries_remaining = 
		smtp_purge_completed_deliveries(instr);


	/*
	 * No delivery instructions remain, so delete both the instructions
	 * message and the message message.
	 */
	if (incomplete_deliveries_remaining <= 0) {
		long delmsgs[2];
		delmsgs[0] = msgnum;
		delmsgs[1] = text_msgid;
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, delmsgs, 2, "");
	}

	/*
	 * Uncompleted delivery instructions remain, so delete the old
	 * instructions and replace with the updated ones.
	 */
	if (incomplete_deliveries_remaining > 0) {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &msgnum, 1, "");
        	msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		msg->cm_fields['M'] = malloc(strlen(instr)+SIZ);
		snprintf(msg->cm_fields['M'],
			strlen(instr)+SIZ,
			"Content-type: %s\n\n%s\n"
			"attempted|%ld\n"
			"retry|%ld\n",
			SPOOLMIME, instr, (long)time(NULL), (long)retry );
		CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
		CtdlFreeMessage(msg);
	}

	free(instr);
}


/*****************************************************************************/
/*                          SMTP UTILITY COMMANDS                            */
/*****************************************************************************/

void cmd_smtp(char *argbuf) {
	char cmd[64];
	char node[256];
	char buf[1024];
	int i;
	int num_mxhosts;

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);

	if (!strcasecmp(cmd, "mx")) {
		extract_token(node, argbuf, 1, '|', sizeof node);
		num_mxhosts = getmx(buf, node);
		cprintf("%d %d MX hosts listed for %s\n",
			LISTING_FOLLOWS, num_mxhosts, node);
		for (i=0; i<num_mxhosts; ++i) {
			extract_token(node, buf, i, '|', sizeof node);
			cprintf("%s\n", node);
		}
		cprintf("000\n");
		return;
	}

	else if (!strcasecmp(cmd, "runqueue")) {
		run_queue_now = 1;
		cprintf("%d All outbound SMTP will be retried now.\n", CIT_OK);
		return;
	}

	else {
		cprintf("%d Invalid command.\n", ERROR + ILLEGAL_VALUE);
	}

}


/*
 * smtp_queue_thread()
 * 
 * Run through the queue sending out messages.
 */
void smtp_do_queue(void) {
	static int is_running = 0;
	int num_processed = 0;

	if (is_running) return;		/* Concurrency check - only one can run */
	is_running = 1;

	syslog(LOG_INFO, "SMTP client: processing outbound queue\n");

	if (CtdlGetRoom(&CC->room, SMTP_SPOOLOUT_ROOM) != 0) {
		syslog(LOG_ERR, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
	}
	else {
		num_processed = CtdlForEachMessage(MSGS_ALL, 0L, NULL, SPOOLMIME, NULL, smtp_do_procmsg, NULL);
	}
	syslog(LOG_INFO, "SMTP client: queue run completed; %d messages processed\n", num_processed);
	is_running = 0;
}


/*
 * Initialize the SMTP outbound queue
 */
void smtp_init_spoolout(void) {
	struct ctdlroom qrbuf;

	/*
	 * Create the room.  This will silently fail if the room already
	 * exists, and that's perfectly ok, because we want it to exist.
	 */
	CtdlCreateRoom(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1, 0, VIEW_MAILBOX);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (CtdlGetRoomLock(&qrbuf, SMTP_SPOOLOUT_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		CtdlPutRoomLock(&qrbuf);
	}
}


#endif

CTDL_MODULE_INIT(smtp_client)
{
#ifndef EXPERIMENTAL_SMTP_EVENT_CLIENT
	if (!threading)
	{
		smtp_init_spoolout();
		CtdlRegisterSessionHook(smtp_do_queue, EVT_TIMER);
		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
	}
	
#endif
	/* return our Subversion id for the Log */
	return "smtpclient";
}

