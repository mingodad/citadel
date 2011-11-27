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

const unsigned short DefaultMXPort = 25;
void DeleteSmtpOutMsg(void *v)
{
	SmtpOutMsg *Msg = v;

	ares_free_data(Msg->AllMX);
	if (Msg->HostLookup.VParsedDNSReply != NULL)
		Msg->HostLookup.DNSReplyFree(Msg->HostLookup.VParsedDNSReply);
	FreeURL(&Msg->Relay);
	FreeStrBuf(&Msg->msgtext);
	FreeAsyncIOContents(&Msg->IO);
	memset (Msg, 0, sizeof(SmtpOutMsg)); /* just to be shure... */
	free(Msg);
}

eNextState SMTP_C_Shutdown(AsyncIO *IO);
eNextState SMTP_C_Timeout(AsyncIO *IO);
eNextState SMTP_C_ConnFail(AsyncIO *IO);
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO);
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO);
eNextState SMTP_C_DNSFail(AsyncIO *IO);
eNextState SMTP_C_Terminate(AsyncIO *IO);
eReadState SMTP_C_ReadServerStatus(AsyncIO *IO);

eNextState mx_connect_ip(AsyncIO *IO);
eNextState get_one_mx_host_ip(AsyncIO *IO);

/******************************************************************************
 * So, we're finished with sending (regardless of success or failure)         *
 * This Message might be referenced by several Queue-Items, if we're the last,*
 * we need to free the memory and send bounce messages (on terminal failure)  *
 * else we just free our SMTP-Message struct.                                 *
 ******************************************************************************/
void FinalizeMessageSend(SmtpOutMsg *Msg)
{
	int IDestructQueItem;
	int nRemain;
	StrBuf *MsgData;
	AsyncIO *IO = &Msg->IO;
	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);

	IDestructQueItem = DecreaseQReference(Msg->MyQItem);

	nRemain = CountActiveQueueEntries(Msg->MyQItem);

	if ((nRemain > 0) || IDestructQueItem)
		MsgData = SerializeQueueItem(Msg->MyQItem);
	else
		MsgData = NULL;

	/*
	 * Uncompleted delivery instructions remain, so delete the old
	 * instructions and replace with the updated ones.
	 */
	EVS_syslog(LOG_DEBUG, "SMTPQD: %ld", Msg->MyQItem->QueMsgID);
	CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->QueMsgID, 1, "");

	if (IDestructQueItem)
		smtpq_do_bounce(Msg->MyQItem,Msg->msgtext);

	if (nRemain > 0)
	{
		struct CtdlMessage *msg;
		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		msg->cm_fields['M'] = SmashStrBuf(&MsgData);
		Msg->MyQItem->QueMsgID =
			CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
		EVS_syslog(LOG_DEBUG, "SMTPQ: %ld", Msg->MyQItem->QueMsgID);
		CtdlFreeMessage(msg);
	}
	else {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM,
				   &Msg->MyQItem->MessageID,
				   1,
				   "");
		FreeStrBuf(&MsgData);
	}
	if (IDestructQueItem)
		RemoveQItem(Msg->MyQItem);

	RemoveContext(Msg->IO.CitContext);
	DeleteSmtpOutMsg(Msg);
}

eNextState FailOneAttempt(AsyncIO *IO)
{
	SmtpOutMsg *SendMsg = IO->Data;

	if (SendMsg->MyQEntry->Status == 2)
		return eAbort;

	/* 
	 * possible ways here: 
	 * - connection timeout 
	 * - 
	 */
	StopClientWatchers(IO);

	if (SendMsg->pCurrRelay != NULL)
		SendMsg->pCurrRelay = SendMsg->pCurrRelay->Next;

	if (SendMsg->pCurrRelay == NULL)
		return eAbort;
	if (SendMsg->pCurrRelay->IsIP)
		return mx_connect_ip(IO);
	else
		return get_one_mx_host_ip(IO);
}


