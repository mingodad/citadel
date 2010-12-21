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
 * Copyright (c) 1998-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
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
#include "event_client.h"

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT

int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */
int MsgCount = 0;
/*****************************************************************************/
/*               SMTP CLIENT (OUTBOUND PROCESSING) STUFF                     */
/*****************************************************************************/


typedef enum _eSMTP_C_States {
	eConnect, 
	eEHLO,
	eHELO,
	eSMTPAuth,
	eFROM,
	eRCPT,
	eDATA,
	eDATABody,
	eDATATerminateBody,
	eQUIT,
	eMaxSMTPC
} eSMTP_C_States;

const long SMTP_C_ReadTimeouts[eMaxSMTPC] = {
	90, /* Greeting... */
	30, /* EHLO */
	30, /* HELO */
	30, /* Auth */
	30, /* From */
	30, /* RCPT */
	30, /* DATA */
	90, /* DATABody */
	900, /* end of body... */
	30  /* QUIT */
};
/*
const long SMTP_C_SendTimeouts[eMaxSMTPC] = {

}; */
const char *ReadErrors[eMaxSMTPC] = {
	"Connection broken during SMTP conversation",
	"Connection broken during SMTP EHLO",
	"Connection broken during SMTP HELO",
	"Connection broken during SMTP AUTH",
	"Connection broken during SMTP MAIL FROM",
	"Connection broken during SMTP RCPT",
	"Connection broken during SMTP DATA",
	"Connection broken during SMTP message transmit",
	""/* quit reply, don't care. */
};


typedef struct _stmp_out_msg {
	long n;
	AsyncIO IO;

	eSMTP_C_States State;

	int SMTPstatus;

	int i_mx;
	int n_mx;
	int num_mxhosts;
	char mx_user[1024];
	char mx_pass[1024];
	char mx_host[1024];
	char mx_port[1024];
	char mxhosts[SIZ];

	StrBuf *msgtext;
	char *envelope_from;
	char user[1024];
	char node[1024];
	char name[1024];
	char addr[SIZ];
	char dsn[1024];
	char envelope_from_buf[1024];
	char mailfrom[1024];
} SmtpOutMsg;

eNextState SMTP_C_DispatchReadDone(void *Data);
eNextState SMTP_C_DispatchWriteDone(void *Data);


typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);


eReadState SMTP_C_ReadServerStatus(AsyncIO *IO)
{
	eReadState Finished = eBufferNotEmpty; 

	while (Finished == eBufferNotEmpty) {
		Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
		
		switch (Finished) {
		case eMustReadMore: /// read new from socket... 
			return Finished;
			break;
		case eBufferNotEmpty: /* shouldn't happen... */
		case eReadSuccess: /// done for now...
			if (StrLength(IO->IOBuf) < 4)
				continue;
			if (ChrPtr(IO->IOBuf)[3] == '-')
				Finished = eBufferNotEmpty;
			else 
				return Finished;
			break;
		case eReadFail: /// WHUT?
			///todo: shut down! 
			break;
		}
	}
	return Finished;
}




/**
 * this one has to have the context for loading the message via the redirect buffer...
 */
SmtpOutMsg *smtp_load_msg(long msgnum, const char *addr, char *envelope_from)
{
	CitContext *CCC=CC;
	SmtpOutMsg *SendMsg;
	
	SendMsg = (SmtpOutMsg *) malloc(sizeof(SmtpOutMsg));

	memset(SendMsg, 0, sizeof(SmtpOutMsg));
	SendMsg->IO.sock = (-1);
	
	SendMsg->n = MsgCount++;
	/* Load the message out of the database */
	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputMsg(msgnum, MT_RFC822, HEADERS_ALL, 0, 1, NULL, (ESC_DOT|SUPPRESS_ENV_TO) );
	SendMsg->msgtext = CCC->redirect_buffer;
	CCC->redirect_buffer = NULL;
	if ((StrLength(SendMsg->msgtext) > 0) && 
	    ChrPtr(SendMsg->msgtext)[StrLength(SendMsg->msgtext) - 1] != '\n') {
		CtdlLogPrintf(CTDL_WARNING, 
			      "SMTP client[%ld]: Possible problem: message did not "
			      "correctly terminate. (expecting 0x10, got 0x%02x)\n",
			      SendMsg->n,
			      ChrPtr(SendMsg->msgtext)[StrLength(SendMsg->msgtext) - 1] );
		StrBufAppendBufPlain(SendMsg->msgtext, HKEY("\r\n"), 0);
	}

	safestrncpy(SendMsg->addr, addr, SIZ);
	safestrncpy(SendMsg->envelope_from_buf, envelope_from, 1024);

	return SendMsg;
}


