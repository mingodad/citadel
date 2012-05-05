/*
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
#include <assert.h>
#include <arpa/inet.h>
#include <libcitadel.h>
#include <curl/curl.h>
#include <curl/multi.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"

#include "ctdl_module.h"

#include "event_client.h"
#include "serv_curl.h"

ev_loop *event_base;
int DebugEventLoop = 0;
int DebugEventLoopBacktrace = 0;
int DebugCurl = 0;

long EvIDSource = 1;
/*****************************************************************************
 *                   libevent / curl integration                             *
 *****************************************************************************/
#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (DebugCurl != 0))

#define EVCURL_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG (LEVEL) syslog(LEVEL, "EVCURL:IO[%ld]CC[%d] " FORMAT,	\
			      IO->ID, CCID, __VA_ARGS__)

#define EVCURLM_syslog(LEVEL, FORMAT)					\
	DBGLOG (LEVEL) syslog(LEVEL, "EVCURL:IO[%ld]CC[%d] " FORMAT,	\
			      IO->ID, CCID)

#define CURL_syslog(LEVEL, FORMAT, ...)					\
	DBGLOG (LEVEL) syslog(LEVEL, "CURL: " FORMAT, __VA_ARGS__)

#define CURLM_syslog(LEVEL, FORMAT)			\
	DBGLOG (LEVEL) syslog(LEVEL, "CURL: " FORMAT)

