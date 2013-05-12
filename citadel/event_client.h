/*
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License, version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#ifndef __EVENT_CLIENT_H__
#define __EVENT_CLIENT_H__
#define EV_COMPAT3 0
#include "sysconfig.h"
#include <ev.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <ares.h>
#include <curl/curl.h>

#ifndef __ASYNCIO__
#define __ASYNCIO__
typedef struct AsyncIO AsyncIO;
#endif
#ifndef __CIT_CONTEXT__
#define __CIT_CONTEXT__
typedef struct CitContext CitContext;
#endif

typedef enum __eIOState { 
	eDBQ,
	eQDBNext,
	eDBAttach,
	eDBNext,
	eDBStop,
	eDBX,
	eDBTerm,
	eIOQ,
	eIOAttach,
	eIOConnectSock,
	eIOAbort,
	eIOTimeout,
	eIOConnfail,
	eIOConnfailNow,
	eIOConnNow,
	eIOConnWait,
	eCurlQ,
	eCurlStart,
	eCurlShutdown,
	eCurlNewIO,
	eCurlGotIO,
	eCurlGotData,
	eCurlGotStatus,
	eCaresStart,
	eCaresDoneIO,
	eCaresFinished,
	eCaresX,
	eKill,
	eExit
}eIOState;

typedef enum _eNextState {
	eSendDNSQuery,
	eReadDNSReply,

	eDBQuery,

	eConnect,
	eSendReply,
	eSendMore,
	eSendFile,

	eReadMessage,
	eReadMore,
	eReadPayload,
	eReadFile,

	eTerminateConnection,
	eAbort
}eNextState;

void SetEVState(AsyncIO *IO, eIOState State);

typedef eNextState (*IO_CallBack)(AsyncIO *IO);
typedef eReadState (*IO_LineReaderCallback)(AsyncIO *IO);
typedef void (*ParseDNSAnswerCb)(AsyncIO*, unsigned char*, int);
typedef void (*FreeDNSReply)(void *DNSData);


typedef struct __ReadAsyncMsg {
	StrBuf *MsgBuf;
	size_t maxlen;		/* maximum message length */

	const char *terminator;	/* token signalling EOT */
	long tlen;
	int dodot;

	int flushing;
/* if we read maxlen, read until nothing more arives and ignore this. */

	int crlf;		/* CRLF newlines instead of LF */
} ReadAsyncMsg;


typedef struct _DNSQueryParts {
	ParseDNSAnswerCb DNS_CB;
	IO_CallBack PostDNS;

	const char *QueryTYPE;
	const char *QStr;
	int DNSStatus;
	void *VParsedDNSReply;
	FreeDNSReply DNSReplyFree;
	void *Data;
} DNSQueryParts;

typedef struct _evcurl_request_data
{
	CURL			*chnd;
	struct curl_slist	*headers;
	char			 errdesc[CURL_ERROR_SIZE];

	int			 attached;

	char			*PlainPostData;
	long			 PlainPostDataLen;
	StrBuf			*PostData;

	StrBuf			*ReplyData;
	long			 httpcode;
} evcurl_request_data;

/* DNS Related */
typedef struct __evcares_data {
	ev_tstamp Start;
	ev_io recv_event,
		send_event;
	ev_timer timeout;           /* timeout while requesting ips */
	short int SourcePort;

	struct ares_options Options;
	ares_channel Channel;
	DNSQueryParts *Query;

	IO_CallBack Fail;      /* the dns lookup didn't work out. */
} evcares_data;

struct AsyncIO {
	long ID;
	ev_tstamp Now;
	ev_tstamp StartIO;
	ev_tstamp StartDB;
	eNextState NextState;

	/* connection related */
	ParsedURL *ConnectMe;

	/* read/send related... */
	StrBuf *IOBuf;
	IOBuffer SendBuf,
		RecvBuf;

	FDIOBuffer IOB;
	/* when sending from / reading into files, this is used. */

