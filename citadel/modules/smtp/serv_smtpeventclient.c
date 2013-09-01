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
#include "event_client.h"
#include "smtpqueue.h"
#include "smtp_clienthandlers.h"

ConstStr SMTPStates[] = {
	{HKEY("looking up mx - record")},
	{HKEY("evaluating what to do next")},
	{HKEY("looking up a - record")},
	{HKEY("looking up aaaa - record")},
	{HKEY("connecting remote")},
	{HKEY("smtp conversation ongoing")},
	{HKEY("smtp sending maildata")},
	{HKEY("smtp sending done")},
	{HKEY("smtp successfully finished")},
	{HKEY("failed one attempt")},
	{HKEY("failed temporarily")},
	{HKEY("failed permanently")}
};

void SetSMTPState(AsyncIO *IO, smtpstate State)
{
	CitContext* CCC = IO->CitContext;
	if (CCC != NULL)
		memcpy(CCC->cs_clientname, SMTPStates[State].Key, SMTPStates[State].len + 1);
}

int SMTPClientDebugEnabled = 0;
void DeleteSmtpOutMsg(void *v)
{
	SmtpOutMsg *Msg = v;
	AsyncIO *IO = &Msg->IO;
	EV_syslog(LOG_DEBUG, "%s Exit\n", __FUNCTION__);

	/* these are kept in our own space and free'd below */
	Msg->IO.ConnectMe = NULL;

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
eNextState SMTP_C_TerminateDB(AsyncIO *IO);
eReadState SMTP_C_ReadServerStatus(AsyncIO *IO);

eNextState mx_connect_ip(AsyncIO *IO);
eNextState get_one_mx_host_ip(AsyncIO *IO);

/******************************************************************************
 * So, we're finished with sending (regardless of success or failure)         *
 * This Message might be referenced by several Queue-Items, if we're the last,*
 * we need to free the memory and send bounce messages (on terminal failure)  *
 * else we just free our SMTP-Message struct.                                 *
 ******************************************************************************/
eNextState FinalizeMessageSend_DB(AsyncIO *IO)
{
	const char *Status;
	SmtpOutMsg *Msg = IO->Data;
	StrBuf *StatusMessage;

	if (Msg->MyQEntry->AllStatusMessages != NULL)
		StatusMessage = Msg->MyQEntry->AllStatusMessages;
	else
		StatusMessage = Msg->MyQEntry->StatusMessage;


	if (Msg->MyQEntry->Status == 2) {
		SetSMTPState(IO, eSTMPfinished);
		Status = "Delivery successful.";
	}
	else if (Msg->MyQEntry->Status == 5) {
		SetSMTPState(IO, eSMTPFailTotal);
		Status = "Delivery failed permanently; giving up.";
	}
	else {
		SetSMTPState(IO, eSMTPFailTemporary);
		Status = "Delivery failed temporarily; will retry later.";
	}
			
	EVS_syslog(LOG_INFO,
		   "%s Time[%fs] Recipient <%s> @ <%s> (%s) Status message: %s\n",
		   Status,
		   Msg->IO.Now - Msg->IO.StartIO,
		   Msg->user,
		   Msg->node,
		   Msg->name,
		   ChrPtr(StatusMessage));


	Msg->IDestructQueItem = DecreaseQReference(Msg->MyQItem);

	Msg->nRemain = CountActiveQueueEntries(Msg->MyQItem, 0);

	if (Msg->MyQEntry->Active && 
	    !Msg->MyQEntry->StillActive &&
	    CheckQEntryIsBounce(Msg->MyQEntry))
	{
		/* are we casue for a bounce mail? */
		Msg->MyQItem->SendBounceMail |= (1<<Msg->MyQEntry->Status);
	}

	if ((Msg->nRemain > 0) || Msg->IDestructQueItem)
		Msg->QMsgData = SerializeQueueItem(Msg->MyQItem);
	else
		Msg->QMsgData = NULL;

	/*
	 * Uncompleted delivery instructions remain, so delete the old
	 * instructions and replace with the updated ones.
	 */
	EVS_syslog(LOG_DEBUG, "%ld", Msg->MyQItem->QueMsgID);
	CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &Msg->MyQItem->QueMsgID, 1, "");
	Msg->MyQItem->QueMsgID = -1;

	if (Msg->IDestructQueItem)
		smtpq_do_bounce(Msg->MyQItem, Msg->msgtext, Msg->pCurrRelay);

	if (Msg->nRemain > 0)
	{
		struct CtdlMessage *msg;
		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		CM_SetAsFieldSB(msg, eMesageText, &Msg->QMsgData);
		CM_SetField(msg, eMsgSubject, HKEY("QMSG"));
		Msg->MyQItem->QueMsgID =
			CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
		EVS_syslog(LOG_DEBUG, "%ld", Msg->MyQItem->QueMsgID);
		CtdlFreeMessage(msg);
	}
	else {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM,
				   &Msg->MyQItem->MessageID,
				   1,
				   "");
		FreeStrBuf(&Msg->QMsgData);
	}

	RemoveContext(Msg->IO.CitContext);
	return eAbort;
}

