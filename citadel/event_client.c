/*
 *
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

extern int event_add_pipe[2];
extern citthread_mutex_t EventQueueMutex;
extern HashList *InboundEventQueue;
extern struct ev_loop *event_base;

	
int QueueEventContext(AsyncIO *IO, IO_CallBack CB)
{
	IOAddHandler *h;
	int i;

	h = (IOAddHandler*)malloc(sizeof(IOAddHandler));
	h->IO = IO;
	h->EvAttch = CB;

	citthread_mutex_lock(&EventQueueMutex);
	if (event_add_pipe[1] == -1) {
		citthread_mutex_unlock(&EventQueueMutex);
		free (h);
		return -1;
	}
	CtdlLogPrintf(CTDL_DEBUG, "EVENT Q\n");
	i = GetCount(InboundEventQueue);
	Put(InboundEventQueue, IKEY(i), h, NULL);
	citthread_mutex_unlock(&EventQueueMutex);

	write(event_add_pipe[1], "+_", 1);
	CtdlLogPrintf(CTDL_DEBUG, "EVENT Q Done.\n");
	return 0;
}


int ShutDownEventQueue(void)
{
	citthread_mutex_lock(&EventQueueMutex);
	if (event_add_pipe[1] == -1) {
		citthread_mutex_unlock(&EventQueueMutex);

		return -1;
	}
	write(event_add_pipe[1], "x_", 1);
	close(event_add_pipe[1]);
	event_add_pipe[1] = -1;
	citthread_mutex_unlock(&EventQueueMutex);
	return 0;
}

void FreeAsyncIOContents(AsyncIO *IO)
{
	FreeStrBuf(&IO->IOBuf);
	FreeStrBuf(&IO->SendBuf.Buf);
	FreeStrBuf(&IO->RecvBuf.Buf);
}


void ShutDownCLient(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "EVENT x %d\n", IO->sock);

	if (IO->sock != 0)
	{
		ev_io_stop(event_base, &IO->send_event);
		ev_io_stop(event_base, &IO->recv_event);
		ev_timer_stop (event_base, &IO->rw_timeout);
		close(IO->sock);
		IO->sock = 0;
		IO->SendBuf.fd = 0;
		IO->RecvBuf.fd = 0;
	}
	if (IO->DNSChannel != NULL) {
		ares_destroy(IO->DNSChannel);
		ev_io_stop(event_base, &IO->dns_recv_event);
		ev_io_stop(event_base, &IO->dns_send_event);
		IO->DNSChannel = NULL;
	}
	assert(IO->Terminate);
	IO->Terminate(IO);
	
}


eReadState HandleInbound(AsyncIO *IO)
{
	eReadState Finished = eBufferNotEmpty;
	
	while ((Finished == eBufferNotEmpty) && (IO->NextState == eReadMessage)){
		if (IO->RecvBuf.nBlobBytesWanted != 0) { 
				
		}
		else { /* Reading lines... */
//// lex line reply in callback, or do it ourselves. as nnn-blabla means continue reading in SMTP
			if (IO->LineReader)
				Finished = IO->LineReader(IO);
			else 
				Finished = StrBufChunkSipLine(IO->IOBuf, &IO->RecvBuf);
				
			switch (Finished) {
			case eMustReadMore: /// read new from socket... 
				return Finished;
				break;
			case eBufferNotEmpty: /* shouldn't happen... */
			case eReadSuccess: /// done for now...
				break;
			case eReadFail: /// WHUT?
				///todo: shut down! 
				break;
			}
					
		}
			
		if (Finished != eMustReadMore) {
			assert(IO->ReadDone);
			ev_io_stop(event_base, &IO->recv_event);
			IO->NextState = IO->ReadDone(IO);
			Finished = StrBufCheckBuffer(&IO->RecvBuf);
		}
	}


	if ((IO->NextState == eSendReply) ||
	    (IO->NextState == eSendMore))
	{
		assert(IO->SendDone);
		IO->NextState = IO->SendDone(IO);
		ev_io_start(event_base, &IO->send_event);
	}
	else if ((IO->NextState == eTerminateConnection) ||
		 (IO->NextState == eAbort) )
		ShutDownCLient(IO);
	return Finished;
}


static void
IO_send_callback(struct ev_loop *loop, ev_io *watcher, int revents)
{
	int rc;
	AsyncIO *IO = watcher->data;

	rc = StrBuf_write_one_chunk_callback(watcher->fd, 0/*TODO*/, &IO->SendBuf);

	if (rc == 0)
	{		
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
			snprintf(fn, SIZ, "/tmp/foolog_ev_%s.%d", "smtpev", IO->sock);
		
			fd = fopen(fn, "a+");
			fprintf(fd, "Read: BufSize: %ld BufContent: [",
				nbytes);
			rv = fwrite(pchh, nbytes, 1, fd);
			fprintf(fd, "]\n");
		
			
			fclose(fd);
		}
#endif
		ev_io_stop(event_base, &IO->send_event);
		switch (IO->NextState) {
		case eSendReply:
			break;
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
		case eReadMessage:
			if (StrBufCheckBuffer(&IO->RecvBuf) == eBufferNotEmpty) {
				HandleInbound(IO);
			}
			else {
				ev_io_start(event_base, &IO->recv_event);
			}

			break;
		case eTerminateConnection:
		case eAbort:
			break;
		}
	}
	else if (rc < 0) {
		assert(IO->Timeout);
		IO->Timeout(IO);
	}
	/* else : must write more. */
}
static void
set_start_callback(struct ev_loop *loop, AsyncIO *IO, int revents)
{
	
	switch(IO->NextState) {
	case eReadMessage:
		ev_io_start(event_base, &IO->recv_event);
		break;
	case eSendReply:
	case eSendMore:
		IO_send_callback(loop, &IO->send_event, revents);
		break;
	case eTerminateConnection:
	case eAbort:
		/// TODO: WHUT?
		break;
	}
}

