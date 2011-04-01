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

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
const unsigned short DefaultMXPort = 25;
void DeleteSmtpOutMsg(void *v)
{
	SmtpOutMsg *Msg = v;

	ares_free_data(Msg->AllMX);
	if (Msg->HostLookup.VParsedDNSReply != NULL)
		Msg->HostLookup.DNSReplyFree(Msg->HostLookup.VParsedDNSReply);
	FreeStrBuf(&Msg->msgtext);
	FreeAsyncIOContents(&Msg->IO);
	free(Msg);
}

eNextState SMTP_C_Shutdown(AsyncIO *IO);
eNextState SMTP_C_Timeout(AsyncIO *IO);
eNextState SMTP_C_ConnFail(AsyncIO *IO);
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO);
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO);
eNextState SMTP_C_Terminate(AsyncIO *IO);
eReadState SMTP_C_ReadServerStatus(AsyncIO *IO);

/******************************************************************************
 * So, we're finished with sending (regardless of success or failure)         *
 * This Message might be referenced by several Queue-Items, if we're the last,*
 * we need to free the memory and send bounce messages (on terminal failure)  *
 * else we just free our SMTP-Message struct.                                 *
 ******************************************************************************/
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

eNextState FailOneAttempt(AsyncIO *IO)
{
	/* 
	 * possible ways here: 
	 * - connection timeout 
	 * - 
	 */	
}

////TODO


void SetConnectStatus(AsyncIO *IO)
{
	
	SmtpOutMsg *SendMsg = IO->Data;
	char buf[256];
	void *src;

	buf[0] = '\0';

	if (IO->IP6) {
		src = &IO->Addr->sin6_addr;
	}
	else {
		struct sockaddr_in *addr = (struct sockaddr_in *)IO->Addr;

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

/*****************************************************************************
 * So we connect our Relay IP here.                                          *
 *****************************************************************************/
eNextState mx_connect_relay_ip(AsyncIO *IO)
{
	/*
	SmtpOutMsg *SendMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	IO->IP6 = SendMsg->pCurrRelay->af == AF_INET6;
	
	if (SendMsg->pCurrRelay->Port != 0)
		IO->dport = SendMsg->pCurrRelay->Port;

	memset(&IO->Addr, 0, sizeof(struct sockaddr_in6));
	if (IO->IP6) {
		memcpy(&IO->Addr.sin6_addr.s6_addr, 
		       &SendMsg->pCurrRelay->Addr,
		       sizeof(struct in6_addr));

		IO->Addr.sin6_family = AF_INET6;
		IO->Addr.sin6_port   = htons(IO->dport);
	}
	else {
		struct sockaddr_in *addr = (struct sockaddr_in*) &IO->Addr;
		/*  Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); * /
		memcpy(&addr->sin_addr,///.s_addr, 
		       &SendMsg->pCurrRelay->Addr,
		       sizeof(struct in_addr));
		
		addr->sin_family = AF_INET;
		addr->sin_port   = htons(IO->dport);
	}

	SetConnectStatus(IO);

	return InitEventIO(IO, SendMsg, 
			   SMTP_C_ConnTimeout, 
			   SMTP_C_ReadTimeouts[0],
			   1);
		*/
}

eNextState get_one_mx_host_ip_done(AsyncIO *IO)
{
	SmtpOutMsg *SendMsg = IO->Data;
	struct hostent *hostent;

	QueryCbDone(IO);

	hostent = SendMsg->HostLookup.VParsedDNSReply;
	if ((SendMsg->HostLookup.DNSStatus == ARES_SUCCESS) && 
	    (hostent != NULL) ) {
		
///		IO->IP6  = hostent->h_addrtype == AF_INET6;
		////IO->HEnt = hostent;
		
///		SendMsg->pCurrRelay->Addr



		memset(&SendMsg->pCurrRelay->Addr, 0, sizeof(struct in6_addr));
		if (IO->IP6) {
			memcpy(&SendMsg->pCurrRelay->Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));
			
			SendMsg->pCurrRelay->Addr.sin6_family = hostent->h_addrtype;
			SendMsg->pCurrRelay->Addr.sin6_port   = htons(IO->dport);
		}
		else {
			struct sockaddr_in *addr = (struct sockaddr_in*) &SendMsg->pCurrRelay->Addr;
			/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
//			addr->sin_addr.s_addr = htonl((uint32_t)&hostent->h_addr_list[0]);
			memcpy(&addr->sin_addr.s_addr, 
			       hostent->h_addr_list[0], 
			       sizeof(uint32_t));
			
			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(IO->dport);
			
		}
		////SendMsg->IO.HEnt = hostent;
		SendMsg->IO.Addr = &SendMsg->pCurrRelay->Addr;
		SetConnectStatus(IO);
		return InitEventIO(IO, 
				   SendMsg, 
				   SMTP_C_ConnTimeout, 
				   SMTP_C_ReadTimeouts[0],
				   1);
	}
	else // TODO: here we need to find out whether there are more mx'es, backup relay, and so on
		return SMTP_C_Terminate(IO);
}

/*

	if (SendMsg->pCurrRelay != NULL) {
		SendMsg->mx_host = Hostname = SendMsg->pCurrRelay->Host;
		if (SendMsg->pCurrRelay->Port != 0)
			SendMsg->IO.dport = SendMsg->pCurrRelay->Port;
	}
       	else
*/
eNextState get_one_mx_host_ip(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;
	const char *Hostname;
	//char *endpart;
	//char buf[SIZ];

	/* 
	 * here we start with the lookup of one host. it might be...
	 * - the relay host *sigh*
	 * - the direct hostname if there was no mx record
	 * - one of the mx'es
	 */ 

	InitC_ares_dns(IO);

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	CtdlLogPrintf(CTDL_DEBUG, 
		      "SMTP client[%ld]: looking up %s : %d ...\n", 
		      SendMsg->n, 
		      SendMsg->pCurrRelay->Host, 
		      SendMsg->IO.dport);

	if (!QueueQuery((SendMsg->pCurrRelay->IPv6)? ns_t_aaaa : ns_t_a, 
			SendMsg->pCurrRelay->Host, 
			&SendMsg->IO, 
			&SendMsg->HostLookup, 
			get_one_mx_host_ip_done))
	{
		SendMsg->MyQEntry->Status = 5;
		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
			     "No MX hosts found for <%s>", SendMsg->node);
		return IO->NextState;
	}
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

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	SendMsg->pCurrRelay = SendMsg->Relay;
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
				p->Port = 25; //// TODO define.
				p->IPv6 = i == 1;
				p->Host = SendMsg->CurrMX->host;
				
				*pp = p;
				pp = &p;
			}
			SendMsg->CurrMX    = SendMsg->CurrMX->next;
		}
