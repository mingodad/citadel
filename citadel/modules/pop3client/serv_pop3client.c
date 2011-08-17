/*
 * Consolidate mail from remote POP3 accounts.
 *
 * Copyright (c) 2007-2009 by the citadel.org team
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "ctdl_module.h"
#include "clientsocket.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "database.h"
#include "citadel_dirs.h"
#include "event_client.h"



citthread_mutex_t POP3QueueMutex; /* locks the access to the following vars: */
HashList *POP3QueueRooms = NULL; /* rss_room_counter */
HashList *POP3FetchUrls = NULL; /* -> rss_aggregator; ->RefCount access to be locked too. */

typedef struct __pop3_room_counter {
	int count;
	long QRnumber;
}pop3_room_counter;

typedef enum ePOP3_C_States {
	ReadGreeting,
	GetUserState,
	GetPassState,
	GetListCommandState,
	GetListOneLine,
	GetOneMessageIDState,
	ReadMessageBodyFollowing,
	ReadMessageBody,
	GetDeleteState,
	ReadQuitState,
	POP3C_MaxRead
}ePOP3_C_States;


typedef struct _FetchItem {
	long MSGID;
	long MSGSize;
	StrBuf *MsgUIDL;
	StrBuf *MsgUID;
	int NeedFetch;
	struct CtdlMessage *Msg;
} FetchItem;

void HfreeFetchItem(void *vItem)
{
	FetchItem *Item = (FetchItem*) vItem;
	FreeStrBuf(&Item->MsgUIDL);
	FreeStrBuf(&Item->MsgUID);
	free(Item);
}

typedef struct __pop3aggr {
	AsyncIO    	 IO;

	long n;
	long RefCount;
	ParsedURL Pop3Host;
	DNSQueryParts HostLookup;

	StrBuf		*rooms;
	long		 QRnumber;
	HashList	*OtherQRnumbers;

	StrBuf		*Url;
///	StrBuf *pop3host; -> URL
	StrBuf *pop3user;
	StrBuf *pop3pass;
	StrBuf *RoomName; // TODO: fill me
	int keep;
	time_t interval;
	ePOP3_C_States State;
	HashList *MsgNumbers;
	HashPos *Pos;
	FetchItem *CurrMsg;
} pop3aggr;

void DeletePOP3Aggregator(void *vptr)
{
	pop3aggr *ptr = vptr;
	DeleteHashPos(&ptr->Pos);
	DeleteHash(&ptr->MsgNumbers);
	FreeStrBuf(&ptr->rooms);
	FreeStrBuf(&ptr->pop3user);
	FreeStrBuf(&ptr->pop3pass);
	FreeStrBuf(&ptr->RoomName);
}


typedef eNextState(*Pop3ClientHandler)(pop3aggr* RecvMsg);

eNextState POP3_C_Shutdown(AsyncIO *IO);
eNextState POP3_C_Timeout(AsyncIO *IO);
eNextState POP3_C_ConnFail(AsyncIO *IO);
eNextState POP3_C_DispatchReadDone(AsyncIO *IO);
eNextState POP3_C_DispatchWriteDone(AsyncIO *IO);
eNextState POP3_C_Terminate(AsyncIO *IO);
eReadState POP3_C_ReadServerStatus(AsyncIO *IO);
eNextState POP3_C_ReAttachToFetchMessages(AsyncIO *IO);

eNextState FinalizePOP3AggrRun(AsyncIO *IO)
{

	return eAbort;
}

eNextState FailAggregationRun(AsyncIO *IO)
{
	return eAbort;
}

#define POP3C_DBG_SEND() CtdlLogPrintf(CTDL_DEBUG, "POP3 client[%ld]: > %s\n", RecvMsg->n, ChrPtr(RecvMsg->IO.SendBuf.Buf))
#define POP3C_DBG_READ() CtdlLogPrintf(CTDL_DEBUG, "POP3 client[%ld]: < %s\n", RecvMsg->n, ChrPtr(RecvMsg->IO.IOBuf))
#define POP3C_OK (strncasecmp(ChrPtr(RecvMsg->IO.IOBuf), "+OK", 3) == 0)

eNextState POP3C_ReadGreeting(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	/* Read the server greeting */
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}