int smtp_resolve_recipients(SmtpOutMsg *SendMsg)
{
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(SendMsg->addr, SendMsg->user, SendMsg->node, SendMsg->name);

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: Attempting delivery to <%s> @ <%s> (%s)\n",
		      SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	/* If no envelope_from is supplied, extract one from the message */
	if ( (SendMsg->envelope_from == NULL) || 
	     (IsEmptyStr(SendMsg->envelope_from)) ) {
		SendMsg->mailfrom[0] = '\0';
		scan_done = 0;
		ptr = ChrPtr(SendMsg->msgtext);
		do {
			if (ptr = cmemreadline(ptr, buf, sizeof buf), *ptr == 0) {
				scan_done = 1;
			}
			if (!strncasecmp(buf, "From:", 5)) {
				safestrncpy(SendMsg->mailfrom, &buf[5], sizeof SendMsg->mailfrom);
				striplt(SendMsg->mailfrom);
				for (i=0; SendMsg->mailfrom[i]; ++i) {
					if (!isprint(SendMsg->mailfrom[i])) {
						strcpy(&SendMsg->mailfrom[i], &SendMsg->mailfrom[i+1]);
						i=0;
					}
				}
	
				/* Strip out parenthesized names */
				lp = (-1);
				rp = (-1);
				for (i=0; !IsEmptyStr(SendMsg->mailfrom + i); ++i) {
					if (SendMsg->mailfrom[i] == '(') lp = i;
					if (SendMsg->mailfrom[i] == ')') rp = i;
				}
				if ((lp>0)&&(rp>lp)) {
					strcpy(&SendMsg->mailfrom[lp-1], &SendMsg->mailfrom[rp+1]);
				}
	
				/* Prefer brokketized names */
				lp = (-1);
				rp = (-1);
				for (i=0; !IsEmptyStr(SendMsg->mailfrom + i); ++i) {
					if (SendMsg->mailfrom[i] == '<') lp = i;
					if (SendMsg->mailfrom[i] == '>') rp = i;
				}
				if ( (lp>=0) && (rp>lp) ) {
					SendMsg->mailfrom[rp] = 0;
					memmove(SendMsg->mailfrom, 
						&SendMsg->mailfrom[lp + 1], 
						rp - lp);
				}
	
				scan_done = 1;
			}
		} while (scan_done == 0);
		if (IsEmptyStr(SendMsg->mailfrom)) strcpy(SendMsg->mailfrom, "someone@somewhere.org");
		stripallbut(SendMsg->mailfrom, '<', '>');
		SendMsg->envelope_from = SendMsg->mailfrom;
	}

	return 0;
}

void resolve_mx_hosts(SmtpOutMsg *SendMsg)
{
	/// well this is blocking and sux, but libevent jsut supports async dns since v2
	/* Figure out what mail exchanger host we have to connect to */
	SendMsg->num_mxhosts = getmx(SendMsg->mxhosts, SendMsg->node);
	CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: Number of MX hosts for <%s> is %d [%s]\n", 
		      SendMsg->n, SendMsg->node, SendMsg->num_mxhosts, SendMsg->mxhosts);
	if (SendMsg->num_mxhosts < 1) {
		SendMsg->SMTPstatus = 5;
		snprintf(SendMsg->dsn, SIZ, "No MX hosts found for <%s>", SendMsg->node);
		return; ///////TODO: abort!
	}

}
/* TODO: abort... */
#define SMTP_ERROR(WHICH_ERR, ERRSTR) {SendMsg->SMTPstatus = WHICH_ERR; memcpy(SendMsg->dsn, HKEY(ERRSTR) + 1); return eAbort; }
#define SMTP_VERROR(WHICH_ERR) { SendMsg->SMTPstatus = WHICH_ERR; safestrncpy(SendMsg->dsn, &ChrPtr(SendMsg->IO.IOBuf)[4], sizeof(SendMsg->dsn)); return eAbort; }
#define SMTP_IS_STATE(WHICH_STATE) (ChrPtr(SendMsg->IO.IOBuf)[0] == WHICH_STATE)

