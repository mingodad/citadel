/*
 * Consolidate mail from remote POP3 accounts.
 *
 * Copyright (c) 2007-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as published
 * by the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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


#define POP3C_OK (strncasecmp(ChrPtr(RecvMsg->IO.IOBuf), "+OK", 3) == 0)
int Pop3ClientID = 0;
int POP3ClientDebugEnabled = 0;

#define N ((pop3aggr*)IO->Data)->n

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (POP3ClientDebugEnabled != 0))

#define EVP3C_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "IO[%ld]CC[%d][%ld]" FORMAT,		\
			     IO->ID, CCID, N, __VA_ARGS__)

#define EVP3CM_syslog(LEVEL, FORMAT)					\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "IO[%ld]CC[%d][%ld]" FORMAT,		\
			     IO->ID, CCID, N)

#define EVP3CQ_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "P3Q:" FORMAT,				\
			     __VA_ARGS__)

#define EVP3CQM_syslog(LEVEL, FORMAT)					\
	DBGLOG(LEVEL) syslog(LEVEL,					\
			     "P3Q" FORMAT				\
		)

#define EVP3CCS_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL, "IO[%ld][%ld]" FORMAT,		\
			     IO->ID, N, __VA_ARGS__)

#define EVP3CCSM_syslog(LEVEL, FORMAT)				\
	DBGLOG(LEVEL) syslog(LEVEL, "IO[%ld][%ld]" FORMAT,	\
			     IO->ID, N)

#define POP3C_DBG_SEND()						\
	EVP3C_syslog(LOG_DEBUG,						\
		     "POP3: > %s\n",					\
		     ChrPtr(RecvMsg->IO.SendBuf.Buf))

#define POP3C_DBG_READ()				\
	EVP3C_syslog(LOG_DEBUG,				\
		     "POP3: < %s\n",			\
		     ChrPtr(RecvMsg->IO.IOBuf))


struct CitContext pop3_client_CC;

pthread_mutex_t POP3QueueMutex; /* locks the access to the following vars: */
HashList *POP3QueueRooms = NULL;
HashList *POP3FetchUrls = NULL;

typedef struct pop3aggr pop3aggr;
typedef eNextState(*Pop3ClientHandler)(pop3aggr* RecvMsg);

eNextState POP3_C_Shutdown(AsyncIO *IO);
eNextState POP3_C_Timeout(AsyncIO *IO);
eNextState POP3_C_ConnFail(AsyncIO *IO);
eNextState POP3_C_DNSFail(AsyncIO *IO);
eNextState POP3_C_DispatchReadDone(AsyncIO *IO);
eNextState POP3_C_DispatchWriteDone(AsyncIO *IO);
eNextState POP3_C_Terminate(AsyncIO *IO);
eReadState POP3_C_ReadServerStatus(AsyncIO *IO);
eNextState POP3_C_ReAttachToFetchMessages(AsyncIO *IO);

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

struct pop3aggr {
	AsyncIO	 IO;

	long n;
	double IOStart;
	long count;
	long RefCount;
	DNSQueryParts HostLookup;

	long		 QRnumber;
	HashList	*OtherQRnumbers;

	StrBuf		*Url;
	StrBuf *pop3user;
	StrBuf *pop3pass;
	StrBuf *Host;
	StrBuf *RoomName; // TODO: fill me
	int keep;
	time_t interval;
	ePOP3_C_States State;
	HashList *MsgNumbers;
	HashPos *Pos;
	FetchItem *CurrMsg;
};

void DeletePOP3Aggregator(void *vptr)
{
	pop3aggr *ptr = vptr;
	DeleteHashPos(&ptr->Pos);
	DeleteHash(&ptr->MsgNumbers);
//	FreeStrBuf(&ptr->rooms);
	FreeStrBuf(&ptr->pop3user);
	FreeStrBuf(&ptr->pop3pass);
	FreeStrBuf(&ptr->Host);
	FreeStrBuf(&ptr->RoomName);
	FreeURL(&ptr->IO.ConnectMe);
	FreeStrBuf(&ptr->Url);
	FreeStrBuf(&ptr->IO.IOBuf);
	FreeStrBuf(&ptr->IO.SendBuf.Buf);
	FreeStrBuf(&ptr->IO.RecvBuf.Buf);
	DeleteAsyncMsg(&ptr->IO.ReadMsg);
	if (((struct CitContext*)ptr->IO.CitContext)) {
		((struct CitContext*)ptr->IO.CitContext)->state = CON_IDLE;
		((struct CitContext*)ptr->IO.CitContext)->kill_me = 1;
	}
	FreeAsyncIOContents(&ptr->IO);
	free(ptr);
}

