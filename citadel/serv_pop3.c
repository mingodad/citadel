/* $Id$ 
 *
 * An implementation of Post Office Protocol version 3 (RFC 1939).
 *
 * Copyright (C) 1998-2000 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
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
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_pop3.h"


long SYM_POP3;


/*
 * This cleanup function blows away the temporary memory and files used by
 * the POP3 server.
 */
void pop3_cleanup_function(void) {
	int i;

	/* Don't do this stuff if this is not a POP3 session! */
	if (CC->h_command_function != pop3_command_loop) return;

	lprintf(9, "Performing POP3 cleanup hook\n");

	if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
		fclose(POP3->msgs[i].temp);
	}
	if (POP3->msgs != NULL) phree(POP3->msgs);

	lprintf(9, "Finished POP3 cleanup hook\n");
}



/*
 * Here's where our POP3 session begins its happy day.
 */
void pop3_greeting(void) {

	strcpy(CC->cs_clientname, "POP3 session");
	CC->internal_pgm = 1;
	CtdlAllocUserData(SYM_POP3, sizeof(struct citpop3));
	POP3->msgs = NULL;
	POP3->num_msgs = 0;

	cprintf("+OK Welcome to the Citadel/UX POP3 server at %s\r\n",
		config.c_fqdn);
}


/*
 * Specify user name (implements POP3 "USER" command)
 */
void pop3_user(char *argbuf) {
	char username[256];

	if (CC->logged_in) {
		cprintf("-ERR You are already logged in.\r\n");
		return;
	}

	strcpy(username, argbuf);
	striplt(username);

	lprintf(9, "Trying <%s>\n", username);
	if (CtdlLoginExistingUser(username) == login_ok) {
		cprintf("+OK Password required for %s\r\n", username);
	}
	else {
		cprintf("-ERR No such user.\r\n");
	}
}



/*
 * Back end for pop3_grab_mailbox()
 */
void pop3_add_message(long msgnum) {
	FILE *fp;
	lprintf(9, "in pop3_add_message()\n");

	++POP3->num_msgs;
	if (POP3->num_msgs < 2) POP3->msgs = mallok(sizeof(struct pop3msg));
	else POP3->msgs = reallok(POP3->msgs, 
		(POP3->num_msgs * sizeof(struct pop3msg)) ) ;
	POP3->msgs[POP3->num_msgs-1].msgnum = msgnum;
	POP3->msgs[POP3->num_msgs-1].deleted = 0;
	fp = tmpfile();
	POP3->msgs[POP3->num_msgs-1].temp = fp;
	CtdlOutputMsg(msgnum, MT_RFC822, 0, 0, fp, 0);
	POP3->msgs[POP3->num_msgs-1].rfc822_length = ftell(fp);
}



/*
 * Open the inbox and read its contents.
 * (This should be called only once, by pop3_pass(), and returns the number
 * of messages in the inbox, or -1 for error)
 */
int pop3_grab_mailbox(void) {
	if (getroom(&CC->quickroom, MAILROOM) != 0) return(-1);
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, pop3_add_message);
	return(POP3->num_msgs);
}

/*
 * Authorize with password (implements POP3 "PASS" command)
 */
void pop3_pass(char *argbuf) {
	char password[256];
	int msgs;

	strcpy(password, argbuf);
	striplt(password);

	lprintf(9, "Trying <%s>\n", password);
	if (CtdlTryPassword(password) == pass_ok) {
		msgs = pop3_grab_mailbox();
		if (msgs >= 0) {
			cprintf("+OK %s is logged in (%d messages)\r\n",
				CC->usersupp.fullname, msgs);
			lprintf(9, "POP3 password login successful\n");
		}
		else {
			cprintf("-ERR Can't open your mailbox\r\n");
		}
	}
	else {
		cprintf("-ERR That is NOT the password!  Go away!\r\n");
	}
}



/*
 * list available msgs
 */
void pop3_list(char *argbuf) {
	int i;
	int which_one;

	which_one = atoi(argbuf);

	/* "list one" mode */
	if (which_one > 0) {
		if (which_one > POP3->num_msgs) {
			cprintf("-ERR no such message, only %d are here\r\n",
				POP3->num_msgs);
			return;
		}
		else if (POP3->msgs[which_one-1].deleted) {
			cprintf("-ERR Sorry, you deleted that message.\r\n");
			return;
		}
		else {
			cprintf("+OK %d %d\n",
				which_one,
				POP3->msgs[which_one-1].rfc822_length
				);
			return;
		}
	}

	/* "list all" (scan listing) mode */
	else {
		cprintf("+OK Here's your mail:\r\n");
		if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
			if (! POP3->msgs[i].deleted) {
				cprintf("%d %d\r\n",
					i+1,
					POP3->msgs[i].rfc822_length);
			}
		}
		cprintf(".\r\n");
	}
}


/*
 * STAT (tally up the total message count and byte count) command
 */
void pop3_stat(char *argbuf) {
	int total_msgs = 0;
	size_t total_octets = 0;
	int i;
	
	if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
		if (! POP3->msgs[i].deleted) {
			++total_msgs;
			total_octets += POP3->msgs[i].rfc822_length;
		}
	}

	cprintf("+OK %d %d\n", total_msgs, total_octets);
}



/* 
 * Main command loop for POP3 sessions.
 */
void pop3_command_loop(void) {
	char cmdbuf[256];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "POP3 socket is broken.  Ending session.\r\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(5, "citserver[%3d]: %s\r\n", CC->cs_pid, cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	if (!strncasecmp(cmdbuf, "NOOP", 4)) {
		cprintf("+OK This command successfully did nothing.\r\n");
	}

	else if (!strncasecmp(cmdbuf, "QUIT", 4)) {
		cprintf("+OK Goodbye...\r\n");
		CC->kill_me = 1;
		return;
	}

	else if (!strncasecmp(cmdbuf, "USER", 4)) {
		pop3_user(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "PASS", 4)) {
		pop3_pass(&cmdbuf[5]);
	}

	else if (!CC->logged_in) {
		cprintf("-ERR Not logged in.\r\n");
	}

	else if (!strncasecmp(cmdbuf, "LIST", 4)) {
		pop3_list(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "STAT", 4)) {
		pop3_stat(&cmdbuf[5]);
	}

	else {
		cprintf("500 I'm afraid I can't do that, Dave.\r\n");
	}

}



char *Dynamic_Module_Init(void)
{
	SYM_POP3 = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(POP3_PORT,
				pop3_greeting,
				pop3_command_loop);
	CtdlRegisterSessionHook(pop3_cleanup_function, EVT_STOP);
	return "$Id$";
}
