/*
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
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
#include <assert.h>
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"

#include "event_client.h"
#include "ctdl_module.h"

static void IO_Timeout_callback(struct ev_loop *loop, ev_timer *watcher, int revents);

static void IO_abort_shutdown_callback(struct ev_loop *loop,
				       ev_cleanup *watcher,
				       int revents)
{
	AsyncIO *IO = watcher->data;
	EV_syslog(LOG_DEBUG, "EVENT Q: %s\n", __FUNCTION__);

	assert(IO->ShutdownAbort);
	IO->ShutdownAbort(IO);
}


/*------------------------------------------------------------------------------
 *				Server DB IO
 *----------------------------------------------------------------------------*/
extern int evdb_count;
extern pthread_mutex_t DBEventQueueMutex;
extern HashList *DBInboundEventQueue;
extern struct ev_loop *event_db;
extern ev_async DBAddJob;
extern ev_async DBExitEventLoop;

eNextState QueueDBOperation(AsyncIO *IO, IO_CallBack CB)
{
	IOAddHandler *h;
	int i;

	h = (IOAddHandler*)malloc(sizeof(IOAddHandler));
	h->IO = IO;
	h->EvAttch = CB;
	ev_cleanup_init(&IO->db_abort_by_shutdown,
			IO_abort_shutdown_callback);
	IO->db_abort_by_shutdown.data = IO;
	ev_cleanup_start(event_db, &IO->db_abort_by_shutdown);

	pthread_mutex_lock(&DBEventQueueMutex);
	EVM_syslog(LOG_DEBUG, "DBEVENT Q\n");
	i = ++evdb_count ;
	Put(DBInboundEventQueue, IKEY(i), h, NULL);
	pthread_mutex_unlock(&DBEventQueueMutex);

	ev_async_send (event_db, &DBAddJob);
	EVM_syslog(LOG_DEBUG, "DBEVENT Q Done.\n");
	return eDBQuery;
}

void ShutDownDBCLient(AsyncIO *IO)
{
	CitContext *Ctx =IO->CitContext;
	become_session(Ctx);

	EVM_syslog(LOG_DEBUG, "DBEVENT Terminating.\n");
	ev_cleanup_stop(event_db, &IO->db_abort_by_shutdown);

	assert(IO->Terminate);
	IO->Terminate(IO);
}

void
DB_PerformNext(struct ev_loop *loop, ev_idle *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	EV_syslog(LOG_DEBUG, "event: %s\n", __FUNCTION__);
	become_session(IO->CitContext);

	ev_idle_stop(event_db, &IO->db_unwind_stack);

	assert(IO->NextDBOperation);
	switch (IO->NextDBOperation(IO))
	{
	case eDBQuery:
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
		ev_cleanup_stop(loop, &IO->db_abort_by_shutdown);
		break;
	case eTerminateConnection:
	case eAbort:
		ev_idle_stop(event_db, &IO->db_unwind_stack);
		ev_cleanup_stop(loop, &IO->db_abort_by_shutdown);
		ShutDownDBCLient(IO);
	}
}

eNextState NextDBOperation(AsyncIO *IO, IO_CallBack CB)
{
	IO->NextDBOperation = CB;
	ev_idle_init(&IO->db_unwind_stack,
		     DB_PerformNext);
	IO->db_unwind_stack.data = IO;
	ev_idle_start(event_db, &IO->db_unwind_stack);
	return eDBQuery;
}

/*------------------------------------------------------------------------------
 *			Client IO
 *----------------------------------------------------------------------------*/
extern int evbase_count;
extern pthread_mutex_t EventQueueMutex;
extern HashList *InboundEventQueue;
extern struct ev_loop *event_base;
extern ev_async AddJob;
extern ev_async ExitEventLoop;


