/* $Id$ */
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

struct citsmtp {
	int command_state;
};

enum {
	smtp_command,
	smtp_user,
	smtp_password
};

#define SMTP ((struct citsmtp *)CtdlGetUserData(SYM_SMTP))

long SYM_SMTP;


/*
 * Here's where our SMTP session begins its happy day.
 */
void smtp_greeting(void) {

	strcpy(CC->cs_clientname, "Citadel SMTP");
	CtdlAllocUserData(SYM_SMTP, sizeof(struct citsmtp));

	cprintf("220 Welcome to the Citadel/UX ESMTP server at %s\n",
		config.c_fqdn);
}


/*
 * Implement HELO and EHLO commands.
 */
void smtp_hello(char *argbuf, int is_esmtp) {

	if (!is_esmtp) {
		cprintf("250 Greetings and joyous salutations.\n");
	}
	else {
		cprintf("250-Greetings and joyous salutations.\n");
		cprintf("250-HELP\n");
		cprintf("250-SIZE %ld\n", config.c_maxmsglen);
		cprintf("250 AUTH=LOGIN\n");
	}
}


/*
 * Implement HELP command.
 */
void smtp_help(void) {
	cprintf("214-Here's the frequency, Kenneth:\n");
	cprintf("214-    EHLO\n");
	cprintf("214-    HELO\n");
	cprintf("214-    HELP\n");
	cprintf("214-    NOOP\n");
	cprintf("214-    QUIT\n");
	cprintf("214 I'd tell you more, but then I'd have to kill you.\n");
}


/*
 *
 */
void smtp_get_user(char *argbuf) {
	char buf[256];
	char username[256];

	decode_base64(username, argbuf);
	lprintf(9, "Trying <%s>\n", username);
	if (CtdlLoginExistingUser(username) == login_ok) {
		encode_base64(buf, "Password:");
		cprintf("334 %s\n", buf);
		SMTP->command_state = smtp_password;
	}
	else {
		cprintf("500 No such user.\n");
		SMTP->command_state = smtp_command;
	}
}


/*
 *
 */
void smtp_get_pass(char *argbuf) {
	char password[256];

	decode_base64(password, argbuf);
	lprintf(9, "Trying <%s>\n", password);
	if (CtdlTryPassword(password) == pass_ok) {
		cprintf("235 Authentication successful.\n");
		lprintf(9, "SMTP auth login successful\n");
	}
	else {
		cprintf("500 Authentication failed.\n");
	}
	SMTP->command_state = smtp_command;
}


/*
 *
 */
void smtp_auth(char *argbuf) {
	char buf[256];

	if (strncasecmp(argbuf, "login", 5) ) {
		cprintf("500 We only support LOGIN authentication.\n");
		return;
	}

	if (strlen(argbuf) >= 7) {
		smtp_get_user(&argbuf[6]);
	}

	else {
		encode_base64(buf, "Username:");
		cprintf("334 %s\n", buf);
		SMTP->command_state = smtp_user;
	}
}


/* 
 * Main command loop for SMTP sessions.
 */
void smtp_command_loop(void) {
	char cmdbuf[256];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "SMTP socket is broken.  Ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(5, "citserver[%3d]: %s\n", CC->cs_pid, cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	if (SMTP->command_state == smtp_user) {
		smtp_get_user(cmdbuf);
	}

	else if (SMTP->command_state == smtp_password) {
		smtp_get_pass(cmdbuf);
	}

	else if (!strncasecmp(cmdbuf, "AUTH", 4)) {
		smtp_auth(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "EHLO", 4)) {
		smtp_hello(&cmdbuf[5], 1);
	}

	else if (!strncasecmp(cmdbuf, "HELO", 4)) {
		smtp_hello(&cmdbuf[5], 0);
	}

	else if (!strncasecmp(cmdbuf, "HELP", 4)) {
		smtp_help();
	}

	else if (!strncasecmp(cmdbuf, "NOOP", 4)) {
		cprintf("250 This command successfully did nothing.\n");
	}

	else if (!strncasecmp(cmdbuf, "QUIT", 4)) {
		cprintf("221 Goodbye...\n");
		CC->kill_me = 1;
		return;
		}

	else {
		cprintf("500 I'm afraid I can't do that, Dave.\n");
	}

}



char *Dynamic_Module_Init(void)
{
	SYM_SMTP = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(25,
				smtp_greeting,
				smtp_command_loop);
	return "$Id$";
}
