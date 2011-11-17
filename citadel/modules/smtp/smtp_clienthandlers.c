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
#include "smtpqueue.h"
#include "smtp_clienthandlers.h"


#define SMTP_ERROR(WHICH_ERR, ERRSTR) do {\
		SendMsg->MyQEntry->Status = WHICH_ERR; \
		StrBufAppendBufPlain(SendMsg->MyQEntry->StatusMessage, HKEY(ERRSTR), 0); \
		return eAbort; } \
	while (0)

#define SMTP_VERROR(WHICH_ERR) do {\
		SendMsg->MyQEntry->Status = WHICH_ERR; \
		StrBufPlain(SendMsg->MyQEntry->StatusMessage, \
			    ChrPtr(SendMsg->IO.IOBuf) + 4, \
			    StrLength(SendMsg->IO.IOBuf) - 4); \
		return eAbort; } \
	while (0)

#define SMTP_IS_STATE(WHICH_STATE) (ChrPtr(SendMsg->IO.IOBuf)[0] == WHICH_STATE)

#define SMTP_DBG_SEND() EV_syslog(LOG_DEBUG, "SMTP client[%ld]: > %s\n", SendMsg->n, ChrPtr(SendMsg->IO.SendBuf.Buf))
#define SMTP_DBG_READ() EV_syslog(LOG_DEBUG, "SMTP client[%ld]: < %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))


/*****************************************************************************/
/*                     SMTP CLIENT STATE CALLBACKS                           */
/*****************************************************************************/
eNextState SMTPC_read_greeting(SmtpOutMsg *SendMsg)
{
	/* Process the SMTP greeting from the server */
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}
	return eSendReply;
}

eNextState SMTPC_send_EHLO(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	/* At this point we know we are talking to a real SMTP server */

	/* Do a EHLO command.  If it fails, try the HELO command. */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "EHLO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_EHLO_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (SMTP_IS_STATE('2')) {
		SendMsg->State ++;

		if ((SendMsg->pCurrRelay == NULL) || 
		    (SendMsg->pCurrRelay->User == NULL))
			SendMsg->State ++; /* Skip auth... */
	}
	/* else we fall back to 'helo' */
	return eSendReply;
}

eNextState STMPC_send_HELO(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "HELO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_HELO_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}
		if ((SendMsg->pCurrRelay == NULL) || 
		    (SendMsg->pCurrRelay->User == NULL))
		SendMsg->State ++; /* Skip auth... */
	return eSendReply;
}

eNextState SMTPC_send_auth(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	char buf[SIZ];
	char encoded[1024];

	if ((SendMsg->pCurrRelay == NULL) || 
	    (SendMsg->pCurrRelay->User == NULL))
		SendMsg->State ++; /* Skip auth, shouldn't even come here!... */
	else {
	/* Do an AUTH command if necessary */
	sprintf(buf, "%s%c%s%c%s", 
		SendMsg->pCurrRelay->User, '\0', 
		SendMsg->pCurrRelay->User, '\0', 
		SendMsg->pCurrRelay->Pass);
	CtdlEncodeBase64(encoded, buf, 
			 strlen(SendMsg->pCurrRelay->User) * 2 +
			 strlen(SendMsg->pCurrRelay->Pass) + 2, 0);
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "AUTH PLAIN %s\r\n", encoded);
	}
	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_auth_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	/* Do an AUTH command if necessary */
	
	SMTP_DBG_READ();
	
	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}
	return eSendReply;
}

eNextState SMTPC_send_FROM(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	/* previous command succeeded, now try the MAIL FROM: command */
	StrBufPrintf(SendMsg->IO.SendBuf.Buf,
		     "MAIL FROM:<%s>\r\n", 
		     SendMsg->envelope_from);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_FROM_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}
	return eSendReply;
}


eNextState SMTPC_send_RCPT(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
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
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}
	return eSendReply;
}

eNextState SMTPC_send_DATAcmd(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	/* RCPT succeeded, now try the DATA command */
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("DATA\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_DATAcmd_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('3')) {
		if (SMTP_IS_STATE('4')) 
			SMTP_VERROR(3);
		else 
			SMTP_VERROR(5);
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
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
	}

	/* We did it! */
	StrBufPlain(SendMsg->MyQEntry->StatusMessage, 
		    &ChrPtr(SendMsg->IO.RecvBuf.Buf)[4],
		    StrLength(SendMsg->IO.RecvBuf.Buf) - 4);
	SendMsg->MyQEntry->Status = 2;
	return eSendReply;
}

eNextState SMTPC_send_QUIT(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	StrBufPlain(SendMsg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_QUIT_reply(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	SMTP_DBG_READ();

	EV_syslog(LOG_INFO, "SMTP client[%ld]: delivery to <%s> @ <%s> (%s) succeeded\n",
		  SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	return eTerminateConnection;
}

eNextState SMTPC_read_dummy(SmtpOutMsg *SendMsg)
{
	return eSendReply;
}

eNextState SMTPC_send_dummy(SmtpOutMsg *SendMsg)
{
	return eReadMessage;
}

/*****************************************************************************/
/*                     SMTP CLIENT DISPATCHER                                */
/*****************************************************************************/
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

const double SMTP_C_ConnTimeout = 300.; /* wail 1 minute for connections... */

const double SMTP_C_ReadTimeouts[eMaxSMTPC] = {
	300., /* Greeting... */
	30., /* EHLO */
	30., /* HELO */
	30., /* Auth */
	30., /* From */
	90., /* RCPT */
	30., /* DATA */
	90., /* DATABody */
	90., /* end of body... */
	30.  /* QUIT */
};
const double SMTP_C_SendTimeouts[eMaxSMTPC] = {
	90., /* Greeting... */
	30., /* EHLO */
	30., /* HELO */
	30., /* Auth */
	30., /* From */
	30., /* RCPT */
	30., /* DATA */
	90., /* DATABody */
	900., /* end of body... */
	30.  /* QUIT */
};

const ConstStr ReadErrors[eMaxSMTPC + 1] = {
	{HKEY("Connection broken during SMTP conversation")},
	{HKEY("Connection broken during SMTP EHLO")},
	{HKEY("Connection broken during SMTP HELO")},
	{HKEY("Connection broken during SMTP AUTH")},
	{HKEY("Connection broken during SMTP MAIL FROM")},
	{HKEY("Connection broken during SMTP RCPT")},
	{HKEY("Connection broken during SMTP DATA")},
	{HKEY("Connection broken during SMTP message transmit")},
        {HKEY("")},/* quit reply, don't care. */
        {HKEY("")},/* quit reply, don't care. */
	{HKEY("")}/* quit reply, don't care. */
};





int smtp_resolve_recipients(SmtpOutMsg *SendMsg)
{
	AsyncIO *IO = &SendMsg->IO;
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	EV_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);

	if ((SendMsg==NULL) || 
	    (SendMsg->MyQEntry == NULL) || 
	    (StrLength(SendMsg->MyQEntry->Recipient) == 0)) {
		return 0;
	}

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(ChrPtr(SendMsg->MyQEntry->Recipient), 
			    SendMsg->user, 
			    SendMsg->node, 
			    SendMsg->name);

	EV_syslog(LOG_DEBUG, "SMTP client[%ld]: Attempting delivery to <%s> @ <%s> (%s)\n",
		  SendMsg->n, SendMsg->user, SendMsg->node, SendMsg->name);
	/* If no envelope_from is supplied, extract one from the message */
	SendMsg->envelope_from = ChrPtr(SendMsg->MyQItem->EnvelopeFrom);
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

	return 1;
}