eNextState FinalizePOP3AggrRun(AsyncIO *IO)
{
	HashPos  *It;
	pop3aggr *cpptr = (pop3aggr *)IO->Data;

	EVP3C_syslog(LOG_INFO,
		     "%s@%s: fetched %ld new of %d messages in %fs. bye.",
		     ChrPtr(cpptr->pop3user),
		     ChrPtr(cpptr->Host),
		     cpptr->count,
		     GetCount(cpptr->MsgNumbers), 
		     IO->Now - cpptr->IOStart 
		);

	It = GetNewHashPos(POP3FetchUrls, 0);
	pthread_mutex_lock(&POP3QueueMutex);
	{
		if (GetHashPosFromKey(POP3FetchUrls, SKEY(cpptr->Url), It))
			DeleteEntryFromHash(POP3FetchUrls, It);
	}
	pthread_mutex_unlock(&POP3QueueMutex);
	DeleteHashPos(&It);
	return eAbort;
}

eNextState FailAggregationRun(AsyncIO *IO)
{
	return eAbort;
}

eNextState POP3C_ReadGreeting(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	/* Read the server greeting */
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}

eNextState POP3C_SendUser(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	/* Identify ourselves.  NOTE: we have to append a CR to each command.
	 *  The LF will automatically be appended by sock_puts().  Believe it
	 * or not, leaving out the CR will cause problems if the server happens
	 * to be Exchange, which is so b0rken it actually barfs on
	 * LF-terminated newlines.
	 */
	StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
		     "USER %s\r\n", ChrPtr(RecvMsg->pop3user));
	POP3C_DBG_SEND();
	return eReadMessage;
}

eNextState POP3C_GetUserState(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}

eNextState POP3C_SendPassword(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	/* Password */
	StrBufPrintf(RecvMsg->IO.SendBuf.Buf,
		     "PASS %s\r\n", ChrPtr(RecvMsg->pop3pass));
	EVP3CM_syslog(LOG_DEBUG, "<PASS <password>\n");
//	POP3C_DBG_SEND(); No, we won't write the passvoid to syslog...
	return eReadMessage;
}

eNextState POP3C_GetPassState(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	else return eSendReply;
}

eNextState POP3C_SendListCommand(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	/* Get the list of messages */
	StrBufPlain(RecvMsg->IO.SendBuf.Buf, HKEY("LIST\r\n"));
	POP3C_DBG_SEND();
	return eReadMessage;
}

eNextState POP3C_GetListCommandState(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	RecvMsg->MsgNumbers = NewHash(1, NULL);
	RecvMsg->State++;
	return eReadMore;
}