#define MOPT(s, v)							\
	do {								\
		sta = curl_multi_setopt(mhnd, (CURLMOPT_##s), (v));	\
		if (sta) {						\
			EVQ_syslog(LOG_ERR, "error setting option "	\
			       #s " on curl multi handle: %s\n",	\
			       curl_easy_strerror(sta));		\
			exit (1);					\
		}							\
	} while (0)


typedef struct _evcurl_global_data {
	int magic;
	CURLM *mhnd;
	ev_timer timeev;
	int nrun;
} evcurl_global_data;

ev_async WakeupCurl;
evcurl_global_data global;

static void
gotstatus(int nnrun)
{
	CURLMsg *msg;
	int nmsg;

	global.nrun = nnrun;

	CURLM_syslog(LOG_DEBUG,
		     "gotstatus(): about to call curl_multi_info_read\n");
	while ((msg = curl_multi_info_read(global.mhnd, &nmsg))) {
		CURL_syslog(LOG_DEBUG,
			    "got curl multi_info message msg=%d\n",
			    msg->msg);

		if (CURLMSG_DONE == msg->msg) {
			CURL *chnd;
			char *chandle = NULL;
			CURLcode sta;
			CURLMcode msta;
			AsyncIO*IO;

			chandle = NULL;;
			chnd = msg->easy_handle;
			sta = curl_easy_getinfo(chnd,
						CURLINFO_PRIVATE,
						&chandle);
			if (sta) {
				EVCURL_syslog(LOG_ERR,
					      "error asking curl for private"
					      " cookie of curl handle: %s\n",
					      curl_easy_strerror(sta));
				continue;
			}
			IO = (AsyncIO *)chandle;

			EVCURLM_syslog(LOG_DEBUG, "request complete\n");

			IO->Now = ev_now(event_base);

			ev_io_stop(event_base, &IO->recv_event);
			ev_io_stop(event_base, &IO->send_event);

			sta = msg->data.result;
			if (sta) {
				EVCURL_syslog(LOG_ERR,
					      "error description: %s\n",
					      IO->HttpReq.errdesc);
				EVCURL_syslog(LOG_ERR,
					      "error performing request: %s\n",
					      curl_easy_strerror(sta));
			}
			sta = curl_easy_getinfo(chnd,
						CURLINFO_RESPONSE_CODE,
						&IO->HttpReq.httpcode);
			if (sta)
				EVCURL_syslog(LOG_ERR,
					      "error asking curl for "
					      "response code from request: %s\n",
					      curl_easy_strerror(sta));
			EVCURL_syslog(LOG_ERR,
				      "http response code was %ld\n",
				      (long)IO->HttpReq.httpcode);


			curl_slist_free_all(IO->HttpReq.headers);
			msta = curl_multi_remove_handle(global.mhnd, chnd);
			if (msta)
				EVCURL_syslog(LOG_ERR,
					      "warning problem detaching "
					      "completed handle from curl multi: "
					      "%s\n",
					      curl_multi_strerror(msta));

			ev_cleanup_stop(event_base, &IO->abort_by_shutdown);

			IO->HttpReq.attached = 0;
			switch(IO->SendDone(IO))
			{
			case eDBQuery:
				curl_easy_cleanup(IO->HttpReq.chnd);
				IO->HttpReq.chnd = NULL;
				break;
			case eSendDNSQuery:
			case eReadDNSReply:
			case eConnect:
			case eSendReply:
			case eSendMore:
			case eSendFile:
			case eReadMessage:
			case eReadMore:
			case eReadPayload:
			case eReadFile:
				curl_easy_cleanup(IO->HttpReq.chnd);
				IO->HttpReq.chnd = NULL;
				break;
			case eTerminateConnection:
			case eAbort:
				curl_easy_cleanup(IO->HttpReq.chnd);
				IO->HttpReq.chnd = NULL;
				FreeStrBuf(&IO->HttpReq.ReplyData);
				FreeURL(&IO->ConnectMe);
				RemoveContext(IO->CitContext);
				IO->Terminate(IO);
			}
		}
	}
}

static void
stepmulti(void *data, curl_socket_t fd, int which)
{
	int running_handles = 0;
	CURLMcode msta;

	msta = curl_multi_socket_action(global.mhnd,
					fd,
					which,
					&running_handles);

	CURLM_syslog(LOG_DEBUG, "stepmulti(): calling gotstatus()\n");
	if (msta)
		CURL_syslog(LOG_ERR,
			    "error in curl processing events"
			    "on multi handle, fd %d: %s\n",
			    (int)fd,
			    curl_multi_strerror(msta));

	if (global.nrun != running_handles)
		gotstatus(running_handles);
}

static void
gottime(struct ev_loop *loop, ev_timer *timeev, int events)
{
	CURLM_syslog(LOG_DEBUG, "EVCURL: waking up curl for timeout\n");
	stepmulti(NULL, CURL_SOCKET_TIMEOUT, 0);
}

static void
got_in(struct ev_loop *loop, ev_io *ioev, int events)
{
	CURL_syslog(LOG_DEBUG,
		    "EVCURL: waking up curl for io on fd %d\n",
		    (int)ioev->fd);

	stepmulti(ioev->data, ioev->fd, CURL_CSELECT_IN);
}

static void
got_out(struct ev_loop *loop, ev_io *ioev, int events)
{
	CURL_syslog(LOG_DEBUG,
		    "waking up curl for io on fd %d\n",
		    (int)ioev->fd);

	stepmulti(ioev->data, ioev->fd, CURL_CSELECT_OUT);
}

static size_t
gotdata(void *data, size_t size, size_t nmemb, void *cglobal) {
	AsyncIO *IO = (AsyncIO*) cglobal;

	if (IO->HttpReq.ReplyData == NULL)
	{
		IO->HttpReq.ReplyData = NewStrBufPlain(NULL, SIZ);
	}
	IO->Now = ev_now(event_base);
	return CurlFillStrBuf_callback(data,
				       size,
				       nmemb,
				       IO->HttpReq.ReplyData);
}

static int
gotwatchtime(CURLM *multi, long tblock_ms, void *cglobal) {
	CURL_syslog(LOG_DEBUG, "EVCURL: gotwatchtime called %ld ms\n", tblock_ms);
	evcurl_global_data *global = cglobal;
	ev_timer_stop(EV_DEFAULT, &global->timeev);
	if (tblock_ms < 0 || 14000 < tblock_ms)
		tblock_ms = 14000;
	ev_timer_set(&global->timeev, 0.5e-3 + 1.0e-3 * tblock_ms, 14.0);
	ev_timer_start(EV_DEFAULT_UC, &global->timeev);
	curl_multi_perform(global, &global->nrun);
	return 0;
}

static int
gotwatchsock(CURL *easy,
	     curl_socket_t fd,
	     int action,
	     void *cglobal,
	     void *vIO)
{
	evcurl_global_data *global = cglobal;
	CURLM *mhnd = global->mhnd;
	char *f;
	AsyncIO *IO = (AsyncIO*) vIO;
	CURLcode sta;
	const char *Action;

	if (IO == NULL) {
		sta = curl_easy_getinfo(easy, CURLINFO_PRIVATE, &f);
		if (sta) {
			EVCURL_syslog(LOG_ERR,
				      "EVCURL: error asking curl for private "
				      "cookie of curl handle: %s\n",
				      curl_easy_strerror(sta));
			return -1;
		}
		IO = (AsyncIO *) f;
		EVCURL_syslog(LOG_DEBUG,
			      "EVCURL: got socket for URL: %s\n",
			      IO->ConnectMe->PlainUrl);

		if (IO->SendBuf.fd != 0)
		{
			ev_io_stop(event_base, &IO->recv_event);
			ev_io_stop(event_base, &IO->send_event);
		}
		IO->SendBuf.fd = fd;
		ev_io_init(&IO->recv_event, &got_in, fd, EV_READ);
		ev_io_init(&IO->send_event, &got_out, fd, EV_WRITE);
		curl_multi_assign(mhnd, fd, IO);
	}

	IO->Now = ev_now(event_base);

	Action = "";
	switch (action)
	{
	case CURL_POLL_NONE:
		Action = "CURL_POLL_NONE";
		break;
	case CURL_POLL_REMOVE:
		Action = "CURL_POLL_REMOVE";
		break;
	case CURL_POLL_IN:
		Action = "CURL_POLL_IN";
		break;
	case CURL_POLL_OUT:
		Action = "CURL_POLL_OUT";
		break;
	case CURL_POLL_INOUT:
		Action = "CURL_POLL_INOUT";
		break;
	}


	EVCURL_syslog(LOG_DEBUG,
		      "EVCURL: gotwatchsock called fd=%d action=%s[%d]\n",
		      (int)fd, Action, action);

	switch (action)
	{
	case CURL_POLL_NONE:
		EVCURLM_syslog(LOG_DEBUG,
			       "called first time "
			       "to register this sockwatcker\n");
		break;
	case CURL_POLL_REMOVE:
		EVCURLM_syslog(LOG_DEBUG,
			       "called last time to unregister "
			       "this sockwatcher\n");
		ev_io_stop(event_base, &IO->recv_event);
		ev_io_stop(event_base, &IO->send_event);
		break;
	case CURL_POLL_IN:
		ev_io_start(event_base, &IO->recv_event);
		ev_io_stop(event_base, &IO->send_event);
		break;
	case CURL_POLL_OUT:
		ev_io_start(event_base, &IO->send_event);
		ev_io_stop(event_base, &IO->recv_event);
		break;
	case CURL_POLL_INOUT:
		ev_io_start(event_base, &IO->send_event);
		ev_io_start(event_base, &IO->recv_event);
		break;
	}
	return 0;
}

void curl_init_connectionpool(void)
{
	CURLM *mhnd ;

	ev_timer_init(&global.timeev, &gottime, 14.0, 14.0);
	global.timeev.data = (void *)&global;
	global.nrun = -1;
	CURLcode sta = curl_global_init(CURL_GLOBAL_ALL);

	if (sta)
	{
		CURL_syslog(LOG_ERR,
			    "error initializing curl library: %s\n",
			    curl_easy_strerror(sta));

		exit(1);
	}
	mhnd = global.mhnd = curl_multi_init();
	if (!mhnd)
	{
		CURLM_syslog(LOG_ERR,
			     "error initializing curl multi handle\n");
		exit(3);
	}

	MOPT(SOCKETFUNCTION, &gotwatchsock);
	MOPT(SOCKETDATA, (void *)&global);
	MOPT(TIMERFUNCTION, &gotwatchtime);
	MOPT(TIMERDATA, (void *)&global);

	return;
}

int evcurl_init(AsyncIO *IO)
{
	CURLcode sta;
	CURL *chnd;

	EVCURLM_syslog(LOG_DEBUG, "EVCURL: evcurl_init called ms\n");
	IO->HttpReq.attached = 0;
	chnd = IO->HttpReq.chnd = curl_easy_init();
	if (!chnd)
	{
		EVCURLM_syslog(LOG_ERR, "EVCURL: error initializing curl handle\n");
		return 0;
	}

#if DEBUG
	OPT(VERBOSE, (long)1);
#endif
	OPT(NOPROGRESS, 1L);

	OPT(NOSIGNAL, 1L);
	OPT(FAILONERROR, (long)1);
	OPT(ENCODING, "");
	OPT(FOLLOWLOCATION, (long)0);
	OPT(MAXREDIRS, (long)0);
	OPT(USERAGENT, CITADEL);

	OPT(TIMEOUT, (long)1800);
	OPT(LOW_SPEED_LIMIT, (long)64);
	OPT(LOW_SPEED_TIME, (long)600);
	OPT(CONNECTTIMEOUT, (long)600);
	OPT(PRIVATE, (void *)IO);

	OPT(FORBID_REUSE, 1);
	OPT(WRITEFUNCTION, &gotdata);
	OPT(WRITEDATA, (void *)IO);
	OPT(ERRORBUFFER, IO->HttpReq.errdesc);

	if ((!IsEmptyStr(config.c_ip_addr))
		&& (strcmp(config.c_ip_addr, "*"))
		&& (strcmp(config.c_ip_addr, "::"))
		&& (strcmp(config.c_ip_addr, "0.0.0.0"))
		)
	{
		OPT(INTERFACE, config.c_ip_addr);
	}

#ifdef CURLOPT_HTTP_CONTENT_DECODING
	OPT(HTTP_CONTENT_DECODING, 1);
	OPT(ENCODING, "");
#endif

	IO->HttpReq.headers = curl_slist_append(IO->HttpReq.headers,
						"Connection: close");

	return 1;
}


static void IOcurl_abort_shutdown_callback(struct ev_loop *loop,
					   ev_cleanup *watcher,
					   int revents)
{
	CURLMcode msta;
	AsyncIO *IO = watcher->data;
	IO->Now = ev_now(event_base);
	EVCURL_syslog(LOG_DEBUG, "EVENT Curl: %s\n", __FUNCTION__);

	curl_slist_free_all(IO->HttpReq.headers);
	msta = curl_multi_remove_handle(global.mhnd, IO->HttpReq.chnd);
	if (msta)
	{
		EVCURL_syslog(LOG_ERR,
			      "EVCURL: warning problem detaching completed handle "
			      "from curl multi: %s\n",
			      curl_multi_strerror(msta));
	}

	curl_easy_cleanup(IO->HttpReq.chnd);
	IO->HttpReq.chnd = NULL;
	ev_cleanup_stop(event_base, &IO->abort_by_shutdown);
	ev_io_stop(event_base, &IO->recv_event);
	ev_io_stop(event_base, &IO->send_event);
	assert(IO->ShutdownAbort);
	IO->ShutdownAbort(IO);
}
eNextState
evcurl_handle_start(AsyncIO *IO)
{
	CURLMcode msta;
	CURLcode sta;
	CURL *chnd;

	chnd = IO->HttpReq.chnd;
	EVCURL_syslog(LOG_DEBUG,
		  "EVCURL: Loading URL: %s\n", IO->ConnectMe->PlainUrl);
	OPT(URL, IO->ConnectMe->PlainUrl);
	if (StrLength(IO->ConnectMe->CurlCreds))
	{
		OPT(HTTPAUTH, (long)CURLAUTH_BASIC);
		OPT(USERPWD, ChrPtr(IO->ConnectMe->CurlCreds));
	}
	if (StrLength(IO->HttpReq.PostData) > 0)
	{
		OPT(POSTFIELDS, ChrPtr(IO->HttpReq.PostData));
		OPT(POSTFIELDSIZE, StrLength(IO->HttpReq.PostData));

	}
	else if ((IO->HttpReq.PlainPostDataLen != 0) &&
		 (IO->HttpReq.PlainPostData != NULL))
	{
		OPT(POSTFIELDS, IO->HttpReq.PlainPostData);
		OPT(POSTFIELDSIZE, IO->HttpReq.PlainPostDataLen);
	}
	OPT(HTTPHEADER, IO->HttpReq.headers);

	IO->NextState = eConnect;
	EVCURLM_syslog(LOG_DEBUG, "EVCURL: attaching to curl multi handle\n");
	msta = curl_multi_add_handle(global.mhnd, IO->HttpReq.chnd);
	if (msta)
	{
		EVCURL_syslog(LOG_ERR,
			  "EVCURL: error attaching to curl multi handle: %s\n",
			  curl_multi_strerror(msta));
	}

	IO->HttpReq.attached = 1;
	ev_async_send (event_base, &WakeupCurl);
	ev_cleanup_init(&IO->abort_by_shutdown,
			IOcurl_abort_shutdown_callback);

	ev_cleanup_start(event_base, &IO->abort_by_shutdown);
	return eReadMessage;
}

static void WakeupCurlCallback(EV_P_ ev_async *w, int revents)
{
	CURLM_syslog(LOG_DEBUG, "waking up curl multi handle\n");

	curl_multi_perform(&global, CURL_POLL_NONE);
}

static void evcurl_shutdown (void)
{
	curl_global_cleanup();
	curl_multi_cleanup(global.mhnd);
	CURLM_syslog(LOG_DEBUG, "exiting\n");
}
/*****************************************************************************
 *                       libevent integration                                *
 *****************************************************************************/
/*
 * client event queue plus its methods.
 * this currently is the main loop (which may change in some future?)
 */
int evbase_count = 0;
int event_add_pipe[2] = {-1, -1};
pthread_mutex_t EventQueueMutex; /* locks the access to the following vars: */
HashList *QueueEvents = NULL;
HashList *InboundEventQueue = NULL;
HashList *InboundEventQueues[2] = { NULL, NULL };

ev_async AddJob;
ev_async ExitEventLoop;

static void QueueEventAddCallback(EV_P_ ev_async *w, int revents)
{
	ev_tstamp Now;
	HashList *q;
	void *v;
	HashPos*It;
	long len;
	const char *Key;

	/* get the control command... */
	pthread_mutex_lock(&EventQueueMutex);

	if (InboundEventQueues[0] == InboundEventQueue) {
		InboundEventQueue = InboundEventQueues[1];
		q = InboundEventQueues[0];
	}
	else {
		InboundEventQueue = InboundEventQueues[0];
		q = InboundEventQueues[1];
	}
	pthread_mutex_unlock(&EventQueueMutex);
	Now = ev_now (event_base);
	It = GetNewHashPos(q, 0);
	while (GetNextHashPos(q, It, &len, &Key, &v))
	{
		IOAddHandler *h = v;
		if (h->IO->ID == 0) {
			h->IO->ID = EvIDSource++;
		}
		if (h->IO->StartIO == 0.0)
			h->IO->StartIO = Now;
		h->IO->Now = Now;
		h->EvAttch(h->IO);
	}
	DeleteHashPos(&It);
	DeleteHashContent(&q);
	EVQM_syslog(LOG_DEBUG, "EVENT Q Add done.\n");
}


static void EventExitCallback(EV_P_ ev_async *w, int revents)
{
	ev_break(event_base, EVBREAK_ALL);

	EVQM_syslog(LOG_DEBUG, "EVENT Q exiting.\n");
}



void InitEventQueue(void)
{
	struct rlimit LimitSet;

	pthread_mutex_init(&EventQueueMutex, NULL);

	if (pipe(event_add_pipe) != 0) {
		syslog(LOG_EMERG,
		       "Unable to create pipe for libev queueing: %s\n",
		       strerror(errno));
		abort();
	}
	LimitSet.rlim_cur = 1;
	LimitSet.rlim_max = 1;
	setrlimit(event_add_pipe[1], &LimitSet);

	QueueEvents = NewHash(1, Flathash);
	InboundEventQueues[0] = NewHash(1, Flathash);
	InboundEventQueues[1] = NewHash(1, Flathash);
	InboundEventQueue = InboundEventQueues[0];
}
extern void CtdlDestroyEVCleanupHooks(void);

extern int EVQShutDown;
/*
 * this thread operates the select() etc. via libev.
 */
void *client_event_thread(void *arg) 
{
	struct CitContext libev_client_CC;

	CtdlFillSystemContext(&libev_client_CC, "LibEv Thread");
//	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	EVQM_syslog(LOG_DEBUG, "client_event_thread() initializing\n");

	event_base = ev_default_loop (EVFLAG_AUTO);
	ev_async_init(&AddJob, QueueEventAddCallback);
	ev_async_start(event_base, &AddJob);
	ev_async_init(&ExitEventLoop, EventExitCallback);
	ev_async_start(event_base, &ExitEventLoop);
	ev_async_init(&WakeupCurl, WakeupCurlCallback);
	ev_async_start(event_base, &WakeupCurl);

	curl_init_connectionpool();

	ev_run (event_base, 0);

	EVQM_syslog(LOG_DEBUG, "client_event_thread() exiting\n");

///what todo here?	CtdlClearSystemContext();
	ev_loop_destroy (EV_DEFAULT_UC);
	DeleteHash(&QueueEvents);
	InboundEventQueue = NULL;
	DeleteHash(&InboundEventQueues[0]);
	DeleteHash(&InboundEventQueues[1]);
/*	citthread_mutex_destroy(&EventQueueMutex); TODO */
	evcurl_shutdown();
	close(event_add_pipe[0]);
	close(event_add_pipe[1]);

	CtdlDestroyEVCleanupHooks();

	EVQShutDown = 1;	
	return(NULL);
}

/*----------------------------------------------------------------------------*/
/*
 * DB-Queue; does async bdb operations.
 * has its own set of handlers.
 */
ev_loop *event_db;
int evdb_count = 0;
int evdb_add_pipe[2] = {-1, -1};
pthread_mutex_t DBEventQueueMutex; /* locks the access to the following vars: */
HashList *DBQueueEvents = NULL;
HashList *DBInboundEventQueue = NULL;
HashList *DBInboundEventQueues[2] = { NULL, NULL };

ev_async DBAddJob;
ev_async DBExitEventLoop;

extern void ShutDownDBCLient(AsyncIO *IO);

static void DBQueueEventAddCallback(EV_P_ ev_async *w, int revents)
{
	ev_tstamp Now;
	HashList *q;
	void *v;
	HashPos *It;
	long len;
	const char *Key;

	/* get the control command... */
	pthread_mutex_lock(&DBEventQueueMutex);

	if (DBInboundEventQueues[0] == DBInboundEventQueue) {
		DBInboundEventQueue = DBInboundEventQueues[1];
		q = DBInboundEventQueues[0];
	}
	else {
		DBInboundEventQueue = DBInboundEventQueues[0];
		q = DBInboundEventQueues[1];
	}
	pthread_mutex_unlock(&DBEventQueueMutex);

	Now = ev_now (event_db);
	It = GetNewHashPos(q, 0);
	while (GetNextHashPos(q, It, &len, &Key, &v))
	{
		IOAddHandler *h = v;
		eNextState rc;
		if (h->IO->ID == 0)
			h->IO->ID = EvIDSource++;
		if (h->IO->StartDB == 0.0)
			h->IO->StartDB = Now;
		h->IO->Now = Now;
		rc = h->EvAttch(h->IO);
		switch (rc)
		{
		case eAbort:
			ShutDownDBCLient(h->IO);
		default:
			break;
		}
	}
	DeleteHashPos(&It);
	DeleteHashContent(&q);
	EVQM_syslog(LOG_DEBUG, "DBEVENT Q Add done.\n");
}


static void DBEventExitCallback(EV_P_ ev_async *w, int revents)
{
	EVQM_syslog(LOG_DEBUG, "DB EVENT Q exiting.\n");
	ev_break(event_db, EVBREAK_ALL);
}



void DBInitEventQueue(void)
{
	struct rlimit LimitSet;

	pthread_mutex_init(&DBEventQueueMutex, NULL);

	if (pipe(evdb_add_pipe) != 0) {
		EVQ_syslog(LOG_EMERG, "Unable to create pipe for libev queueing: %s\n", strerror(errno));
		abort();
	}
	LimitSet.rlim_cur = 1;
	LimitSet.rlim_max = 1;
	setrlimit(evdb_add_pipe[1], &LimitSet);

	DBQueueEvents = NewHash(1, Flathash);
	DBInboundEventQueues[0] = NewHash(1, Flathash);
	DBInboundEventQueues[1] = NewHash(1, Flathash);
	DBInboundEventQueue = DBInboundEventQueues[0];
}

/*
 * this thread operates writing to the message database via libev.
 */
void *db_event_thread(void *arg)
{
	struct CitContext libev_msg_CC;

	CtdlFillSystemContext(&libev_msg_CC, "LibEv DB IO Thread");
//	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	EVQM_syslog(LOG_DEBUG, "dbevent_thread() initializing\n");

	event_db = ev_loop_new (EVFLAG_AUTO);

	ev_async_init(&DBAddJob, DBQueueEventAddCallback);
	ev_async_start(event_db, &DBAddJob);
	ev_async_init(&DBExitEventLoop, DBEventExitCallback);
	ev_async_start(event_db, &DBExitEventLoop);

	ev_run (event_db, 0);

	EVQM_syslog(LOG_DEBUG, "dbevent_thread() exiting\n");

//// what to do here?	CtdlClearSystemContext();
	ev_loop_destroy (event_db);

	DeleteHash(&DBQueueEvents);
	DBInboundEventQueue = NULL;
	DeleteHash(&DBInboundEventQueues[0]);
	DeleteHash(&DBInboundEventQueues[1]);

	close(evdb_add_pipe[0]);
	close(evdb_add_pipe[1]);
/*	citthread_mutex_destroy(&DBEventQueueMutex); TODO */

	return(NULL);
}

void ShutDownEventQueues(void)
{
	EVQM_syslog(LOG_DEBUG, "EVENT Qs triggering exits.\n");

	pthread_mutex_lock(&DBEventQueueMutex);
	ev_async_send (event_db, &DBExitEventLoop);
	pthread_mutex_unlock(&DBEventQueueMutex);

	pthread_mutex_lock(&EventQueueMutex);
	ev_async_send (EV_DEFAULT_ &ExitEventLoop);
	pthread_mutex_unlock(&EventQueueMutex);
}

void DebugEventloopEnable(const int n)
{
	DebugEventLoop = n;
}
void DebugEventloopBacktraceEnable(const int n)
{
	DebugEventLoopBacktrace = n;
}

void DebugCurlEnable(const int n)
{
	DebugCurl = n;
}

CTDL_MODULE_INIT(event_client)
{
	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("eventloop"), DebugEventloopEnable, &DebugEventLoop);
		CtdlRegisterDebugFlagHook(HKEY("eventloopbacktrace"), DebugEventloopBacktraceEnable, &DebugEventLoopBacktrace);
		CtdlRegisterDebugFlagHook(HKEY("curl"), DebugCurlEnable, &DebugCurl);
		InitEventQueue();
		DBInitEventQueue();
		CtdlThreadCreate(client_event_thread);
		CtdlThreadCreate(db_event_thread);
	}
	return "event";
}
