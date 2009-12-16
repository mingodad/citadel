/*
 * $Id$
 * 
 * This module handles all "real time" communication between users.  The
 * modes of communication currently supported are Chat and Paging.
 *
 * Copyright (c) 1987-2009 by the citadel.org team
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
#include "serv_chat.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "user_ops.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#include "ctdl_module.h"

struct ChatLine *ChatQueue = NULL;
int ChatLastMsg = 0;

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
 * FIXME: OMG this module is realy horrible to the rest of the system when accessing contexts.
 * It pays no regard at all to how long it may have the context list locked for. 
 * It carries out IO whilst the context list is locked.
 * I'd recomend disabling this module altogether for the moment.
 */

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
 * This message can be set to anything you want, but it is
 * checked for consistency so don't move it away from here.
 */
#define KICKEDMSG "You have been kicked out of this room."

void allwrite(char *cmdbuf, int flag, char *username)
{
	FILE *fp;
	char bcast[SIZ];
	char *un;
	struct ChatLine *clptr, *clnew;
	time_t now;

	if (CC->fake_username[0])
		un = CC->fake_username;
	else
		un = CC->user.fullname;
	if (flag == 1) {
		snprintf(bcast, sizeof bcast, ":|<%s %s>", un, cmdbuf);
	} else if (flag == 0) {
		snprintf(bcast, sizeof bcast, "%s|%s", un, cmdbuf);
	} else if (flag == 2) {
		snprintf(bcast, sizeof bcast, ":|<%s whispers %s>", un, cmdbuf);
	} else if (flag == 3) {
		snprintf(bcast, sizeof bcast, ":|%s", KICKEDMSG);
	}
	if ((strcasecmp(cmdbuf, "NOOP")) && (flag != 2)) {
		fp = fopen(CHATLOG, "a");
		if (fp != NULL)
			fprintf(fp, "%s\n", bcast);
		fclose(fp);
	}
	clnew = (struct ChatLine *) malloc(sizeof(struct ChatLine));
	memset(clnew, 0, sizeof(struct ChatLine));
	if (clnew == NULL) {
		fprintf(stderr, "citserver: cannot alloc chat line: %s\n",
			strerror(errno));
		return;
	}
	time(&now);
	clnew->next = NULL;
	clnew->chat_time = now;
	safestrncpy(clnew->chat_room, CC->room.QRname,
			sizeof clnew->chat_room);
	clnew->chat_room[sizeof clnew->chat_room - 1] = 0;
	if (username) {
		safestrncpy(clnew->chat_username, username,
			sizeof clnew->chat_username);
		clnew->chat_username[sizeof clnew->chat_username - 1] = 0;
	} else
		clnew->chat_username[0] = '\0';
	safestrncpy(clnew->chat_text, bcast, sizeof clnew->chat_text);

	/* Here's the critical section.
	 * First, add the new message to the queue...
	 */
	begin_critical_section(S_CHATQUEUE);
	++ChatLastMsg;
	clnew->chat_seq = ChatLastMsg;
	if (ChatQueue == NULL) {
		ChatQueue = clnew;
	} else {
		for (clptr = ChatQueue; clptr->next != NULL; clptr = clptr->next);;
		clptr->next = clnew;
	}

	/* Then, before releasing the lock, free the expired messages */
	while ((ChatQueue != NULL) && (now - ChatQueue->chat_time >= 120L)) {
		clptr = ChatQueue;
		ChatQueue = ChatQueue->next;
		free(clptr);
	}
	end_critical_section(S_CHATQUEUE);
}


CitContext *find_context(char **unstr)
{
	CitContext *t_cc, *found_cc = NULL;
	char *name, *tptr;

	if ((!*unstr) || (!unstr))
		return (NULL);

	begin_critical_section(S_SESSION_TABLE);
	for (t_cc = ContextList; ((t_cc) && (!found_cc)); t_cc = t_cc->next) {
		if (t_cc->fake_username[0])
			name = t_cc->fake_username;
		else
			name = t_cc->curr_user;
		tptr = *unstr;
		if ((!strncasecmp(name, tptr, strlen(name))) && (tptr[strlen(name)] == ' ')) {
			found_cc = t_cc;
			*unstr = &(tptr[strlen(name) + 1]);
		}
	}
	end_critical_section(S_SESSION_TABLE);

	return (found_cc);
}

