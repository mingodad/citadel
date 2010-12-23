/*
 * This module is an SMTP and ESMTP implementation for the Citadel system.
 * It is compliant with all of the following:
 *
 * RFC  821 - Simple Mail Transfer Protocol
 * RFC  876 - Survey of SMTP Implementations
 * RFC 1047 - Duplicate messages and SMTP
 * RFC 1652 - 8 bit MIME
 * RFC 1869 - Extended Simple Mail Transfer Protocol
 * RFC 1870 - SMTP Service Extension for Message Size Declaration
 * RFC 2033 - Local Mail Transfer Protocol
 * RFC 2197 - SMTP Service Extension for Command Pipelining
 * RFC 2476 - Message Submission
 * RFC 2487 - SMTP Service Extension for Secure SMTP over TLS
 * RFC 2554 - SMTP Service Extension for Authentication
 * RFC 2821 - Simple Mail Transfer Protocol
 * RFC 2822 - Internet Message Format
 * RFC 2920 - SMTP Service Extension for Command Pipelining
 *  
 * The VRFY and EXPN commands have been removed from this implementation
 * because nobody uses these commands anymore, except for spammers.
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

#include "ctdl_module.h"

#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT

#include "event_client.h"

int event_add_pipe[2];

citthread_mutex_t EventQueueMutex; /* locks the access to the following vars: */
void *QueueEventAddPtr = NULL;
EventContextAttach EventContextAttachPtr;
AsyncIO *QueueThisIO;
HashList *QueueEvents = NULL;

static struct event_base *event_base;
struct event queue_add_event;

static void QueueEventAddCallback(int fd, short event, void *ctx)
{
	char buf[10];

	/* get the control command... */
	read(fd, buf, 1);
	switch (buf[0]) {
	case '+':
		EventContextAttachPtr(QueueEventAddPtr);
/// TODO: add it to QueueEvents
		break;
	case 'x':
		event_del(&queue_add_event);
		close(event_add_pipe[0]);
/// TODO; flush QueueEvents fd's and delete it.
		event_base_loopexit(event_base, NULL);
	}
	/* Unblock the other side */
	read(fd, buf, 1);
}

/*
 * this thread operates the select() etc. via libevent.
 * 
 * 
 */
void *client_event_thread(void *arg) {
	struct CitContext libevent_client_CC;
	event_base = event_init();
/*
	base = event_base_new();
	if (!base)
		return NULL; /*XXXerr*/

	citthread_mutex_init(&EventQueueMutex, NULL);
	CtdlFillSystemContext(&libevent_client_CC, "LibEvent Thread");
//	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	CtdlLogPrintf(CTDL_DEBUG, "client_event_thread() initializing\n");
	
	if (pipe(event_add_pipe) != 0) {
		CtdlLogPrintf(CTDL_EMERG, "Unable to create pipe for libevent queueing: %s\n", strerror(errno));
		abort();
	}

	event_set(&queue_add_event, 
		  event_add_pipe[0], 
		  EV_READ|EV_PERSIST,
		  QueueEventAddCallback, 
		  NULL);
	
	event_add(&queue_add_event, NULL);


	event_dispatch();
	CtdlClearSystemContext();
	event_base_free(event_base);
	citthread_mutex_destroy(&EventQueueMutex);
	return(NULL);
}

#endif

CTDL_MODULE_INIT(event_client)
{
#ifdef EXPERIMENTAL_SMTP_EVENT_CLIENT
	if (!threading)
	{
		CtdlThreadCreate("Client event", CTDLTHREAD_BIGSTACK, client_event_thread, NULL);
		QueueEvents = NewHash(1, Flathash);
/// todo register shutdown callback.
	}
#endif
	return "event";
}