eNextState QueueEventContext(AsyncIO *IO, IO_CallBack CB)
{
	IOAddHandler *h;
	int i;

	h = (IOAddHandler*)malloc(sizeof(IOAddHandler));
	h->IO = IO;
	h->EvAttch = CB;
	ev_cleanup_init(&IO->abort_by_shutdown,
			IO_abort_shutdown_callback);
	IO->abort_by_shutdown.data = IO;
	ev_cleanup_start(event_base, &IO->abort_by_shutdown);

	pthread_mutex_lock(&EventQueueMutex);
	EVM_syslog(LOG_DEBUG, "EVENT Q\n");
	i = ++evbase_count;
	Put(InboundEventQueue, IKEY(i), h, NULL);
	pthread_mutex_unlock(&EventQueueMutex);

	ev_async_send (event_base, &AddJob);
	EVM_syslog(LOG_DEBUG, "EVENT Q Done.\n");
	return eSendReply;
}

extern eNextState evcurl_handle_start(AsyncIO *IO);

eNextState QueueCurlContext(AsyncIO *IO)
{
	IOAddHandler *h;
	int i;

	h = (IOAddHandler*)malloc(sizeof(IOAddHandler));
	h->IO = IO;
	h->EvAttch = evcurl_handle_start;

	pthread_mutex_lock(&EventQueueMutex);
	EVM_syslog(LOG_DEBUG, "EVENT Q\n");
	i = ++evbase_count;
	Put(InboundEventQueue, IKEY(i), h, NULL);
	pthread_mutex_unlock(&EventQueueMutex);

	ev_async_send (event_base, &AddJob);
	EVM_syslog(LOG_DEBUG, "EVENT Q Done.\n");
	return eSendReply;
}

void DestructCAres(AsyncIO *IO);
void FreeAsyncIOContents(AsyncIO *IO)
{
	CitContext *Ctx = IO->CitContext;

	FreeStrBuf(&IO->IOBuf);
	FreeStrBuf(&IO->SendBuf.Buf);
	FreeStrBuf(&IO->RecvBuf.Buf);

	DestructCAres(IO);

	FreeURL(&IO->ConnectMe);
	FreeStrBuf(&IO->HttpReq.ReplyData);

	if (Ctx) {
		Ctx->state = CON_IDLE;
		Ctx->kill_me = 1;
	}
}


void StopClientWatchers(AsyncIO *IO)
{
	ev_timer_stop (event_base, &IO->rw_timeout);
	ev_timer_stop(event_base, &IO->conn_fail);
	ev_idle_stop(event_base, &IO->unwind_stack);

	ev_io_stop(event_base, &IO->conn_event);
	ev_io_stop(event_base, &IO->send_event);
	ev_io_stop(event_base, &IO->recv_event);

	if (IO->SendBuf.fd != 0) {
		close(IO->SendBuf.fd);
	}
	IO->SendBuf.fd = 0;
	IO->RecvBuf.fd = 0;
}

void ShutDownCLient(AsyncIO *IO)
{
	CitContext *Ctx =IO->CitContext;
	become_session(Ctx);

	EVM_syslog(LOG_DEBUG, "EVENT Terminating \n");

	ev_cleanup_stop(event_base, &IO->abort_by_shutdown);
	StopClientWatchers(IO);

	if (IO->DNS.Channel != NULL) {
		ares_destroy(IO->DNS.Channel);
		EV_DNS_LOG_STOP(DNS.recv_event);
		EV_DNS_LOG_STOP(DNS.send_event);
		ev_io_stop(event_base, &IO->DNS.recv_event);
		ev_io_stop(event_base, &IO->DNS.send_event);
		IO->DNS.Channel = NULL;
	}
	assert(IO->Terminate);
	IO->Terminate(IO);
}