eNextState POP3C_GetListOneLine(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
#if 0
	int rc;
#endif
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

	/*
	 * work around buggy pop3 servers which send
	 * empty lines in their listings.
	*/
	if ((StrLength(RecvMsg->IO.IOBuf) == 0) ||
	    !isdigit(ChrPtr(RecvMsg->IO.IOBuf)[0]))
	{
		return eReadMore;
	}

	OneMsg = (FetchItem*) malloc(sizeof(FetchItem));
	memset(OneMsg, 0, sizeof(FetchItem));
	OneMsg->MSGID = atol(ChrPtr(RecvMsg->IO.IOBuf));

	pch = strchr(ChrPtr(RecvMsg->IO.IOBuf), ' ');
	if (pch != NULL)
	{
		OneMsg->MSGSize = atol(pch + 1);
	}
#if 0
	rc = TestValidateHash(RecvMsg->MsgNumbers);
	if (rc != 0)
		EVP3CCS_syslog(LOG_DEBUG, "Hash Invalid: %d\n", rc);
#endif

	Put(RecvMsg->MsgNumbers, LKEY(OneMsg->MSGID), OneMsg, HfreeFetchItem);
#if 0
	rc = TestValidateHash(RecvMsg->MsgNumbers);
	if (rc != 0)
		EVP3CCS_syslog(LOG_DEBUG, "Hash Invalid: %d\n", rc);
#endif
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

	if((RecvMsg->Pos != NULL) &&
	   GetNextHashPos(RecvMsg->MsgNumbers,
			  RecvMsg->Pos,
			  &HKLen,
			  &HKey,
			  &vData))
	{
		struct UseTable ut;
		if (server_shutting_down)
			return eAbort;

		RecvMsg->CurrMsg = (FetchItem*) vData;
		EVP3CCS_syslog(LOG_DEBUG,
			       "CHECKING: whether %s has already been seen: ",
			       ChrPtr(RecvMsg->CurrMsg->MsgUID));

		/* Find out if we've already seen this item */
		safestrncpy(ut.ut_msgid,
			    ChrPtr(RecvMsg->CurrMsg->MsgUID),
			    sizeof(ut.ut_msgid));
		ut.ut_timestamp = time(NULL);/// TODO: libev timestamp!

		cdbut = cdb_fetch(CDB_USETABLE, SKEY(RecvMsg->CurrMsg->MsgUID));
		if (cdbut != NULL) {
			/* Item has already been seen */
			EVP3CCSM_syslog(LOG_DEBUG, "YES\n");
			cdb_free(cdbut);

			/* rewrite the record anyway, to update the timestamp */
			cdb_store(CDB_USETABLE,
				  SKEY(RecvMsg->CurrMsg->MsgUID),
				  &ut, sizeof(struct UseTable) );
			RecvMsg->CurrMsg->NeedFetch = 0; ////TODO0;
		}
		else
		{
			EVP3CCSM_syslog(LOG_DEBUG, "NO\n");
			RecvMsg->CurrMsg->NeedFetch = 1;
		}
		return NextDBOperation(&RecvMsg->IO,
				       POP3_FetchNetworkUsetableEntry);
	}
	else
	{
		/* ok, now we know them all,
		 * continue with reading the actual messages. */
		DeleteHashPos(&RecvMsg->Pos);
		StopDBWatchers(IO);
		return QueueEventContext(IO, POP3_C_ReAttachToFetchMessages);
	}
}

eNextState POP3C_GetOneMessagID(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	long HKLen;
	const char *HKey;
	void *vData;

#if 0
	int rc;
	rc = TestValidateHash(RecvMsg->MsgNumbers);
	if (rc != 0)
		EVP3CCS_syslog(LOG_DEBUG, "Hash Invalid: %d\n", rc);
#endif
	if((RecvMsg->Pos != NULL) &&
	   GetNextHashPos(RecvMsg->MsgNumbers,
			  RecvMsg->Pos,
			  &HKLen, &HKey,
			  &vData))
	{
		RecvMsg->CurrMsg = (FetchItem*) vData;
		/* Find out the UIDL of the message,
		 * to determine whether we've already downloaded it */
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
		return QueueDBOperation(&RecvMsg->IO,
					POP3_FetchNetworkUsetableEntry);
	}
	return eReadMore; /* TODO */
}

eNextState POP3C_GetOneMessageIDState(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
#if 0
	int rc;
	rc = TestValidateHash(RecvMsg->MsgNumbers);
	if (rc != 0)
		EVP3CCS_syslog(LOG_DEBUG, "Hash Invalid: %d\n", rc);
#endif

	POP3C_DBG_READ();
	if (!POP3C_OK) return eTerminateConnection;
	RecvMsg->CurrMsg->MsgUIDL =
		NewStrBufPlain(NULL, StrLength(RecvMsg->IO.IOBuf));
	RecvMsg->CurrMsg->MsgUID =
		NewStrBufPlain(NULL, StrLength(RecvMsg->IO.IOBuf) * 2);

	StrBufExtract_token(RecvMsg->CurrMsg->MsgUIDL,
			    RecvMsg->IO.IOBuf, 2, ' ');

	StrBufPrintf(RecvMsg->CurrMsg->MsgUID,
		     "pop3/%s/%s:%s@%s",
		     ChrPtr(RecvMsg->RoomName),
		     ChrPtr(RecvMsg->CurrMsg->MsgUIDL),
		     RecvMsg->IO.ConnectMe->User,
		     RecvMsg->IO.ConnectMe->Host);
	RecvMsg->State --;
	return eSendReply;
}


