#include "webcit.h"
#include "webserver.h"
#include "modules_init.h"
#include <stdio.h>


wcsession *TestSessionList = NULL;

extern StrBuf *Username;
extern StrBuf *Passvoid;


extern int ReadHttpSubject(ParsedHttpHdrs *Hdr, StrBuf *Line, StrBuf *Buf);
extern wcsession *CreateSession(int Lockable, wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex);



void SetHttpURL(ParsedHttpHdrs *Hdr, const char *Title, long tlen, StrBuf *Buf)
{
	StrBuf *Line = NewStrBufPlain (Title, tlen);
	ReadHttpSubject(Hdr, Line, Buf);
	FreeStrBuf(&Line);
}

void test_worker_entry(void)
{
/* Session loop pregap */
        ParsedHttpHdrs Hdr;
	wcsession *TheSession;
	StrBuf *Response;
	StrBuf *Buf;

        memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
        Hdr.HR.eReqType = eGET;
        http_new_modules(&Hdr); 


	Hdr.http_sock = 1; /* STDOUT */
/* Context loop */
	Hdr.HR.dav_depth = 32767; /* TODO: find a general way to have non-0 defaults */
	TheSession = CreateSession(1, &TestSessionList, &Hdr, NULL);
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->Hdr = &Hdr;
	Hdr.HTTPHeaders = NewHash(1, NULL);
	session_attach_modules(TheSession);
/* Session Loop */
	Buf = NewStrBuf();
	SetHttpURL(&Hdr, HKEY("GET /groupdav/ HTTP/1.0\r\n"), Buf);
	if (GetConnected ())
	{
		Response = NewStrBuf();
		become_logged_in(Username, Passvoid, Response);
	}
	FreeStrBuf(&Buf);
/* End Session Loop */
	session_detach_modules(TheSession);
	session_destroy_modules(&TheSession);
/* End Context loop */
	http_detach_modules(&Hdr);
	http_destroy_modules(&Hdr);

/* End Session loop */
/* now shut it down clean. */
//	shutdown_sessions();
	do_housekeeping();


}