eNextState POP3C_SendUser(pop3aggr *RecvMsg)
{
	/* Identify ourselves.  NOTE: we have to append a CR to each command.  The LF will
	 * automatically be appended by sock_puts().  Believe it or not, leaving out the CR
	 * will cause problems if the server happens to be Exchange, which is so b0rken it
	 * actually barfs on LF-terminated newlines.
	 */
	StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
		     "USER %s\r\n", ChrPtr(RecvMsg->pop3user));
	POP3C_DBG_SEND();
	return eReadMessage;
}

eNextState POP3C_GetUserState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}

eNextState POP3C_SendPassword(pop3aggr *RecvMsg)
{
	/* Password */
	StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
		     "PASS %s\r\n", ChrPtr(RecvMsg->pop3pass));
	CtdlLogPrintf(CTDL_DEBUG, "<PASS <password>\n");
//	POP3C_DBG_SEND();
	return eReadMessage;
}

eNextState POP3C_GetPassState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}

eNextState POP3C_SendListCommand(pop3aggr *RecvMsg)
{
	/* Get the list of messages */
	StrBufPlain(RecvMsg->IO.SendBuf.Buf, HKEY("LIST\r\n"));
	POP3C_DBG_SEND();
	return eReadMessage;
}

eNextState POP3C_GetListCommandState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	RecvMsg->MsgNumbers = NewHash(1, NULL);
	RecvMsg->State++;	
	return eReadMore;
}


eNextState POP3C_GetListOneLine(pop3aggr *RecvMsg)
{
	const char *pch;
	FetchItem *OneMsg = NULL;
	POP3C_DBG_READ();

	if ((StrLength(RecvMsg->IO.IOBuf) == 1) && 
	    (ChrPtr(RecvMsg->IO.IOBuf)[0] == '.'))
	{
		if (GetCount(RecvMsg->MsgNumbers) == 0)
		{
			////	RecvMsg->Sate = ReadQuitState;
		}
		else
		{
			RecvMsg->Pos = GetNewHashPos(RecvMsg->MsgNumbers, 0);
		}
		return eSendReply;

	}
	OneMsg = (FetchItem*) malloc(sizeof(FetchItem));
	memset(OneMsg, 0, sizeof(FetchItem));
	OneMsg->MSGID = atol(ChrPtr(RecvMsg->IO.IOBuf));

	pch = strchr(ChrPtr(RecvMsg->IO.IOBuf), ' ');
	if (pch != NULL)
	{
		OneMsg->MSGSize = atol(pch + 1);
	}
	Put(RecvMsg->MsgNumbers, LKEY(OneMsg->MSGID), OneMsg, HfreeFetchItem);

	//RecvMsg->State --; /* read next Line */
	return eReadMore;
}

eNextState POP3_FetchNetworkUsetableEntry(AsyncIO *IO)
{
	long HKLen;
	const char *HKey;
	void *vData;
	struct cdbdata *cdbut;
	pop3aggr *RecvMsg = (pop3aggr *) IO->Data;

	if(GetNextHashPos(RecvMsg->MsgNumbers, RecvMsg->Pos, &HKLen, &HKey, &vData))
	{
		struct UseTable ut;

		RecvMsg->CurrMsg = (FetchItem*) vData;
		/* Find out if we've already seen this item */
		safestrncpy(ut.ut_msgid, 
			    ChrPtr(RecvMsg->CurrMsg->MsgUIDL),
			    sizeof(ut.ut_msgid));
		ut.ut_timestamp = time(NULL);/// TODO: libev timestamp!
		
		cdbut = cdb_fetch(CDB_USETABLE, SKEY(RecvMsg->CurrMsg->MsgUIDL));
		if (cdbut != NULL) {
			/* Item has already been seen */
			CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", ChrPtr(RecvMsg->CurrMsg->MsgUIDL));
			cdb_free(cdbut);
		
			/* rewrite the record anyway, to update the timestamp */
			cdb_store(CDB_USETABLE, 
				  SKEY(RecvMsg->CurrMsg->MsgUIDL), 
				  &ut, sizeof(struct UseTable) );
			RecvMsg->CurrMsg->NeedFetch = 0;
		}
		else
		{
			RecvMsg->CurrMsg->NeedFetch = 1;
		}
		return NextDBOperation(&RecvMsg->IO, POP3_FetchNetworkUsetableEntry);
	}
	else
	{
		/* ok, now we know them all, continue with reading the actual messages. */
		DeleteHashPos(&RecvMsg->Pos);

		return QueueEventContext(IO, POP3_C_ReAttachToFetchMessages);
	}
}