eNextState POP3C_SendGetOneMsg(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	long HKLen;
	const char *HKey;
	void *vData;

	RecvMsg->CurrMsg = NULL;
	while ((RecvMsg->Pos != NULL) && 
	       GetNextHashPos(RecvMsg->MsgNumbers,
			      RecvMsg->Pos,
			      &HKLen, &HKey,
			      &vData) &&
	       (RecvMsg->CurrMsg = (FetchItem*) vData,
		RecvMsg->CurrMsg->NeedFetch == 0))
	{}

	if ((RecvMsg->CurrMsg != NULL ) && (RecvMsg->CurrMsg->NeedFetch == 1))
	{
		/* Message has not been seen.
		 * Tell the server to fetch the message... */
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
	AsyncIO *IO = &RecvMsg->IO;
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

	EVP3CCS_syslog(LOG_DEBUG,
		       "MARKING: %s as seen: ",
		       ChrPtr(RecvMsg->CurrMsg->MsgUID));

	safestrncpy(ut.ut_msgid,
		    ChrPtr(RecvMsg->CurrMsg->MsgUID),
		    sizeof(ut.ut_msgid));
	ut.ut_timestamp = time(NULL); /* TODO: use libev time */
	cdb_store(CDB_USETABLE,
		  ChrPtr(RecvMsg->CurrMsg->MsgUID),
		  StrLength(RecvMsg->CurrMsg->MsgUID),
		  &ut,
		  sizeof(struct UseTable) );
	StopDBWatchers(IO);
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
	if (msgnum > 0L)
	{
		/* Message has been committed to the store
		 * write the uidl to the use table
		 * so we don't fetch this message again
		 */
	}
	CtdlFreeMessage(RecvMsg->CurrMsg->Msg);

	RecvMsg->count ++;
	return NextDBOperation(&RecvMsg->IO, POP3C_StoreMsgRead);
}

eNextState POP3C_ReadMessageBody(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	EVP3CM_syslog(LOG_DEBUG, "Converting message...");
	RecvMsg->CurrMsg->Msg =
		convert_internet_message_buf(&RecvMsg->IO.ReadMsg->MsgBuf);
	StopClientWatchers(IO, 0);
	return QueueDBOperation(&RecvMsg->IO, POP3C_SaveMsg);
}

eNextState POP3C_SendDelete(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
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
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	RecvMsg->State = GetOneMessageIDState;
	return eReadMessage;
}

eNextState POP3C_SendQuit(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	/* Log out */
	StrBufPlain(RecvMsg->IO.SendBuf.Buf,
		    HKEY("QUIT\r\n3)"));
	POP3C_DBG_SEND();
	return eReadMessage;
}


eNextState POP3C_ReadQuitState(pop3aggr *RecvMsg)
{
	AsyncIO *IO = &RecvMsg->IO;
	POP3C_DBG_READ();
	return eTerminateConnection;
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
	AsyncIO *IO = &pMsg->IO;
	double Timeout = 0.0;

	EVP3C_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);

	switch (NextTCPState) {
	case eSendFile:
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
	case eReadFile:
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
/*	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__); to noisy anyways. */
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
	pop3aggr *pMsg = IO->Data;
	eNextState rc;

/*	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__); to noisy anyways. */
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

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
	FinalizePOP3AggrRun(IO);
	return eAbort;
}
eNextState POP3_C_TerminateDB(AsyncIO *IO)
{
///	pop3aggr *pMsg = (pop3aggr *)IO->Data;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
	FinalizePOP3AggrRun(IO);
	return eAbort;
}
eNextState POP3_C_Timeout(AsyncIO *IO)
{
	pop3aggr *pMsg = IO->Data;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State]));
	return FailAggregationRun(IO);
}
eNextState POP3_C_ConnFail(AsyncIO *IO)
{
	pop3aggr *pMsg = (pop3aggr *)IO->Data;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State]));
	return FailAggregationRun(IO);
}
eNextState POP3_C_DNSFail(AsyncIO *IO)
{
	pop3aggr *pMsg = (pop3aggr *)IO->Data;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
	StrBufPlain(IO->ErrMsg, CKEY(POP3C_ReadErrors[pMsg->State]));
	return FailAggregationRun(IO);
}
eNextState POP3_C_Shutdown(AsyncIO *IO)
{
	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
////	pop3aggr *pMsg = IO->Data;

////pMsg->MyQEntry->Status = 3;
///StrBufPlain(pMsg->MyQEntry->StatusMessage, HKEY("server shutdown during message retrieval."));
	FinalizePOP3AggrRun(IO);
	return eAbort;
}