//		SendMsg->mx_host   = SendMsg->CurrMX->host;
//		SendMsg->CurrMX    = SendMsg->CurrMX->next;
		SendMsg->CXFlags   = SendMsg->CXFlags & F_HAVE_MX;
	}
	else { /* else fall back to the plain hostname */
		int i;
		for (i = 0; i < 2; i++) {
			ParsedURL *p;

			p = (ParsedURL*) malloc(sizeof(ParsedURL));
			memset(p, 0, sizeof(ParsedURL));
			p->IsIP = 0;
			p->Port = 25; //// TODO define.
			p->IPv6 = i == 1;
			p->Host = SendMsg->node;
				
			*pp = p;
			pp = &p;
		}
		///	SendMsg->mx_host   = SendMsg->node;
		SendMsg->CXFlags   = SendMsg->CXFlags & F_DIRECT;
	}
	(*pp)->Next = SendMsg->MyQItem->FallBackHost;
	return get_one_mx_host_ip(IO);
}

eNextState resolve_mx_records(AsyncIO *IO)
{
	SmtpOutMsg * SendMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
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
	return eAbort;
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
	SendMsg->IO.Timeout       = SMTP_C_Timeout;
	SendMsg->IO.ShutdownAbort = SMTP_C_Shutdown;

	SendMsg->IO.SendBuf.Buf   = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.RecvBuf.Buf   = NewStrBufPlain(NULL, 1024);
	SendMsg->IO.IOBuf         = NewStrBuf();

	SendMsg->IO.sock          = (-1);
	SendMsg->IO.NextState     = eReadMessage;
	SendMsg->IO.dport         = DefaultMXPort;

	return SendMsg;
}

void smtp_try_one_queue_entry(OneQueItem *MyQItem, 
			      MailQEntry *MyQEntry, 
			      StrBuf *MsgText, 
			      int KeepMsgText,  /* KeepMsgText allows us to use MsgText as ours. */
			      int MsgCount)
{
	SmtpOutMsg *SendMsg;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);

	SendMsg = new_smtp_outmsg(MyQItem, MyQEntry, MsgCount);
	if (KeepMsgText) SendMsg->msgtext = MsgText;
	else 		 SendMsg->msgtext = NewStrBufDup(MsgText);
	
	if (smtp_resolve_recipients(SendMsg)) {
		if (SendMsg->pCurrRelay == NULL)
			QueueEventContext(&SendMsg->IO,
					  resolve_mx_records);
		else { /* oh... via relay host */
			if (SendMsg->pCurrRelay->IsIP) {
				QueueEventContext(&SendMsg->IO,
						  mx_connect_relay_ip);
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
	SmtpOutMsg *pMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	FinalizeMessageSend(pMsg);
	return eAbort;
}
eNextState SMTP_C_Timeout(AsyncIO *IO)
{
	SmtpOutMsg *pMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	return FailOneAttempt(IO);
}
eNextState SMTP_C_ConnFail(AsyncIO *IO)
{
	SmtpOutMsg *pMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(ReadErrors[pMsg->State]));
	return FailOneAttempt(IO);
}
eNextState SMTP_C_Shutdown(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP: %s\n", __FUNCTION__);
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

#endif
CTDL_MODULE_INIT(smtp_eventclient)
{
	return "smtpeventclient";
}