eNextState POP3C_GetOneMessagID(pop3aggr *RecvMsg)
{
	long HKLen;
	const char *HKey;
	void *vData;

	if(GetNextHashPos(RecvMsg->MsgNumbers, RecvMsg->Pos, &HKLen, &HKey, &vData))
	{
		RecvMsg->CurrMsg = (FetchItem*) vData;
		/* Find out the UIDL of the message, to determine whether we've already downloaded it */
		StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
			     "UIDL %ld\r\n", RecvMsg->CurrMsg->MSGID);
		POP3C_DBG_SEND();
	}
	else
	{
	    RecvMsg->State++;
		DeleteHashPos(&RecvMsg->Pos);
		/// done receiving uidls.. start looking them up now.
		RecvMsg->Pos = GetNewHashPos(RecvMsg->MsgNumbers, 0);
		return QueueDBOperation(&RecvMsg->IO, POP3_FetchNetworkUsetableEntry);
	}
	return eReadMore; /* TODO */
}

eNextState POP3C_GetOneMessageIDState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	RecvMsg->CurrMsg->MsgUIDL = NewStrBufPlain(NULL, StrLength(RecvMsg->IO.IOBuf));
	RecvMsg->CurrMsg->MsgUID = NewStrBufPlain(NULL, StrLength(RecvMsg->IO.IOBuf) * 2);

	StrBufExtract_token(RecvMsg->CurrMsg->MsgUIDL, RecvMsg->IO.IOBuf, 2, ' ');
	StrBufPrintf(RecvMsg->CurrMsg->MsgUID, 
		     "pop3/%s/%s@%s", 
		     ChrPtr(RecvMsg->RoomName), 
		     ChrPtr(RecvMsg->CurrMsg->MsgUIDL),
		     RecvMsg->Pop3Host.Host);
	RecvMsg->State --;
	return eSendReply;
}

eNextState POP3C_GetOneMessageIDFromUseTable(pop3aggr *RecvMsg)
{

	struct cdbdata *cdbut;
	struct UseTable ut;

	cdbut = cdb_fetch(CDB_USETABLE, SKEY(RecvMsg->CurrMsg->MsgUID));
	if (cdbut != NULL) {
		/* message has already been seen */
		CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", ChrPtr(RecvMsg->CurrMsg->MsgUID));
		cdb_free(cdbut);

		/* rewrite the record anyway, to update the timestamp */
		strcpy(ut.ut_msgid, ChrPtr(RecvMsg->CurrMsg->MsgUID));
		ut.ut_timestamp = time(NULL);
		cdb_store(CDB_USETABLE, SKEY(RecvMsg->CurrMsg->MsgUID), &ut, sizeof(struct UseTable) );
	}

	return eReadMessage;
}

eNextState POP3C_SendGetOneMsg(pop3aggr *RecvMsg)
{
	long HKLen;
	const char *HKey;
	void *vData;

	RecvMsg->CurrMsg = NULL;
	while (GetNextHashPos(RecvMsg->MsgNumbers, RecvMsg->Pos, &HKLen, &HKey, &vData) && 
	       (RecvMsg->CurrMsg = (FetchItem*) vData, RecvMsg->CurrMsg->NeedFetch == 0))
	{}

	if ((RecvMsg->CurrMsg != NULL ) && (RecvMsg->CurrMsg->NeedFetch == 1))
	{
		/* Message has not been seen. Tell the server to fetch the message... */
		StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
			     "RETR %ld\r\n", RecvMsg->CurrMsg->MSGID);
		POP3C_DBG_SEND();
		return eReadMessage;
	}
	else {
		RecvMsg->State = ReadQuitState;
		return POP3_C_DispatchWriteDone(&RecvMsg->IO);
	}
}


