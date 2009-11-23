#include "webcit.h"
#include "webserver.h"
#include "modules_init.h"
#include <stdio.h>


wcsession *TestSessionList = NULL;


extern wcsession *CreateSession(int Lockable, wcsession **wclist, ParsedHttpHdrs *Hdr, pthread_mutex_t *ListMutex);


void test_worker_entry(void)
{
/* Session loop pregap */
        ParsedHttpHdrs Hdr;
	wcsession *TheSession;

        memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
        Hdr.HR.eReqType = eGET;
        http_new_modules(&Hdr); 


	Hdr.http_sock = 1; /* STDOUT */
/* Context loop */
	Hdr.HR.dav_depth = 32767; /* TODO: find a general way to have non-0 defaults */
	TheSession = CreateSession(1, &TestSessionList, &Hdr, NULL);
	TheSession->lastreq = time(NULL);			/* log */
	TheSession->Hdr = &Hdr;
	session_attach_modules(TheSession);
/* Session Loop */


/* End Session Loop */
	session_detach_modules(TheSession);

/* End Context loop */
	http_detach_modules(&Hdr);
	http_destroy_modules(&Hdr);

/* End Session loop */
/* now shut it down clean. */
	shutdown_sessions();
	do_housekeeping();

	ShutDownWebcit();

}
