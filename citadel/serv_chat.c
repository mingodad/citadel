/*
 * serv_chat.c
 * 
 * This module handles all "real time" communication between users.  The
 * modes of communication currently supported are Chat and Paging.
 *
 * $Id$
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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include "serv_chat.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "tools.h"
#include "msgbase.h"

struct ChatLine *ChatQueue = NULL;
int ChatLastMsg = 0;

extern struct CitContext *ContextList;

#define MODULE_NAME 	"Chat module"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	2
#define MINOR_VERSION	0

static struct DLModule_Info info =
{
	MODULE_NAME,
	MODULE_AUTHOR,
	MODULE_EMAIL,
	MAJOR_VERSION,
	MINOR_VERSION
};

struct DLModule_Info *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_chat, "CHAT", "Begin real-time chat");
	CtdlRegisterProtoHook(cmd_pexp, "PEXP", "Poll for express messages");
	CtdlRegisterProtoHook(cmd_gexp, "GEXP", "Get express messages");
	CtdlRegisterProtoHook(cmd_sexp, "SEXP", "Send an express message");
	CtdlRegisterSessionHook(delete_express_messages, EVT_STOP);
	return &info;
}

void allwrite(char *cmdbuf, int flag, char *roomname, char *username)
{
	FILE *fp;
	char bcast[256];
	char *un;
	struct ChatLine *clptr, *clnew;
	time_t now;

	if (CC->fake_username[0])
		un = CC->fake_username;
	else
		un = CC->usersupp.fullname;
	if (flag == 1) {
		sprintf(bcast, ":|<%s %s>", un, cmdbuf);
	} else if (flag == 0) {
		sprintf(bcast, "%s|%s", un, cmdbuf);
	} else if (flag == 2) {
		sprintf(bcast, ":|<%s whispers %s>", un, cmdbuf);
	}
	if ((strcasecmp(cmdbuf, "NOOP")) && (flag != 2)) {
		fp = fopen(CHATLOG, "a");
		fprintf(fp, "%s\n", bcast);
		fclose(fp);
	}
	clnew = (struct ChatLine *) mallok(sizeof(struct ChatLine));
	memset(clnew, 0, sizeof(struct ChatLine));
	if (clnew == NULL) {
		fprintf(stderr, "citserver: cannot alloc chat line: %s\n",
			strerror(errno));
		return;
	}
	time(&now);
	clnew->next = NULL;
	clnew->chat_time = now;
	strncpy(clnew->chat_room, roomname, sizeof clnew->chat_room);
	clnew->chat_room[sizeof clnew->chat_room - 1] = 0;
	if (username) {
		strncpy(clnew->chat_username, username,
			sizeof clnew->chat_username);
		clnew->chat_username[sizeof clnew->chat_username - 1] = 0;
	} else
		clnew->chat_username[0] = '\0';
	strcpy(clnew->chat_text, bcast);

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
	while (1) {
		if (ChatQueue == NULL)
			goto DONE_FREEING;
		if ((now - ChatQueue->chat_time) < 120L)
			goto DONE_FREEING;
		clptr = ChatQueue;
		ChatQueue = ChatQueue->next;
		phree(clptr);
	}
      DONE_FREEING:end_critical_section(S_CHATQUEUE);
}


t_context *find_context(char **unstr)
{
	t_context *t_cc, *found_cc = NULL;
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
 * List users in chat.  Setting allflag to 1 also lists users elsewhere.
 */

void do_chat_listing(int allflag)
{
	struct CitContext *ccptr;

	cprintf(":|\n:| Users currently in chat:\n");
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if ((!strcasecmp(ccptr->cs_room, "<chat>"))
		    && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
			cprintf(":| %-25s <%s>\n", (ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user, ccptr->chat_room);
		}
	}

	if (allflag == 1) {
		cprintf(":|\n:| Users not in chat:\n");
		for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
			if ((strcasecmp(ccptr->cs_room, "<chat>"))
			    && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
				cprintf(":| %-25s <%s>:\n", (ccptr->fake_username[0]) ? ccptr->fake_username : ccptr->curr_user, (ccptr->fake_roomname[0]) ? ccptr->fake_roomname : ccptr->cs_room);
			}
		}
	}
	end_critical_section(S_SESSION_TABLE);
	cprintf(":|\n");
}