void SetConnectStatus(AsyncIO *IO)
{
	
	SmtpOutMsg *SendMsg = IO->Data;
	char buf[256];
	void *src;

	buf[0] = '\0';

	if (IO->ConnectMe->IPv6) {
		src = &IO->ConnectMe->Addr.sin6_addr;
	}
	else {
		struct sockaddr_in *addr = (struct sockaddr_in *)&IO->ConnectMe->Addr;

		src = &addr->sin_addr.s_addr;
	}

	inet_ntop((IO->ConnectMe->IPv6)?AF_INET6:AF_INET,
		  src,
		  buf, 
		  sizeof(buf));

	if (SendMsg->mx_host == NULL)
		SendMsg->mx_host = "<no MX-Record>";

	EVS_syslog(LOG_DEBUG,
		  "SMTP client[%ld]: connecting to %s [%s]:%d ...\n", 
		  SendMsg->n, 
		  SendMsg->mx_host, 
		  buf,
		  SendMsg->IO.ConnectMe->Port);

	SendMsg->MyQEntry->Status = 5; 
	StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
		     "Timeout while connecting %s [%s]:%d ", 
		     SendMsg->mx_host,
		     buf,
		     SendMsg->IO.ConnectMe->Port);
	SendMsg->IO.NextState = eConnect;
}

/*****************************************************************************
 * So we connect our Relay IP here.                                          *
 *****************************************************************************/
eNextState mx_connect_ip(AsyncIO *IO)
{
	SmtpOutMsg *SendMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	
	IO->ConnectMe = SendMsg->pCurrRelay;
	/*  Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */

	SetConnectStatus(IO);

	return InitEventIO(IO, SendMsg, 
			   SMTP_C_ConnTimeout, 
			   SMTP_C_ReadTimeouts[0],
			   1);
}

eNextState get_one_mx_host_ip_done(AsyncIO *IO)
{
	SmtpOutMsg *SendMsg = IO->Data;
	struct hostent *hostent;

	QueryCbDone(IO);

	hostent = SendMsg->HostLookup.VParsedDNSReply;
	if ((SendMsg->HostLookup.DNSStatus == ARES_SUCCESS) && 
	    (hostent != NULL) ) {
		memset(&SendMsg->pCurrRelay->Addr, 0, sizeof(struct in6_addr));
		if (SendMsg->pCurrRelay->IPv6) {
			memcpy(&SendMsg->pCurrRelay->Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));
			
			SendMsg->pCurrRelay->Addr.sin6_family = hostent->h_addrtype;
			SendMsg->pCurrRelay->Addr.sin6_port   = htons(DefaultMXPort);
		}
		else {
			struct sockaddr_in *addr = (struct sockaddr_in*) &SendMsg->pCurrRelay->Addr;
			/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
//			addr->sin_addr.s_addr = htonl((uint32_t)&hostent->h_addr_list[0]);
			memcpy(&addr->sin_addr.s_addr, 
			       hostent->h_addr_list[0], 
			       sizeof(uint32_t));
			
			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(DefaultMXPort);
			
		}
		SendMsg->mx_host = SendMsg->pCurrRelay->Host;
		return mx_connect_ip(IO);
	}
	else // TODO: here we need to find out whether there are more mx'es, backup relay, and so on
		return FailOneAttempt(IO);
}

eNextState get_one_mx_host_ip(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;
	/* 
	 * here we start with the lookup of one host. it might be...
	 * - the relay host *sigh*
	 * - the direct hostname if there was no mx record
	 * - one of the mx'es
	 */ 

	InitC_ares_dns(IO);

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);

	EVS_syslog(LOG_DEBUG, 
		  "SMTP client[%ld]: looking up %s-Record %s : %d ...\n", 
		  SendMsg->n, 
		  (SendMsg->pCurrRelay->IPv6)? "aaaa": "a",
		  SendMsg->pCurrRelay->Host, 
		  SendMsg->pCurrRelay->Port);

	if (!QueueQuery((SendMsg->pCurrRelay->IPv6)? ns_t_aaaa : ns_t_a, 
			SendMsg->pCurrRelay->Host, 
			&SendMsg->IO, 
			&SendMsg->HostLookup, 
			get_one_mx_host_ip_done))
	{
		SendMsg->MyQEntry->Status = 5;
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "No MX hosts found for <%s>", SendMsg->node);
		SendMsg->IO.NextState = eTerminateConnection;
		return IO->NextState;
	}
	IO->NextState = eReadDNSReply;
	return IO->NextState;
}