eReadState HandleInbound(AsyncIO *IO)
{
	const char *Err = NULL;
	eReadState Finished = eBufferNotEmpty;

	become_session(IO->CitContext);

	while ((Finished == eBufferNotEmpty) &&
	       ((IO->NextState == eReadMessage)||
		(IO->NextState == eReadMore)||
		(IO->NextState == eReadFile)||
		(IO->NextState == eReadPayload)))
	{
		/* Reading lines...
		 * lex line reply in callback,
		 * or do it ourselves.
		 * i.e. as nnn-blabla means continue reading in SMTP
		 */
		if ((IO->NextState == eReadFile) &&
		    (Finished == eBufferNotEmpty))
		{
			Finished = WriteIOBAlreadyRead(&IO->IOB, &Err);
			if (Finished == eReadSuccess)
			{
				IO->NextState = eSendReply;
			}
		}
		else if (IO->LineReader)
			Finished = IO->LineReader(IO);
		else
			Finished = StrBufChunkSipLine(IO->IOBuf,
						      &IO->RecvBuf);

		switch (Finished) {
		case eMustReadMore: /// read new from socket...
			break;
		case eBufferNotEmpty: /* shouldn't happen... */
		case eReadSuccess: /// done for now...
			break;
		case eReadFail: /// WHUT?
				///todo: shut down!
			break;
		}

		if (Finished != eMustReadMore) {
			assert(IO->ReadDone);
			ev_io_stop(event_base, &IO->recv_event);
			IO->NextState = IO->ReadDone(IO);
			Finished = StrBufCheckBuffer(&IO->RecvBuf);
		}
	}

	switch (IO->NextState) {
	case eSendFile:
		ev_io_start(event_base, &IO->send_event);
		break;
	case eSendReply:
	case eSendMore:
		assert(IO->SendDone);
		IO->NextState = IO->SendDone(IO);
		ev_io_start(event_base, &IO->send_event);
		break;
	case eReadPayload:
	case eReadMore:
	case eReadFile:
		ev_io_start(event_base, &IO->recv_event);
		break;
	case eTerminateConnection:
		ShutDownCLient(IO);
		break;
	case eAbort:
		ShutDownCLient(IO);
		break;
	case eSendDNSQuery:
	case eReadDNSReply:
	case eDBQuery:
	case eConnect:
	case eReadMessage:
		break;
	}
	return Finished;
}


static void
IO_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	int rc;
	AsyncIO *IO = watcher->data;
	const char *errmsg = NULL;

	become_session(IO->CitContext);
#ifdef BIGBAD_IODBG
	{
		int rv = 0;
		char fn [SIZ];
		FILE *fd;
		const char *pch = ChrPtr(IO->SendBuf.Buf);
		const char *pchh = IO->SendBuf.ReadWritePointer;
		long nbytes;

		if (pchh == NULL)
			pchh = pch;

		nbytes = StrLength(IO->SendBuf.Buf) - (pchh - pch);
		snprintf(fn, SIZ, "/tmp/foolog_ev_%s.%d",
			 ((CitContext*)(IO->CitContext))->ServiceName,
			 IO->SendBuf.fd);

		fd = fopen(fn, "a+");
		fprintf(fd, "Send: BufSize: %ld BufContent: [",
			nbytes);
		rv = fwrite(pchh, nbytes, 1, fd);
		if (!rv) printf("failed to write debug to %s!\n", fn);
		fprintf(fd, "]\n");
#endif
		switch (IO->NextState) {
		case eSendFile:
			rc = FileSendChunked(&IO->IOB, &errmsg);
			if (rc < 0)
				StrBufPlain(IO->ErrMsg, errmsg, -1);
			break;
		default:
			rc = StrBuf_write_one_chunk_callback(watcher->fd,
							     0/*TODO*/,
							     &IO->SendBuf);
		}

#ifdef BIGBAD_IODBG
		fprintf(fd, "Sent: BufSize: %d bytes.\n", rc);
		fclose(fd);
	}
