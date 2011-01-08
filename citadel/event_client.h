#include <ev.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/nameser.h>
#include <ares.h>

typedef struct AsyncIO AsyncIO;

typedef enum _eNextState {
	eSendReply, 
	eSendMore,
	eReadMessage, 
	eTerminateConnection,
	eAbort
}eNextState;

typedef int (*EventContextAttach)(void *Data);
typedef eNextState (*IO_CallBack)(void *Data);
typedef eReadState (*IO_LineReaderCallback)(AsyncIO *IO);
typedef void (*ParseDNSAnswerCb)(AsyncIO*, unsigned char*, int);
typedef void (*FreeDNSReply)(void *DNSData);

struct AsyncIO {
	StrBuf *Host;
	char service[32];

	/* To cycle through several possible services... */
	struct addrinfo *res;
	struct addrinfo *curr_ai;

	/* connection related */
	int IP6;
	struct hostent *HEnt;
	int sock;
	int active_event;
       	eNextState NextState;
	ev_io recv_event, 
		send_event, 
		dns_io_event;
	StrBuf *ErrMsg; /* if we fail to connect, or lookup, error goes here. */

	/* read/send related... */
	StrBuf *IOBuf;
	IOBuffer SendBuf, 
		RecvBuf;

	/* Citadel application callbacks... */
	IO_CallBack ReadDone, /* Theres new data to read... */
		SendDone,     /* we may send more data */
		Terminate,    /* shutting down... */
		Timeout,      /* Timeout handler; may also be connection timeout */
		ConnFail;     /* What to do when one connection failed? */

	IO_LineReaderCallback LineReader; /* if we have linereaders, maybe we want to read more lines before the real application logic is called? */

	struct ares_options DNSOptions;
	ares_channel DNSChannel;
	ParseDNSAnswerCb DNS_CB;
	IO_CallBack PostDNS;
	int DNSStatus;
	void *VParsedDNSReply;
	FreeDNSReply DNSReplyFree;

	/* Custom data; its expected to contain  AsyncIO so we can save malloc()s... */
	DeleteHashDataFunc DeleteData; /* so if we have to destroy you, what to do... */
	void *Data; /* application specific data */
};

typedef struct _IOAddHandler {
	void *Ctx;
	EventContextAttach EvAttch;
}IOAddHandler; 

void FreeAsyncIOContents(AsyncIO *IO);

int QueueEventContext(void *Ctx, AsyncIO *IO, EventContextAttach CB);
int ShutDownEventQueue(void);

void InitEventIO(AsyncIO *IO, 
		 void *pData, 
		 IO_CallBack ReadDone, 
		 IO_CallBack SendDone, 
		 IO_CallBack Terminate, 
		 IO_CallBack Timeout, 
		 IO_CallBack ConnFail, 
		 IO_LineReaderCallback LineReader,
		 int ReadFirst);

int QueueQuery(ns_type Type, char *name, AsyncIO *IO, IO_CallBack PostDNS);
