/*
 * XMPP event queue
 *
 * Copyright (c) 2007-2009 by Art Cancro
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
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <expat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"
#include "serv_xmpp.h"

int queue_event_seq = 0;

void xmpp_queue_event(int event_type, char *email_addr) {

	struct xmpp_event *xptr = NULL;
	struct xmpp_event *new_event = NULL;
	struct xmpp_event *last = NULL;
	int purged_something = 0;
	struct CitContext *cptr;

	MARK_syslog(LOG_DEBUG, "xmpp_queue_event(%d, %s)\n", event_type, email_addr);

	/* Purge events more than a minute old */
	begin_critical_section(S_XMPP_QUEUE);
	do {
		purged_something = 0;
		if (xmpp_queue != NULL) {
			if ((time(NULL) - xmpp_queue->event_time) > 60) {
				xptr = xmpp_queue->next;
				free(xmpp_queue);
				xmpp_queue = xptr;
				purged_something = 1;
			}
		}
	} while(purged_something);
	end_critical_section(S_XMPP_QUEUE);

	/* Create a new event */
	new_event = (struct xmpp_event *) malloc(sizeof(struct xmpp_event));
	new_event->next = NULL;
	new_event->event_time = time(NULL);
	new_event->event_seq = ++queue_event_seq;
	new_event->event_type = event_type;
	new_event->session_which_generated_this_event = CC->cs_pid;
	safestrncpy(new_event->event_jid, email_addr, sizeof new_event->event_jid);

	/* Add it to the list */
	begin_critical_section(S_XMPP_QUEUE);
	if (xmpp_queue == NULL) {
		xmpp_queue = new_event;
	}
	else {
		for (xptr = xmpp_queue; xptr != NULL; xptr = xptr->next) {
			if (xptr->next == NULL) {
				last = xptr;
			}
		}
		last->next = new_event;
	}
	end_critical_section(S_XMPP_QUEUE);

	/* Tell the sessions that something is happening */
	begin_critical_section(S_SESSION_TABLE);
	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if ((cptr->logged_in) && (cptr->h_async_function == xmpp_async_loop)) {
			set_async_waiting(cptr);
		}
	}
	end_critical_section(S_SESSION_TABLE);
}


/* 
 * Are we interested in anything from the queue?  (Called in async loop)
 */
void xmpp_process_events(void) {
	struct xmpp_event *xptr = NULL;
	int highest_event = 0;

	for (xptr=xmpp_queue; xptr!=NULL; xptr=xptr->next) {
		if (xptr->event_seq > XMPP->last_event_processed) {

			switch(xptr->event_type) {

				case XMPP_EVT_LOGIN:
				case XMPP_EVT_LOGOUT:
					if (xptr->session_which_generated_this_event != CC->cs_pid) {
						xmpp_presence_notify(xptr->event_jid, xptr->event_type);
					}
					break;
			}

			if (xptr->event_seq > highest_event) {
				highest_event = xptr->event_seq;
			}
		}
	}

	XMPP->last_event_processed = highest_event;
}


void xmpp_cleanup_events(void)
{
        struct xmpp_event *ptr, *ptr2;
        begin_critical_section(S_XMPP_QUEUE);
	ptr = xmpp_queue;
	xmpp_queue = NULL;
	while (ptr != NULL) {
		ptr2 = ptr->next;
		free(ptr);
		ptr = ptr2;
	}
        end_critical_section(S_XMPP_QUEUE);

}