eNextState Terminate(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;

	if (Msg->IDestructQueItem)
		RemoveQItem(Msg->MyQItem);

	DeleteSmtpOutMsg(Msg);
	return eAbort;
}
eNextState FinalizeMessageSend(SmtpOutMsg *Msg)
{
	/* hand over to DB Queue */
	return EventQueueDBOperation(&Msg->IO, FinalizeMessageSend_DB);
}

eNextState FailOneAttempt(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;

	SetSMTPState(IO, eSTMPfailOne);
	if (Msg->MyQEntry->Status == 2)
		return eAbort;

	/*
	 * possible ways here:
	 * - connection timeout
	 * - dns lookup failed
	 */
	StopClientWatchers(IO, 1);

	Msg->MyQEntry->nAttempt ++;
	if (Msg->MyQEntry->AllStatusMessages == NULL)
		Msg->MyQEntry->AllStatusMessages = NewStrBuf();

	StrBufAppendPrintf(Msg->MyQEntry->AllStatusMessages, "%ld) ", Msg->MyQEntry->nAttempt);
	StrBufAppendBuf(Msg->MyQEntry->AllStatusMessages, Msg->MyQEntry->StatusMessage, 0);
	StrBufAppendBufPlain(Msg->MyQEntry->AllStatusMessages, HKEY("; "), 0);

	if (Msg->pCurrRelay != NULL)
		Msg->pCurrRelay = Msg->pCurrRelay->Next;
	if ((Msg->pCurrRelay != NULL) &&
	    !Msg->pCurrRelay->IsRelay &&
	    Msg->MyQItem->HaveRelay)
	{
		EVS_syslog(LOG_DEBUG, "%s Aborting; last relay failed.\n", __FUNCTION__);
		return eAbort;
	}

	if (Msg->pCurrRelay == NULL) {
		EVS_syslog(LOG_DEBUG, "%s Aborting\n", __FUNCTION__);
		return eAbort;
	}
	if (Msg->pCurrRelay->IsIP) {
		EVS_syslog(LOG_DEBUG, "%s connecting IP\n", __FUNCTION__);
		return mx_connect_ip(IO);
	}
	else {
		EVS_syslog(LOG_DEBUG,
			   "%s resolving next MX Record\n",
			   __FUNCTION__);
		return get_one_mx_host_ip(IO);
	}
}