/**
 * @brief lineread Handler; understands when to read more POP3 lines,
 *   and when this is a one-lined reply.
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
	case eSendFile:
	case eSendReply:
	case eSendMore:
	case eReadMore:
	case eReadMessage:
		Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
		break;
	case eReadFile:
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

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);
////???	cpptr->State ++;
	if (cpptr->Pos == NULL)
		cpptr->Pos = GetNewHashPos(cpptr->MsgNumbers, 0);

	POP3_C_DispatchWriteDone(IO);
	ReAttachIO(IO, cpptr, 0);
	IO->NextState = eReadMessage;
	return IO->NextState;
}

eNextState pop3_connect_ip(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;

	if (cpptr->IOStart == 0.0) /* whith or without DNS? */
		cpptr->IOStart = IO->Now;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);

	return EvConnectSock(IO,
			     POP3_C_ConnTimeout,
			     POP3_C_ReadTimeouts[0],
			     1);
}

eNextState pop3_get_one_host_ip_done(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;
	struct hostent *hostent;

	QueryCbDone(IO);

	hostent = cpptr->HostLookup.VParsedDNSReply;
	if ((cpptr->HostLookup.DNSStatus == ARES_SUCCESS) && 
	    (hostent != NULL) ) {
		memset(&cpptr->IO.ConnectMe->Addr, 0, sizeof(struct in6_addr));
		if (cpptr->IO.ConnectMe->IPv6) {
			memcpy(&cpptr->IO.ConnectMe->Addr.sin6_addr.s6_addr, 
			       &hostent->h_addr_list[0],
			       sizeof(struct in6_addr));

			cpptr->IO.ConnectMe->Addr.sin6_family =
				hostent->h_addrtype;
			cpptr->IO.ConnectMe->Addr.sin6_port   =
				htons(DefaultPOP3Port);
		}
		else {
			struct sockaddr_in *addr =
				(struct sockaddr_in*)
				&cpptr->IO.ConnectMe->Addr;

			memcpy(&addr->sin_addr.s_addr,
			       hostent->h_addr_list[0],
			       sizeof(uint32_t));

			addr->sin_family = hostent->h_addrtype;
			addr->sin_port   = htons(DefaultPOP3Port);
		}
		return pop3_connect_ip(IO);
	}
	else
		return eAbort;
}

eNextState pop3_get_one_host_ip(AsyncIO *IO)
{
	pop3aggr *cpptr = IO->Data;

	cpptr->IOStart = IO->Now;

	EVP3CCS_syslog(LOG_DEBUG, "POP3: %s\n", __FUNCTION__);

	EVP3CCS_syslog(LOG_DEBUG, 
		       "POP3 client[%ld]: looking up %s-Record %s : %d ...\n",
		       cpptr->n,
		       (cpptr->IO.ConnectMe->IPv6)? "aaaa": "a",
		       cpptr->IO.ConnectMe->Host,
		       cpptr->IO.ConnectMe->Port);

	QueueQuery((cpptr->IO.ConnectMe->IPv6)? ns_t_aaaa : ns_t_a,
		   cpptr->IO.ConnectMe->Host,
		   &cpptr->IO,
		   &cpptr->HostLookup,
		   pop3_get_one_host_ip_done);
	IO->NextState = eReadDNSReply;
	return IO->NextState;
}