eNextState POP3C_ReadMessageBodyFollowing(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	RecvMsg->IO.ReadMsg = NewAsyncMsg(HKEY("."), 
					  RecvMsg->CurrMsg->MSGSize,
					  config.c_maxmsglen, 
					  NULL, -1,
					  1);

	return eReadPayload;
}	


eNextState POP3C_StoreMsgRead(AsyncIO *IO)
{
	pop3aggr *RecvMsg = (pop3aggr *) IO->Data;
	struct UseTable ut;

	safestrncpy(ut.ut_msgid, 
		    ChrPtr(RecvMsg->CurrMsg->MsgUID),
		    sizeof(ut.ut_msgid));
	ut.ut_timestamp = time(NULL); /* TODO: use libev time */
	cdb_store(CDB_USETABLE, 
		  ChrPtr(RecvMsg->CurrMsg->MsgUID), 
		  StrLength(RecvMsg->CurrMsg->MsgUID),
		  &ut, 
		  sizeof(struct UseTable) );

	return QueueEventContext(&RecvMsg->IO, POP3_C_ReAttachToFetchMessages);
}
eNextState POP3C_SaveMsg(AsyncIO *IO)
{
	long msgnum;
	pop3aggr *RecvMsg = (pop3aggr *) IO->Data;

	/* Do Something With It (tm) */
	msgnum = CtdlSubmitMsg(RecvMsg->CurrMsg->Msg, 
			       NULL, 
			       ChrPtr(RecvMsg->RoomName), 
			       0);
	if (msgnum > 0L) {
		/* Message has been committed to the store */
		/* write the uidl to the use table so we don't fetch this message again */
	}
	CtdlFreeMessage(RecvMsg->CurrMsg->Msg);

	return NextDBOperation(&RecvMsg->IO, POP3C_StoreMsgRead);
	return eReadMessage;
}

eNextState POP3C_ReadMessageBody(pop3aggr *RecvMsg)
{
	CtdlLogPrintf(CTDL_DEBUG, "Converting message...\n");
	RecvMsg->CurrMsg->Msg = convert_internet_message_buf(&RecvMsg->IO.ReadMsg->MsgBuf);

	return QueueDBOperation(&RecvMsg->IO, POP3C_SaveMsg);
}

eNextState POP3C_SendDelete(pop3aggr *RecvMsg)
{
	if (!RecvMsg->keep) {
		StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
			     "DELE %ld\r\n", RecvMsg->CurrMsg->MSGID);
		POP3C_DBG_SEND();
		return eReadMessage;
	}
	else {
		RecvMsg->State = ReadMessageBodyFollowing;
		return POP3_C_DispatchWriteDone(&RecvMsg->IO);
	}
}
eNextState POP3C_ReadDeleteState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	RecvMsg->State = GetOneMessageIDState;
	return eReadMessage;
}

eNextState POP3C_SendQuit(pop3aggr *RecvMsg)
{
	/* Log out */
	StrBufPlain(RecvMsg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n3)"));
	POP3C_DBG_SEND();
	return eReadMessage;
}


eNextState POP3C_ReadQuitState(pop3aggr *RecvMsg)
{
	POP3C_DBG_READ();
	return eAbort;
}

const long POP3_C_ConnTimeout = 1000;
const long DefaultPOP3Port = 110;

Pop3ClientHandler POP3C_ReadHandlers[] = {
	POP3C_ReadGreeting,
	POP3C_GetUserState,
	POP3C_GetPassState,
	POP3C_GetListCommandState,
	POP3C_GetListOneLine,
	POP3C_GetOneMessageIDState,
	POP3C_ReadMessageBodyFollowing,
	POP3C_ReadMessageBody,
	POP3C_ReadDeleteState,
	POP3C_ReadQuitState,
};

const long POP3_C_SendTimeouts[POP3C_MaxRead] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100
};
const ConstStr POP3C_ReadErrors[POP3C_MaxRead] = {
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")},
	{HKEY("Connection broken during ")}
};

Pop3ClientHandler POP3C_SendHandlers[] = {
	NULL, /* we don't send a greeting */
	POP3C_SendUser,
	POP3C_SendPassword,
	POP3C_SendListCommand,
	NULL,
	POP3C_GetOneMessagID,
	POP3C_SendGetOneMsg,
	NULL,
	POP3C_SendDelete,
	POP3C_SendQuit
};

