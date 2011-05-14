/*
 * Copyright (c) 1998-2009 by the citadel.org team
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
 *
 *  thanks to some guy in #libev
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
#include <arpa/inet.h>

#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"

#include "ctdl_module.h"
#include "serv_curl.h"

evcurl_global_data global;


extern struct ev_loop *event_base;

void SockStateCb(void *data, int sock, int read, int write);

#define MOPT(s, v) \
	do {							    \
	sta = curl_multi_setopt(mhnd, (CURLMOPT_##s), (v));	    \
	if (sta) {						    \
		CtdlLogPrintf(CTDL_ERR, "error setting option " #s " on curl multi handle: %s\n", curl_easy_strerror(sta)); \
		exit (1);					    \
	}							    \
} while (0)


void ____InitC_ares_dns(AsyncIO *IO)
{
	int optmask = 0;
	if (IO->DNSChannel == NULL) {
		optmask |= ARES_OPT_SOCK_STATE_CB;
		IO->DNSOptions.sock_state_cb = SockStateCb;
		IO->DNSOptions.sock_state_cb_data = IO;
		ares_init_options(&IO->DNSChannel, &IO->DNSOptions, optmask);
	}
}



/*****************************************************************************
 *                   libevent / c-ares integration                           *
 *****************************************************************************/
static void ____DNS_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, ARES_SOCKET_BAD, IO->dns_send_event.fd);
}
static void ____DNS_recv_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	
	ares_process_fd(IO->DNSChannel, IO->dns_recv_event.fd, ARES_SOCKET_BAD);
}

void ____SockStateCb(void *data, int sock, int read, int write) 
{
/*
	struct timeval tvbuf, maxtv, *ret;
	
	int64_t time = 10;
*/
	AsyncIO *IO = data;
/* already inside of the event queue. */	

	if (read) {
		if ((IO->dns_recv_event.fd != sock) &&
		    (IO->dns_recv_event.fd != 0)) {
			ev_io_stop(event_base, &IO->dns_recv_event);
		}
		IO->dns_recv_event.fd = sock;
		ev_io_init(&IO->dns_recv_event, ____DNS_recv_callback, IO->dns_recv_event.fd, EV_READ);
		IO->dns_recv_event.data = IO;
		ev_io_start(event_base, &IO->dns_recv_event);
	} 
	if (write) {
		if ((IO->dns_send_event.fd != sock) &&
		    (IO->dns_send_event.fd != 0)) {
			ev_io_stop(event_base, &IO->dns_send_event);
		}
		IO->dns_send_event.fd = sock;
		ev_io_init(&IO->dns_send_event, ____DNS_send_callback, IO->dns_send_event.fd, EV_WRITE);
		IO->dns_send_event.data = IO;
		ev_io_start(event_base, &IO->dns_send_event);
	}
/*

		ev_io_start(event_base, &IO->dns_io_event);
	
		maxtv.tv_sec = time/1000;
		maxtv.tv_usec = (time % 1000) * 1000;
		
		ret = ares_timeout(IO->DNSChannel, &maxtv, &tvbuf);
	}
*/
	if ((read == 0) && (write == 0)) {
		ev_io_stop(event_base, &IO->dns_recv_event);
		ev_io_stop(event_base, &IO->dns_send_event);
	}
}






static void
gotstatus(evcurl_global_data *global, int nnrun) 
{
	CURLM *mhnd;
	CURLMsg *msg;
	int nmsg;
/*
	if (EVCURL_GLOBAL_MAGIC != global.magic)
	{
		CtdlLogPrintf(CTDL_ERR, "internal error: gotstatus on wrong struct");
		return;
	}
*/
	global->nrun = nnrun;
	mhnd = global->mhnd;

	CtdlLogPrintf(CTDL_ERR, "about to call curl_multi_info_read\n");
	while ((msg = curl_multi_info_read(mhnd, &nmsg))) {
		CtdlLogPrintf(CTDL_ERR, "got curl multi_info message msg=%d", msg->msg);
		if (CURLMSG_DONE == msg->msg) {
			CtdlLogPrintf(CTDL_ERR, "request complete\n");
			CURL *chnd = msg->easy_handle;
			char *chandle;
			CURLcode sta = curl_easy_getinfo(chnd, CURLINFO_PRIVATE, &chandle);
			if (sta)
				CtdlLogPrintf(CTDL_ERR, "error asking curl for private cookie of curl handle: %s\n", curl_easy_strerror(sta));
			evcurl_request_data  *handle = (void *)chandle;
			if (global != handle->global || chnd != handle->chnd)
				CtdlLogPrintf(CTDL_ERR, "internal evcurl error: unknown curl handle completed\n");
			sta = msg->data.result;
			if (sta) {
				CtdlLogPrintf(CTDL_ERR, "error description: %s\n", handle->errdesc);
				CtdlLogPrintf(CTDL_ERR, "error performing request: %s\n", curl_easy_strerror(sta));
			}
			long httpcode;
			sta = curl_easy_getinfo(chnd, CURLINFO_RESPONSE_CODE, &httpcode);
			if (sta)
				CtdlLogPrintf(CTDL_ERR, "error asking curl for response code from request: %s\n", curl_easy_strerror(sta));
			CtdlLogPrintf(CTDL_ERR, "http response code was %ld\n", (long)httpcode);
			CURLMcode msta = curl_multi_remove_handle(mhnd, chnd);
			if (msta)
				CtdlLogPrintf(CTDL_ERR, "warning problem detaching completed handle from curl multi: %s\n", curl_multi_strerror(msta));
			handle->attached = 0;
		}
	}
	if (0 == nnrun) {
		ev_unloop(EV_DEFAULT, EVUNLOOP_ONE); /* remove in production */
	}
}