int pop3_do_fetching(pop3aggr *cpptr)
{
	AsyncIO *IO = &cpptr->IO;

	InitIOStruct(IO,
		     cpptr,
		     eReadMessage,
		     POP3_C_ReadServerStatus,
		     POP3_C_DNSFail,
		     POP3_C_DispatchWriteDone,
		     POP3_C_DispatchReadDone,
		     POP3_C_Terminate,
		     POP3_C_TerminateDB,
		     POP3_C_ConnFail,
		     POP3_C_Timeout,
		     POP3_C_Shutdown);

	safestrncpy(((CitContext *)cpptr->IO.CitContext)->cs_host,
		    ChrPtr(cpptr->Url),
		    sizeof(((CitContext *)cpptr->IO.CitContext)->cs_host));

	if (cpptr->IO.ConnectMe->IsIP) {
		QueueEventContext(&cpptr->IO,
				  pop3_connect_ip);
	}
	else {
		QueueEventContext(&cpptr->IO,
				  pop3_get_one_host_ip);
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

//	pop3_room_counter *Count = NULL;
//	pop3aggr *cpptr;

	pthread_mutex_lock(&POP3QueueMutex);
	if (GetHash(POP3QueueRooms, LKEY(qrbuf->QRnumber), &vptr))
	{
		pthread_mutex_unlock(&POP3QueueMutex);
		EVP3CQ_syslog(LOG_DEBUG,
			      "pop3client: [%ld] %s already in progress.",
			      qrbuf->QRnumber,
			      qrbuf->QRname);
		return;
	}
	pthread_mutex_unlock(&POP3QueueMutex);

	if (server_shutting_down) return;

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	if (server_shutting_down)
		return;

	/* Only do net processing for rooms that have netconfigs */
	fd = open(filename, 0);
	if (fd <= 0) {
		return;
	}
	if (server_shutting_down)
		return;
	if (fstat(fd, &statbuf) == -1) {
		EVP3CQ_syslog(LOG_INFO,
			      "ERROR: could not stat configfile '%s' - %s",
			      filename,
			      strerror(errno));
		return;
	}
	if (server_shutting_down)
		return;
	CfgData = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(CfgData, &fd, 1, statbuf.st_size, &Err) < 0) {
		close(fd);
		FreeStrBuf(&CfgData);
		EVP3CQ_syslog(LOG_INFO,
			      "ERROR: reading config '%s' - %s",
			      filename, strerror(errno));
		return;
	}
	close(fd);
	if (server_shutting_down)
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
/*
				if (Count == NULL)
				{
				Count = malloc(sizeof(pop3_room_counter));
					Count->count = 0;
				}
				Count->count ++;
*/
				cptr = (pop3aggr *) malloc(sizeof(pop3aggr));
				memset(cptr, 0, sizeof(pop3aggr));
				///TODO do we need this? cptr->roomlist_parts=1;
				cptr->RoomName =
					NewStrBufPlain(qrbuf->QRname, -1);
				cptr->pop3user =
					NewStrBufPlain(NULL, StrLength(Line));
				cptr->pop3pass =
					NewStrBufPlain(NULL, StrLength(Line));
				cptr->Url = NewStrBuf();
				cptr->Host =
					NewStrBufPlain(NULL, StrLength(Line));

				StrBufExtract_NextToken(cptr->Host, Line, &lPtr, '|');
				StrBufExtract_NextToken(cptr->pop3user,
							Line,
							&lPtr,
							'|');

				StrBufExtract_NextToken(cptr->pop3pass,
							Line,
							&lPtr,
							'|');

				cptr->keep = StrBufExtractNext_long(Line,
								    &lPtr,
								    '|');

				cptr->interval = StrBufExtractNext_long(Line,
									&lPtr,
									'|');

				StrBufAppendBufPlain(cptr->Url, HKEY("pop3://"), 0);
				StrBufUrlescUPAppend(cptr->Url, cptr->pop3user, NULL);
				StrBufAppendBufPlain(cptr->Url, HKEY(":"), 0);
				StrBufUrlescUPAppend(cptr->Url, cptr->pop3pass, NULL);
				StrBufAppendBufPlain(cptr->Url, HKEY("@"), 0);
				StrBufAppendBuf(cptr->Url, cptr->Host, 0);
				StrBufAppendBufPlain(cptr->Url, HKEY("/"), 0);
				StrBufUrlescAppend(cptr->Url, cptr->RoomName, NULL);

				ParseURL(&cptr->IO.ConnectMe, cptr->Url, 110);


#if 0
/* todo: we need to reunite the url to be shure. */

				pthread_mutex_lock(&POP3ueueMutex);
				GetHash(POP3FetchUrls, SKEY(ptr->Url), &vptr);
				use_this_cptr = (pop3aggr *)vptr;

				if (use_this_rncptr != NULL)
				{
					/* mustn't attach to an active session */
					if (use_this_cptr->RefCount > 0)
					{
						DeletePOP3Cfg(cptr);
///						Count->count--;
					}
					else
					{
						long *QRnumber;
						StrBufAppendBufPlain(
							use_this_cptr->rooms,
							qrbuf->QRname,
							-1, 0);
						if (use_this_cptr->roomlist_parts == 1)
						{
							use_this_cptr->OtherQRnumbers
								= NewHash(1, lFlathash);
						}
						QRnumber = (long*)malloc(sizeof(long));
						*QRnumber = qrbuf->QRnumber;
						Put(use_this_cptr->OtherQRnumbers,
						    LKEY(qrbuf->QRnumber),
						    QRnumber,
						    NULL);

						use_this_cptr->roomlist_parts++;
					}
					pthread_mutex_unlock(&POP3QueueMutex);
					continue;
				}
				pthread_mutex_unlock(&RSSQueueMutex);
#endif
				cptr->n = Pop3ClientID++;
				pthread_mutex_lock(&POP3QueueMutex);
				Put(POP3FetchUrls,
				    SKEY(cptr->Url),
				    cptr,
				    DeletePOP3Aggregator);

				pthread_mutex_unlock(&POP3QueueMutex);

			}

		}

		///fclose(fp);

	}
	FreeStrBuf(&Line);
	FreeStrBuf(&CfgType);
	FreeStrBuf(&CfgData);
}