const long POP3_C_ReadTimeouts[] = {
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100,
	100
};
/*****************************************************************************/
/*                     POP3 CLIENT DISPATCHER                                */
/*****************************************************************************/

void POP3SetTimeout(eNextState NextTCPState, pop3aggr *pMsg)
{
	double Timeout = 0.0;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);

	switch (NextTCPState) {
	case eSendReply:
	case eSendMore:
		Timeout = POP3_C_SendTimeouts[pMsg->State];
/*
  if (pMsg->State == eDATABody) {
  / * if we're sending a huge message, we need more time. * /
  Timeout += StrLength(pMsg->msgtext) / 1024;
  }
*/
		break;
	case eReadMessage:
		Timeout = POP3_C_ReadTimeouts[pMsg->State];
/*
  if (pMsg->State == eDATATerminateBody) {
  / * 
  * some mailservers take a nap before accepting the message
  * content inspection and such.
  * /
  Timeout += StrLength(pMsg->msgtext) / 1024;
  }
*/
		break;
	case eReadPayload:
		Timeout = 100000;
		/* TODO!!! */
		break;
	case eSendDNSQuery:
	case eReadDNSReply:
	case eConnect:
	case eTerminateConnection:
	case eDBQuery:
	case eAbort:
	case eReadMore://// TODO
		return;
	}
	SetNextTimeout(&pMsg->IO, Timeout);
}
eNextState POP3_C_DispatchReadDone(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	pop3aggr *pMsg = IO->Data;
	eNextState rc;

	rc = POP3C_ReadHandlers[pMsg->State](pMsg);
	if (rc != eReadMore)
	    pMsg->State++;
	POP3SetTimeout(rc, pMsg);
	return rc;
}
eNextState POP3_C_DispatchWriteDone(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	pop3aggr *pMsg = IO->Data;
	eNextState rc;

	rc = POP3C_SendHandlers[pMsg->State](pMsg);
	POP3SetTimeout(rc, pMsg);
	return rc;
}


/*****************************************************************************/
/*                     POP3 CLIENT ERROR CATCHERS                            */
/*****************************************************************************/
eNextState POP3_C_Terminate(AsyncIO *IO)
{
///	pop3aggr *pMsg = (pop3aggr *)IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	FinalizePOP3AggrRun(IO);
	return eAbort;
}
eNextState POP3_C_Timeout(AsyncIO *IO)
{
	pop3aggr *pMsg = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State]));
	return FailAggregationRun(IO);
}
eNextState POP3_C_ConnFail(AsyncIO *IO)
{
	pop3aggr *pMsg = (pop3aggr *)IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State]));
	return FailAggregationRun(IO);
}
eNextState POP3_C_Shutdown(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
////	pop3aggr *pMsg = IO->Data;

	////pMsg->MyQEntry->Status = 3;
	///StrBufPlain(pMsg->MyQEntry->StatusMessage, HKEY("server shutdown during message retrieval."));
	FinalizePOP3AggrRun(IO);
	return eAbort;
}


/**
 * @brief lineread Handler; understands when to read more POP3 lines, and when this is a one-lined reply.
 */
eReadState POP3_C_ReadServerStatus(AsyncIO *IO)
{
	eReadState Finished = eBufferNotEmpty; 

	switch (IO->NextState) {
	case eSendDNSQuery:
	case eReadDNSReply:
	case eDBQuery:
	case eConnect:
	case eTerminateConnection:
	case eAbort:
		Finished = eReadFail;
		break;
	case eSendReply: 
	case eSendMore:
	case eReadMore:
	case eReadMessage: 
		Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
		break;
	case eReadPayload:
		Finished = CtdlReadMessageBodyAsync(IO);
		break;
	}
	return Finished;
}

/*****************************************************************************
 * So we connect our Server IP here.                                         *
 *****************************************************************************/
eNextState POP3_C_ReAttachToFetchMessages(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
////???	cpptr->State ++;
	if (cpptr->Pos == NULL)
		cpptr->Pos = GetNewHashPos(cpptr->MsgNumbers, 0);

	POP3_C_DispatchWriteDone(IO);
	ReAttachIO(IO, cpptr, 0);
	IO->NextState = eReadMessage;
	return IO->NextState;
}