#define SMTP_DBG_SEND() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: > %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))
#define SMTP_DBG_READ() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: < %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))

void connect_one_smtpsrv(SmtpOutMsg *SendMsg)
{
	char *endpart;
	char buf[SIZ];

	extract_token(buf, SendMsg->mxhosts, SendMsg->n_mx, '|', sizeof(buf));
	strcpy(SendMsg->mx_user, "");
	strcpy(SendMsg->mx_pass, "");
	if (num_tokens(buf, '@') > 1) {
		strcpy (SendMsg->mx_user, buf);
		endpart = strrchr(SendMsg->mx_user, '@');
		*endpart = '\0';
		strcpy (SendMsg->mx_host, endpart + 1);
		endpart = strrchr(SendMsg->mx_user, ':');
		if (endpart != NULL) {
			strcpy(SendMsg->mx_pass, endpart+1);
			*endpart = '\0';
		}
	}
	else
		strcpy (SendMsg->mx_host, buf);
	endpart = strrchr(SendMsg->mx_host, ':');
	if (endpart != 0){
		*endpart = '\0';
		strcpy(SendMsg->mx_port, endpart + 1);
	}		
	else {
		strcpy(SendMsg->mx_port, "25");
	}
	CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: connecting to %s : %s ...\n", 
		      SendMsg->n, SendMsg->mx_host, SendMsg->mx_port);

}


int connect_one_smtpsrv_xamine_result(void *Ctx)
{
	SmtpOutMsg *SendMsg = Ctx;
	SendMsg->IO.SendBuf.fd = 
	SendMsg->IO.RecvBuf.fd = 
	SendMsg->IO.sock = sock_connect(SendMsg->mx_host, SendMsg->mx_port);

	snprintf(SendMsg->dsn, SIZ, "Could not connect: %s", strerror(errno));
	if (SendMsg->IO.sock >= 0) 
	{
		CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: connected!\n", SendMsg->n);
		int fdflags; 
		fdflags = fcntl(SendMsg->IO.sock, F_GETFL);
		if (fdflags < 0)
			CtdlLogPrintf(CTDL_DEBUG,
				      "SMTP client[%ld]: unable to get socket flags! %s \n",
				      SendMsg->n, strerror(errno));
		fdflags = fdflags | O_NONBLOCK;
		if (fcntl(SendMsg->IO.sock, F_SETFL, fdflags) < 0)
			CtdlLogPrintf(CTDL_DEBUG,
				      "SMTP client[%ld]: unable to set socket nonblocking flags! %s \n",
				      SendMsg->n, strerror(errno));
	}
	if (SendMsg->IO.sock < 0) {
		if (errno > 0) {
			snprintf(SendMsg->dsn, SIZ, "%s", strerror(errno));
		}
		else {
			snprintf(SendMsg->dsn, SIZ, "Unable to connect to %s : %s\n", 
				 SendMsg->mx_host, SendMsg->mx_port);
		}
	}
	/// hier: naechsten mx ausprobieren.
	if (SendMsg->IO.sock < 0) {
		SendMsg->SMTPstatus = 4;	/* dsn is already filled in */
		//// hier: abbrechen & bounce.
		return -1;
	}
	SendMsg->IO.SendBuf.Buf = NewStrBuf();
	SendMsg->IO.RecvBuf.Buf = NewStrBuf();
	SendMsg->IO.IOBuf = NewStrBuf();
	InitEventIO(&SendMsg->IO, SendMsg, 
		    SMTP_C_DispatchReadDone, 
		    SMTP_C_DispatchWriteDone, 
		    SMTP_C_ReadServerStatus,
		    1);
	return 0;
}

eNextState SMTPC_read_greeting(SmtpOutMsg *SendMsg)
{
	/* Process the SMTP greeting from the server */
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_EHLO(SmtpOutMsg *SendMsg)
{
	/* At this point we know we are talking to a real SMTP server */

	/* Do a EHLO command.  If it fails, try the HELO command. */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "EHLO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_EHLO_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (SMTP_IS_STATE('2')) {
		SendMsg->State ++;
		if (IsEmptyStr(SendMsg->mx_user))
			SendMsg->State ++; /* Skip auth... */
	}
	/* else we fall back to 'helo' */
	return eSendReply;
}