void cmd_chat(char *argbuf)
{
	char cmdbuf[256];
	char *un;
	char *strptr1;
	char hold_cs_room[ROOMNAMELEN];
	int MyLastMsg, ThisLastMsg;
	struct ChatLine *clptr;
	struct CitContext *t_context;
	int retval;

	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}
	strcpy(CC->chat_room, "Main room");

	strcpy(hold_cs_room, CC->cs_room);
	CC->cs_flags = CC->cs_flags | CS_CHAT;
	set_wtmpsupp("<chat>");
	cprintf("%d Entering chat mode (type '/help' for available commands)\n",
		START_CHAT_MODE);

	MyLastMsg = ChatLastMsg;

	if ((CC->cs_flags & CS_STEALTH) == 0) {
		allwrite("<entering chat>", 0, CC->chat_room, NULL);
	}
	strcpy(cmdbuf, "");

	while (1) {
		int ok_cmd;

		ok_cmd = 0;
		cmdbuf[strlen(cmdbuf) + 1] = 0;
		retval = client_read_to(&cmdbuf[strlen(cmdbuf)], 1, 2);

		/* if we have a complete line, do send processing */
		if (strlen(cmdbuf) > 0)
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
						allwrite("<exiting chat>", 0, CC->chat_room, NULL);
					}
					sleep(1);
					cprintf("000\n");
					CC->cs_flags = CC->cs_flags - CS_CHAT;
					set_wtmpsupp(hold_cs_room);
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
					cprintf(":|/join   (join new room) \n");
					cprintf(":|/quit   (return to the BBS) \n");
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
					allwrite(&cmdbuf[4], 1, CC->chat_room, NULL);
					ok_cmd = 1;
				}
				if (!strncasecmp(cmdbuf, "/msg ", 5)) {
					ok_cmd = 1;
					strptr1 = &cmdbuf[5];
					if ((t_context = find_context(&strptr1))) {
						allwrite(strptr1, 2, "", CC->curr_user);
						if (strcasecmp(CC->curr_user, t_context->curr_user))
							allwrite(strptr1, 2, "", t_context->curr_user);
					} else
						cprintf(":|User not found.\n", cmdbuf);
					cprintf("\n");
				}
				if (!strncasecmp(cmdbuf, "/join ", 6)) {
					ok_cmd = 1;
					allwrite("<changing rooms>", 0, CC->chat_room, NULL);
					if (!cmdbuf[6])
						strcpy(CC->chat_room, "Main room");
					else {
						strncpy(CC->chat_room, &cmdbuf[6],
						   sizeof CC->chat_room);
						CC->chat_room[sizeof CC->chat_room - 1] = 0;
					}
					allwrite("<joining room>", 0, CC->chat_room, NULL);
					cprintf("\n");
				}
				if ((cmdbuf[0] != '/') && (strlen(cmdbuf) > 0)) {
					ok_cmd = 1;
					allwrite(cmdbuf, 0, CC->chat_room, NULL);
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
					if ((!clptr->chat_room[0]) || (!strncasecmp(CC->chat_room, clptr->chat_room, ROOMNAMELEN))) {
						cprintf("%s\n", clptr->chat_text);
					}
				}
			}
			MyLastMsg = ThisLastMsg;
		}
	}
}



/*
 * Delete any remaining express messages
 */
void delete_express_messages(void) {
	struct ExpressMessage *ptr;

	begin_critical_section(S_SESSION_TABLE);
	while (CC->FirstExpressMessage != NULL) {
		ptr = CC->FirstExpressMessage->next;
		if (CC->FirstExpressMessage->text != NULL)
			phree(CC->FirstExpressMessage->text);
		phree(CC->FirstExpressMessage);
		CC->FirstExpressMessage = ptr;
		}
	end_critical_section(S_SESSION_TABLE);
	}




/*
 * Poll for express messages (OLD METHOD -- ***DEPRECATED ***)
 */
void cmd_pexp(char *argbuf)
{
	struct ExpressMessage *ptr, *holdptr;

	if (CC->FirstExpressMessage == NULL) {
		cprintf("%d No express messages waiting.\n", ERROR);
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
			cprintf("%s\n\n", ptr->text);

		holdptr = ptr->next;
		if (ptr->text != NULL) phree(ptr->text);
		phree(ptr);
		ptr = holdptr;
	}
	cprintf("000\n");
}


/*
 * Get express messages (new method)
 */
void cmd_gexp(char *argbuf) {
	struct ExpressMessage *ptr;

	if (CC->FirstExpressMessage == NULL) {
		cprintf("%d No express messages waiting.\n", ERROR);
		return;
	}

	begin_critical_section(S_SESSION_TABLE);
	ptr = CC->FirstExpressMessage;
	CC->FirstExpressMessage = CC->FirstExpressMessage->next;
	end_critical_section(S_SESSION_TABLE);

	cprintf("%d %d|%ld|%d|%s|%s\n",
		LISTING_FOLLOWS,
		((ptr->next != NULL) ? 1 : 0),		/* more msgs? */
		ptr->timestamp,				/* time sent */
		ptr->flags,				/* flags */
		ptr->sender,				/* sender of msg */
		config.c_nodename);			/* static for now */
	if (ptr->text != NULL) {
		cprintf("%s", ptr->text);
		if (ptr->text[strlen(ptr->text)-1] != '\n') cprintf("\n");
		phree(ptr->text);
		}
	cprintf("000\n");
	phree(ptr);
}