static void
stepmulti(evcurl_global_data *global, curl_socket_t fd) {
	int nnrun;
	CURLMcode msta = curl_multi_socket_action(global->mhnd, fd, 0, &nnrun);
	if (msta)
		CtdlLogPrintf(CTDL_ERR, "error in curl processing events on multi handle, fd %d: %s\n", (int)fd, curl_multi_strerror(msta));
	if (global->nrun != nnrun)
		gotstatus(global, nnrun);
}

static void
gottime(struct ev_loop *loop, ev_timer *timeev, int events) {
	CtdlLogPrintf(CTDL_ERR,"waking up curl for timeout\n");
	evcurl_global_data *global = (void *)timeev->data;
	stepmulti(global, CURL_SOCKET_TIMEOUT);
}

static void
gotio(struct ev_loop *loop, ev_io *ioev, int events) {
	CtdlLogPrintf(CTDL_ERR,"waking up curl for io on fd %d\n", (int)ioev->fd);
	sockwatcher_data *sockwatcher = (void *)ioev->data;
	stepmulti(sockwatcher->global, ioev->fd);
}

static size_t
gotdata(void *data, size_t size, size_t nmemb, void *cglobal) {
	evcurl_request_data *D = (evcurl_request_data*) data;
	return CurlFillStrBuf_callback(D->ReplyData, size, nmemb, cglobal);
}

static int
gotwatchtime(CURLM *multi, long tblock_ms, void *cglobal) {
	CtdlLogPrintf(CTDL_ERR,"gotwatchtime called %ld ms\n", tblock_ms);
	evcurl_global_data *global = cglobal;
	ev_timer_stop(EV_DEFAULT, &global->timeev);
	if (tblock_ms < 0 || 14000 < tblock_ms)
		tblock_ms = 14000;
	ev_timer_set(&global->timeev, 0.5e-3 + 1.0e-3 * tblock_ms, 14.0);
	ev_timer_start(EV_DEFAULT_UC, &global->timeev);
	return 0;
}

static int
gotwatchsock(CURL *easy, curl_socket_t fd, int action, void *cglobal, void *csockwatcher) {
	evcurl_global_data *global = cglobal;
	CURLM *mhnd = global->mhnd;
	CtdlLogPrintf(CTDL_ERR,"gotwatchsock called fd=%d action=%d\n", (int)fd, action);
	sockwatcher_data *sockwatcher = csockwatcher;
	if (!sockwatcher) {
		CtdlLogPrintf(CTDL_ERR,"called first time to register this sockwatcker\n");
		sockwatcher = malloc(sizeof(sockwatcher_data));
		sockwatcher->global = global;
		ev_init(&sockwatcher->ioev, &gotio);
		sockwatcher->ioev.data = (void *)sockwatcher;
		curl_multi_assign(mhnd, fd, sockwatcher);
	}
	if (CURL_POLL_REMOVE == action) {
		CtdlLogPrintf(CTDL_ERR,"called last time to unregister this sockwatcher\n");
		free(sockwatcher);
	} else {
		int events = (action & CURL_POLL_IN ? EV_READ : 0) | (action & CURL_POLL_OUT ? EV_WRITE : 0);
		ev_io_stop(EV_DEFAULT, &sockwatcher->ioev);
		if (events) {
			ev_io_set(&sockwatcher->ioev, fd, events);
			ev_io_start(EV_DEFAULT, &sockwatcher->ioev);
		}
	}
	return 0;
}