#endif
	if (rc == 0)
	{
		ev_io_stop(event_base, &IO->send_event);
		switch (IO->NextState) {
		case eSendMore:
			assert(IO->SendDone);
			IO->NextState = IO->SendDone(IO);

			if ((IO->NextState == eTerminateConnection) ||
			    (IO->NextState == eAbort) )
				ShutDownCLient(IO);
			else {
				ev_io_start(event_base, &IO->send_event);
			}
			break;
		case eSendFile:
			if (IO->IOB.ChunkSendRemain > 0) {
				ev_io_start(event_base, &IO->recv_event);
				SetNextTimeout(IO, 100.0);

			} else {
				assert(IO->ReadDone);
				IO->NextState = IO->ReadDone(IO);
				switch(IO->NextState) {
				case eSendDNSQuery:
				case eReadDNSReply:
				case eDBQuery:
				case eConnect:
					break;
				case eSendReply:
				case eSendMore:
				case eSendFile:
					ev_io_start(event_base,
						    &IO->send_event);
					break;
				case eReadMessage:
				case eReadMore:
				case eReadPayload:
				case eReadFile:
					break;
				case eTerminateConnection:
				case eAbort:
					break;
				}
			}
			break;
		case eSendReply:
		    if (StrBufCheckBuffer(&IO->SendBuf) != eReadSuccess)
			break;
		    IO->NextState = eReadMore;
		case eReadMore:
		case eReadMessage:
		case eReadPayload:
		case eReadFile:
			if (StrBufCheckBuffer(&IO->RecvBuf) == eBufferNotEmpty)
			{
				HandleInbound(IO);
			}
			else {
				ev_io_start(event_base, &IO->recv_event);
			}

			break;
		case eDBQuery:
			/*
			 * we now live in another queue,
			 * so we have to unregister.
			 */
			ev_cleanup_stop(loop, &IO->abort_by_shutdown);
			break;
		case eSendDNSQuery:
		case eReadDNSReply:
		case eConnect:
		case eTerminateConnection:
		case eAbort:
			break;
		}
	}
	else if (rc < 0) {
		IO_Timeout_callback(loop, &IO->rw_timeout, revents);
	}
	/* else : must write more. */
}
static void
set_start_callback(struct ev_loop *loop, AsyncIO *IO, int revents)
{
	switch(IO->NextState) {
	case eReadMore:
	case eReadMessage:
	case eReadFile:
		ev_io_start(event_base, &IO->recv_event);
		break;
	case eSendReply:
	case eSendMore:
	case eReadPayload:
	case eSendFile:
		become_session(IO->CitContext);
		IO_send_callback(loop, &IO->send_event, revents);
		break;
	case eDBQuery:
	case eSendDNSQuery:
	case eReadDNSReply:
	case eConnect:
	case eTerminateConnection:
	case eAbort:
		/// TODO: WHUT?
		break;
	}
}

static void
IO_Timeout_callback(struct ev_loop *loop, ev_timer *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	ev_timer_stop (event_base, &IO->rw_timeout);
	become_session(IO->CitContext);

	if (IO->SendBuf.fd != 0)
	{
		ev_io_stop(event_base, &IO->send_event);
		ev_io_stop(event_base, &IO->recv_event);
		ev_timer_stop (event_base, &IO->rw_timeout);
		close(IO->SendBuf.fd);
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
	}

	assert(IO->Timeout);
	switch (IO->Timeout(IO))
	{
	case eAbort:
		ShutDownCLient(IO);
	default:
		break;
	}
}

static void
IO_connfail_callback(struct ev_loop *loop, ev_timer *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	ev_timer_stop (event_base, &IO->conn_fail);

	if (IO->SendBuf.fd != 0)
	{
		ev_io_stop(loop, &IO->conn_event);
		ev_io_stop(event_base, &IO->send_event);
		ev_io_stop(event_base, &IO->recv_event);
		ev_timer_stop (event_base, &IO->rw_timeout);
		close(IO->SendBuf.fd);
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
	}
	become_session(IO->CitContext);

	assert(IO->ConnFail);
	switch (IO->ConnFail(IO))
	{
	case eAbort:
		ShutDownCLient(IO);
	default:
		break;

	}
}