void SetConnectStatus(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;
	char buf[256];
	void *src;

	buf[0] = '\0';

	if (IO->ConnectMe->IPv6) {
		src = &IO->ConnectMe->Addr.sin6_addr;
	}
	else {
		struct sockaddr_in *addr;

		addr = (struct sockaddr_in *)&IO->ConnectMe->Addr;
		src = &addr->sin_addr.s_addr;
	}

	inet_ntop((IO->ConnectMe->IPv6)?AF_INET6:AF_INET,
		  src,
		  buf,
		  sizeof(buf));

	if (Msg->mx_host == NULL)
		Msg->mx_host = "<no MX-Record>";

	EVS_syslog(LOG_INFO,
		  "connecting to %s [%s]:%d ...\n",
		  Msg->mx_host,
		  buf,
		  Msg->IO.ConnectMe->Port);

	Msg->MyQEntry->Status = 4;
	StrBufPrintf(Msg->MyQEntry->StatusMessage,
		     "Timeout while connecting %s [%s]:%d ",
		     Msg->mx_host,
		     buf,
		     Msg->IO.ConnectMe->Port);
	Msg->IO.NextState = eConnect;
}

/*****************************************************************************
 * So we connect our Relay IP here.                                          *
 *****************************************************************************/
eNextState mx_connect_ip(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;
	SetSMTPState(IO, eSTMPconnecting);

	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);

	IO->ConnectMe = Msg->pCurrRelay;
	Msg->State = eConnectMX;

	SetConnectStatus(IO);

	return EvConnectSock(IO,
			     SMTP_C_ConnTimeout,
			     SMTP_C_ReadTimeouts[0],
			     1);
}

eNextState get_one_mx_host_ip_done(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;
	struct hostent *hostent;

	IO->ConnectMe = Msg->pCurrRelay;

	QueryCbDone(IO);
	EVS_syslog(LOG_DEBUG, "%s Time[%fs]\n",
		   __FUNCTION__,
		   IO->Now - IO->DNS.Start);

	hostent = Msg->HostLookup.VParsedDNSReply;
	if ((Msg->HostLookup.DNSStatus == ARES_SUCCESS) &&
	    (hostent != NULL) ) {
		memset(&Msg->pCurrRelay->Addr, 0, sizeof(struct in6_addr));
		if (Msg->pCurrRelay->IPv6) {
			memcpy(&Msg->pCurrRelay->Addr.sin6_addr.s6_addr,
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));

			Msg->pCurrRelay->Addr.sin6_family =
				hostent->h_addrtype;
			Msg->pCurrRelay->Addr.sin6_port =
				htons(Msg->IO.ConnectMe->Port);
		}
		else {
			struct sockaddr_in *addr;
			/*
			 * Bypass the ns lookup result like this:
			 * IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1");
			 * addr->sin_addr.s_addr =
			 *   htonl((uint32_t)&hostent->h_addr_list[0]);
			 */

			addr = (struct sockaddr_in*) &Msg->pCurrRelay->Addr;

			memcpy(&addr->sin_addr.s_addr,
			       hostent->h_addr_list[0],
			       sizeof(uint32_t));

			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(Msg->IO.ConnectMe->Port);
		}
		Msg->mx_host = Msg->pCurrRelay->Host;
		if (Msg->HostLookup.VParsedDNSReply != NULL) {
			Msg->HostLookup.DNSReplyFree(Msg->HostLookup.VParsedDNSReply);
			Msg->HostLookup.VParsedDNSReply = NULL;
		}
		return mx_connect_ip(IO);
	}
	else {
		SetSMTPState(IO, eSTMPfailOne);
		if (Msg->HostLookup.VParsedDNSReply != NULL) {
			Msg->HostLookup.DNSReplyFree(Msg->HostLookup.VParsedDNSReply);
			Msg->HostLookup.VParsedDNSReply = NULL;
		}
		return FailOneAttempt(IO);
	}
}