	/* our events... */
	ev_cleanup abort_by_shutdown, /* server wants to go down... */
		db_abort_by_shutdown; /* server wants to go down... */
	ev_timer conn_fail,           /* connection establishing timed out */
		rw_timeout;           /* timeout while sending data */
	ev_idle unwind_stack,         /* get c-ares out of the stack */
		db_unwind_stack,      /* wait for next db operation... */
		conn_fail_immediate;  /* unwind stack, but fail immediately. */
	ev_io recv_event,             /* receive data from the client */
		send_event,           /* send more data to the client */
		conn_event;           /* Connection successfully established */

	StrBuf *ErrMsg; /* if we fail to connect, or lookup, error goes here. */

	/* Citadel application callbacks... */
	IO_CallBack ReadDone, /* Theres new data to read... */
		SendDone,     /* we may send more data */
		Terminate,    /* shutting down... */
		DBTerminate,  /* shutting down... */
		Timeout,      /* Timeout handler;may also be conn. timeout */
		ConnFail,     /* What to do when one connection failed? */
		ShutdownAbort,/* we're going down. make your piece. */
		NextDBOperation; /* Perform Database IO */

	/* if we have linereaders, maybe we want to read more lines before
	 * the real application logic is called? */
	IO_LineReaderCallback LineReader;

	evcares_data DNS;

	evcurl_request_data HttpReq;

	/* Saving / loading a message async from / to disk */
	ReadAsyncMsg *ReadMsg;
	struct CtdlMessage *AsyncMsg;
	struct recptypes *AsyncRcp;

	/* Context specific data; Hint: put AsyncIO in there */
	void *Data;        /* application specific data */
	CitContext *CitContext;  /* Citadel Session context... */
};

typedef struct _IOAddHandler {
	AsyncIO *IO;
	IO_CallBack EvAttch;
} IOAddHandler;



extern int DebugEventLoop;
extern int DebugCAres;

#define EDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (DebugEventLoop != 0))

#define CCID ((CitContext*)IO->CitContext)->cs_pid

#define EVQ_syslog(LEVEL, FORMAT, ...)					\
	EDBGLOG (LEVEL) syslog(LEVEL, "IOQ " FORMAT, __VA_ARGS__)

#define EVQM_syslog(LEVEL, FORMAT)			\
	EDBGLOG (LEVEL) syslog(LEVEL, "IO " FORMAT)

#define EV_syslog(LEVEL, FORMAT, ...)					\
	EDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld]CC[%d] " FORMAT, IO->ID, CCID, __VA_ARGS__)

#define EVM_syslog(LEVEL, FORMAT)					\
	EDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld]CC[%d] " FORMAT, IO->ID, CCID)

#define EVNC_syslog(LEVEL, FORMAT, ...)					\
	EDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld] " FORMAT, IO->ID, __VA_ARGS__)

#define EVNCM_syslog(LEVEL, FORMAT) EDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld]" FORMAT, IO->ID)