static void
IO_connfailimmediate_callback(struct ev_loop *loop,
			      ev_idle *watcher,
			      int revents)
{
	AsyncIO *IO = watcher->data;

	ev_idle_stop (event_base, &IO->conn_fail_immediate);

	if (IO->SendBuf.fd != 0)
	{
		close(IO->SendBuf.fd);
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
	}
	become_session(IO->CitContext);

	assert(IO->ConnFail);
	switch (IO->ConnFail(IO))
	{
	case eAbort:
		ShutDownCLient(IO);
	default:
		break;

	}
}

static void
IO_connestd_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	ev_io_stop(loop, &IO->conn_event);
	ev_timer_stop (event_base, &IO->conn_fail);
	set_start_callback(loop, IO, revents);
}
static void
IO_recv_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	const char *errmsg;
	ssize_t nbytes;
	AsyncIO *IO = watcher->data;

	switch (IO->NextState) {
	case eReadFile:
		nbytes = FileRecvChunked(&IO->IOB, &errmsg);
		if (nbytes < 0)
			StrBufPlain(IO->ErrMsg, errmsg, -1);
		else
		{
			if (IO->IOB.ChunkSendRemain == 0)
			{
				IO->NextState = eSendReply;
			}
			else
				return;
		}
		break;
	default:
		nbytes = StrBuf_read_one_chunk_callback(watcher->fd,
							0 /*TODO */,
							&IO->RecvBuf);
		break;
	}

#ifdef BIGBAD_IODBG
	{
		long nbytes;
		int rv = 0;
		char fn [SIZ];
		FILE *fd;
		const char *pch = ChrPtr(IO->RecvBuf.Buf);
		const char *pchh = IO->RecvBuf.ReadWritePointer;

		if (pchh == NULL)
			pchh = pch;

		nbytes = StrLength(IO->RecvBuf.Buf) - (pchh - pch);
		snprintf(fn, SIZ, "/tmp/foolog_ev_%s.%d",
			 ((CitContext*)(IO->CitContext))->ServiceName,
			 IO->SendBuf.fd);

		fd = fopen(fn, "a+");
		fprintf(fd, "Read: BufSize: %ld BufContent: [",
			nbytes);
		rv = fwrite(pchh, nbytes, 1, fd);
		if (!rv) printf("failed to write debug to %s!\n", fn);
		fprintf(fd, "]\n");
		fclose(fd);
	}
#endif
	if (nbytes > 0) {
		HandleInbound(IO);
	} else if (nbytes == 0) {
		assert(IO->Timeout);

		switch (IO->Timeout(IO))
		{
		case eAbort:
			ShutDownCLient(IO);
		default:
			break;
		}
		return;
	} else if (nbytes == -1) {
		// FD is gone. kick it. 
		StopClientWatchers(IO);
		EV_syslog(LOG_DEBUG,
			  "EVENT: Socket Invalid! %s \n",
			  strerror(errno));
		ShutDownCLient(IO);
		return;
	}
}

void
IO_postdns_callback(struct ev_loop *loop, ev_idle *watcher, int revents)
{
	AsyncIO *IO = watcher->data;
	EV_syslog(LOG_DEBUG, "event: %s\n", __FUNCTION__);
	become_session(IO->CitContext);
	assert(IO->DNS.Query->PostDNS);
	switch (IO->DNS.Query->PostDNS(IO))
	{
	case eAbort:
		assert(IO->DNS.Fail);
		switch (IO->DNS.Fail(IO)) {
		case eAbort:
////			StopClientWatchers(IO);
			ShutDownCLient(IO);
		default:
			break;
		}
	default:
		break;
	}
}