eNextState get_one_mx_host_ip(AsyncIO *IO)
{
	SmtpOutMsg * Msg = IO->Data;
	/*
	 * here we start with the lookup of one host. it might be...
	 * - the relay host *sigh*
	 * - the direct hostname if there was no mx record
	 * - one of the mx'es
	 */
	SetSMTPState(IO, (Msg->pCurrRelay->IPv6)?eSTMPalookup:eSTMPaaaalookup);

	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);

	EVS_syslog(LOG_DEBUG,
		  "looking up %s-Record %s : %d ...\n",
		  (Msg->pCurrRelay->IPv6)? "aaaa": "a",
		  Msg->pCurrRelay->Host,
		  Msg->pCurrRelay->Port);

	if (!QueueQuery((Msg->pCurrRelay->IPv6)? ns_t_aaaa : ns_t_a,
			Msg->pCurrRelay->Host,
			&Msg->IO,
			&Msg->HostLookup,
			get_one_mx_host_ip_done))
	{
		Msg->MyQEntry->Status = 5;
		StrBufPrintf(Msg->MyQEntry->StatusMessage,
			     "No MX hosts found for <%s>", Msg->node);
		Msg->IO.NextState = eTerminateConnection;
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
	SmtpOutMsg * Msg = IO->Data;
	ParsedURL **pp;

	QueryCbDone(IO);

	EVS_syslog(LOG_DEBUG, "%s Time[%fs]\n",
		   __FUNCTION__,
		   IO->Now - IO->DNS.Start);

	pp = &Msg->Relay;
	while ((pp != NULL) && (*pp != NULL) && ((*pp)->Next != NULL))
		pp = &(*pp)->Next;

	if ((IO->DNS.Query->DNSStatus == ARES_SUCCESS) &&
	    (IO->DNS.Query->VParsedDNSReply != NULL))
	{ /* ok, we found mx records. */

		Msg->CurrMX
			= Msg->AllMX
			= IO->DNS.Query->VParsedDNSReply;
		while (Msg->CurrMX) {
			int i;
			for (i = 0; i < 2; i++) {
				ParsedURL *p;

				p = (ParsedURL*) malloc(sizeof(ParsedURL));
				memset(p, 0, sizeof(ParsedURL));
				p->Priority = Msg->CurrMX->priority;
				p->IsIP = 0;
				p->Port = DefaultMXPort;
				p->IPv6 = i == 1;
				p->Host = Msg->CurrMX->host;
				if (*pp == NULL)
					*pp = p;
				else {
					ParsedURL *ppp = *pp;

					while ((ppp->Next != NULL) &&
					       (ppp->Next->Priority <= p->Priority))
					       ppp = ppp->Next;
					if ((ppp == *pp) &&
					    (ppp->Priority > p->Priority)) {
						p->Next = *pp;
						*pp = p;
					}
					else {
						p->Next = ppp->Next;
						ppp->Next = p;
					}
				}
			}
			Msg->CurrMX    = Msg->CurrMX->next;
		}
		Msg->CXFlags   = Msg->CXFlags & F_HAVE_MX;
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
			p->Host = Msg->node;

			*pp = p;
			pp = &p->Next;
		}
		Msg->CXFlags   = Msg->CXFlags & F_DIRECT;
	}
	if (Msg->MyQItem->FallBackHost != NULL)
	{
		Msg->MyQItem->FallBackHost->Next = *pp;
		*pp = Msg->MyQItem->FallBackHost;
	}
	Msg->pCurrRelay = Msg->Relay;
	return get_one_mx_host_ip(IO);
}

eNextState resolve_mx_records(AsyncIO *IO)
{
	SmtpOutMsg * Msg = IO->Data;

	SetSMTPState(IO, eSTMPmxlookup);

	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	/* start resolving MX records here. */
	if (!QueueQuery(ns_t_mx,
			Msg->node,
			&Msg->IO,
			&Msg->MxLookup,
			smtp_resolve_mx_record_done))
	{
		Msg->MyQEntry->Status = 5;
		StrBufPrintf(Msg->MyQEntry->StatusMessage,
			     "No MX hosts found for <%s>", Msg->node);
		return IO->NextState;
	}
	Msg->IO.NextState = eReadDNSReply;
	return IO->NextState;
}



/******************************************************************************
 *  so, we're going to start a SMTP delivery.  lets get it on.                *
 ******************************************************************************/

