/* $Id$ */

#define SMTP_PORT	2525

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


struct citsmtp {		/* Information about the current session */
	int command_state;
	struct usersupp vrfy_buffer;
	int vrfy_count;
	char vrfy_match[256];
	char from[256];
	char recipient[256];
	int number_of_recipients;
};

enum {				/* Command states for login authentication */
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
	cprintf("214-    DATA\n");
	cprintf("214-    EHLO\n");
	cprintf("214-    EXPN\n");
	cprintf("214-    HELO\n");
	cprintf("214-    HELP\n");
	cprintf("214-    MAIL\n");
	cprintf("214-    NOOP\n");
	cprintf("214-    QUIT\n");
	cprintf("214-    RCPT\n");
	cprintf("214-    RSET\n");
	cprintf("214-    VRFY\n");
	cprintf("214 I could tell you more, but then I'd have to kill you.\n");
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
		cprintf("550 We only support LOGIN authentication.\n");
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
 * Back end for smtp_vrfy() command
 */
void smtp_vrfy_backend(struct usersupp *us) {

	if (!fuzzy_match(us, SMTP->vrfy_match)) {
		++SMTP->vrfy_count;
		memcpy(&SMTP->vrfy_buffer, us, sizeof(struct usersupp));
	}
}


/* 
 * Implements the VRFY (verify user name) command.
 * Performs fuzzy match on full user names.
 */
void smtp_vrfy(char *argbuf) {
	SMTP->vrfy_count = 0;
	strcpy(SMTP->vrfy_match, argbuf);
	ForEachUser(smtp_vrfy_backend);

	if (SMTP->vrfy_count < 1) {
		cprintf("550 String does not match anything.\n");
	}
	else if (SMTP->vrfy_count == 1) {
		cprintf("250 %s <cit%ld@%s>\n",
			SMTP->vrfy_buffer.fullname,
			SMTP->vrfy_buffer.usernum,
			config.c_fqdn);
	}
	else if (SMTP->vrfy_count > 1) {
		cprintf("553 Request ambiguous: %d users matched.\n",
			SMTP->vrfy_count);
	}

}



/*
 * Back end for smtp_expn() command
 */
void smtp_expn_backend(struct usersupp *us) {

	if (!fuzzy_match(us, SMTP->vrfy_match)) {

		if (SMTP->vrfy_count >= 1) {
			cprintf("250-%s <cit%ld@%s>\n",
				SMTP->vrfy_buffer.fullname,
				SMTP->vrfy_buffer.usernum,
				config.c_fqdn);
		}

		++SMTP->vrfy_count;
		memcpy(&SMTP->vrfy_buffer, us, sizeof(struct usersupp));
	}
}


/* 
 * Implements the EXPN (expand user name) command.
 * Performs fuzzy match on full user names.
 */
void smtp_expn(char *argbuf) {
	SMTP->vrfy_count = 0;
	strcpy(SMTP->vrfy_match, argbuf);
	ForEachUser(smtp_expn_backend);

	if (SMTP->vrfy_count < 1) {
		cprintf("550 String does not match anything.\n");
	}
	else if (SMTP->vrfy_count >= 1) {
		cprintf("250 %s <cit%ld@%s>\n",
			SMTP->vrfy_buffer.fullname,
			SMTP->vrfy_buffer.usernum,
			config.c_fqdn);
	}
}


/*
 * Implements the RSET (reset state) command.
 * Currently this just zeroes out the state buffer.  If pointers to data
 * allocated with mallok() are ever placed in the state buffer, we have to
 * be sure to phree() them first!
 */
void smtp_rset(void) {
	memset(SMTP, 0, sizeof(struct citsmtp));
	if (CC->logged_in) logout(CC);
	cprintf("250 Zap!\n");
}



/*
 * Implements the "MAIL From:" command
 */
void smtp_mail(char *argbuf) {
	char user[256];
	char node[256];
	int cvt;

	if (strlen(SMTP->from) != 0) {
		cprintf("503 Only one sender permitted\n");
		return;
	}

	if (strncasecmp(argbuf, "From:", 5)) {
		cprintf("501 Syntax error\n");
		return;
	}

	strcpy(SMTP->from, &argbuf[5]);
	striplt(SMTP->from);

	if (strlen(SMTP->from) == 0) {
		cprintf("501 Empty sender name is not permitted\n");
		return;
	}


	/* If this SMTP connection is from a logged-in user, make sure that
	 * the user only sends email from his/her own address.
	 */
	if (CC->logged_in) {
		cvt = convert_internet_address(user, node, SMTP->from);
		lprintf(9, "cvt=%d, citaddr=<%s@%s>\n", cvt, user, node);
		if ( (cvt != 0) || (strcasecmp(user, CC->usersupp.fullname))) {
			cprintf("550 <%s> is not your address.\n", SMTP->from);
			strcpy(SMTP->from, "");
			return;
		}
	}

	/* Otherwise, make sure outsiders aren't trying to forge mail from
	 * this system.
	 */
	else {
		cvt = convert_internet_address(user, node, SMTP->from);
		lprintf(9, "cvt=%d, citaddr=<%s@%s>\n", cvt, user, node);
		if (!strcasecmp(node, config.c_nodename)) { /* FIX use fcn */
			cprintf("550 You must log in to send mail from %s\n",
				config.c_fqdn);
			strcpy(SMTP->from, "");
			return;
		}
	}

	cprintf("250 Sender ok.  Groovy.\n");
}



/*
 * Implements the "RCPT To:" command
 */
void smtp_rcpt(char *argbuf) {
	int cvt;
	char user[256];
	char node[256];

	if (strlen(SMTP->from) == 0) {
		cprintf("503 MAIL first, then RCPT.  Duh.\n");
		return;
	}

	if (strlen(SMTP->recipient) > 0) {
		cprintf("550 Only one recipient allowed (FIX)\n");
		return;
	}

	if (strncasecmp(argbuf, "To:", 3)) {
		cprintf("501 Syntax error\n");
		return;
	}

	strcpy(SMTP->recipient, &argbuf[3]);
	striplt(SMTP->recipient);

	cvt = convert_internet_address(user, node, SMTP->recipient);
	switch(cvt) {
		case rfc822_address_locally_validated:
			cprintf("250 %s is a valid recipient.\n", user);
			return;
		case rfc822_no_such_user:
			cprintf("550 %s: no such user\n", SMTP->recipient);
			strcpy(SMTP->recipient, "");
			return;
	}

	strcpy(SMTP->recipient, "");
	cprintf("599 Unknown error (FIX)\n");
}




/*
 * Implements the DATA command
 */
void smtp_data(void) {
	char *body;
	struct CtdlMessage *msg;

/*
	if (strlen(SMTP->from) == 0) {
		cprintf("503 Need MAIL command first.\n");
		return;
	}

	if (SMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\n");
		return;
	}

*/

	cprintf("354 Transmit message now; terminate with '.' by itself\n");
	body = CtdlReadMessageBody(".", config.c_maxmsglen);
	if (body == NULL) {
		cprintf("550 Unable to save message text: internal error.\n");
		return;
	}

	fprintf(stderr, "Converting message...\n");
	msg = convert_internet_message(body);
	phree(body);

	CtdlSaveMsg(msg, "", BASEROOM, MES_LOCAL, 1);	/* FIX temporary */
	CtdlFreeMessage(msg);

	cprintf("599 command unfinished but message saved\n");
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

	else if (!strncasecmp(cmdbuf, "DATA", 4)) {
		smtp_data();
	}

	else if (!strncasecmp(cmdbuf, "EHLO", 4)) {
		smtp_hello(&cmdbuf[5], 1);
	}

	else if (!strncasecmp(cmdbuf, "EXPN", 4)) {
		smtp_expn(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "HELO", 4)) {
		smtp_hello(&cmdbuf[5], 0);
	}

	else if (!strncasecmp(cmdbuf, "HELP", 4)) {
		smtp_help();
	}

	else if (!strncasecmp(cmdbuf, "MAIL", 4)) {
		smtp_mail(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "NOOP", 4)) {
		cprintf("250 This command successfully did nothing.\n");
	}

	else if (!strncasecmp(cmdbuf, "QUIT", 4)) {
		cprintf("221 Goodbye...\n");
		CC->kill_me = 1;
		return;
		}

	else if (!strncasecmp(cmdbuf, "RCPT", 4)) {
		smtp_rcpt(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "RSET", 4)) {
		smtp_rset();
	}

	else if (!strncasecmp(cmdbuf, "VRFY", 4)) {
		smtp_vrfy(&cmdbuf[5]);
	}

	else {
		cprintf("500 I'm afraid I can't do that, Dave.\n");
	}

}



char *Dynamic_Module_Init(void)
{
	SYM_SMTP = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(SMTP_PORT,
				smtp_greeting,
				smtp_command_loop);
	return "$Id$";
}
