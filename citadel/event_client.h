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

typedef eNextState (*IO_CallBack)(AsyncIO *IO);
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
	struct sockaddr_in6 Addr;

	int sock;
	unsigned short dport;
       	eNextState NextState;

	ev_timer conn_fail, 
		rw_timeout;
	ev_io recv_event, 
		send_event, 
		conn_event;
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


	int active_dns_event;
	ev_io dns_recv_event, 
		dns_send_event;
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
	AsyncIO *IO;
	IO_CallBack EvAttch;
}IOAddHandler; 

void FreeAsyncIOContents(AsyncIO *IO);

int QueueEventContext(AsyncIO *IO, IO_CallBack CB);
int ShutDownEventQueue(void);

eNextState InitEventIO(AsyncIO *IO, 
		       void *pData, 
		       double conn_timeout, 
		       double first_rw_timeout,
		       int ReadFirst);

int QueueQuery(ns_type Type, char *name, AsyncIO *IO, IO_CallBack PostDNS);

void StopClient(AsyncIO *IO);

void SetNextTimeout(AsyncIO *IO, double timeout);

void InitC_ares_dns(AsyncIO *IO);