SmtpOutMsg *new_smtp_outmsg(OneQueItem *MyQItem,
			    MailQEntry *MyQEntry,
			    int MsgCount)
{
	SmtpOutMsg * Msg;

	Msg = (SmtpOutMsg *) malloc(sizeof(SmtpOutMsg));
	if (Msg == NULL)
		return NULL;
	memset(Msg, 0, sizeof(SmtpOutMsg));

	Msg->n                = MsgCount;
	Msg->MyQEntry         = MyQEntry;
	Msg->MyQItem          = MyQItem;
	Msg->pCurrRelay       = MyQItem->URL;

	InitIOStruct(&Msg->IO,
		     Msg,
		     eReadMessage,
		     SMTP_C_ReadServerStatus,
		     SMTP_C_DNSFail,
		     SMTP_C_DispatchWriteDone,
		     SMTP_C_DispatchReadDone,
		     SMTP_C_Terminate,
		     SMTP_C_TerminateDB,
		     SMTP_C_ConnFail,
		     SMTP_C_Timeout,
		     SMTP_C_Shutdown);

	Msg->IO.ErrMsg = Msg->MyQEntry->StatusMessage;

	return Msg;
}

void smtp_try_one_queue_entry(OneQueItem *MyQItem,
			      MailQEntry *MyQEntry,
			      StrBuf *MsgText,
			/*KeepMsgText allows us to use MsgText as ours.*/
			      int KeepMsgText,
			      int MsgCount)
{
	SmtpOutMsg *Msg;

	SMTPC_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);

	Msg = new_smtp_outmsg(MyQItem, MyQEntry, MsgCount);
	if (Msg == NULL) {
		SMTPC_syslog(LOG_DEBUG, "%s Failed to alocate message context.\n", __FUNCTION__);
		if (KeepMsgText) 
			FreeStrBuf (&MsgText);
		return;
	}
	if (KeepMsgText) Msg->msgtext = MsgText;
	else		 Msg->msgtext = NewStrBufDup(MsgText);

	if (smtp_resolve_recipients(Msg) &&
	    (!MyQItem->HaveRelay ||
	     (MyQItem->URL != NULL)))
	{
		safestrncpy(
			((CitContext *)Msg->IO.CitContext)->cs_host,
			Msg->node,
			sizeof(((CitContext *)
				Msg->IO.CitContext)->cs_host));

		SMTPC_syslog(LOG_DEBUG, "Starting: [%ld] <%s> CC <%d> \n",
			     Msg->MyQItem->MessageID,
			     ChrPtr(Msg->MyQEntry->Recipient),
			     ((CitContext*)Msg->IO.CitContext)->cs_pid);
		if (Msg->pCurrRelay == NULL) {
			SetSMTPState(&Msg->IO, eSTMPmxlookup);
			QueueEventContext(&Msg->IO,
					  resolve_mx_records);
		}
		else { /* oh... via relay host */
			if (Msg->pCurrRelay->IsIP) {
				SetSMTPState(&Msg->IO, eSTMPconnecting);
				QueueEventContext(&Msg->IO,
						  mx_connect_ip);
			}
			else {
				SetSMTPState(&Msg->IO, eSTMPalookup);
				/* uneducated admin has chosen to
				   add DNS to the equation... */
				QueueEventContext(&Msg->IO,
						  get_one_mx_host_ip);
			}
		}
	}
	else {
		SetSMTPState(&Msg->IO, eSMTPFailTotal);
		/* No recipients? well fail then. */
		if (Msg->MyQEntry != NULL) {
			Msg->MyQEntry->Status = 5;
			if (StrLength(Msg->MyQEntry->StatusMessage) == 0)
				StrBufPlain(Msg->MyQEntry->StatusMessage,
					    HKEY("Invalid Recipient!"));
		}
		FinalizeMessageSend_DB(&Msg->IO);
		DeleteSmtpOutMsg(Msg);
	}
}






/*****************************************************************************/
/*                     SMTP CLIENT DISPATCHER                                */
/*****************************************************************************/

