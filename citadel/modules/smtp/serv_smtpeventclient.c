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

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
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

static const ConstStr ReadErrors[eMaxSMTPC] = {
	{HKEY("Connection broken during SMTP conversation")},
	{HKEY("Connection broken during SMTP EHLO")},
	{HKEY("Connection broken during SMTP HELO")},
	{HKEY("Connection broken during SMTP AUTH")},
	{HKEY("Connection broken during SMTP MAIL FROM")},
	{HKEY("Connection broken during SMTP RCPT")},
	{HKEY("Connection broken during SMTP DATA")},
	{HKEY("Connection broken during SMTP message transmit")},
	{HKEY("")}/* quit reply, don't care. */
};


typedef struct _stmp_out_msg {
	MailQEntry *MyQEntry;
	OneQueItem *MyQItem;
	long n;
	AsyncIO IO;

	eSMTP_C_States State;

	struct ares_mx_reply *AllMX;
	struct ares_mx_reply *CurrMX;
	const char *mx_port;
	const char *mx_host;

	struct hostent *OneMX;

	ParsedURL *Relay;
	ParsedURL *pCurrRelay;
	StrBuf *msgtext;
	char *envelope_from;
	char user[1024];
	char node[1024];
	char name[1024];
	char mailfrom[1024];
} SmtpOutMsg;

void DeleteSmtpOutMsg(void *v)
{
	SmtpOutMsg *Msg = v;

	ares_free_data(Msg->AllMX);
	
	FreeStrBuf(&Msg->msgtext);
	FreeAsyncIOContents(&Msg->IO);
	free(Msg);
}

eNextState SMTP_C_Timeout(AsyncIO *IO);
eNextState SMTP_C_ConnFail(AsyncIO *IO);
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO);
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO);
eNextState SMTP_C_Terminate(AsyncIO *IO);
eReadState SMTP_C_ReadServerStatus(AsyncIO *IO);

typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);


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

#define SMTP_DBG_SEND() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: > %s\n", SendMsg->n, ChrPtr(SendMsg->IO.SendBuf.Buf))
#define SMTP_DBG_READ() CtdlLogPrintf(CTDL_DEBUG, "SMTP client[%ld]: < %s\n", SendMsg->n, ChrPtr(SendMsg->IO.IOBuf))


void FinalizeMessageSend(SmtpOutMsg *Msg)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	
	if (DecreaseQReference(Msg->MyQItem)) 
	{
		int nRemain;
		StrBuf *MsgData;

		nRemain = CountActiveQueueEntries(Msg->MyQItem);

		MsgData = SerializeQueueItem(Msg->MyQItem);
		/*
		 * Uncompleted delivery instructions remain, so delete the old
		 * instructions and replace with the updated ones.
		 */
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->QueMsgID, 1, "");
		smtpq_do_bounce(Msg->MyQItem,
			       Msg->msgtext); 
		if (nRemain > 0) {
			struct CtdlMessage *msg;
			msg = malloc(sizeof(struct CtdlMessage));
			memset(msg, 0, sizeof(struct CtdlMessage));
			msg->cm_magic = CTDLMESSAGE_MAGIC;
			msg->cm_anon_type = MES_NORMAL;
			msg->cm_format_type = FMT_RFC822;
			msg->cm_fields['M'] = SmashStrBuf(&MsgData);
			CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
			CtdlFreeMessage(msg);
		}
		else {
			CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->MessageID, 1, "");
			FreeStrBuf(&MsgData);
		}

		RemoveQItem(Msg->MyQItem);
	}
	DeleteSmtpOutMsg(Msg);
}


void SetConnectStatus(AsyncIO *IO)
{
	
	SmtpOutMsg *SendMsg = IO->Data;
	char buf[256];
	void *src;

	buf[0] = '\0';

	if (IO->IP6) {
		src = &IO->Addr.sin6_addr;
	}
	else {
		struct sockaddr_in *addr = (struct sockaddr_in *)&IO->Addr;

		src = &addr->sin_addr.s_addr;
	}

	inet_ntop((IO->IP6)?AF_INET6:AF_INET,
		  src,
		  buf, sizeof(buf));
	if (SendMsg->mx_host == NULL)
		SendMsg->mx_host = "<no name>";

	CtdlLogPrintf(CTDL_DEBUG, 
		      "SMTP client[%ld]: connecting to %s [%s]:%d ...\n", 
		      SendMsg->n, 
		      SendMsg->mx_host, 
		      buf,
		      SendMsg->IO.dport);

	SendMsg->MyQEntry->Status = 5; 
	StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
		     "Timeout while connecting %s [%s]:%d ", 
		     SendMsg->mx_host,
		     buf,
		     SendMsg->IO.dport);
}

