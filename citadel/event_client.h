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

typedef struct AsyncIO AsyncIO;

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

	int flushing;		/* if we read maxlen, read until nothing more arives and ignore this. */

	int crlf;		/* CRLF newlines instead of LF */
} ReadAsyncMsg;


typedef struct _DNSQueryParts {
	ParseDNSAnswerCb DNS_CB;
	IO_CallBack PostDNS;

	int DNSStatus;
	void *VParsedDNSReply;
	FreeDNSReply DNSReplyFree;
	void *Data;
} DNSQueryParts;

typedef struct _evcurl_request_data 
{
	CURL   		  *chnd;
	struct curl_slist *headers;
	char   		   errdesc[CURL_ERROR_SIZE];

	int    		   attached;

	char   		  *PlainPostData;
	long   		   PlainPostDataLen;
	StrBuf 		  *PostData;

	StrBuf 		  *ReplyData;
	long   		   httpcode;
} evcurl_request_data;

/* DNS Related */
typedef struct __evcares_data {
	ev_io recv_event, 
		send_event;
	ev_timer timeout;           /* timeout while requesting ips */
#ifdef DEBUG_CARES
	short int SourcePort;
#endif
	struct ares_options Options;
	ares_channel Channel;
	DNSQueryParts *Query;
	
	IO_CallBack Fail;      /* the dns lookup didn't work out. */
} evcares_data;

struct AsyncIO {
	long ID;
       	eNextState NextState;

	/* connection related */
	ParsedURL *ConnectMe;
	
	/* read/send related... */
	StrBuf *IOBuf;
	IOBuffer SendBuf, 
		RecvBuf;

	FDIOBuffer IOB; /* when sending from / reading into files, this is used. */

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
		Timeout,      /* Timeout handler; may also be connection timeout */
		ConnFail,     /* What to do when one connection failed? */
		ShutdownAbort,/* we're going down. make your piece. */ 
		NextDBOperation; /* Perform Database IO */

	IO_LineReaderCallback LineReader; /* if we have linereaders, maybe we want to read more lines before the real application logic is called? */

	evcares_data DNS;

	evcurl_request_data HttpReq;

	/* Saving / loading a message async from / to disk */
	ReadAsyncMsg *ReadMsg;
	struct CtdlMessage *AsyncMsg;
	struct recptypes *AsyncRcp;

	/* Custom data; its expected to contain  AsyncIO so we can save malloc()s... */
	void *Data;        /* application specific data */
	void *CitContext;  /* Citadel Session context... */
};

typedef struct _IOAddHandler {
	AsyncIO *IO;
	IO_CallBack EvAttch;
}IOAddHandler; 

#define CCID ((CitContext*)IO->CitContext)->cs_pid
#define EV_syslog(LEVEL, FORMAT, ...) syslog(LEVEL, "IO[%ld]CC[%d]" FORMAT, IO->ID, CCID, __VA_ARGS__)
#define EVM_syslog(LEVEL, FORMAT) syslog(LEVEL, "IO[%ld]CC[%d]" FORMAT, IO->ID, CCID)

#define EVNC_syslog(LEVEL, FORMAT, ...) syslog(LEVEL, "IO[%ld]" FORMAT, IO->ID, __VA_ARGS__)
#define EVNCM_syslog(LEVEL, FORMAT) syslog(LEVEL, "IO[%ld]" FORMAT, IO->ID)

void FreeAsyncIOContents(AsyncIO *IO);

eNextState NextDBOperation(AsyncIO *IO, IO_CallBack CB);
eNextState QueueDBOperation(AsyncIO *IO, IO_CallBack CB);
eNextState QueueEventContext(AsyncIO *IO, IO_CallBack CB);
eNextState QueueCurlContext(AsyncIO *IO);

eNextState InitEventIO(AsyncIO *IO, 
		       void *pData, 
		       double conn_timeout, 
		       double first_rw_timeout,
		       int ReadFirst);
void IO_postdns_callback(struct ev_loop *loop, ev_idle *watcher, int revents);

int QueueQuery(ns_type Type, const char *name, AsyncIO *IO, DNSQueryParts *QueryParts, IO_CallBack PostDNS);
void QueueGetHostByName(AsyncIO *IO, const char *Hostname, DNSQueryParts *QueryParts, IO_CallBack PostDNS);

void QueryCbDone(AsyncIO *IO);

void StopClient(AsyncIO *IO);

void StopClientWatchers(AsyncIO *IO);

void SetNextTimeout(AsyncIO *IO, double timeout);

void InitC_ares_dns(AsyncIO *IO);

#include <curl/curl.h>

#define OPT(s, v) \
	do { \
		sta = curl_easy_setopt(chnd, (CURLOPT_##s), (v)); \
		if (sta)  {						\
			syslog(LOG_ERR, "error setting option " #s " on curl handle: %s", curl_easy_strerror(sta)); \
	} } while (0)

int evcurl_init(AsyncIO *IO,
                void *CustomData,
                const char* Desc,
                IO_CallBack CallBack,
                IO_CallBack Terminate, 
		IO_CallBack ShutdownAbort);

eNextState ReAttachIO(AsyncIO *IO, 
		      void *pData, 
		      int ReadFirst);

#endif /* __EVENT_CLIENT_H__ */