void SMTPSetTimeout(eNextState NextTCPState, SmtpOutMsg *Msg)
{
	double Timeout = 0.0;
	AsyncIO *IO = &Msg->IO;

	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);

	switch (NextTCPState) {
	case eSendFile:
	case eSendReply:
	case eSendMore:
		Timeout = SMTP_C_SendTimeouts[Msg->State];
		if (Msg->State == eDATABody) {
			/* if we're sending a huge message,
			 * we need more time.
			 */
			Timeout += StrLength(Msg->msgtext) / 512;
		}
		break;
	case eReadMessage:
		Timeout = SMTP_C_ReadTimeouts[Msg->State];
		if (Msg->State == eDATATerminateBody) {
			/*
			 * some mailservers take a nap before accepting
			 * the message content inspection and such.
			 */
			Timeout += StrLength(Msg->msgtext) / 512;
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
	SetNextTimeout(&Msg->IO, Timeout);
}
eNextState SMTP_C_DispatchReadDone(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	SmtpOutMsg *Msg = IO->Data;
	eNextState rc;

	rc = ReadHandlers[Msg->State](Msg);
	if (rc != eAbort)
	{
		Msg->State++;
		SMTPSetTimeout(rc, Msg);
	}
	return rc;
}
eNextState SMTP_C_DispatchWriteDone(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	SmtpOutMsg *Msg = IO->Data;
	eNextState rc;

	rc = SendHandlers[Msg->State](Msg);
	SMTPSetTimeout(rc, Msg);
	return rc;
}


/*****************************************************************************/
/*                     SMTP CLIENT ERROR CATCHERS                            */
/*****************************************************************************/
eNextState SMTP_C_Terminate(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;

	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	return FinalizeMessageSend(Msg);
}
eNextState SMTP_C_TerminateDB(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	return Terminate(IO);
}
eNextState SMTP_C_Timeout(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;

	Msg->MyQEntry->Status = 4;
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	StrBufPrintf(IO->ErrMsg, "Timeout: %s while talking to %s",
		     ReadErrors[Msg->State].Key,
		     Msg->mx_host);
	if (Msg->State > eRCPT)
		return eAbort;
	else
		return FailOneAttempt(IO);
}
eNextState SMTP_C_ConnFail(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;

	Msg->MyQEntry->Status = 4;
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	StrBufPrintf(IO->ErrMsg, "Connection failure: %s while talking to %s",
		     ReadErrors[Msg->State].Key,
		     Msg->mx_host);

	return FailOneAttempt(IO);
}
eNextState SMTP_C_DNSFail(AsyncIO *IO)
{
	SmtpOutMsg *Msg = IO->Data;
	Msg->MyQEntry->Status = 4;
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	return FailOneAttempt(IO);
}
eNextState SMTP_C_Shutdown(AsyncIO *IO)
{
	EVS_syslog(LOG_DEBUG, "%s\n", __FUNCTION__);
	SmtpOutMsg *Msg = IO->Data;

	switch (IO->NextState) {
	case eSendDNSQuery:
	case eReadDNSReply:

		/* todo: abort c-ares */
	case eConnect:
	case eSendReply:
	case eSendMore:
	case eSendFile:
	case eReadMessage:
	case eReadMore:
	case eReadPayload:
	case eReadFile:
		StopClientWatchers(IO, 1);
		break;
	case eDBQuery:

		break;
	case eTerminateConnection:
	case eAbort:
		break;
	}
	Msg->MyQEntry->Status = 3;
	StrBufPlain(Msg->MyQEntry->StatusMessage,
		    HKEY("server shutdown during message submit."));
	return FinalizeMessageSend(Msg);
}


/**
 * @brief lineread Handler;
 * understands when to read more SMTP lines, and when this is a one-lined reply.
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

void LogDebugEnableSMTPClient(const int n)
{
	SMTPClientDebugEnabled = n;
}

CTDL_MODULE_INIT(smtp_eventclient)
{
	if (!threading)
		CtdlRegisterDebugFlagHook(HKEY("smtpeventclient"), LogDebugEnableSMTPClient, &SMTPClientDebugEnabled);
	return "smtpeventclient";
}