/*
 * List users in chat.
 * allflag ==	0 = list users in chat
 *		1 = list users in chat, followed by users not in chat
 *		2 = display count only
 */

void do_chat_listing(int allflag)
{
	struct CitContext *ccptr;
	int count = 0;
	int count_elsewhere = 0;
	char roomname[ROOMNAMELEN];

	if ((allflag == 0) || (allflag == 1))
		cprintf(":|\n:| Users currently in chat:\n");
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (ccptr->cs_flags & CS_CHAT) {
			if (!strcasecmp(ccptr->room.QRname,
			   CC->room.QRname)) {
				++count;
			}
			else {
				++count_elsewhere;
			}
		}

		GenerateRoomDisplay(roomname, ccptr, CC);
		if ((CC->user.axlevel < 6) && (!IsEmptyStr(ccptr->fake_roomname))) {
			strcpy(roomname, ccptr->fake_roomname);
		}

		if ((ccptr->cs_flags & CS_CHAT) && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
			if ((allflag == 0) || (allflag == 1)) {
				cprintf(":| %-25s <%s>:\n",
					(ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user,
					roomname);
			}
		}
	}

	if (allflag == 1) {
		cprintf(":|\n:| Users not in chat:\n");
		for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {

			GenerateRoomDisplay(roomname, ccptr, CC);
			if ((CC->user.axlevel < 6)
		   	&& (!IsEmptyStr(ccptr->fake_roomname))) {
				strcpy(roomname, ccptr->fake_roomname);
			}

			if (((ccptr->cs_flags & CS_CHAT) == 0)
			    && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
				cprintf(":| %-25s <%s>:\n",
					(ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user,
					roomname);
			}
		}
	}
	end_critical_section(S_SESSION_TABLE);

	if (allflag == 2) {
		if (count > 1) {
			cprintf(":|There are %d users here.\n", count);
		}
		else {
			cprintf(":|Note: you are the only one here.\n");
		}
		if (count_elsewhere > 0) {
			cprintf(":|There are %d users chatting in other rooms.\n", count_elsewhere);
		}
	}

	cprintf(":|\n");
}


void cmd_chat(char *argbuf)
{
	char cmdbuf[SIZ];
	char *un;
	char *strptr1;
	int MyLastMsg, ThisLastMsg;
	struct ChatLine *clptr;
	struct CitContext *t_context;
	int retval;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}

	CC->cs_flags = CC->cs_flags | CS_CHAT;
	cprintf("%d Entering chat mode (type '/help' for available commands)\n",
		START_CHAT_MODE);
	unbuffer_output();

	MyLastMsg = ChatLastMsg;

	if ((CC->cs_flags & CS_STEALTH) == 0) {
		allwrite("<entering chat>", 0, NULL);
	}
	strcpy(cmdbuf, "");

	do_chat_listing(2);

	while (1) {
		int ok_cmd;
		int linelen;

		ok_cmd = 0;
		linelen = strlen(cmdbuf);
		if (linelen > 100) --linelen;	/* truncate too-long lines */
		cmdbuf[linelen + 1] = 0;

		retval = client_read_to(&cmdbuf[linelen], 1, 2);

		if (retval < 0 || CC->kill_me) {	/* socket broken? */
			if ((CC->cs_flags & CS_STEALTH) == 0) {
				allwrite("<disconnected>", 0, NULL);
			}
			return;
		}

		/* if we have a complete line, do send processing */
		if (!IsEmptyStr(cmdbuf))
			if (cmdbuf[strlen(cmdbuf) - 1] == 10) {
				cmdbuf[strlen(cmdbuf) - 1] = 0;
				time(&CC->lastcmd);
				time(&CC->lastidle);

				if ((!strcasecmp(cmdbuf, "exit"))
				    || (!strcasecmp(cmdbuf, "/exit"))
				    || (!strcasecmp(cmdbuf, "quit"))
				    || (!strcasecmp(cmdbuf, "logout"))
				    || (!strcasecmp(cmdbuf, "logoff"))
				    || (!strcasecmp(cmdbuf, "/q"))
				    || (!strcasecmp(cmdbuf, ".q"))
				    || (!strcasecmp(cmdbuf, "/quit"))
				    )
					strcpy(cmdbuf, "000");

				if (!strcmp(cmdbuf, "000")) {
					if ((CC->cs_flags & CS_STEALTH) == 0) {
						allwrite("<exiting chat>", 0, NULL);
					}
					sleep(1);
					cprintf("000\n");
					CC->cs_flags = CC->cs_flags - CS_CHAT;
					return;
				}
				if ((!strcasecmp(cmdbuf, "/help"))
				    || (!strcasecmp(cmdbuf, "help"))
				    || (!strcasecmp(cmdbuf, "/?"))
				    || (!strcasecmp(cmdbuf, "?"))) {
					cprintf(":|\n");
					cprintf(":|Available commands: \n");
					cprintf(":|/help   (prints this message) \n");
					cprintf(":|/who    (list users currently in chat) \n");
					cprintf(":|/whobbs (list users in chat -and- elsewhere) \n");
					cprintf(":|/me     ('action' line, ala irc) \n");
					cprintf(":|/msg    (send private message, ala irc) \n");
					if (is_room_aide()) {
						cprintf(":|/kick   (kick another user out of this room) \n");
					}
					cprintf(":|/quit   (exit from this chat) \n");
					cprintf(":|\n");
					ok_cmd = 1;
				}
				if (!strcasecmp(cmdbuf, "/who")) {
					do_chat_listing(0);
					ok_cmd = 1;
				}
				if (!strcasecmp(cmdbuf, "/whobbs")) {
					do_chat_listing(1);
					ok_cmd = 1;
				}
				if (!strncasecmp(cmdbuf, "/me ", 4)) {
					allwrite(&cmdbuf[4], 1, NULL);
					ok_cmd = 1;
				}
				if (!strncasecmp(cmdbuf, "/msg ", 5)) {
					ok_cmd = 1;
					strptr1 = &cmdbuf[5];
					if ((t_context = find_context(&strptr1))) {
						allwrite(strptr1, 2, CC->curr_user);
						if (strcasecmp(CC->curr_user, t_context->curr_user))
							allwrite(strptr1, 2, t_context->curr_user);
					} else
						cprintf(":|User not found.\n");
					cprintf("\n");
				}
				/* The /kick function is implemented by sending a specific
				 * message to the kicked-out user's context.  When that message
				 * is processed by the read loop, that context will exit.
				 */
				if ( (!strncasecmp(cmdbuf, "/kick ", 6)) && (is_room_aide()) ) {
					ok_cmd = 1;
					strptr1 = &cmdbuf[6];
					strcat(strptr1, " ");
					if ((t_context = find_context(&strptr1))) {
						if (strcasecmp(CC->curr_user, t_context->curr_user))
							allwrite(strptr1, 3, t_context->curr_user);
					} else
						cprintf(":|User not found.\n");
					cprintf("\n");
				}
				if ((cmdbuf[0] != '/') && (strlen(cmdbuf) > 0)) {
					ok_cmd = 1;
					allwrite(cmdbuf, 0, NULL);
				}
				if ((!ok_cmd) && (cmdbuf[0]) && (cmdbuf[0] != '\n'))
					cprintf(":|Command %s is not understood.\n", cmdbuf);

				strcpy(cmdbuf, "");

			}
		/* now check the queue for new incoming stuff */

		if (CC->fake_username[0])
			un = CC->fake_username;
		else
			un = CC->curr_user;
		if (ChatLastMsg > MyLastMsg) {
			ThisLastMsg = ChatLastMsg;
			for (clptr = ChatQueue; clptr != NULL; clptr = clptr->next) {
				if ((clptr->chat_seq > MyLastMsg) && ((!clptr->chat_username[0]) || (!strncasecmp(un, clptr->chat_username, 32)))) {
					if ((!clptr->chat_room[0]) || (!strncasecmp(CC->room.QRname, clptr->chat_room, ROOMNAMELEN))) {
						/* Output new chat data */
						cprintf("%s\n", clptr->chat_text);

						/* See if we've been force-quitted (kicked etc.) */
						if (!strcmp(&clptr->chat_text[2], KICKEDMSG)) {
							allwrite("<kicked out of this room>", 0, NULL);
							cprintf("000\n");
							CC->cs_flags = CC->cs_flags - CS_CHAT;

							/* Kick user out of room */
							CtdlInvtKick(CC->user.fullname, 0);

							/* And return to the Lobby */
							CtdlUserGoto(config.c_baseroom, 0, 0, NULL, NULL);
							return;
						}
					}
				}
			}
			MyLastMsg = ThisLastMsg;
		}
	}
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
 * Poll for instant messages (OLD METHOD -- ***DEPRECATED ***)
 */
void cmd_pexp(char *argbuf)
{
	struct ExpressMessage *ptr, *holdptr;

	if (CC->FirstExpressMessage == NULL) {
		cprintf("%d No instant messages waiting.\n", ERROR + MESSAGE_NOT_FOUND);
		return;
	}
	begin_critical_section(S_SESSION_TABLE);
	ptr = CC->FirstExpressMessage;
	CC->FirstExpressMessage = NULL;
	end_critical_section(S_SESSION_TABLE);

	cprintf("%d Express msgs:\n", LISTING_FOLLOWS);
	while (ptr != NULL) {
		if (ptr->flags && EM_BROADCAST)
			cprintf("Broadcast message ");
		else if (ptr->flags && EM_CHAT)
			cprintf("Chat request ");
		else if (ptr->flags && EM_GO_AWAY)
			cprintf("Please logoff now, as requested ");
		else
			cprintf("Message ");
		cprintf("from %s:\n", ptr->sender);
		if (ptr->text != NULL)
			memfmout(ptr->text, 0, "\n");

		holdptr = ptr->next;
		if (ptr->text != NULL) free(ptr->text);
		free(ptr);
		ptr = holdptr;
	}
	cprintf("000\n");
}


/*
 * Get instant messages (new method)
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
		memfmout(ptr->text, 0, "\n");
		if (ptr->text[strlen(ptr->text)-1] != '\n') cprintf("\n");
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
	if (ccptr->is_async) {
		ccptr->async_waiting = 1;
		if (ccptr->state == CON_IDLE) {
			ccptr->state = CON_READY;
		}
	}
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
	size_t msglen = 0;
	int do_send = 0;		/* 1 = send message; 0 = only check for valid recipient */
	static int serial_number = 0;	/* this keeps messages from getting logged twice */

	if (strlen(x_msg) > 0) {
		msglen = strlen(x_msg) + 4;
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
		    || (CC->user.axlevel >= 6)) ) {
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
	if ((!strcasecmp(x_user, "broadcast")) && (CC->user.axlevel < 6)) {
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
		msg->cm_fields['A'] = strdup(im->usernames[0]);
	} else {
		msg->cm_fields['A'] = strdup("Citadel");
	}
	if (!IsEmptyStr(im->usernames[1])) {
		msg->cm_fields['R'] = strdup(im->usernames[1]);
	}
	msg->cm_fields['O'] = strdup(PAGELOGROOM);
	msg->cm_fields['N'] = strdup(NODENAME);
	msg->cm_fields['M'] = SmashStrBuf(&im->conversation);	/* we own this memory now */

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

	begin_critical_section(S_IM_LOGS);
	while (imlist)
	{
		imptr = imlist;
		imlist = imlist->next;
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



void chat_timer(void) {
	flush_conversations_to_disk(300);	/* Anything that hasn't peeped in more than 5 minutes */
}

void chat_shutdown(void) {
	flush_conversations_to_disk(0);		/* Get it ALL onto disk NOW. */
}

CTDL_MODULE_INIT(chat)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_chat, "CHAT", "Begin real-time chat");
		CtdlRegisterProtoHook(cmd_pexp, "PEXP", "Poll for instant messages");
		CtdlRegisterProtoHook(cmd_gexp, "GEXP", "Get instant messages");
		CtdlRegisterProtoHook(cmd_sexp, "SEXP", "Send an instant message");
		CtdlRegisterProtoHook(cmd_dexp, "DEXP", "Disable instant messages");
		CtdlRegisterProtoHook(cmd_reqt, "REQT", "Request client termination");
		CtdlRegisterSessionHook(cmd_gexp_async, EVT_ASYNC);
		CtdlRegisterSessionHook(delete_instant_messages, EVT_STOP);
		CtdlRegisterXmsgHook(send_instant_message, XMSG_PRI_LOCAL);
		CtdlRegisterSessionHook(chat_timer, EVT_TIMER);
		CtdlRegisterSessionHook(chat_shutdown, EVT_SHUTDOWN);
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
