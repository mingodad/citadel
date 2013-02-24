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
#include "event_client.h"
#include "smtpqueue.h"
#include "smtp_clienthandlers.h"


#define SMTP_ERROR(WHICH_ERR, ERRSTR) do {			       \
		Msg->MyQEntry->Status = WHICH_ERR;		       \
		StrBufAppendBufPlain(Msg->MyQEntry->StatusMessage,     \
				     HKEY(ERRSTR), 0);		       \
		StrBufTrim(Msg->MyQEntry->StatusMessage);	       \
		return eAbort; }				       \
	while (0)

#define SMTP_VERROR(WHICH_ERR) do {			       \
		Msg->MyQEntry->Status = WHICH_ERR;	       \
		StrBufPlain(Msg->MyQEntry->StatusMessage,      \
			    ChrPtr(Msg->IO.IOBuf) + 4,	       \
			    StrLength(Msg->IO.IOBuf) - 4);     \
		StrBufTrim(Msg->MyQEntry->StatusMessage);      \
		return eAbort; }			       \
	while (0)

#define SMTP_IS_STATE(WHICH_STATE) (ChrPtr(Msg->IO.IOBuf)[0] == WHICH_STATE)

#define SMTP_DBG_SEND() \
	EVS_syslog(LOG_DEBUG, "> %s\n", ChrPtr(Msg->IO.SendBuf.Buf))

#define SMTP_DBG_READ() \
	EVS_syslog(LOG_DEBUG, "< %s\n", ChrPtr(Msg->IO.IOBuf))


/*****************************************************************************/
/*                     SMTP CLIENT STATE CALLBACKS                           */
/*****************************************************************************/
eNextState SMTPC_read_greeting(SmtpOutMsg *Msg)
{
	/* Process the SMTP greeting from the server */
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();
	SetSMTPState(IO, eSTMPsmtp);

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else
			SMTP_VERROR(5);
	}
	return eSendReply;
}

eNextState SMTPC_send_EHLO(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	/* At this point we know we are talking to a real SMTP server */

	/* Do a EHLO command.  If it fails, try the HELO command. */
	StrBufPrintf(Msg->IO.SendBuf.Buf,
		     "EHLO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_EHLO_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (SMTP_IS_STATE('2')) {
		Msg->State ++;

		if ((Msg->pCurrRelay == NULL) ||
		    (Msg->pCurrRelay->User == NULL))
			Msg->State ++; /* Skip auth... */
		if (Msg->pCurrRelay != NULL)
		{
			if (strstr(ChrPtr(Msg->IO.IOBuf), "LOGIN") != NULL)
				Msg->SendLogin = 1;
		}
	}
	/* else we fall back to 'helo' */
	return eSendReply;
}

eNextState STMPC_send_HELO(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	StrBufPrintf(Msg->IO.SendBuf.Buf,
		     "HELO %s\r\n", config.c_fqdn);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_HELO_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2'))
	{
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else
			SMTP_VERROR(5);
	}
	if (Msg->pCurrRelay != NULL)
	{
		if (strstr(ChrPtr(Msg->IO.IOBuf), "LOGIN") != NULL)
			Msg->SendLogin = 1;
	}
	if ((Msg->pCurrRelay == NULL) ||
	    (Msg->pCurrRelay->User == NULL))
		Msg->State ++; /* Skip auth... */

	return eSendReply;
}

