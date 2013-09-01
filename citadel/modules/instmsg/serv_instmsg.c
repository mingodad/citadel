/*
 * This module handles instant messaging between users.
 * 
 * Copyright (c) 1987-2012 by the citadel.org team
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
#include "serv_instmsg.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "user_ops.h"
#include "ctdl_module.h"

struct imlog {
	struct imlog *next;
	long usernums[2];
	char usernames[2][128];
	time_t lastmsg;
	int last_serial;
	StrBuf *conversation;
};

struct imlog *imlist = NULL;

/*
 * This function handles the logging of instant messages to disk.
 */
void log_instant_message(struct CitContext *me, struct CitContext *them, char *msgtext, int serial_number)
{
	long usernums[2];
	long t;
	struct imlog *iptr = NULL;
	struct imlog *this_im = NULL;
	
	memset(usernums, 0, sizeof usernums);
	usernums[0] = me->user.usernum;
	usernums[1] = them->user.usernum;

	/* Always put the lower user number first, so we can use the array as a hash value which
	 * represents a pair of users.  For a broadcast message one of the users will be 0.
	 */
	if (usernums[0] > usernums[1]) {
		t = usernums[0];
		usernums[0] = usernums[1];
		usernums[1] = t;
	}

	begin_critical_section(S_IM_LOGS);

	/* Look for an existing conversation in the hash table.
	 * If not found, create a new one.
	 */

	this_im = NULL;
	for (iptr = imlist; iptr != NULL; iptr = iptr->next) {
		if ((iptr->usernums[0] == usernums[0]) && (iptr->usernums[1] == usernums[1])) {
			/* Existing conversation */
			this_im = iptr;
		}
	}
	if (this_im == NULL) {
		/* New conversation */
		this_im = malloc(sizeof(struct imlog));
		memset(this_im, 0, sizeof (struct imlog));
		this_im->usernums[0] = usernums[0];
		this_im->usernums[1] = usernums[1];
		/* usernames[] and usernums[] might not be in the same order.  This is not an error. */
		if (me) {
			safestrncpy(this_im->usernames[0], me->user.fullname, sizeof this_im->usernames[0]);
		}
		if (them) {
			safestrncpy(this_im->usernames[1], them->user.fullname, sizeof this_im->usernames[1]);
		}
		this_im->conversation = NewStrBuf();
		this_im->next = imlist;
		imlist = this_im;
		StrBufAppendBufPlain(this_im->conversation, HKEY(
			"Content-type: text/html\r\n"
			"Content-transfer-encoding: 7bit\r\n"
			"\r\n"
			"<html><body>\r\n"
			), 0);
	}


	/* Since it's possible for this function to get called more than once if a user is logged
	 * in on multiple sessions, we use the message's serial number to keep track of whether
	 * we've already logged it.
	 */
	if (this_im->last_serial != serial_number)
	{
		this_im->lastmsg = time(NULL);		/* Touch the timestamp so we know when to flush */
		this_im->last_serial = serial_number;
		StrBufAppendBufPlain(this_im->conversation, HKEY("<p><b>"), 0);
		StrBufAppendBufPlain(this_im->conversation, me->user.fullname, -1, 0);
		StrBufAppendBufPlain(this_im->conversation, HKEY(":</b> "), 0);
		StrEscAppend(this_im->conversation, NULL, msgtext, 0, 0);
		StrBufAppendBufPlain(this_im->conversation, HKEY("</p>\r\n"), 0);
	}
	end_critical_section(S_IM_LOGS);
}


/*
 * Delete any remaining instant messages
 */
void delete_instant_messages(void) {
	struct ExpressMessage *ptr;

	begin_critical_section(S_SESSION_TABLE);
	while (CC->FirstExpressMessage != NULL) {
		ptr = CC->FirstExpressMessage->next;
		if (CC->FirstExpressMessage->text != NULL)
			free(CC->FirstExpressMessage->text);
		free(CC->FirstExpressMessage);
		CC->FirstExpressMessage = ptr;
	}
	end_critical_section(S_SESSION_TABLE);
}



/*
 * Retrieve instant messages
 */
void cmd_gexp(char *argbuf) {
	struct ExpressMessage *ptr;

	if (CC->FirstExpressMessage == NULL) {
		cprintf("%d No instant messages waiting.\n", ERROR + MESSAGE_NOT_FOUND);
		return;
	}

	begin_critical_section(S_SESSION_TABLE);
	ptr = CC->FirstExpressMessage;
	CC->FirstExpressMessage = CC->FirstExpressMessage->next;
	end_critical_section(S_SESSION_TABLE);

	cprintf("%d %d|%ld|%d|%s|%s|%s\n",
		LISTING_FOLLOWS,
		((ptr->next != NULL) ? 1 : 0),		/* more msgs? */
		(long)ptr->timestamp,			/* time sent */
		ptr->flags,				/* flags */
		ptr->sender,				/* sender of msg */
		config.c_nodename,			/* static for now (and possibly deprecated) */
		ptr->sender_email			/* email or jid of sender */
	);

	if (ptr->text != NULL) {
		memfmout(ptr->text, "\n");
		free(ptr->text);
	}

	cprintf("000\n");
	free(ptr);
}

