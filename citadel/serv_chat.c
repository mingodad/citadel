/*
 * $Id$
 * 
 * This module handles all "real time" communication between users.  The
 * modes of communication currently supported are Chat and Paging.
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
#include <sys/time.h>
#ifdef TIME_WITH_SYS_TIME
#include <time.h>
#endif
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include "serv_chat.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "tools.h"
#include "msgbase.h"
#include "user_ops.h"
#include "room_ops.h"

struct ChatLine *ChatQueue = NULL;
int ChatLastMsg = 0;

extern struct CitContext *ContextList;



void allwrite(char *cmdbuf, int flag, char *username)
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
		snprintf(bcast, sizeof bcast, ":|<%s %s>", un, cmdbuf);
	} else if (flag == 0) {
		snprintf(bcast, sizeof bcast, "%s|%s", un, cmdbuf);
	} else if (flag == 2) {
		snprintf(bcast, sizeof bcast, ":|<%s whispers %s>", un, cmdbuf);
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
	safestrncpy(clnew->chat_room, CC->quickroom.QRname,
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
		phree(clptr);
	}
	end_critical_section(S_CHATQUEUE);
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
			if (!strcasecmp(ccptr->quickroom.QRname,
			   CC->quickroom.QRname)) {
				++count;
			}
			else {
				++count_elsewhere;
			}
		}

		GenerateRoomDisplay(roomname, ccptr, CC);
		if ((CC->usersupp.axlevel < 6)
		   && (strlen(ccptr->fake_roomname)>0)) {
			strcpy(roomname, ccptr->fake_roomname);
		}

		if ((ccptr->cs_flags & CS_CHAT)
		    && ((ccptr->cs_flags & CS_STEALTH) == 0)) {
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
			if ((CC->usersupp.axlevel < 6)
		   	&& (strlen(ccptr->fake_roomname)>0)) {
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
	char cmdbuf[256];
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

		if (retval < 0) {	/* socket broken? */
			if ((CC->cs_flags & CS_STEALTH) == 0) {
				allwrite("<disconnected>", 0, NULL);
			}
			return;
		}

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
						cprintf(":|User not found.\n", cmdbuf);
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
					if ((!clptr->chat_room[0]) || (!strncasecmp(CC->quickroom.QRname, clptr->chat_room, ROOMNAMELEN))) {
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
			memfmout(80, ptr->text, 0, "\n");

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
		memfmout(80, ptr->text, 0, "\n");
		if (ptr->text[strlen(ptr->text)-1] != '\n') cprintf("\n");
		phree(ptr->text);
		}
	cprintf("000\n");
	phree(ptr);
}


/*
 * Back end support function for send_express_message() and company
 */
void add_xmsg_to_context(struct CitContext *ccptr, 
			struct ExpressMessage *newmsg) 
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
}




/* 
 * This is the back end to the express message sending function.  
 * Returns the number of users to which the message was sent.
 * Sending a zero-length message tests for recipients without sending messages.
 */