/*****************************************************************************
 * here we try to find out about the MX records for our recipients.          *
 *****************************************************************************/
eNextState smtp_resolve_mx_record_done(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;
	ParsedURL **pp;

	QueryCbDone(IO);

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	pp = &SendMsg->Relay;
	while ((pp != NULL) && (*pp != NULL) && ((*pp)->Next != NULL))
		pp = &(*pp)->Next;

	if ((IO->DNSQuery->DNSStatus == ARES_SUCCESS) && 
	    (IO->DNSQuery->VParsedDNSReply != NULL))
	{ /* ok, we found mx records. */
		SendMsg->IO.ErrMsg = SendMsg->MyQEntry->StatusMessage;
		
		SendMsg->CurrMX    = SendMsg->AllMX 
			           = IO->DNSQuery->VParsedDNSReply;
		while (SendMsg->CurrMX) {
			int i;
			for (i = 0; i < 2; i++) {
				ParsedURL *p;

				p = (ParsedURL*) malloc(sizeof(ParsedURL));
				memset(p, 0, sizeof(ParsedURL));
				p->IsIP = 0;
				p->Port = DefaultMXPort;
				p->IPv6 = i == 1;
				p->Host = SendMsg->CurrMX->host;
				
				*pp = p;
				pp = &p->Next;
			}
			SendMsg->CurrMX    = SendMsg->CurrMX->next;
		}
		SendMsg->CXFlags   = SendMsg->CXFlags & F_HAVE_MX;
	}
	else { /* else fall back to the plain hostname */
		int i;
		for (i = 0; i < 2; i++) {
			ParsedURL *p;

			p = (ParsedURL*) malloc(sizeof(ParsedURL));
			memset(p, 0, sizeof(ParsedURL));
			p->IsIP = 0;
			p->Port = DefaultMXPort;
			p->IPv6 = i == 1;
			p->Host = SendMsg->node;
				
			*pp = p;
			pp = &p->Next;
		}
		SendMsg->CXFlags   = SendMsg->CXFlags & F_DIRECT;
	}
	*pp = SendMsg->MyQItem->FallBackHost;
	SendMsg->pCurrRelay = SendMsg->Relay;
	return get_one_mx_host_ip(IO);
}

eNextState resolve_mx_records(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	/* start resolving MX records here. */
	if (!QueueQuery(ns_t_mx, 
			SendMsg->node, 
			&SendMsg->IO, 
			&SendMsg->MxLookup, 
			smtp_resolve_mx_record_done))
	{
		SendMsg->MyQEntry->Status = 5;
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "No MX hosts found for <%s>", SendMsg->node);
		return IO->NextState;
	}
	SendMsg->IO.NextState = eReadDNSReply;
	return IO->NextState;
}



/******************************************************************************
 *  so, we're going to start a SMTP delivery.  lets get it on.                *
 ******************************************************************************/

SmtpOutMsg *new_smtp_outmsg(OneQueItem *MyQItem, 
			    MailQEntry *MyQEntry, 
			    int MsgCount)
{
	SmtpOutMsg * SendMsg;

	SendMsg = (SmtpOutMsg *) malloc(sizeof(SmtpOutMsg));
	memset(SendMsg, 0, sizeof(SmtpOutMsg));

	SendMsg->n                = MsgCount;
	SendMsg->MyQEntry         = MyQEntry;
	SendMsg->MyQItem          = MyQItem;
	SendMsg->pCurrRelay       = MyQItem->URL;

	SendMsg->IO.Data          = SendMsg;

	SendMsg->IO.SendDone      = SMTP_C_DispatchWriteDone;
	SendMsg->IO.ReadDone      = SMTP_C_DispatchReadDone;
	SendMsg->IO.Terminate     = SMTP_C_Terminate;
	SendMsg->IO.LineReader    = SMTP_C_ReadServerStatus;
	SendMsg->IO.ConnFail      = SMTP_C_ConnFail;
	SendMsg->IO.DNSFail       = SMTP_C_DNSFail;
	SendMsg->IO.Timeout       = SMTP_C_Timeout;
	SendMsg->IO.ShutdownAbort = SMTP_C_Shutdown;

	SendMsg->IO.SendBuf.Buf   = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf   = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf         = NewStrBuf();

	SendMsg->IO.NextState     = eReadMessage;
	
	return SendMsg;
}

