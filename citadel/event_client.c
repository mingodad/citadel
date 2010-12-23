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
extern void *QueueEventAddPtr;
extern AsyncIO *QueueThisIO;
extern EventContextAttach EventContextAttachPtr;

int QueueEventContext(void *Ctx, AsyncIO *IO, EventContextAttach CB)
{
	citthread_mutex_lock(&EventQueueMutex);

	QueueEventAddPtr = Ctx;
	EventContextAttachPtr = CB;
	QueueThisIO = IO;

	write(event_add_pipe[1], "+_", 1);
	citthread_mutex_unlock(&EventQueueMutex);
	return 0;
}


int ShutDownEventQueue(void)
{
	write(event_add_pipe[1], "x_", 1);
	close(event_add_pipe[1]);
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
			IO->NextState = IO->ReadDone(IO->Data);
			Finished = StrBufCheckBuffer(&IO->RecvBuf);
		}
	}


	if ((IO->NextState == eSendReply) ||
	    (IO->NextState == eSendMore))
	{
		IO->NextState = IO->SendDone(IO->Data);
		event_add(&IO->send_event, NULL);
			
	}
	else if ((IO->NextState == eTerminateConnection) ||
		 (IO->NextState == eAbort) )
{


	}
	return Finished;
}


static void
IO_send_callback(int fd, short event, void *ctx)
{
	int rc;
	AsyncIO *IO = ctx;

	(void)fd;
	(void)event;
	
///    assert(fd == IO->sock);
	
	rc = StrBuf_write_one_chunk_callback(fd, event, &IO->SendBuf);

	if (rc == 0)
	{
	    event_del(&IO->send_event);
	    switch (IO->NextState) {
	    case eSendReply:
		    break;
	    case eSendMore:
		    IO->NextState = IO->SendDone(IO->Data);
		    event_add(&IO->send_event, NULL);
		    break;
	    case eReadMessage:
		    if (StrBufCheckBuffer(&IO->RecvBuf) == eBufferNotEmpty) {
			    HandleInbound(IO);
		    }
		    else {
			    event_add(&IO->recv_event, NULL);
		    }

		    break;
	    case eAbort:
		    break;
	    }
	}
	else if (rc > 0)
		return;
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

	nbytes = StrBuf_read_one_chunk_callback(fd, event, &IO->RecvBuf);
	if (nbytes > 0) {
		HandleInbound(IO);
	} else if (nbytes == 0) {
		///  TODO: this is a timeout???  sock_buff_invoke_free(sb, 0);
		return;
	} else if (nbytes == -1) {
/// TODO: FD is gone. kick it.        sock_buff_invoke_free(sb, errno);
		return;
	}
}

void IOReadNextLine(AsyncIO *IO, int timeout)
{

}

void IOReadNextBLOB(AsyncIO *IO, int timeout, long size)
{
}

void InitEventIO(AsyncIO *IO, 
		 void *pData, 
		 IO_CallBack ReadDone, 
		 IO_CallBack SendDone, 
		 IO_LineReaderCallback LineReader,
		 int ReadFirst)
{
	IO->Data = pData;
	IO->SendDone = SendDone;
	IO->ReadDone = ReadDone;
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
	}
	else {
		IO->NextState = eSendReply;
	}
}


#endif