eNextState mx_connect_relay_ip(AsyncIO *IO)
{
	
	SmtpOutMsg *SendMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	IO->IP6 = SendMsg->pCurrRelay->af == AF_INET6;
	
	if (SendMsg->pCurrRelay->Port != 0)
		IO->dport = SendMsg->pCurrRelay->Port;

	memset(&IO->Addr, 0, sizeof(struct in6_addr));
	if (IO->IP6) {
		memcpy(&IO->Addr.sin6_addr.s6_addr, 
		       &SendMsg->pCurrRelay->Addr,
		       sizeof(struct in6_addr));
		
		IO->Addr.sin6_family = AF_INET6;
		IO->Addr.sin6_port = htons(IO->dport);
	}
	else {
		struct sockaddr_in *addr = (struct sockaddr_in*) &IO->Addr;
		/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
		memcpy(&addr->sin_addr,///.s_addr, 
		       &SendMsg->pCurrRelay->Addr,
		       sizeof(struct in_addr));
		
		addr->sin_family = AF_INET;
		addr->sin_port = htons(IO->dport);
	}

	SetConnectStatus(IO);

	return InitEventIO(IO, SendMsg, 
			   SMTP_C_ConnTimeout, 
			   SMTP_C_ReadTimeouts[0],
			    1);
}

void get_one_mx_host_ip_done(void *Ctx, 
			     int status,
			     int timeouts,
			     struct hostent *hostent)
{
	AsyncIO *IO = (AsyncIO *) Ctx;
	SmtpOutMsg *SendMsg = IO->Data;
	eNextState State = eAbort;

	if ((status == ARES_SUCCESS) && (hostent != NULL) ) {
		
		IO->IP6  = hostent->h_addrtype == AF_INET6;
		IO->HEnt = hostent;

		memset(&IO->Addr, 0, sizeof(struct in6_addr));
		if (IO->IP6) {
			memcpy(&IO->Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));
			
			IO->Addr.sin6_family = hostent->h_addrtype;
			IO->Addr.sin6_port = htons(IO->dport);
		}
		else {
			struct sockaddr_in *addr = (struct sockaddr_in*) &IO->Addr;
			/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
//			addr->sin_addr.s_addr = htonl((uint32_t)&hostent->h_addr_list[0]);
			memcpy(&addr->sin_addr.s_addr, hostent->h_addr_list[0], sizeof(uint32_t));
			
			addr->sin_family = hostent->h_addrtype;
			addr->sin_port = htons(IO->dport);
			
		}
		SendMsg->IO.HEnt = hostent;
		SetConnectStatus(IO);
		State = InitEventIO(IO, SendMsg, 
				   SMTP_C_ConnTimeout, 
				   SMTP_C_ReadTimeouts[0],
				   1);
	}
	if ((State == eAbort) && (IO->sock != -1))
		SMTP_C_Terminate(IO);
}

const unsigned short DefaultMXPort = 25;
eNextState get_one_mx_host_ip(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;
	const char *Hostname;
	//char *endpart;
	//char buf[SIZ];
	InitC_ares_dns(IO);

	if (SendMsg->CurrMX) {
		SendMsg->mx_host = SendMsg->CurrMX->host;
		SendMsg->CurrMX = SendMsg->CurrMX->next;
	}

	if (SendMsg->pCurrRelay != NULL) {
		SendMsg->mx_host = Hostname = SendMsg->pCurrRelay->Host;
		if (SendMsg->pCurrRelay->Port != 0)
			SendMsg->IO.dport = SendMsg->pCurrRelay->Port;
	}
       	else if (SendMsg->mx_host != NULL) Hostname = SendMsg->mx_host;
	else Hostname = SendMsg->node;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	CtdlLogPrintf(CTDL_DEBUG, 
		      "SMTP client[%ld]: looking up %s : %d ...\n", 
		      SendMsg->n, 
		      Hostname, 
		      SendMsg->IO.dport);

	ares_gethostbyname(SendMsg->IO.DNSChannel,
			   Hostname,   
			   AF_INET6, /* it falls back to ipv4 in doubt... */
			   get_one_mx_host_ip_done,
			   &SendMsg->IO);
	return IO->NextState;
}


eNextState smtp_resolve_mx_done(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;

	SendMsg->CurrMX = SendMsg->AllMX = IO->VParsedDNSReply;
	//// TODO: should we remove the current ares context???
	get_one_mx_host_ip(IO);
	return IO->NextState;
}


eNextState resolve_mx_records(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	if (!QueueQuery(ns_t_mx, 
			SendMsg->node, 
			&SendMsg->IO, 
			smtp_resolve_mx_done))
	{
		SendMsg->MyQEntry->Status = 5;
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "No MX hosts found for <%s>", SendMsg->node);
		return IO->NextState;
	}
	return eAbort;
}


int smtp_resolve_recipients(SmtpOutMsg *SendMsg)
{
	const char *ptr;
	char buf[1024];
	int scan_done;
	int lp, rp;
	int i;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

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

	return 1;
}