/*
 * Asynchronously deliver instant messages
 */
void cmd_gexp_async(void) {

	/* Only do this if the session can handle asynchronous protocol */
	if (CC->is_async == 0) return;

	/* And don't do it if there's nothing to send. */
	if (CC->FirstExpressMessage == NULL) return;

	cprintf("%d instant msg\n", ASYNC_MSG + ASYNC_GEXP);
}

/*
 * Back end support function for send_instant_message() and company
 */
void add_xmsg_to_context(struct CitContext *ccptr, struct ExpressMessage *newmsg) 
{
	struct ExpressMessage *findend;

	if (ccptr->FirstExpressMessage == NULL) {
		ccptr->FirstExpressMessage = newmsg;
	}
	else {
		findend = ccptr->FirstExpressMessage;
		while (findend->next != NULL) {
			findend = findend->next;
		}
		findend->next = newmsg;
	}

	/* If the target context is a session which can handle asynchronous
	 * messages, go ahead and set the flag for that.
	 */
	set_async_waiting(ccptr);
}




/* 
 * This is the back end to the instant message sending function.  
 * Returns the number of users to which the message was sent.
 * Sending a zero-length message tests for recipients without sending messages.
 */
int send_instant_message(char *lun, char *lem, char *x_user, char *x_msg)
{
	int message_sent = 0;		/* number of successful sends */
	struct CitContext *ccptr;
	struct ExpressMessage *newmsg = NULL;
	char *un;
	int do_send = 0;		/* 1 = send message; 0 = only check for valid recipient */
	static int serial_number = 0;	/* this keeps messages from getting logged twice */

	if (strlen(x_msg) > 0) {
		do_send = 1;
	}

	/* find the target user's context and append the message */
	begin_critical_section(S_SESSION_TABLE);
	++serial_number;
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {

		if (ccptr->fake_username[0]) {
			un = ccptr->fake_username;
		}
		else {
			un = ccptr->user.fullname;
		}

		if ( ((!strcasecmp(un, x_user))
		    || (!strcasecmp(x_user, "broadcast")))
		    && (ccptr->can_receive_im)
		    && ((ccptr->disable_exp == 0)
		    || (CC->user.axlevel >= AxAideU)) ) {
			if (do_send) {
				newmsg = (struct ExpressMessage *) malloc(sizeof (struct ExpressMessage));
				memset(newmsg, 0, sizeof (struct ExpressMessage));
				time(&(newmsg->timestamp));
				safestrncpy(newmsg->sender, lun, sizeof newmsg->sender);
				safestrncpy(newmsg->sender_email, lem, sizeof newmsg->sender_email);
				if (!strcasecmp(x_user, "broadcast")) {
					newmsg->flags |= EM_BROADCAST;
				}
				newmsg->text = strdup(x_msg);

				add_xmsg_to_context(ccptr, newmsg);

				/* and log it ... */
				if (ccptr != CC) {
					log_instant_message(CC, ccptr, newmsg->text, serial_number);
				}
			}
			++message_sent;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	return (message_sent);
}

/*
 * send instant messages
 */
void cmd_sexp(char *argbuf)
{
	int message_sent = 0;
	char x_user[USERNAME_SIZE];
	char x_msg[1024];
	char *lun;
	char *lem;
	char *x_big_msgbuf = NULL;

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}
	if (CC->fake_username[0])
		lun = CC->fake_username;
	else
		lun = CC->user.fullname;

	lem = CC->cs_inet_email;

	extract_token(x_user, argbuf, 0, '|', sizeof x_user);
	extract_token(x_msg, argbuf, 1, '|', sizeof x_msg);

	if (!x_user[0]) {
		cprintf("%d You were not previously paged.\n", ERROR + NO_SUCH_USER);
		return;
	}
	if ((!strcasecmp(x_user, "broadcast")) && (CC->user.axlevel < AxAideU)) {
		cprintf("%d Higher access required to send a broadcast.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	/* This loop handles text-transfer pages */
	if (!strcmp(x_msg, "-")) {
		message_sent = PerformXmsgHooks(lun, lem, x_user, "");
		if (message_sent == 0) {
			if (CtdlGetUser(NULL, x_user))
				cprintf("%d '%s' does not exist.\n",
						ERROR + NO_SUCH_USER, x_user);
			else
				cprintf("%d '%s' is not logged in "
						"or is not accepting pages.\n",
						ERROR + RESOURCE_NOT_OPEN, x_user);
			return;
		}
		unbuffer_output();
		cprintf("%d Transmit message (will deliver to %d users)\n",
			SEND_LISTING, message_sent);
		x_big_msgbuf = malloc(SIZ);
		memset(x_big_msgbuf, 0, SIZ);
		while (client_getln(x_msg, sizeof x_msg) >= 0 && strcmp(x_msg, "000")) {
			x_big_msgbuf = realloc(x_big_msgbuf,
			       strlen(x_big_msgbuf) + strlen(x_msg) + 4);
			if (!IsEmptyStr(x_big_msgbuf))
			   if (x_big_msgbuf[strlen(x_big_msgbuf)] != '\n')
				strcat(x_big_msgbuf, "\n");
			strcat(x_big_msgbuf, x_msg);
		}
		PerformXmsgHooks(lun, lem, x_user, x_big_msgbuf);
		free(x_big_msgbuf);

		/* This loop handles inline pages */
	} else {
		message_sent = PerformXmsgHooks(lun, lem, x_user, x_msg);

		if (message_sent > 0) {
			if (!IsEmptyStr(x_msg))
				cprintf("%d Message sent", CIT_OK);
			else
				cprintf("%d Ok to send message", CIT_OK);
			if (message_sent > 1)
				cprintf(" to %d users", message_sent);
			cprintf(".\n");
		} else {
			if (CtdlGetUser(NULL, x_user))
				cprintf("%d '%s' does not exist.\n",
						ERROR + NO_SUCH_USER, x_user);
			else
				cprintf("%d '%s' is not logged in "
						"or is not accepting pages.\n",
						ERROR + RESOURCE_NOT_OPEN, x_user);
		}


	}
}



/*
 * Enter or exit paging-disabled mode
 */
void cmd_dexp(char *argbuf)
{
	int new_state;

	if (CtdlAccessCheck(ac_logged_in)) return;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		CC->disable_exp = new_state;
	}

	cprintf("%d %d\n", CIT_OK, CC->disable_exp);
}


/*
 * Request client termination
 */
void cmd_reqt(char *argbuf) {
	struct CitContext *ccptr;
	int sessions = 0;
	int which_session;
	struct ExpressMessage *newmsg;

	if (CtdlAccessCheck(ac_aide)) return;
	which_session = extract_int(argbuf, 0);

	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if ((ccptr->cs_pid == which_session) || (which_session == 0)) {

			newmsg = (struct ExpressMessage *)
				malloc(sizeof (struct ExpressMessage));
			memset(newmsg, 0,
				sizeof (struct ExpressMessage));
			time(&(newmsg->timestamp));
			safestrncpy(newmsg->sender, CC->user.fullname,
				    sizeof newmsg->sender);
			newmsg->flags |= EM_GO_AWAY;
			newmsg->text = strdup("Automatic logoff requested.");

			add_xmsg_to_context(ccptr, newmsg);
			++sessions;

		}
	}
	end_critical_section(S_SESSION_TABLE);
	cprintf("%d Sent termination request to %d sessions.\n", CIT_OK, sessions);
}


/*
 * This is the back end for flush_conversations_to_disk()
 * At this point we've isolated a single conversation (struct imlog)
 * and are ready to write it to disk.
 */
void flush_individual_conversation(struct imlog *im) {
	struct CtdlMessage *msg;
	long msgnum = 0;
	char roomname[ROOMNAMELEN];

	StrBufAppendBufPlain(im->conversation, HKEY(
		"</body>\r\n"
		"</html>\r\n"
		), 0
	);

	msg = malloc(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = FMT_RFC822;
	if (!IsEmptyStr(im->usernames[0])) {
		CM_SetField(msg, eAuthor, im->usernames[0], strlen(im->usernames[0]));
	} else {
		CM_SetField(msg, eAuthor, HKEY("Citadel"));
	}
	if (!IsEmptyStr(im->usernames[1])) {
		CM_SetField(msg, eRecipient, im->usernames[1], strlen(im->usernames[1]));
	}

	CM_SetField(msg, eOriginalRoom, HKEY(PAGELOGROOM));
	CM_SetField(msg, eNodeName, NODENAME, strlen(NODENAME));
	CM_SetAsFieldSB(msg, eMesageText, &im->conversation);	/* we own this memory now */

	/* Start with usernums[1] because it's guaranteed to be higher than usernums[0],
	 * so if there's only one party, usernums[0] will be zero but usernums[1] won't.
	 * Create the room if necessary.  Note that we create as a type 5 room rather
	 * than 4, which indicates that it's a personal room but we've already supplied
	 * the namespace prefix.
	 *
	 * In the unlikely event that usernums[1] is zero, a room with an invalid namespace
	 * prefix will be created.  That's ok because the auto-purger will clean it up later.
	 */
	snprintf(roomname, sizeof roomname, "%010ld.%s", im->usernums[1], PAGELOGROOM);
	CtdlCreateRoom(roomname, 5, "", 0, 1, 1, VIEW_BBS);
	msgnum = CtdlSubmitMsg(msg, NULL, roomname, 0);
	CtdlFreeMessage(msg);

	/* If there is a valid user number in usernums[0], save a copy for them too. */
	if (im->usernums[0] > 0) {
		snprintf(roomname, sizeof roomname, "%010ld.%s", im->usernums[0], PAGELOGROOM);
		CtdlCreateRoom(roomname, 5, "", 0, 1, 1, VIEW_BBS);
		CtdlSaveMsgPointerInRoom(roomname, msgnum, 0, NULL);
	}

	/* Finally, if we're logging instant messages globally, do that now. */
	if (!IsEmptyStr(config.c_logpages)) {
		CtdlCreateRoom(config.c_logpages, 3, "", 0, 1, 1, VIEW_BBS);
		CtdlSaveMsgPointerInRoom(config.c_logpages, msgnum, 0, NULL);
	}

}

/*
 * Locate instant message conversations which have gone idle
 * (or, if the server is shutting down, locate *all* conversations)
 * and flush them to disk (in the participants' log rooms, etc.)
 */
void flush_conversations_to_disk(time_t if_older_than) {

	struct imlog *flush_these = NULL;
	struct imlog *dont_flush_these = NULL;
	struct imlog *imptr = NULL;
	struct CitContext *nptr;
	int nContexts, i;

	nptr = CtdlGetContextArray(&nContexts) ;	/* Make a copy of the current wholist */

	begin_critical_section(S_IM_LOGS);
	while (imlist)
	{
		imptr = imlist;
		imlist = imlist->next;

		/* For a two party conversation, if one party has logged out, force flush. */
		if (nptr) {
			int user0_is_still_online = 0;
			int user1_is_still_online = 0;
			for (i=0; i<nContexts; i++)  {
				if (nptr[i].user.usernum == imptr->usernums[0]) ++user0_is_still_online;
				if (nptr[i].user.usernum == imptr->usernums[1]) ++user1_is_still_online;
			}
			if (imptr->usernums[0] != imptr->usernums[1]) {		/* two party conversation */
				if ((!user0_is_still_online) || (!user1_is_still_online)) {
					imptr->lastmsg = 0L;	/* force flush */
				}
			}
			else {		/* one party conversation (yes, people do IM themselves) */
				if (!user0_is_still_online) {
					imptr->lastmsg = 0L;	/* force flush */
				}
			}
		}

		/* Now test this conversation to see if it qualifies for flushing. */
		if ((time(NULL) - imptr->lastmsg) > if_older_than)
		{
			/* This conversation qualifies.  Move it to the list of ones to flush. */
			imptr->next = flush_these;
			flush_these = imptr;
		}
		else  {
			/* Move it to the list of ones not to flush. */
			imptr->next = dont_flush_these;
			dont_flush_these = imptr;
		}
	}
	imlist = dont_flush_these;
	end_critical_section(S_IM_LOGS);
	free(nptr);

	/* We are now outside of the critical section, and we are the only thread holding a
	 * pointer to a linked list of conversations to be flushed to disk.
	 */
	while (flush_these) {

		flush_individual_conversation(flush_these);	/* This will free the string buffer */
		imptr = flush_these;
		flush_these = flush_these->next;
		free(imptr);
	}
}



void instmsg_timer(void) {
	flush_conversations_to_disk(300);	/* Anything that hasn't peeped in more than 5 minutes */
}

void instmsg_shutdown(void) {
	flush_conversations_to_disk(0);		/* Get it ALL onto disk NOW. */
}

CTDL_MODULE_INIT(instmsg)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_gexp, "GEXP", "Get instant messages");
		CtdlRegisterProtoHook(cmd_sexp, "SEXP", "Send an instant message");
		CtdlRegisterProtoHook(cmd_dexp, "DEXP", "Disable instant messages");
		CtdlRegisterProtoHook(cmd_reqt, "REQT", "Request client termination");
		CtdlRegisterSessionHook(cmd_gexp_async, EVT_ASYNC, PRIO_ASYNC + 1);
		CtdlRegisterSessionHook(delete_instant_messages, EVT_STOP, PRIO_STOP + 1);
		CtdlRegisterXmsgHook(send_instant_message, XMSG_PRI_LOCAL);
		CtdlRegisterSessionHook(instmsg_timer, EVT_TIMER, PRIO_CLEANUP + 400);
		CtdlRegisterSessionHook(instmsg_shutdown, EVT_SHUTDOWN, PRIO_SHUTDOWN + 10);
	}
	
	/* return our module name for the log */
	return "instmsg";
}
