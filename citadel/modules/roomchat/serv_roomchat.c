/*
 * This module handles instant messaging between users.
 * 
 * Copyright (c) 2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include <assert.h>

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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "user_ops.h"
#include "ctdl_module.h"

struct chatmsg {
	struct chatmsg *next;
	time_t timestamp;
	int seq;
	long roomnum;
	char *sender;
	char *msgtext;
};

struct chatmsg *first_chat_msg = NULL;
struct chatmsg *last_chat_msg = NULL;


/* 
 * Periodically called for housekeeping.  Expire old chat messages so they don't take up memory forever.
 */
void roomchat_timer(void) {
	struct chatmsg *ptr;

	begin_critical_section(S_CHATQUEUE);

	while ((first_chat_msg != NULL) && ((time(NULL) - first_chat_msg->timestamp) > 300)) {
		ptr = first_chat_msg->next;
		free(first_chat_msg->sender);
		free(first_chat_msg->msgtext);
		free(first_chat_msg);
		first_chat_msg = ptr;
		if (first_chat_msg == NULL) {
			last_chat_msg = NULL;
		}
	}

	end_critical_section(S_CHATQUEUE);
}


/*
 * Perform shutdown-related activities...
 */
void roomchat_shutdown(void) {
	/* if we ever start logging chats, we have to flush them to disk here .*/
}


/*
 * Add a message into the chat queue
 */
void add_to_chat_queue(char *msg) {
	static int seq = 0;

	struct chatmsg *m = malloc(sizeof(struct chatmsg));
	if (!m) return;

	m->next = NULL;
	m->timestamp = time(NULL);
	m->roomnum = CC->room.QRnumber;
	m->sender = strdup(CC->user.fullname);
	m->msgtext = strdup(msg);

	if ((m->sender == NULL) || (m->msgtext == NULL)) {
		free(m->sender);
		free(m->msgtext);
		free(m);
		return;
	}

	begin_critical_section(S_CHATQUEUE);
	m->seq = ++seq;

	if (first_chat_msg == NULL) {
		assert(last_chat_msg == NULL);
		first_chat_msg = m;
		last_chat_msg = m;
	}
	else {
		assert(last_chat_msg != NULL);
		assert(last_chat_msg->next == NULL);
		last_chat_msg->next = m;
		last_chat_msg = m;
	}

	end_critical_section(S_CHATQUEUE);
}


/*
 * Transmit a message into a room chat
 */
void roomchat_send(char *argbuf) {
	char buf[1024];

	if ((CC->cs_flags & CS_CHAT) == 0) {
		cprintf("%d Session is not in chat mode.\n", ERROR);
		return;
	}

	cprintf("%d send now\n", SEND_LISTING);
	while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000")) {
		add_to_chat_queue(buf);
	}
}


/*
 * Poll room for incoming chat messages
 */
void roomchat_poll(char *argbuf) {
	int newer_than = 0;
	struct chatmsg *found = NULL;
	struct chatmsg *ptr = NULL;

	newer_than = extract_int(argbuf, 1);

	if ((CC->cs_flags & CS_CHAT) == 0) {
		cprintf("%d Session is not in chat mode.\n", ERROR);
		return;
	}

	begin_critical_section(S_CHATQUEUE);
	for (ptr = first_chat_msg; ((ptr != NULL) && (found == NULL)); ptr = ptr->next) {
		if ((ptr->seq > newer_than) && (ptr->roomnum == CC->room.QRnumber)) {
			found = ptr;
		}
	}
	end_critical_section(S_CHATQUEUE);

	if (found == NULL) {
		cprintf("%d no messages\n", ERROR + MESSAGE_NOT_FOUND);
		return;
	}

	cprintf("%d %d|%ld|%s\n", LISTING_FOLLOWS, found->seq, found->timestamp, found->sender);
	cprintf("%s\n", found->msgtext);
	cprintf("000\n");
}



/*
 * list users in chat in this room
 */
void roomchat_rwho(char *argbuf) {
	struct CitContext *nptr;
	int nContexts, i;

	if ((CC->cs_flags & CS_CHAT) == 0) {
		cprintf("%d Session is not in chat mode.\n", ERROR);
		return;
	}

	cprintf("%d%c \n", LISTING_FOLLOWS, CtdlCheckExpress() );
	
	nptr = CtdlGetContextArray(&nContexts) ;		// grab a copy of the wholist
	if (nptr) {
		for (i=0; i<nContexts; i++)  {			// list the users
		        if ( (nptr[i].room.QRnumber == CC->room.QRnumber) 
			   && (nptr[i].cs_flags & CS_CHAT)
			) {
				cprintf("%s\n", nptr[i].user.fullname);
			}
		}
		free(nptr);					// free our copy
	}

	cprintf("000\n");
}



/*
 * Participate in real time chat in a room
 */
void cmd_rcht(char *argbuf)
{
	char subcmd[16];

	if (CtdlAccessCheck(ac_logged_in)) return;

	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);

	if (!strcasecmp(subcmd, "enter")) {
		CC->cs_flags |= CS_CHAT;
		cprintf("%d Entering chat mode.\n", CIT_OK);
	}
	else if (!strcasecmp(subcmd, "exit")) {
		CC->cs_flags &= ~CS_CHAT;
		cprintf("%d Exiting chat mode.\n", CIT_OK);
	}
	else if (!strcasecmp(subcmd, "send")) {
		roomchat_send(argbuf);
	}
	else if (!strcasecmp(subcmd, "poll")) {
		roomchat_poll(argbuf);
	}
	else if (!strcasecmp(subcmd, "rwho")) {
		roomchat_rwho(argbuf);
	}
	else {
		cprintf("%d Invalid subcommand\n", ERROR + CMD_NOT_SUPPORTED);
	}
}


CTDL_MODULE_INIT(roomchat)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_rcht, "RCHT", "Participate in real time chat in a room");
		CtdlRegisterSessionHook(roomchat_timer, EVT_TIMER, PRIO_CLEANUP + 400);
		CtdlRegisterSessionHook(roomchat_shutdown, EVT_SHUTDOWN, PRIO_SHUTDOWN + 55);
	}
	
	/* return our module name for the log */
	return "roomchat";
}