int send_express_message(char *lun, char *x_user, char *x_msg)
{
	int message_sent = 0;		/* number of successful sends */

	struct CitContext *ccptr;
	struct ExpressMessage *newmsg;
	char *un;
	size_t msglen = 0;
	int do_send = 0;		/* set to 1 to actually page, not
					 * just check to see if we can.
					 */
	struct savelist *sl = NULL;	/* list of rooms to save this page */
	struct savelist *sptr;
	struct CtdlMessage *logmsg;
	char roomname[ROOMNAMELEN];
	long msgnum;

	if (strlen(x_msg) > 0) {
		msglen = strlen(x_msg) + 4;
		do_send = 1;
		}

	/* find the target user's context and append the message */
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {

		if (ccptr->fake_username[0])	/* <bc> */
			un = ccptr->fake_username;
		else
			un = ccptr->usersupp.fullname;

		if ( ((!strcasecmp(un, x_user))
		    || (!strcasecmp(x_user, "broadcast")))
		    && ((ccptr->disable_exp == 0)
		    || (CC->usersupp.axlevel >= 6)) ) {
			if (do_send) {
				newmsg = (struct ExpressMessage *)
					mallok(sizeof (struct ExpressMessage));
				memset(newmsg, 0,
					sizeof (struct ExpressMessage));
				time(&(newmsg->timestamp));
				safestrncpy(newmsg->sender, lun,
					    sizeof newmsg->sender);
				if (!strcasecmp(x_user, "broadcast"))
					newmsg->flags |= EM_BROADCAST;
				newmsg->text = strdoop(x_msg);

				add_xmsg_to_context(ccptr, newmsg);

				/* and log it ... */
				if (ccptr != CC) {
					sptr = (struct savelist *)
						malloc(sizeof(struct savelist));
					sptr->next = sl;
					MailboxName(sptr->roomname,
						&ccptr->usersupp, PAGELOGROOM);
					sl = sptr;
				}
			}
			++message_sent;
		}
	}
	end_critical_section(S_SESSION_TABLE);

	/* Log the page to disk if configured to do so  */
	if ( (do_send) && (message_sent) ) {

		logmsg = mallok(sizeof(struct CtdlMessage));
		memset(logmsg, 0, sizeof(struct CtdlMessage));
		logmsg->cm_magic = CTDLMESSAGE_MAGIC;
		logmsg->cm_anon_type = MES_NORMAL;
		logmsg->cm_format_type = 0;
		logmsg->cm_fields['A'] = strdoop(lun);
		logmsg->cm_fields['N'] = strdoop(NODENAME);
		logmsg->cm_fields['O'] = strdoop(PAGELOGROOM);
		logmsg->cm_fields['R'] = strdoop(x_user);
		logmsg->cm_fields['M'] = strdoop(x_msg);

		MailboxName(roomname, &CC->usersupp, PAGELOGROOM);
		create_room(roomname, 4, "", 0);
		msgnum = CtdlSaveMsg(logmsg, "", roomname, MES_LOCAL);
		if (strlen(config.c_logpages) > 0) {
			create_room(config.c_logpages, 3, "", 0);
			CtdlSaveMsgPointerInRoom(config.c_logpages, msgnum, 0);
		}
		while (sl != NULL) {
			create_room(sl->roomname, 4, "", 0);
			CtdlSaveMsgPointerInRoom(sl->roomname, msgnum, 0);
			sptr = sl->next;
			phree(sl);
			sl = sptr;
		}

		CtdlFreeMessage(logmsg);
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
		message_sent = PerformXmsgHooks(lun, x_user, "");
		if (message_sent == 0) {
			cprintf("%d '%s' is not logged in "
				"or is not accepting pages.\n",
				ERROR, x_user);
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
		PerformXmsgHooks(lun, x_user, x_big_msgbuf);
		phree(x_big_msgbuf);

		/* This loop handles inline pages */
	} else {
		message_sent = PerformXmsgHooks(lun, x_user, x_msg);

		if (message_sent > 0) {
			if (strlen(x_msg) > 0)
				cprintf("%d Message sent", OK);
			else
				cprintf("%d Ok to send message", OK);
			if (message_sent > 1)
				cprintf(" to %d users", message_sent);
			cprintf(".\n");
		} else {
			cprintf("%d '%s' is not logged in "
				"or is not accepting pages.\n",
				ERROR, x_user);
		}


	}
}



/*
 * Enter or exit paging-disabled mode
 */
void cmd_dexp(char *argbuf)
{
	int new_state;

	if (!CC->logged_in) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		CC->disable_exp = new_state;
		}
	cprintf("%d %d\n", OK, CC->disable_exp);
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
				mallok(sizeof (struct ExpressMessage));
			memset(newmsg, 0,
				sizeof (struct ExpressMessage));
			time(&(newmsg->timestamp));
			safestrncpy(newmsg->sender, CC->usersupp.fullname,
				    sizeof newmsg->sender);
			newmsg->flags |= EM_GO_AWAY;
			newmsg->text = strdoop("Automatic logoff requested.");

			add_xmsg_to_context(ccptr, newmsg);
			++sessions;

		}
	}
	end_critical_section(S_SESSION_TABLE);
	cprintf("%d Sent termination request to %d sessions.\n", OK, sessions);
}



char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_chat, "CHAT", "Begin real-time chat");
	CtdlRegisterProtoHook(cmd_pexp, "PEXP", "Poll for express messages");
	CtdlRegisterProtoHook(cmd_gexp, "GEXP", "Get express messages");
	CtdlRegisterProtoHook(cmd_sexp, "SEXP", "Send an express message");
	CtdlRegisterProtoHook(cmd_dexp, "DEXP", "Disable express messages");
	CtdlRegisterProtoHook(cmd_reqt, "REQT", "Request client termination");
	CtdlRegisterSessionHook(delete_express_messages, EVT_STOP);
	CtdlRegisterXmsgHook(send_express_message, XMSG_PRI_LOCAL);
	return "$Id$";
}

