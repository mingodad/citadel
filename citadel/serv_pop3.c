/* $Id$ */

#define POP3_PORT	1110

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


struct citpop3 {		/* Information about the current session */
	char FIX[3];
};

#define POP3 ((struct citpop3 *)CtdlGetUserData(SYM_POP3))

long SYM_POP3;


/*
 * Here's where our POP3 session begins its happy day.
 */
void pop3_greeting(void) {

	strcpy(CC->cs_clientname, "POP3 session");
	CC->internal_pgm = 1;
	CtdlAllocUserData(SYM_POP3, sizeof(struct citpop3));

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
 * Authorize with password (implements POP3 "PASS" command)
 */
void pop3_pass(char *argbuf) {
	char password[256];

	strcpy(password, argbuf);
	striplt(password);

	lprintf(9, "Trying <%s>\n", password);
	if (CtdlTryPassword(password) == pass_ok) {
		cprintf("+OK %s is logged in!\r\n", CC->usersupp.fullname);
		lprintf(9, "POP3 password login successful\n");
	}
	else {
		cprintf("-ERR That is NOT the password!  Go away!\r\n");
	}
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
	return "$Id$";
}
