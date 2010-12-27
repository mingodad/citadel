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

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT

#include <event.h>
#include "event_client.h"

extern int event_add_pipe[2];
extern citthread_mutex_t EventQueueMutex;
extern HashList *InboundEventQueue;

#define SEND_EVENT 1
#define RECV_EVENT 2
	
int QueueEventContext(void *Ctx, AsyncIO *IO, EventContextAttach CB)
{
	IOAddHandler *h;
	int i;

	h = (IOAddHandler*)malloc(sizeof(IOAddHandler));
	h->Ctx = Ctx;
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

/*
  static void
  setup_signal_handlers(struct instance *instance)
  {
  signal(SIGPIPE, SIG_IGN);

  event_set(&instance->sigterm_event, SIGTERM, EV_SIGNAL|EV_PERSIST,
  exit_event_callback, instance);
  event_add(&instance->sigterm_event, NULL);

  event_set(&instance->sigint_event, SIGINT, EV_SIGNAL|EV_PERSIST,
  exit_event_callback, instance);
  event_add(&instance->sigint_event, NULL);

  event_set(&instance->sigquit_event, SIGQUIT, EV_SIGNAL|EV_PERSIST,
  exit_event_callback, instance);
  event_add(&instance->sigquit_event, NULL);
  }
*/

void ShutDownCLient(AsyncIO *IO)
{
	CtdlLogPrintf(CTDL_DEBUG, "EVENT x %d\n", IO->sock);
	switch (IO->active_event) {
	case SEND_EVENT:
		event_del(&IO->send_event);
		break;
	case RECV_EVENT:
		event_del(&IO->recv_event);
		break;
	case 0:
		// no event active here; just bail out.
		break;
	}
	IO->active_event = 0;
	IO->Terminate(IO->Data);

//	citthread_mutex_lock(&EventQueueMutex);

///QueueEvents /// todo remove from hash.

//	citthread_mutex_unlock(&EventQueueMutex);
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
			event_del(&IO->recv_event);
			IO->active_event = 0;
			IO->NextState = IO->ReadDone(IO->Data);
			Finished = StrBufCheckBuffer(&IO->RecvBuf);
		}
	}


	if ((IO->NextState == eSendReply) ||
	    (IO->NextState == eSendMore))
	{
		IO->NextState = IO->SendDone(IO->Data);
		event_add(&IO->send_event, NULL);
		IO->active_event = SEND_EVENT;
			
	}
	else if ((IO->NextState == eTerminateConnection) ||
		 (IO->NextState == eAbort) )
		ShutDownCLient(IO);
	return Finished;
}


static void
IO_send_callback(int fd, short event, void *ctx)
{
	int rc;
	AsyncIO *IO = ctx;

	(void)fd;
	(void)event;
	CtdlLogPrintf(CTDL_DEBUG, "EVENT -> %d  : [%s%s%s%s]\n",
		      (int) fd,
		      (event&EV_TIMEOUT) ? " timeout" : "",
		      (event&EV_READ)    ? " read" : "",
		      (event&EV_WRITE)   ? " write" : "",
		      (event&EV_SIGNAL)  ? " signal" : "");

///    assert(fd == IO->sock);
	
	rc = StrBuf_write_one_chunk_callback(fd, event, &IO->SendBuf);

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
		event_del(&IO->send_event);
		IO->active_event = 0;
		switch (IO->NextState) {
		case eSendReply:
			break;
		case eSendMore:
			IO->NextState = IO->SendDone(IO->Data);

			if ((IO->NextState == eTerminateConnection) ||
			    (IO->NextState == eAbort) )
				ShutDownCLient(IO);
			else {
				event_add(&IO->send_event, NULL);
				IO->active_event = SEND_EVENT;
			}
			break;
		case eReadMessage:
			if (StrBufCheckBuffer(&IO->RecvBuf) == eBufferNotEmpty) {
				HandleInbound(IO);
			}
			else {
				event_add(&IO->recv_event, NULL);
				IO->active_event = RECV_EVENT;
			}

			break;
		case eAbort:
			break;
		}
	}
	else if (rc > 0)
		ShutDownCLient(IO);
//	else 
		///abort!
}


static void
IO_recv_callback(int fd, short event, void *ctx)
{
	ssize_t nbytes;
	AsyncIO *IO = ctx;

//    assert(fd == IO->sock);
	
//    assert(fd == sb->fd);

	CtdlLogPrintf(CTDL_DEBUG, "EVENT <- %d  : [%s%s%s%s]\n",
            (int) fd,
            (event&EV_TIMEOUT) ? " timeout" : "",
            (event&EV_READ)    ? " read" : "",
            (event&EV_WRITE)   ? " write" : "",
            (event&EV_SIGNAL)  ? " signal" : "");
	nbytes = StrBuf_read_one_chunk_callback(fd, event, &IO->RecvBuf);
	if (nbytes > 0) {
		HandleInbound(IO);
	} else if (nbytes == 0) {
		ShutDownCLient(IO);
///  TODO: this is a timeout???  sock_buff_invoke_free(sb, 0); seems as if socket is gone then?
		return;
	} else if (nbytes == -1) {
/// TODO: FD is gone. kick it.        sock_buff_invoke_free(sb, errno);
		return;
	}
}

void InitEventIO(AsyncIO *IO, 
		 void *pData, 
		 IO_CallBack ReadDone, 
		 IO_CallBack SendDone, 
		 IO_CallBack Terminate, 
		 IO_LineReaderCallback LineReader,
		 int ReadFirst)
{
	IO->Data = pData;
	IO->SendDone = SendDone;
	IO->ReadDone = ReadDone;
	IO->Terminate = Terminate;
	IO->LineReader = LineReader;

	event_set(&IO->recv_event, 
		  IO->sock, 
		  EV_READ|EV_PERSIST,
		  IO_recv_callback, 
		  IO);

	event_set(&IO->send_event, 
		  IO->sock,
		  EV_WRITE|EV_PERSIST,
		  IO_send_callback, 
		  IO);

	if (ReadFirst) {
		IO->NextState = eReadMessage;
		event_add(&IO->recv_event, NULL);
		IO->active_event = RECV_EVENT;
	}
	else {
		IO->NextState = eSendReply;
	}
}


#endif