eNextState SMTPC_send_auth(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	char buf[SIZ];
	char encoded[1024];

	if ((Msg->pCurrRelay == NULL) ||
	    (Msg->pCurrRelay->User == NULL))
		Msg->State ++; /* Skip auth, shouldn't even come here!... */
	else {
		/* Do an AUTH command if necessary */
		if (Msg->SendLogin)
		{
			sprintf(buf, "%s",
				Msg->pCurrRelay->User);

			CtdlEncodeBase64(encoded, buf,
					 strlen(Msg->pCurrRelay->User) * 2 +
					 strlen(Msg->pCurrRelay->Pass) + 2, 0);
			
			StrBufPrintf(Msg->IO.SendBuf.Buf,
				     "AUTH LOGIN %s\r\n",
				     encoded);
			sprintf(buf, "%s",
				Msg->pCurrRelay->Pass);
			
			CtdlEncodeBase64(encoded, buf,
					 strlen(Msg->pCurrRelay->User) * 2 +
					 strlen(Msg->pCurrRelay->Pass) + 2, 0);
			
			StrBufAppendPrintf(Msg->IO.SendBuf.Buf,
					   "%s\r\n",
					   encoded);
		}
		else
		{
			sprintf(buf, "%s%c%s%c%s",
				Msg->pCurrRelay->User, '\0',
				Msg->pCurrRelay->User, '\0',
				Msg->pCurrRelay->Pass);
			
			CtdlEncodeBase64(encoded, buf,
					 strlen(Msg->pCurrRelay->User) * 2 +
					 strlen(Msg->pCurrRelay->Pass) + 2, 0);
			
			StrBufPrintf(Msg->IO.SendBuf.Buf,
				     "AUTH PLAIN %s\r\n",
				     encoded);
		}
	}
	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_auth_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
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

eNextState SMTPC_send_FROM(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	/* previous command succeeded, now try the MAIL FROM: command */
	StrBufPrintf(Msg->IO.SendBuf.Buf,
		     "MAIL FROM:<%s>\r\n",
		     Msg->envelope_from);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_FROM_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else
			SMTP_VERROR(5);
	}
	return eSendReply;
}


eNextState SMTPC_send_RCPT(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	/* MAIL succeeded, now try the RCPT To: command */
	StrBufPrintf(Msg->IO.SendBuf.Buf,
		     "RCPT TO:<%s@%s>\r\n",
		     Msg->user,
		     Msg->node);

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_RCPT_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else
			SMTP_VERROR(5);
	}
	return eSendReply;
}

eNextState SMTPC_send_DATAcmd(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	/* RCPT succeeded, now try the DATA command */
	StrBufPlain(Msg->IO.SendBuf.Buf,
		    HKEY("DATA\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_DATAcmd_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('3')) {
		SetSMTPState(IO, eSTMPfailOne);
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(3);
		else
			SMTP_VERROR(5);
	}
	SetSMTPState(IO, eSTMPsmtpdata);
	return eSendReply;
}

eNextState SMTPC_send_data_body(SmtpOutMsg *Msg)
{
	StrBuf *Buf;
	/* If we reach this point, the server is expecting data.*/

	Buf = Msg->IO.SendBuf.Buf;
	Msg->IO.SendBuf.Buf = Msg->msgtext;
	Msg->msgtext = Buf;
	Msg->State ++;

	return eSendMore;
}

eNextState SMTPC_send_terminate_data_body(SmtpOutMsg *Msg)
{
	StrBuf *Buf;

	Buf = Msg->IO.SendBuf.Buf;
	Msg->IO.SendBuf.Buf = Msg->msgtext;
	Msg->msgtext = Buf;

	StrBufPlain(Msg->IO.SendBuf.Buf,
		    HKEY(".\r\n"));

	return eReadMessage;

}

eNextState SMTPC_read_data_body_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	if (!SMTP_IS_STATE('2')) {
		if (SMTP_IS_STATE('4'))
			SMTP_VERROR(4);
		else
			SMTP_VERROR(5);
	}

	SetSMTPState(IO, eSTMPsmtpdone);
	/* We did it! */
	StrBufPlain(Msg->MyQEntry->StatusMessage,
		    &ChrPtr(Msg->IO.RecvBuf.Buf)[4],
		    StrLength(Msg->IO.RecvBuf.Buf) - 4);
	StrBufTrim(Msg->MyQEntry->StatusMessage);
	Msg->MyQEntry->Status = 2;
	return eSendReply;
}