static void
IO_Timout_callback(struct ev_loop *loop, ev_timer *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	ev_timer_stop (event_base, &IO->rw_timeout);
	assert(IO->Timeout);
	IO->Timeout(IO);
}
static void
IO_connfail_callback(struct ev_loop *loop, ev_timer *watcher, int revents)
{
	AsyncIO *IO = watcher->data;

	ev_timer_stop (event_base, &IO->conn_fail);
	ev_io_stop(loop, &IO->conn_event);
	assert(IO->ConnFail);
	IO->ConnFail(IO);
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
	ssize_t nbytes;
	AsyncIO *IO = watcher->data;

	nbytes = StrBuf_read_one_chunk_callback(watcher->fd, 0 /*TODO */, &IO->RecvBuf);
	if (nbytes > 0) {
		HandleInbound(IO);
	} else if (nbytes == 0) {
		assert(IO->Timeout);
		IO->Timeout(IO); /* this is a timeout... */
		return;
	} else if (nbytes == -1) {
/// TODO: FD is gone. kick it.        sock_buff_invoke_free(sb, errno);
		return;
	}
}



eNextState event_connect_socket(AsyncIO *IO, double conn_timeout, double first_rw_timeout)
{
	int fdflags; 
	int rc = -1;

	IO->SendBuf.fd = IO->RecvBuf.fd = 
		IO->sock = socket(
			(IO->IP6)?PF_INET6:PF_INET, 
			SOCK_STREAM, 
			IPPROTO_TCP);

	if (IO->sock < 0) {
		CtdlLogPrintf(CTDL_ERR, "EVENT: socket() failed: %s\n", strerror(errno));
		StrBufPrintf(IO->ErrMsg, "Failed to create socket: %s", strerror(errno));
//		freeaddrinfo(res);
		return eAbort;
	}
	fdflags = fcntl(IO->sock, F_GETFL);
	if (fdflags < 0) {
		CtdlLogPrintf(CTDL_DEBUG,
			      "EVENT: unable to get socket flags! %s \n",
			      strerror(errno));
		StrBufPrintf(IO->ErrMsg, "Failed to get socket flags: %s", strerror(errno));
		return eAbort;
	}
	fdflags = fdflags | O_NONBLOCK;
	if (fcntl(IO->sock, F_SETFL, fdflags) < 0) {
		CtdlLogPrintf(CTDL_DEBUG,
			      "EVENT: unable to set socket nonblocking flags! %s \n",
			      strerror(errno));
		StrBufPrintf(IO->ErrMsg, "Failed to set socket flags: %s", strerror(errno));
		close(IO->sock);
		return eAbort;
	}
/* TODO: maye we could use offsetof() to calc the position of data... 
 * http://doc.dvgu.ru/devel/ev.html#associating_custom_data_with_a_watcher
 */
	ev_io_init(&IO->recv_event, IO_recv_callback, IO->sock, EV_READ);
	IO->recv_event.data = IO;
	ev_io_init(&IO->send_event, IO_send_callback, IO->sock, EV_WRITE);
	IO->send_event.data = IO;

	ev_timer_init(&IO->conn_fail, IO_connfail_callback, conn_timeout, 0);
	IO->conn_fail.data = IO;
	ev_timer_init(&IO->rw_timeout, IO_Timout_callback, first_rw_timeout, 0);
	IO->rw_timeout.data = IO;

	if (IO->IP6)
		rc = connect(IO->sock, &IO->Addr, sizeof(struct sockaddr_in6));
	else
		rc = connect(IO->sock, (struct sockaddr_in *)&IO->Addr, sizeof(struct sockaddr_in));

	if (rc >= 0){
////		freeaddrinfo(res);
		set_start_callback(event_base, IO, 0);
		ev_timer_start(event_base, &IO->rw_timeout);
		return IO->NextState;
	}
	else if (errno == EINPROGRESS) {

		ev_io_init(&IO->conn_event, IO_connestd_callback, IO->sock, EV_READ|EV_WRITE);
		IO->conn_event.data = IO;

		ev_io_start(event_base, &IO->conn_event);
		ev_timer_start(event_base, &IO->conn_fail);
		return IO->NextState;
	}
	else {
		CtdlLogPrintf(CTDL_ERR, "connect() failed: %s\n", strerror(errno));
		StrBufPrintf(IO->ErrMsg, "Failed to connect: %s", strerror(errno));
		assert(IO->ConnFail);
		IO->ConnFail(IO);
		return eAbort;
	}
	return IO->NextState;
}

void SetNextTimeout(AsyncIO *IO, double timeout)
{
	IO->rw_timeout.repeat = timeout;
	ev_timer_again (event_base,  &IO->rw_timeout);
}

eNextState InitEventIO(AsyncIO *IO, 
		       void *pData, 
		       double conn_timeout, 
		       double first_rw_timeout,
		       int ReadFirst)
{
	IO->Data = pData;
	
	if (ReadFirst) {
		IO->NextState = eReadMessage;
	}
	else {
		IO->NextState = eSendReply;
	}
	return event_connect_socket(IO, conn_timeout, first_rw_timeout);
}