void curl_init_connectionpool(void) 
{
	CURLM *mhnd ;
//	global.magic = EVCURL_GLOBAL_MAGIC;

	ev_timer_init(&global.timeev, &gottime, 14.0, 14.0);
	global.timeev.data = (void *)&global;
	global.nrun = -1;
	CURLcode sta = curl_global_init(CURL_GLOBAL_ALL);
		/* note: probably replace with curl_global_init_mem if used with perl */
	if (sta) 
	{
		CtdlLogPrintf(CTDL_ERR,"error initializing curl library: %s\n", curl_easy_strerror(sta));
		exit(1);
	}
/*
	if (!ev_default_loop(EVFLAG_AUTO))
	{
		CtdlLogPrintf(CTDL_ERR,"error initializing libev\n");
		exit(2);
	}
*/
	mhnd = global.mhnd = curl_multi_init();
	if (!mhnd)
	{
		CtdlLogPrintf(CTDL_ERR,"error initializing curl multi handle\n");
		exit(3);
	}

	MOPT(SOCKETFUNCTION, &gotwatchsock);
	MOPT(SOCKETDATA, (void *)&global);
	MOPT(TIMERFUNCTION, &gotwatchtime);
	MOPT(TIMERDATA, (void *)&global);

	/* well, just there to fire the sample request?*/
 	ev_timer_start(EV_DEFAULT, &global.timeev);
	return;
}




int evcurl_init(evcurl_request_data *handle, 
		void *CustomData, 
		const char* Desc,
		int CallBack) 
{
	CURLcode sta;
	CURL *chnd;

	handle->global = &global;
	handle->attached = 0;
	chnd = handle->chnd = curl_easy_init();
	if (!chnd)
	{
		CtdlLogPrintf(CTDL_ERR, "error initializing curl handle\n");
		return 1;
	}

	strcpy(handle->errdesc, Desc);

	OPT(VERBOSE, (long)1);
		/* unset in production */
	OPT(NOPROGRESS, (long)1); 
	OPT(NOSIGNAL, (long)1);
	OPT(FAILONERROR, (long)1);
	OPT(ENCODING, "");
	OPT(FOLLOWLOCATION, (long)1);
	OPT(MAXREDIRS, (long)7);
	OPT(USERAGENT, CITADEL);

	OPT(TIMEOUT, (long)1800);
	OPT(LOW_SPEED_LIMIT, (long)64);
	OPT(LOW_SPEED_TIME, (long)600);
	OPT(CONNECTTIMEOUT, (long)600); 
	OPT(PRIVATE, (void *)handle);


	OPT(WRITEFUNCTION, &gotdata); 
	OPT(WRITEDATA, (void *)handle);
	OPT(ERRORBUFFER, handle->errdesc);

		/* point to a structure that points back to the perl structure and stuff */
	OPT(URL, handle->URL->PlainUrl);
	if (StrLength(handle->URL->CurlCreds))
	{
		OPT(HTTPAUTH, (long)CURLAUTH_BASIC);
		OPT(USERPWD, ChrPtr(handle->URL->CurlCreds));
	}
#ifdef CURLOPT_HTTP_CONTENT_DECODING
	OPT(HTTP_CONTENT_DECODING, 1);
	OPT(ENCODING, "");
#endif
	if (StrLength(handle->PostData) > 0)
	{ 
		OPT(POSTFIELDS, ChrPtr(handle->PostData));
		OPT(POSTFIELDSIZE, StrLength(handle->PostData));

	}
	else if ((handle->PlainPostDataLen != 0) && (handle->PlainPostData != NULL))
	{
		OPT(POSTFIELDS, handle->PlainPostData);
		OPT(POSTFIELDSIZE, handle->PlainPostDataLen);
	}

	if (handle->headers != NULL)
		OPT(HTTPHEADER, handle->headers);

	return 1;
}

void
evcurl_handle_start(evcurl_request_data *handle) 
{
	CURLMcode msta = curl_multi_add_handle(handle->global->mhnd, handle->chnd);
	if (msta)
		CtdlLogPrintf(CTDL_ERR, "error attaching to curl multi handle: %s\n", curl_multi_strerror(msta));
	handle->attached = 1;
}



CTDL_MODULE_INIT(curl_client)
{
	if (!threading)
	{
//		curl_init_connectionpool();
/*
		int r = ares_library_init(ARES_LIB_INIT_ALL);
		if (0 != r) {
			// TODO
			// ThrowException(Exception::Error(String::New(ares_strerror(r))));
////			assert(r == 0);
		}
*/	}
	return "curl";
}