eNextState EvConnectSock(AsyncIO *IO,
			 double conn_timeout,
			 double first_rw_timeout,
			 int ReadFirst)
{
	struct sockaddr_in egress_sin;
	int fdflags;
	int rc = -1;

	become_session(IO->CitContext);

	if (ReadFirst) {
		IO->NextState = eReadMessage;
	}
	else {
		IO->NextState = eSendReply;
	}

	IO->SendBuf.fd = IO->RecvBuf.fd =
		socket(
			(IO->ConnectMe->IPv6)?PF_INET6:PF_INET,
			SOCK_STREAM,
			IPPROTO_TCP);

	if (IO->SendBuf.fd < 0) {
		EV_syslog(LOG_ERR,
			  "EVENT: socket() failed: %s\n",
			  strerror(errno));

		StrBufPrintf(IO->ErrMsg,
			     "Failed to create socket: %s",
			     strerror(errno));
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
		return eAbort;
	}
	fdflags = fcntl(IO->SendBuf.fd, F_GETFL);
	if (fdflags < 0) {
		EV_syslog(LOG_DEBUG,
			  "EVENT: unable to get socket flags! %s \n",
			  strerror(errno));
		StrBufPrintf(IO->ErrMsg,
			     "Failed to get socket flags: %s",
			     strerror(errno));
		close(IO->SendBuf.fd);
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
		return eAbort;
	}
	fdflags = fdflags | O_NONBLOCK;
	if (fcntl(IO->SendBuf.fd, F_SETFL, fdflags) < 0) {
		EV_syslog(
			LOG_DEBUG,
			"EVENT: unable to set socket nonblocking flags! %s \n",
			strerror(errno));
		StrBufPrintf(IO->ErrMsg,
			     "Failed to set socket flags: %s",
			     strerror(errno));
		close(IO->SendBuf.fd);
		IO->SendBuf.fd = IO->RecvBuf.fd = 0;
		return eAbort;
	}
/* TODO: maye we could use offsetof() to calc the position of data...
 * http://doc.dvgu.ru/devel/ev.html#associating_custom_data_with_a_watcher
 */
	ev_io_init(&IO->recv_event, IO_recv_callback, IO->RecvBuf.fd, EV_READ);
	IO->recv_event.data = IO;
	ev_io_init(&IO->send_event, IO_send_callback, IO->SendBuf.fd, EV_WRITE);
	IO->send_event.data = IO;

	ev_timer_init(&IO->conn_fail, IO_connfail_callback, conn_timeout, 0);
	IO->conn_fail.data = IO;
	ev_timer_init(&IO->rw_timeout, IO_Timeout_callback, first_rw_timeout,0);
	IO->rw_timeout.data = IO;




	/* for debugging you may bypass it like this:
	 * IO->Addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	 * ((struct sockaddr_in)IO->ConnectMe->Addr).sin_addr.s_addr =
	 *   inet_addr("127.0.0.1");
	 */
	if (IO->ConnectMe->IPv6) {
		rc = connect(IO->SendBuf.fd,
			     &IO->ConnectMe->Addr,
			     sizeof(struct sockaddr_in6));
	}
	else {
		/* If citserver is bound to a specific IP address on the host, make
		 * sure we use that address for outbound connections.
		 */
	
		memset(&egress_sin, 0, sizeof(egress_sin));
		egress_sin.sin_family = AF_INET;
		if (!IsEmptyStr(config.c_ip_addr)) {
			egress_sin.sin_addr.s_addr = inet_addr(config.c_ip_addr);
			if (egress_sin.sin_addr.s_addr == !INADDR_ANY) {
				egress_sin.sin_addr.s_addr = INADDR_ANY;
			}

			/* If this bind fails, no problem; we can still use INADDR_ANY */
			bind(IO->SendBuf.fd, (struct sockaddr *)&egress_sin, sizeof(egress_sin));
		}
		rc = connect(IO->SendBuf.fd,
			     (struct sockaddr_in *)&IO->ConnectMe->Addr,
			     sizeof(struct sockaddr_in));
	}

	if (rc >= 0){
		EVM_syslog(LOG_DEBUG, "connect() immediate success.\n");
		set_start_callback(event_base, IO, 0);
		ev_timer_start(event_base, &IO->rw_timeout);
		return IO->NextState;
	}
	else if (errno == EINPROGRESS) {
		EVM_syslog(LOG_DEBUG, "connect() have to wait now.\n");

		ev_io_init(&IO->conn_event,
			   IO_connestd_callback,
			   IO->SendBuf.fd,
			   EV_READ|EV_WRITE);

		IO->conn_event.data = IO;

		ev_io_start(event_base, &IO->conn_event);
		ev_timer_start(event_base, &IO->conn_fail);
		return IO->NextState;
	}
	else {
		ev_idle_init(&IO->conn_fail_immediate,
			     IO_connfailimmediate_callback);
		IO->conn_fail_immediate.data = IO;
		ev_idle_start(event_base, &IO->conn_fail_immediate);

		EV_syslog(LOG_ERR, "connect() failed: %s\n", strerror(errno));
		StrBufPrintf(IO->ErrMsg,
			     "Failed to connect: %s",
			     strerror(errno));
		return IO->NextState;
	}
	return IO->NextState;
}