eNextState connect_ip(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);
	
////	IO->ConnectMe = &cpptr->Pop3Host;
	/*  Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */

	/////// SetConnectStatus(IO);

	return InitEventIO(IO, cpptr, 
			   POP3_C_ConnTimeout, 
			   POP3_C_ReadTimeouts[0],
			   1);
}

eNextState get_one_host_ip_done(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;
	struct hostent *hostent;

	QueryCbDone(IO);

	hostent = cpptr->HostLookup.VParsedDNSReply;
	if ((cpptr->HostLookup.DNSStatus == ARES_SUCCESS) && 
	    (hostent != NULL) ) {
		memset(&cpptr->Pop3Host.Addr, 0, sizeof(struct in6_addr));
		if (cpptr->Pop3Host.IPv6) {
			memcpy(&cpptr->Pop3Host.Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));
			
			cpptr->Pop3Host.Addr.sin6_family = hostent->h_addrtype;
			cpptr->Pop3Host.Addr.sin6_port   = htons(DefaultPOP3Port);
		}
		else {
			struct sockaddr_in *addr = (struct sockaddr_in*) &cpptr->Pop3Host.Addr;
			/* Bypass the ns lookup result like this: IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1"); */
//			addr->sin_addr.s_addr = htonl((uint32_t)&hostent->h_addr_list[0]);
			memcpy(&addr->sin_addr.s_addr, 
			       hostent->h_addr_list[0], 
			       sizeof(uint32_t));
			
			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(DefaultPOP3Port);
			
		}
		return connect_ip(IO);
	}
	else
		return eAbort;
}

eNextState get_one_host_ip(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;
	/* 
	 * here we start with the lookup of one host. it might be...
	 * - the relay host *sigh*
	 * - the direct hostname if there was no mx record
	 * - one of the mx'es
	 */ 

	InitC_ares_dns(IO);

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s\n", __FUNCTION__);

	CtdlLogPrintf(CTDL_DEBUG, 
		      "POP3 client[%ld]: looking up %s-Record %s : %d ...\n", 
		      cpptr->n, 
		      (cpptr->Pop3Host.IPv6)? "aaaa": "a",
		      cpptr->Pop3Host.Host, 
		      cpptr->Pop3Host.Port);

	if (!QueueQuery((cpptr->Pop3Host.IPv6)? ns_t_aaaa : ns_t_a, 
			cpptr->Pop3Host.Host, 
			&cpptr->IO, 
			&cpptr->HostLookup, 
			get_one_host_ip_done))
	{
//		cpptr->MyQEntry->Status = 5;
//		StrBufPrintf(SendMsg->MyQEntry->StatusMessage, 
//			     "No MX hosts found for <%s>", SendMsg->node);
		cpptr->IO.NextState = eTerminateConnection;
		return IO->NextState;
	}
	IO->NextState = eReadDNSReply;
	return IO->NextState;
}



int pop3_do_fetching(pop3aggr *cpptr)
{
	CitContext *SubC;

	cpptr->IO.Data          = cpptr;

	cpptr->IO.SendDone      = POP3_C_DispatchWriteDone;
	cpptr->IO.ReadDone      = POP3_C_DispatchReadDone;
	cpptr->IO.Terminate     = POP3_C_Terminate;
	cpptr->IO.LineReader    = POP3_C_ReadServerStatus;
	cpptr->IO.ConnFail      = POP3_C_ConnFail;
	cpptr->IO.Timeout       = POP3_C_Timeout;
	cpptr->IO.ShutdownAbort = POP3_C_Shutdown;
	
	cpptr->IO.SendBuf.Buf   = NewStrBufPlain(NULL, 1024);
	cpptr->IO.RecvBuf.Buf   = NewStrBufPlain(NULL, 1024);
	cpptr->IO.IOBuf         = NewStrBuf();
	
	cpptr->IO.NextState     = eReadMessage;
/* TODO
   CtdlLogPrintf(CTDL_DEBUG, "POP3: %s %s %s <password>\n", roomname, pop3host, pop3user);
   CtdlLogPrintf(CTDL_NOTICE, "Connecting to <%s>\n", pop3host);
*/
	
	SubC = CloneContext (CC);
	SubC->session_specific_data = (char*) cpptr;
	cpptr->IO.CitContext = SubC;

	if (cpptr->IO.ConnectMe->IsIP) {
		QueueEventContext(&cpptr->IO,
				  connect_ip);
	}
	else { /* uneducated admin has chosen to add DNS to the equation... */
		QueueEventContext(&cpptr->IO,
				  get_one_host_ip);
	}
	return 1;
}