void smtp_try_one_queue_entry(OneQueItem *MyQItem, 
			      MailQEntry *MyQEntry, 
			      StrBuf *MsgText, 
			      int KeepMsgText,  /* KeepMsgText allows us to use MsgText as ours. */
			      int MsgCount)
{
	SmtpOutMsg *SendMsg;

	syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);

	SendMsg = new_smtp_outmsg(MyQItem, MyQEntry, MsgCount);
	if (KeepMsgText) SendMsg->msgtext = MsgText;
	else 		 SendMsg->msgtext = NewStrBufDup(MsgText);
	
	if (smtp_resolve_recipients(SendMsg)) {
		CitContext *SubC;
		SubC = CloneContext (CC);
		SubC->session_specific_data = (char*) SendMsg;
		SendMsg->IO.CitContext = SubC;

		syslog(LOG_DEBUG, "SMTP Starting: [%ld] <%s> \n",
		       SendMsg->MyQItem->MessageID, 
		       ChrPtr(SendMsg->MyQEntry->Recipient));
		if (SendMsg->pCurrRelay == NULL)
			QueueEventContext(&SendMsg->IO,
					  resolve_mx_records);
		else { /* oh... via relay host */
			if (SendMsg->pCurrRelay->IsIP) {
				QueueEventContext(&SendMsg->IO,
						  mx_connect_ip);
			}
			else { /* uneducated admin has chosen to add DNS to the equation... */
				QueueEventContext(&SendMsg->IO,
						  get_one_mx_host_ip);
			}
		}
	}
	else {
		/* No recipients? well fail then. */
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
/*                     SMTP CLIENT DISPATCHER                                */
/*****************************************************************************/

void SMTPSetTimeout(eNextState NextTCPState, SmtpOutMsg *pMsg)
{
	double Timeout = 0.0;
	AsyncIO *IO = &pMsg->IO;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);

	switch (NextTCPState) {
	case eSendFile:
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
	case eSendDNSQuery:
	case eReadDNSReply:
	case eDBQuery:
	case eReadFile:
	case eReadMore:
	case eReadPayload:
	case eConnect:
	case eTerminateConnection:
	case eAbort:
		return;
	}
	SetNextTimeout(&pMsg->IO, Timeout);
}
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;
	eNextState rc;

	rc = ReadHandlers[pMsg->State](pMsg);
	if (rc != eAbort)
	{
		pMsg->State++;
		SMTPSetTimeout(rc, pMsg);
	}
	return rc;
}
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
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
	SmtpOutMsg *pMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	FinalizeMessageSend(pMsg);
	return eAbort;
}
eNextState SMTP_C_Timeout(AsyncIO *IO)
{
	SmtpOutMsg *pMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	return FailOneAttempt(IO);
}
eNextState SMTP_C_ConnFail(AsyncIO *IO)
{
	SmtpOutMsg *pMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	return FailOneAttempt(IO);
}
eNextState SMTP_C_DNSFail(AsyncIO *IO)
{
	SmtpOutMsg *pMsg = IO->Data;

	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	return FailOneAttempt(IO);
}
eNextState SMTP_C_Shutdown(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SmtpOutMsg *pMsg = IO->Data;

	pMsg->MyQEntry->Status = 3;
	StrBufPlain(pMsg->MyQEntry->StatusMessage, HKEY("server shutdown during message submit."));
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

CTDL_MODULE_INIT(smtp_eventclient)
{
	return "smtpeventclient";
}