void SetNextTimeout(AsyncIO *IO, double timeout)
{
	IO->rw_timeout.repeat = timeout;
	ev_timer_again (event_base,  &IO->rw_timeout);
}


eNextState ReAttachIO(AsyncIO *IO,
		      void *pData,
		      int ReadFirst)
{
	IO->Data = pData;
	become_session(IO->CitContext);
	ev_cleanup_start(event_base, &IO->abort_by_shutdown);
	if (ReadFirst) {
		IO->NextState = eReadMessage;
	}
	else {
		IO->NextState = eSendReply;
	}
	set_start_callback(event_base, IO, 0);

	return IO->NextState;
}

void InitIOStruct(AsyncIO *IO,
		  void *Data,
		  eNextState NextState,
		  IO_LineReaderCallback LineReader,
		  IO_CallBack DNS_Fail,
		  IO_CallBack SendDone,
		  IO_CallBack ReadDone,
		  IO_CallBack Terminate,
		  IO_CallBack ConnFail,
		  IO_CallBack Timeout,
		  IO_CallBack ShutdownAbort)
{
	IO->Data          = Data;

	IO->CitContext    = CloneContext(CC);
	((CitContext *)IO->CitContext)->session_specific_data = (char*) Data;

	IO->NextState     = NextState;

	IO->SendDone      = SendDone;
	IO->ReadDone      = ReadDone;
	IO->Terminate     = Terminate;
	IO->LineReader    = LineReader;
	IO->ConnFail      = ConnFail;
	IO->Timeout       = Timeout;
	IO->ShutdownAbort = ShutdownAbort;

	IO->DNS.Fail      = DNS_Fail;

	IO->SendBuf.Buf   = NewStrBufPlain(NULL, 1024);
	IO->RecvBuf.Buf   = NewStrBufPlain(NULL, 1024);
	IO->IOBuf         = NewStrBuf();
	EV_syslog(LOG_DEBUG,
		  "EVENT: Session lives at %p IO at %p \n",
		  Data, IO);

}

extern int evcurl_init(AsyncIO *IO);

int InitcURLIOStruct(AsyncIO *IO,
		     void *Data,
		     const char* Desc,
		     IO_CallBack SendDone,
		     IO_CallBack Terminate,
		     IO_CallBack ShutdownAbort)
{
	IO->Data          = Data;

	IO->CitContext    = CloneContext(CC);
	((CitContext *)IO->CitContext)->session_specific_data = (char*) Data;

	IO->SendDone = SendDone;
	IO->Terminate = Terminate;
	IO->ShutdownAbort = ShutdownAbort;

	strcpy(IO->HttpReq.errdesc, Desc);


	return  evcurl_init(IO);

}

void EV_backtrace(AsyncIO *IO)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;


	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			EV_syslog(LOG_ALERT, " BT %s\n", strings[i]);
		else
			EV_syslog(LOG_ALERT, " BT %p\n", stack_frames[i]);
	}
	free(strings);
#endif
}


ev_tstamp ctdl_ev_now (void)
{
	return ev_now(event_base);
}