void smtp_try(OneQueItem *MyQItem, 
	      MailQEntry *MyQEntry, 
	      StrBuf *MsgText, 
	      int KeepMsgText,  /* KeepMsgText allows us to use MsgText as ours. */
	      int MsgCount)
{
	SmtpOutMsg * SendMsg;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	SendMsg = (SmtpOutMsg *) malloc(sizeof(SmtpOutMsg));
	memset(SendMsg, 0, sizeof(SmtpOutMsg));
	SendMsg->IO.sock      = (-1);
	SendMsg->IO.NextState = eReadMessage;
	SendMsg->n            = MsgCount++;
	SendMsg->MyQEntry     = MyQEntry;
	SendMsg->MyQItem      = MyQItem;
	SendMsg->pCurrRelay   = MyQItem->URL;

	SendMsg->IO.dport       = DefaultMXPort;
	SendMsg->IO.Data        = SendMsg;
	SendMsg->IO.SendDone    = SMTP_C_DispatchWriteDone;
	SendMsg->IO.ReadDone    = SMTP_C_DispatchReadDone;
	SendMsg->IO.Terminate   = SMTP_C_Terminate;
	SendMsg->IO.LineReader  = SMTP_C_ReadServerStatus;
	SendMsg->IO.ConnFail    = SMTP_C_ConnFail;
	SendMsg->IO.Timeout     = SMTP_C_Timeout;
	SendMsg->IO.SendBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf       = NewStrBuf();

	if (KeepMsgText) {
		SendMsg->msgtext    = MsgText;
	}
	else {
		SendMsg->msgtext = NewStrBufDup(MsgText);
	}

	if (smtp_resolve_recipients(SendMsg)) {
		if (SendMsg->pCurrRelay == NULL)
			QueueEventContext(&SendMsg->IO,
					  resolve_mx_records);
		else {
			if (SendMsg->pCurrRelay->IsIP) {
				QueueEventContext(&SendMsg->IO,
						  mx_connect_relay_ip);
			}
			else {
				QueueEventContext(&SendMsg->IO,
						  get_one_mx_host_ip);
			}
		}
	}
	else {
		if ((SendMsg==NULL) || 
		    (SendMsg->MyQEntry == NULL)) {
			SendMsg->MyQEntry->Status = 5;
			StrBufPlain(SendMsg->MyQEntry->StatusMessage, 
				    HKEY("Invalid Recipient!"));
		}
		FinalizeMessageSend(SendMsg);
	}
}





/*****************************************************************************/
/*                     SMTP CLIENT STATE CALLBACKS                           */
/*****************************************************************************/
eNextState SMTPC_read_greeting(SmtpOutMsg *SendMsg)
{
	/* Process the SMTP greeting from the server */
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

		if ((SendMsg->pCurrRelay == NULL) || 
		    (SendMsg->pCurrRelay->User == NULL))
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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
			SMTP_VERROR(4);
		else 
			SMTP_VERROR(5);
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

void SMTPSetTimeout(eNextState NextTCPState, SmtpOutMsg *pMsg)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	double Timeout;
	switch (NextTCPState) {
	case eSendReply:
	case eSendMore:
		Timeout = SMTP_C_SendTimeouts[pMsg->State];
		if (pMsg->State == eDATABody) {
			/* if we're sending a huge message, we need more time. */
			Timeout += StrLength(pMsg->msgtext) / 1024;
		}
		break;
	case eReadMessage:
		Timeout = SMTP_C_ReadTimeouts[pMsg->State];
		if (pMsg->State == eDATATerminateBody) {
			/* 
			 * some mailservers take a nap before accepting the message
			 * content inspection and such.
			 */
			Timeout += StrLength(pMsg->msgtext) / 1024;
		}
		break;
	case eTerminateConnection:
	case eAbort:
		return;
	}
	SetNextTimeout(&pMsg->IO, Timeout);
}
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	eNextState rc;

	rc = ReadHandlers[pMsg->State](pMsg);
	pMsg->State++;
	SMTPSetTimeout(rc, pMsg);
	return rc;
}
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	eNextState rc;

	rc = SendHandlers[pMsg->State](pMsg);
	SMTPSetTimeout(rc, pMsg);
	return rc;
}


/*****************************************************************************/
/*                     SMTP CLIENT ERROR CATCHERS                            */
/*****************************************************************************/
eNextState SMTP_C_Terminate(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	FinalizeMessageSend(pMsg);
	return eAbort;
}
eNextState SMTP_C_Timeout(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	FinalizeMessageSend(pMsg);
	return eAbort;
}
eNextState SMTP_C_ConnFail(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	FinalizeMessageSend(pMsg);
	return eAbort;
}


/**
 * @brief lineread Handler; understands when to read more SMTP lines, and when this is a one-lined reply.
 */
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

#endif
CTDL_MODULE_INIT(smtp_eventclient)
{
	return "smtpeventclient";
}
