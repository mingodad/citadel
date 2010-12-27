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

int event_add_pipe[2] = {-1, -1};

citthread_mutex_t EventQueueMutex; /* locks the access to the following vars: */
HashList *QueueEvents = NULL;

HashList *InboundEventQueue = NULL;
HashList *InboundEventQueues[2] = { NULL, NULL };

static struct event_base *event_base;
struct event queue_add_event;

static void QueueEventAddCallback(int fd, short event, void *ctx)
{
	char buf[10];
	HashList *q;
	void *v;
	HashPos  *It;
	long len;
	const char *Key;

	/* get the control command... */
	read(fd, buf, 1);
	switch (buf[0]) {
	case '+':
		citthread_mutex_lock(&EventQueueMutex);

		if (InboundEventQueues[0] == InboundEventQueue) {
			InboundEventQueue = InboundEventQueues[1];
			q = InboundEventQueues[0];
		}
		else {
			InboundEventQueue = InboundEventQueues[0];
			q = InboundEventQueues[1];
		}
		citthread_mutex_unlock(&EventQueueMutex);

		It = GetNewHashPos(q, 0);
		while (GetNextHashPos(q, It, &len, &Key, &v))
		{
			IOAddHandler *h = v;
			h->EvAttch(h->Ctx);
		}
		DeleteHashPos(&It);
		DeleteHashContent(&q);
/// TODO: add it to QueueEvents
		break;
	case 'x':
		event_del(&queue_add_event);
		close(event_add_pipe[0]);
/// TODO; flush QueueEvents fd's and delete it.
		event_base_loopexit(event_base, NULL);
	}
	/* Unblock the other side */
//	read(fd, buf, 1);
	CtdlLogPrintf(CTDL_DEBUG, "EVENT Q Read done.\n");
}


void InitEventQueue(void)
{
	struct rlimit LimitSet;

	event_base = event_init();
/*
	base = event_base_new();
	if (!base)
		return NULL; / *XXXerr*/

	citthread_mutex_init(&EventQueueMutex, NULL);

	if (pipe(event_add_pipe) != 0) {
		CtdlLogPrintf(CTDL_EMERG, "Unable to create pipe for libevent queueing: %s\n", strerror(errno));
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
/*
 * this thread operates the select() etc. via libevent.
 * 
 * 
 */
void *client_event_thread(void *arg) 
{
	struct CitContext libevent_client_CC;
	CtdlFillSystemContext(&libevent_client_CC, "LibEvent Thread");
//	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC);
	CtdlLogPrintf(CTDL_DEBUG, "client_event_thread() initializing\n");
	
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
		InitEventQueue();
		CtdlThreadCreate("Client event", CTDLTHREAD_BIGSTACK, client_event_thread, NULL);
/// todo register shutdown callback.
	}
#endif
	return "event";
}