eNextState STMPC_send_HELO(SmtpOutMsg *SendMsg)
{
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "HELO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_HELO_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	if (!IsEmptyStr(SendMsg->mx_user))
		SendMsg->State ++; /* Skip auth... */
	return eSendReply;
}

eNextState SMTPC_send_auth(SmtpOutMsg *SendMsg)
{
	char buf[SIZ];
	char encoded[1024];

	/* Do an AUTH command if necessary */
	sprintf(buf, "%s%c%s%c%s", 
		SendMsg->mx_user, '\0', 
		SendMsg->mx_user, '\0', 
		SendMsg->mx_pass);
	CtdlEncodeBase64(encoded, buf, 
			 strlen(SendMsg->mx_user) + 
			 strlen(SendMsg->mx_user) + 
			 strlen(SendMsg->mx_pass) + 2, 0);
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "AUTH PLAIN %s\r\n", encoded);
	
	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_auth_reply(SmtpOutMsg *SendMsg)
{
	/* Do an AUTH command if necessary */
	
	SMTP_DBG_READ();
	
	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_FROM(SmtpOutMsg *SendMsg)
{
	/* previous command succeeded, now try the MAIL FROM: command */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "MAIL FROM:<%s>\r\n", 
		     SendMsg->envelope_from);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_FROM_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}


eNextState SMTPC_send_RCPT(SmtpOutMsg *SendMsg)
{
	/* MAIL succeeded, now try the RCPT To: command */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "RCPT TO:<%s@%s>\r\n", 
		     SendMsg->user, 
		     SendMsg->node);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_RCPT_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_DATAcmd(SmtpOutMsg *SendMsg)
{
	/* RCPT succeeded, now try the DATA command */
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("DATA\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_DATAcmd_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('3')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(3)
		else 
			SMTP_VERROR(5)
	}
	return eSendReply;
}

eNextState SMTPC_send_data_body(SmtpOutMsg *SendMsg)
{
	StrBuf *Buf;
	/* If we reach this point, the server is expecting data.*/

	Buf = SendMsg->IO.SendBuf.Buf;
	SendMsg->IO.SendBuf.Buf = SendMsg->msgtext;
	SendMsg->msgtext = Buf;
	//// TODO timeout like that: (SendMsg->msg_size / 128) + 50);
	SendMsg->State ++;

	return eSendMore;
}

eNextState SMTPC_send_terminate_data_body(SmtpOutMsg *SendMsg)
{
	StrBuf *Buf;

	Buf = SendMsg->IO.SendBuf.Buf;
	SendMsg->IO.SendBuf.Buf = SendMsg->msgtext;
	SendMsg->msgtext = Buf;

	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY(".\r\n"));

	return eReadMessage;

}

eNextState SMTPC_read_data_body_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4)
		else 
			SMTP_VERROR(5)
	}

	/* We did it! */
	safestrncpy(SendMsg->dsn, &ChrPtr(SendMsg->IO.RecvBuf.Buf)[4], 1023);
	SendMsg->SMTPstatus = 2;
	return eSendReply;
}