/* 
 * This is the back end to the express message sending function.  
 * Returns the number of users to which the message was sent.
 * Sending a zero-length message tests for recipients without sending messages.
 */
int send_express_message(char *lun, char *x_user, char *x_msg)
{
	int message_sent = 0;
	struct CitContext *ccptr;
	struct ExpressMessage *newmsg, *findend;
	char *un;
	FILE *fp;

	/* find the target user's context and append the message */
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {

		if (ccptr->fake_username[0])	/* <bc> */
			un = ccptr->fake_username;
		else
			un = ccptr->usersupp.fullname;

		if ((!strcasecmp(un, x_user))
		    || (!strcasecmp(x_user, "broadcast"))) {
			if (strlen(x_msg) > 0) {
				newmsg = (struct ExpressMessage *)
					mallok(sizeof (struct ExpressMessage));
				memset(newmsg, 0,
					sizeof (struct ExpressMessage));
				strcpy(newmsg->sender, un);
				if (!strcasecmp(x_user, "broadcast"))
					newmsg->flags |= EM_BROADCAST;
				newmsg->text = mallok(strlen(x_msg)+2);
				strcpy(newmsg->text, x_msg);

				if (ccptr->FirstExpressMessage == NULL)
					ccptr->FirstExpressMessage = newmsg;
				else {
					findend = ccptr->FirstExpressMessage;
					while (findend->next != NULL)
						findend = findend->next;
					findend->next = newmsg;
				}
			}
			++message_sent;
		}
	}
	end_critical_section(S_SESSION_TABLE);

	/* Log the page to disk if configured to do so */
	if ((strlen(config.c_logpages) > 0) && (strlen(x_msg) > 0)) {
		fp = fopen(CC->temp, "wb");
		fprintf(fp, "%c%c%c", 255, MES_NORMAL, 0);
		fprintf(fp, "Psysop%c", 0);
		fprintf(fp, "T%ld%c", time(NULL), 0);
		fprintf(fp, "A%s%c", lun, 0);
		fprintf(fp, "R%s%c", x_user, 0);
		fprintf(fp, "O%s%c", config.c_logpages, 0);
		fprintf(fp, "N%s%c", NODENAME, 0);
		fprintf(fp, "M%s\n%c", x_msg, 0);
		fclose(fp);
		save_message(CC->temp, "", config.c_logpages, M_LOCAL, 1);
		unlink(CC->temp);
	}
	return (message_sent);
}

/*
 * send express messages  <bc>
 */
void cmd_sexp(char *argbuf)
{
	int message_sent = 0;
	char x_user[256];
	char x_msg[256];
	char *lun;		/* <bc> */
	char *x_big_msgbuf = NULL;

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		return;
	}
	if (CC->fake_username[0])
		lun = CC->fake_username;
	else
		lun = CC->usersupp.fullname;

	extract(x_user, argbuf, 0);

	extract(x_msg, argbuf, 1);

	if (!x_user[0]) {
		cprintf("%d You were not previously paged.\n", ERROR);
		return;
	}
	if ((!strcasecmp(x_user, "broadcast")) && (CC->usersupp.axlevel < 6)) {
		cprintf("%d Higher access required to send a broadcast.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	/* This loop handles text-transfer pages */
	if (!strcmp(x_msg, "-")) {
		message_sent = send_express_message(lun, x_user, "");
		if (message_sent == 0) {
			cprintf("%d No user '%s' logged in.\n", ERROR, x_user);
			return;
		}
		cprintf("%d Transmit message (will deliver to %d users)\n",
			SEND_LISTING, message_sent);
		x_big_msgbuf = mallok(256);
		memset(x_big_msgbuf, 0, 256);
		while (client_gets(x_msg), strcmp(x_msg, "000")) {
			x_big_msgbuf = reallok(x_big_msgbuf,
			       strlen(x_big_msgbuf) + strlen(x_msg) + 4);
			if (strlen(x_big_msgbuf) > 0)
			   if (x_big_msgbuf[strlen(x_big_msgbuf)] != '\n')
				strcat(x_big_msgbuf, "\n");
			strcat(x_big_msgbuf, x_msg);
		}
		send_express_message(lun, x_user, x_big_msgbuf);
		phree(x_big_msgbuf);

		/* This loop handles inline pages */
	} else {
		message_sent = send_express_message(lun, x_user, x_msg);

		if (message_sent > 0) {
			if (strlen(x_msg) > 0)
				cprintf("%d Message sent", OK);
			else
				cprintf("%d Ok to send message", OK);
			if (message_sent > 1)
				cprintf(" to %d users", message_sent);
			cprintf(".\n");
		} else {
			cprintf("%d No user '%s' logged in.\n", ERROR, x_user);
		}


	}
}