#define CDBGLOG() if (DebugCAres != 0)
#define CEDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (DebugCAres != 0))
#define EV_DNS_LOG_START(a)							\
	CDBGLOG () {syslog(LOG_DEBUG, "IO[%ld]CC[%d] + Starting " #a " %s %p FD %d", IO->ID, CCID, __FUNCTION__, &IO->a, IO->a.fd); \
		    EV_backtrace(IO);}

#define EV_DNS_LOG_STOP(a)							\
	CDBGLOG () { syslog(LOG_DEBUG, "IO[%ld]CC[%d] - Stopping " #a " %s %p FD %d", IO->ID, CCID, __FUNCTION__, &IO->a, IO->a.fd); \
		     EV_backtrace(IO);}

#define EV_DNS_LOG_INIT(a)							\
	CDBGLOG () { syslog(LOG_DEBUG, "IO[%ld]CC[%d] * Init " #a " %s %p FD %d", IO->ID, CCID, __FUNCTION__, &IO->a, IO->a.fd); \
		     EV_backtrace(IO);}

#define EV_DNS_LOGT_START(a)							\
	CDBGLOG () { syslog(LOG_DEBUG, "IO[%ld]CC[%d] + Starting " #a " %s %p", IO->ID, CCID, __FUNCTION__, &IO->a); \
		     EV_backtrace(IO);}

#define EV_DNS_LOGT_STOP(a)							\
	CDBGLOG () { syslog(LOG_DEBUG, "IO[%ld]CC[%d] - Stopping " #a " %s %p", IO->ID, CCID, __FUNCTION__, &IO->a); \
		     EV_backtrace(IO); }

#define EV_DNS_LOGT_INIT(a)							\
	CDBGLOG () { syslog(LOG_DEBUG, "IO[%ld]CC[%d] * Init " #a " %p", IO->ID, CCID, &IO->a); \
		     EV_backtrace(IO);}

#define EV_DNS_syslog(LEVEL, FORMAT, ...)				\
	CEDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld]CC[%d] " FORMAT, IO->ID, CCID, __VA_ARGS__)

#define EVM_DNS_syslog(LEVEL, FORMAT)					\
	CEDBGLOG (LEVEL) syslog(LEVEL, "IO[%ld]CC[%d] " FORMAT, IO->ID, CCID)

void FreeAsyncIOContents(AsyncIO *IO);

eNextState NextDBOperation(AsyncIO *IO, IO_CallBack CB);
eNextState QueueDBOperation(AsyncIO *IO, IO_CallBack CB);
void StopDBWatchers(AsyncIO *IO);
eNextState QueueEventContext(AsyncIO *IO, IO_CallBack CB);
eNextState QueueCurlContext(AsyncIO *IO);

eNextState EvConnectSock(AsyncIO *IO,
			 double conn_timeout,
			 double first_rw_timeout,
			 int ReadFirst);
void IO_postdns_callback(struct ev_loop *loop, ev_idle *watcher, int revents);

int QueueQuery(ns_type Type,
	       const char *name,
	       AsyncIO *IO,
	       DNSQueryParts *QueryParts,
	       IO_CallBack PostDNS);

void QueueGetHostByName(AsyncIO *IO,
			const char *Hostname,
			DNSQueryParts *QueryParts,
			IO_CallBack PostDNS);

void QueryCbDone(AsyncIO *IO);

void StopClient(AsyncIO *IO);

void StopClientWatchers(AsyncIO *IO, int CloseFD);

void SetNextTimeout(AsyncIO *IO, double timeout);

#include <curl/curl.h>

#define OPT(s, v) \
	do { \
		sta = curl_easy_setopt(chnd, (CURLOPT_##s), (v));	\
		if (sta)  {						\
			EVQ_syslog(LOG_ERR,				\
			       "error setting option " #s		\
			       " on curl handle: %s",			\
			       curl_easy_strerror(sta));		\
	} } while (0)

void InitIOStruct(AsyncIO *IO,
		  void *Data,
		  eNextState NextState,
		  IO_LineReaderCallback LineReader,
		  IO_CallBack DNS_Fail,
		  IO_CallBack SendDone,
		  IO_CallBack ReadDone,
		  IO_CallBack Terminate,
		  IO_CallBack DBTerminate,
		  IO_CallBack ConnFail,
		  IO_CallBack Timeout,
		  IO_CallBack ShutdownAbort);

int InitcURLIOStruct(AsyncIO *IO,
		     void *Data,
		     const char* Desc,
		     IO_CallBack SendDone,
		     IO_CallBack Terminate,
		     IO_CallBack DBTerminate,
		     IO_CallBack ShutdownAbort);
void KillAsyncIOContext(AsyncIO *IO);
void StopCurlWatchers(AsyncIO *IO);


eNextState ReAttachIO(AsyncIO *IO,
		      void *pData,
		      int ReadFirst);

void EV_backtrace(AsyncIO *IO);
ev_tstamp ctdl_ev_now (void);

#endif /* __EVENT_CLIENT_H__ */