eNextState SMTPC_send_QUIT(SmtpOutMsg *SendMsg)
{
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_QUIT_reply(SmtpOutMsg *SendMsg)
{
	SMTP_DBG_READ();

	CtdlLogPrintf(CTDL_INFO, "SMTP client[%ld]: delivery to <%s> @ <%s> (%s) succeeded\n",
		      SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	return eSendReply;
}

eNextState SMTPC_read_dummy(SmtpOutMsg *SendMsg)
{
	return eSendReply;
}

eNextState SMTPC_send_dummy(SmtpOutMsg *SendMsg)
{
	return eReadMessage;
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

	CtdlLogPrintf(CTDL_DEBUG, "smtp_do_bounce() called\n");
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

		CtdlLogPrintf(CTDL_DEBUG, "key=<%s> addr=<%s> status=%d dsn=<%s>\n",
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
	CtdlLogPrintf(CTDL_DEBUG, "num_bounces = %d\n", num_bounces);
	if (num_bounces > 0) {

		/* First try the user who sent the message */
		CtdlLogPrintf(CTDL_DEBUG, "bounce to user? <%s>\n", bounceto);
		if (IsEmptyStr(bounceto)) {
			CtdlLogPrintf(CTDL_ERR, "No bounce address specified\n");
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
	CtdlLogPrintf(CTDL_DEBUG, "Done processing bounces\n");
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

void smtp_try(const char *key, const char *addr, int *status,
              char *dsn, size_t n, long msgnum, char *envelope_from)
{
	SmtpOutMsg * SmtpC = smtp_load_msg(msgnum, addr, envelope_from);
	smtp_resolve_recipients(SmtpC);
	resolve_mx_hosts(SmtpC);
	connect_one_smtpsrv(SmtpC);
	QueueEventContext(SmtpC, connect_one_smtpsrv_xamine_result);
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

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client: smtp_do_procmsg(%ld)\n", msgnum);
	strcpy(envelope_from, "");

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		CtdlLogPrintf(CTDL_ERR, "SMTP client: tried %ld but no such message!\n", msgnum);
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
	 * /
	if (((time(NULL) - last_attempted) < retry) && (run_queue_now == 0)) {
		CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Retry time not yet reached.\n");
		free(instr);
		return;
	}
TMP TODO	*/

	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (text_msgid < 0L) {
		CtdlLogPrintf(CTDL_ERR, "SMTP client: no 'msgid' directive found!\n");
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
			CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Trying <%s>\n", addr);
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
void *smtp_queue_thread(void *arg) {
	int num_processed = 0;
	struct CitContext smtp_queue_CC;

	CtdlFillSystemContext(&smtp_queue_CC, "SMTP Send");
	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	CtdlLogPrintf(CTDL_DEBUG, "smtp_queue_thread() initializing\n");

	while (!CtdlThreadCheckStop()) {
		
		CtdlLogPrintf(CTDL_INFO, "SMTP client: processing outbound queue\n");

		if (CtdlGetRoom(&CC->room, SMTP_SPOOLOUT_ROOM) != 0) {
			CtdlLogPrintf(CTDL_ERR, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
		}
		else {
			num_processed = CtdlForEachMessage(MSGS_ALL, 0L, NULL, SPOOLMIME, NULL, smtp_do_procmsg, NULL);
		}
		CtdlLogPrintf(CTDL_INFO, "SMTP client: queue run completed; %d messages processed\n", num_processed);
		CtdlThreadSleep(60);
	}

	CtdlClearSystemContext();
	return(NULL);
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


SMTPReadHandler ReadHandlers[eMaxSMTPC] = {
	SMTPC_read_greeting,
	SMTPC_read_EHLO_reply,
	SMTPC_read_HELO_reply,
	SMTPC_read_auth_reply,
	SMTPC_read_FROM_reply,
	SMTPC_read_RCPT_reply,
	SMTPC_read_DATAcmd_reply,
	SMTPC_read_dummy,
	SMTPC_read_data_body_reply,
	SMTPC_read_QUIT_reply
};

SMTPSendHandler SendHandlers[eMaxSMTPC] = {
	SMTPC_send_dummy, /* we don't send a greeting, the server does... */
	SMTPC_send_EHLO,
	STMPC_send_HELO,
	SMTPC_send_auth,
	SMTPC_send_FROM,
	SMTPC_send_RCPT,
	SMTPC_send_DATAcmd,
	SMTPC_send_data_body,
	SMTPC_send_terminate_data_body,
	SMTPC_send_QUIT
};

eNextState SMTP_C_DispatchReadDone(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	eNextState rc = ReadHandlers[pMsg->State](pMsg);
	pMsg->State++;
	return rc;
}

eNextState SMTP_C_DispatchWriteDone(void *Data)
{
	SmtpOutMsg *pMsg = Data;
	return SendHandlers[pMsg->State](pMsg);
	
}


#endif
CTDL_MODULE_INIT(smtp_eventclient)
{
#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
	if (!threading)
	{
		smtp_init_spoolout();
		CtdlThreadCreate("SMTPEvent Send", CTDLTHREAD_BIGSTACK, smtp_queue_thread, NULL);

		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
	}
#endif
	
	/* return our Subversion id for the Log */
	return "smtpeventclient";
}