/*
 * Scan a room's netconfig to determine whether it requires POP3 aggregation
 */
void pop3client_scan_room(struct ctdlroom *qrbuf, void *data)
{
	StrBuf *CfgData;
	StrBuf *CfgType;
	StrBuf *Line;

	struct stat statbuf;
	char filename[PATH_MAX];
	int  fd;
	int Done;
	void *vptr;
	const char *CfgPtr, *lPtr;
	const char *Err;

	pop3_room_counter *Count = NULL;
//	pop3aggr *cpptr;

	citthread_mutex_lock(&POP3QueueMutex);
	if (GetHash(POP3QueueRooms, LKEY(qrbuf->QRnumber), &vptr))
	{
		CtdlLogPrintf(CTDL_DEBUG, 
			      "pop3client: [%ld] %s already in progress.\n", 
			      qrbuf->QRnumber, 
			      qrbuf->QRname);
		citthread_mutex_unlock(&POP3QueueMutex);
		return;
	}
	citthread_mutex_unlock(&POP3QueueMutex);

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	if (CtdlThreadCheckStop())
		return;
		
	/* Only do net processing for rooms that have netconfigs */
	fd = open(filename, 0);
	if (fd <= 0) {
		//CtdlLogPrintf(CTDL_DEBUG, "rssclient: %s no config.\n", qrbuf->QRname);
		return;
	}
	if (CtdlThreadCheckStop())
		return;
	if (fstat(fd, &statbuf) == -1) {
		CtdlLogPrintf(CTDL_DEBUG,  "ERROR: could not stat configfile '%s' - %s\n",
			      filename, strerror(errno));
		return;
	}
	if (CtdlThreadCheckStop())
		return;
	CfgData = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(CfgData, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		FreeStrBuf(&CfgData);
		CtdlLogPrintf(CTDL_DEBUG,  "ERROR: reading config '%s' - %s<br>\n",
			      filename, strerror(errno));
		return;
	}
	close(fd);
	if (CtdlThreadCheckStop())
		return;
	
	CfgPtr = NULL;
	CfgType = NewStrBuf();
	Line = NewStrBufPlain(NULL, StrLength(CfgData));
	Done = 0;

	while (!Done)
	{
		Done = StrBufSipLine(Line, CfgData, &CfgPtr) == 0;
		if (StrLength(Line) > 0)
		{
			lPtr = NULL;
			StrBufExtract_NextToken(CfgType, Line, &lPtr, '|');
			if (!strcasecmp("pop3client", ChrPtr(CfgType)))
			{
				pop3aggr *cptr;

				if (Count == NULL)
				{
					Count = malloc(sizeof(pop3_room_counter));
					Count->count = 0;
				}
				Count->count ++;
				cptr = (pop3aggr *) malloc(sizeof(pop3aggr));
				memset(cptr, 0, sizeof(pop3aggr));
				/// TODO do we need this? cptr->roomlist_parts = 1;
				cptr->rooms = NewStrBufPlain(qrbuf->QRname, -1);
				cptr->pop3user = NewStrBufPlain(NULL, StrLength(Line));
				cptr->pop3pass = NewStrBufPlain(NULL, StrLength(Line));
				cptr->Url = NewStrBuf();

				StrBufExtract_NextToken(cptr->Url, Line, &lPtr, '|');
				StrBufExtract_NextToken(cptr->pop3user, Line, &lPtr, '|');
				StrBufExtract_NextToken(cptr->pop3pass, Line, &lPtr, '|');
				cptr->keep = StrBufExtractNext_long(Line, &lPtr, '|');
				cptr->interval = StrBufExtractNext_long(Line, &lPtr, '|');
		    
				ParseURL(&cptr->IO.ConnectMe, cptr->Url, 110);

				cptr->IO.ConnectMe->CurlCreds = cptr->pop3user;
				cptr->IO.ConnectMe->User = ChrPtr(cptr->IO.ConnectMe->CurlCreds);
				cptr->IO.ConnectMe->UrlWithoutCred = cptr->pop3pass;
				cptr->IO.ConnectMe->Pass = ChrPtr(cptr->IO.ConnectMe->UrlWithoutCred);



#if 0 
/* todo: we need to reunite the url to be shure. */
				
				citthread_mutex_lock(&POP3ueueMutex);
				GetHash(POP3FetchUrls, SKEY(ptr->Url), &vptr);
				use_this_cptr = (pop3aggr *)vptr;
				
				if (use_this_rncptr != NULL)
				{
					/* mustn't attach to an active session */
					if (use_this_cptr->RefCount > 0)
					{
						DeletePOP3Cfg(cptr);
						Count->count--;
					}
					else 
					{
						long *QRnumber;
						StrBufAppendBufPlain(use_this_cptr->rooms, 
								     qrbuf->QRname, 
								     -1, 0);
						if (use_this_cptr->roomlist_parts == 1)
						{
							use_this_cptr->OtherQRnumbers = NewHash(1, lFlathash);
						}
						QRnumber = (long*)malloc(sizeof(long));
						*QRnumber = qrbuf->QRnumber;
						Put(use_this_cptr->OtherQRnumbers, LKEY(qrbuf->QRnumber), QRnumber, NULL);
						use_this_cptr->roomlist_parts++;
					}
					citthread_mutex_unlock(&POP3QueueMutex);
					continue;
				}
				citthread_mutex_unlock(&RSSQueueMutex);
#endif

				citthread_mutex_lock(&POP3QueueMutex);
				Put(POP3FetchUrls, SKEY(cptr->Url), cptr, DeletePOP3Aggregator);
				citthread_mutex_unlock(&POP3QueueMutex);

			}

		}

		///fclose(fp);

	}
}