eNextState SMTPC_send_QUIT(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	StrBufPlain(Msg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n"));

	SMTP_DBG_SEND();
	return eReadMessage;
}

eNextState SMTPC_read_QUIT_reply(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	SMTP_DBG_READ();

	EVS_syslog(LOG_DEBUG,
		   "delivery to <%s> @ <%s> (%s) succeeded\n",
		   Msg->user,
		   Msg->node,
		   Msg->name);

	return eTerminateConnection;
}

eNextState SMTPC_read_dummy(SmtpOutMsg *Msg)
{
	return eSendReply;
}

eNextState SMTPC_send_dummy(SmtpOutMsg *Msg)
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
	{HKEY("Connection broken during SMTP message transmit")},/* quit reply, don't care. */
	{HKEY("Connection broken during SMTP message transmit")},/* quit reply, don't care. */
	{HKEY("")}/* quit reply, don't care. */
};





int smtp_resolve_recipients(SmtpOutMsg *Msg)
{
	AsyncIO *IO = &Msg->IO;
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	EVNCS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);

	if ((Msg==NULL) ||
	    (Msg->MyQEntry == NULL) ||
	    (StrLength(Msg->MyQEntry->Recipient) == 0)) {
		return 0;
	}

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(ChrPtr(Msg->MyQEntry->Recipient),
			    Msg->user,
			    Msg->node,
			    Msg->name);

	EVNCS_syslog(LOG_DEBUG,
		     "Attempting delivery to <%s> @ <%s> (%s)\n",
		     Msg->user,
		     Msg->node,
		     Msg->name);

	/* If no envelope_from is supplied, extract one from the message */
	Msg->envelope_from = ChrPtr(Msg->MyQItem->EnvelopeFrom);
	if ( (Msg->envelope_from == NULL) ||
	     (IsEmptyStr(Msg->envelope_from)) ) {
		Msg->mailfrom[0] = '\0';
		scan_done = 0;
		ptr = ChrPtr(Msg->msgtext);
		do {
			if (ptr = cmemreadline(ptr, buf, sizeof buf), *ptr == 0)
			{
				scan_done = 1;
			}
			if (!strncasecmp(buf, "From:", 5))
			{
				safestrncpy(Msg->mailfrom,
					    &buf[5],
					    sizeof Msg->mailfrom);

				striplt(Msg->mailfrom);
				for (i=0; Msg->mailfrom[i]; ++i) {
					if (!isprint(Msg->mailfrom[i]))
					{
						strcpy(&Msg->mailfrom[i],
						       &Msg->mailfrom[i+1]);
						i=0;
					}
				}

				/* Strip out parenthesized names */
				lp = (-1);
				rp = (-1);
				for (i=0;
				     !IsEmptyStr(Msg->mailfrom + i);
				     ++i)
				{
					if (Msg->mailfrom[i] == '(') lp = i;
					if (Msg->mailfrom[i] == ')') rp = i;
				}
				if ((lp>0)&&(rp>lp))
				{
					strcpy(&Msg->mailfrom[lp-1],
					       &Msg->mailfrom[rp+1]);
				}

				/* Prefer brokketized names */
				lp = (-1);
				rp = (-1);
				for (i=0;
				     !IsEmptyStr(Msg->mailfrom + i);
				     ++i)
				{
					if (Msg->mailfrom[i] == '<') lp = i;
					if (Msg->mailfrom[i] == '>') rp = i;
				}
				if ( (lp>=0) && (rp>lp) ) {
					Msg->mailfrom[rp] = 0;
					memmove(Msg->mailfrom,
						&Msg->mailfrom[lp + 1],
						rp - lp);
				}

				scan_done = 1;
			}
		} while (scan_done == 0);
		if (IsEmptyStr(Msg->mailfrom))
			strcpy(Msg->mailfrom, "someone@somewhere.org");

		stripallbut(Msg->mailfrom, '<', '>');
		Msg->envelope_from = Msg->mailfrom;
	}

	return 1;
}
