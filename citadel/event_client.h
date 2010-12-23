#include <event.h>

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

struct AsyncIO {
	int sock;
	struct event recv_event, send_event;
	IOBuffer SendBuf, RecvBuf;
	IO_LineReaderCallback LineReader;
	IO_CallBack ReadDone, SendDone;
	StrBuf *IOBuf;
	void *Data;
	DeleteHashDataFunc DeleteData; /* data is expected to contain AsyncIO... */
       	eNextState NextState;
};

void FreeAsyncIOContents(AsyncIO *IO);

int QueueEventContext(void *Ctx, AsyncIO *IO, EventContextAttach CB);

void InitEventIO(AsyncIO *IO, 
		 void *pData, 
		 IO_CallBack ReadDone, 
		 IO_CallBack SendDone, 
		 IO_LineReaderCallback LineReader,
		 int ReadFirst);