void pop3client_scan(void) {
	static time_t last_run = 0L;
	static int doing_pop3client = 0;
///	struct pop3aggr *pptr;
	time_t fastest_scan;
	HashPos *it;
	long len;
	const char *Key;
	void *vrptr;
	pop3aggr *cptr;

	if (config.c_pop3_fastest < config.c_pop3_fetch)
		fastest_scan = config.c_pop3_fastest;
	else
		fastest_scan = config.c_pop3_fetch;

	/*
	 * Run POP3 aggregation no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < fastest_scan ) {
		return;
	}

	/*
	 * This is a simple concurrency check to make sure only one pop3client run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_pop3client) return;
	doing_pop3client = 1;

	CtdlLogPrintf(CTDL_DEBUG, "pop3client started\n");
	CtdlForEachRoom(pop3client_scan_room, NULL);


	citthread_mutex_lock(&POP3QueueMutex);
	it = GetNewHashPos(POP3FetchUrls, 0);
	while (GetNextHashPos(POP3FetchUrls, it, &len, &Key, &vrptr) && 
	       (vrptr != NULL)) {
		cptr = (pop3aggr *)vrptr;
		if (cptr->RefCount == 0) 
			if (!pop3_do_fetching(cptr))
				DeletePOP3Aggregator(cptr);////TODO
	}
	DeleteHashPos(&it);
	citthread_mutex_unlock(&POP3QueueMutex);

	CtdlLogPrintf(CTDL_DEBUG, "pop3client ended\n");
	last_run = time(NULL);
	doing_pop3client = 0;
}


void pop3_cleanup(void)
{
	citthread_mutex_destroy(&POP3QueueMutex);
	DeleteHash(&POP3FetchUrls);
	DeleteHash(&POP3QueueRooms);
}

CTDL_MODULE_INIT(pop3client)
{
	if (!threading)
	{
		citthread_mutex_init(&POP3QueueMutex, NULL);
		POP3QueueRooms = NewHash(1, lFlathash);
		POP3FetchUrls = NewHash(1, NULL);
		CtdlRegisterSessionHook(pop3client_scan, EVT_TIMER);
                CtdlRegisterCleanupHook(pop3_cleanup);
	}
	/* return our Subversion id for the Log */
        return "pop3client";
}