static int doing_pop3client = 0;

void pop3client_scan(void) {
	static time_t last_run = 0L;
	time_t fastest_scan;
	HashPos *it;
	long len;
	const char *Key;
	void *vrptr;
	pop3aggr *cptr;

	become_session(&pop3_client_CC);

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
	 * This is a simple concurrency check to make sure only one pop3client
	 * run is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_pop3client) return;
	doing_pop3client = 1;

	EVP3CQM_syslog(LOG_DEBUG, "pop3client started");
	CtdlForEachRoom(pop3client_scan_room, NULL);

	pthread_mutex_lock(&POP3QueueMutex);
	it = GetNewHashPos(POP3FetchUrls, 0);
	while (!server_shutting_down &&
	       GetNextHashPos(POP3FetchUrls, it, &len, &Key, &vrptr) &&
	       (vrptr != NULL)) {
		cptr = (pop3aggr *)vrptr;
		if (cptr->RefCount == 0)
			if (!pop3_do_fetching(cptr))
				DeletePOP3Aggregator(cptr);////TODO

/*
	if ((palist->interval && time(NULL) > (last_run + palist->interval))
			|| (time(NULL) > last_run + config.c_pop3_fetch))
			pop3_do_fetching(palist->roomname, palist->pop3host,
			palist->pop3user, palist->pop3pass, palist->keep);
		pptr = palist;
		palist = palist->next;
		free(pptr);
*/
	}
	DeleteHashPos(&it);
	pthread_mutex_unlock(&POP3QueueMutex);

	EVP3CQM_syslog(LOG_DEBUG, "pop3client ended");
	last_run = time(NULL);
	doing_pop3client = 0;
}


void pop3_cleanup(void)
{
	/* citthread_mutex_destroy(&POP3QueueMutex); TODO */
	while (doing_pop3client != 0) ;
	DeleteHash(&POP3FetchUrls);
	DeleteHash(&POP3QueueRooms);
}



void LogDebugEnablePOP3Client(const int n)
{
	POP3ClientDebugEnabled = n;
}

CTDL_MODULE_INIT(pop3client)
{
	if (!threading)
	{
		CtdlFillSystemContext(&pop3_client_CC, "POP3aggr");
		pthread_mutex_init(&POP3QueueMutex, NULL);
		POP3QueueRooms = NewHash(1, lFlathash);
		POP3FetchUrls = NewHash(1, NULL);
		CtdlRegisterSessionHook(pop3client_scan, EVT_TIMER, PRIO_AGGR + 50);
		CtdlRegisterEVCleanupHook(pop3_cleanup);
		CtdlRegisterDebugFlagHook(HKEY("pop3client"), LogDebugEnablePOP3Client, &POP3ClientDebugEnabled);
	}

	/* return our module id for the log */
 	return "pop3client";
}
