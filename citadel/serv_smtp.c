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
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"


struct citsmtp {		/* Information about the current session */
	int command_state;
	struct usersupp vrfy_buffer;
	int vrfy_count;
	char vrfy_match[256];
	char from[256];
	int number_of_recipients;
	int delivery_mode;
};

enum {				/* Command states for login authentication */
	smtp_command,
	smtp_user,
	smtp_password
};

enum {				/* Delivery modes */
	smtp_deliver_local,
	smtp_deliver_remote
};

#define SMTP		((struct citsmtp *)CtdlGetUserData(SYM_SMTP))
#define SMTP_RECP	((char *)CtdlGetUserData(SYM_SMTP_RECP))

long SYM_SMTP;
long SYM_SMTP_RECP;



/*****************************************************************************/
/*                      SMTP SERVER (INBOUND) STUFF                          */
/*****************************************************************************/


/*
 * Here's where our SMTP session begins its happy day.
 */
void smtp_greeting(void) {

	strcpy(CC->cs_clientname, "SMTP session");
	CC->internal_pgm = 1;
	CC->cs_flags |= CS_STEALTH;
	CtdlAllocUserData(SYM_SMTP, sizeof(struct citsmtp));
	CtdlAllocUserData(SYM_SMTP_RECP, 256);
	sprintf(SMTP_RECP, "%s", "");

	cprintf("220 Welcome to the Citadel/UX ESMTP server at %s\r\n",
		config.c_fqdn);
}


/*
 * Implement HELO and EHLO commands.
 */
void smtp_hello(char *argbuf, int is_esmtp) {

	if (!is_esmtp) {
		cprintf("250 Greetings and joyous salutations.\r\n");
	}
	else {
		cprintf("250-Greetings and joyous salutations.\r\n");
		cprintf("250-HELP\r\n");
		cprintf("250-SIZE %ld\r\n", config.c_maxmsglen);
		cprintf("250 AUTH=LOGIN\r\n");
	}
}


/*
 * Implement HELP command.
 */
void smtp_help(void) {
	cprintf("214-Here's the frequency, Kenneth:\r\n");
	cprintf("214-    DATA\r\n");
	cprintf("214-    EHLO\r\n");
	cprintf("214-    EXPN\r\n");
	cprintf("214-    HELO\r\n");
	cprintf("214-    HELP\r\n");
	cprintf("214-    MAIL\r\n");
	cprintf("214-    NOOP\r\n");
	cprintf("214-    QUIT\r\n");
	cprintf("214-    RCPT\r\n");
	cprintf("214-    RSET\r\n");
	cprintf("214-    VRFY\r\n");
	cprintf("214 I could tell you more, but then I'd have to kill you.\r\n");
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
		cprintf("334 %s\r\n", buf);
		SMTP->command_state = smtp_password;
	}
	else {
		cprintf("500 No such user.\r\n");
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
		cprintf("235 Authentication successful.\r\n");
		lprintf(9, "SMTP authenticated login successful\n");
		CC->internal_pgm = 0;
		CC->cs_flags &= ~CS_STEALTH;
	}
	else {
		cprintf("500 Authentication failed.\r\n");
	}
	SMTP->command_state = smtp_command;
}


/*
 *
 */
void smtp_auth(char *argbuf) {
	char buf[256];

	if (strncasecmp(argbuf, "login", 5) ) {
		cprintf("550 We only support LOGIN authentication.\r\n");
		return;
	}

	if (strlen(argbuf) >= 7) {
		smtp_get_user(&argbuf[6]);
	}

	else {
		encode_base64(buf, "Username:");
		cprintf("334 %s\r\n", buf);
		SMTP->command_state = smtp_user;
	}
}


/*
 * Back end for smtp_vrfy() command
 */
void smtp_vrfy_backend(struct usersupp *us, void *data) {

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
	ForEachUser(smtp_vrfy_backend, NULL);

	if (SMTP->vrfy_count < 1) {
		cprintf("550 String does not match anything.\r\n");
	}
	else if (SMTP->vrfy_count == 1) {
		cprintf("250 %s <cit%ld@%s>\r\n",
			SMTP->vrfy_buffer.fullname,
			SMTP->vrfy_buffer.usernum,
			config.c_fqdn);
	}
	else if (SMTP->vrfy_count > 1) {
		cprintf("553 Request ambiguous: %d users matched.\r\n",
			SMTP->vrfy_count);
	}

}



/*
 * Back end for smtp_expn() command
 */
void smtp_expn_backend(struct usersupp *us, void *data) {

	if (!fuzzy_match(us, SMTP->vrfy_match)) {

		if (SMTP->vrfy_count >= 1) {
			cprintf("250-%s <cit%ld@%s>\r\n",
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
	ForEachUser(smtp_expn_backend, NULL);

	if (SMTP->vrfy_count < 1) {
		cprintf("550 String does not match anything.\r\n");
	}
	else if (SMTP->vrfy_count >= 1) {
		cprintf("250 %s <cit%ld@%s>\r\n",
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
	cprintf("250 Zap!\r\n");
}



/*
 * Implements the "MAIL From:" command
 */
void smtp_mail(char *argbuf) {
	char user[256];
	char node[256];
	int cvt;

	if (strlen(SMTP->from) != 0) {
		cprintf("503 Only one sender permitted\r\n");
		return;
	}

	if (strncasecmp(argbuf, "From:", 5)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	strcpy(SMTP->from, &argbuf[5]);
	striplt(SMTP->from);

	if (strlen(SMTP->from) == 0) {
		cprintf("501 Empty sender name is not permitted\r\n");
		return;
	}


	/* If this SMTP connection is from a logged-in user, make sure that
	 * the user only sends email from his/her own address.
	 */
	if (CC->logged_in) {
		cvt = convert_internet_address(user, node, SMTP->from);
		lprintf(9, "cvt=%d, citaddr=<%s@%s>\n", cvt, user, node);
		if ( (cvt != 0) || (strcasecmp(user, CC->usersupp.fullname))) {
			cprintf("550 <%s> is not your address.\r\n", SMTP->from);
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
			cprintf("550 You must log in to send mail from %s\r\n",
				config.c_fqdn);
			strcpy(SMTP->from, "");
			return;
		}
	}

	cprintf("250 Sender ok.  Groovy.\r\n");
}



/*
 * Implements the "RCPT To:" command
 */
void smtp_rcpt(char *argbuf) {
	int cvt;
	char user[256];
	char node[256];
	char recp[256];
	int is_spam = 0;	/* FIX implement anti-spamming */

	if (strlen(SMTP->from) == 0) {
		cprintf("503 MAIL first, then RCPT.  Duh.\r\n");
		return;
	}

	if (strncasecmp(argbuf, "To:", 3)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	strcpy(recp, &argbuf[3]);
	striplt(recp);
	alias(recp);

	cvt = convert_internet_address(user, node, recp);
	sprintf(recp, "%s@%s", user, node);


	switch(cvt) {
		case rfc822_address_locally_validated:
			cprintf("250 %s is a valid recipient.\r\n", user);
			++SMTP->number_of_recipients;
			CtdlReallocUserData(SYM_SMTP_RECP,
				strlen(SMTP_RECP) + 1024 );
			strcat(SMTP_RECP, "local|");
			strcat(SMTP_RECP, user);
			strcat(SMTP_RECP, "|0\n");
			return;

		case rfc822_room_delivery:
			cprintf("250 Delivering to room '%s'\r\n", user);
			++SMTP->number_of_recipients;
			CtdlReallocUserData(SYM_SMTP_RECP,
				strlen(SMTP_RECP) + 1024 );
			strcat(SMTP_RECP, "room|");
			strcat(SMTP_RECP, user);
			strcat(SMTP_RECP, "|0|\n");
			return;

		case rfc822_no_such_user:
			cprintf("550 %s: no such user\r\n", recp);
			return;

		case rfc822_address_invalid:
			if (is_spam) {
				cprintf("551 Away with thee, spammer!\r\n");
			}
			else {
				cprintf("250 Remote recipient %s ok\r\n", recp);
				++SMTP->number_of_recipients;
				CtdlReallocUserData(SYM_SMTP_RECP,
					strlen(SMTP_RECP) + 1024 );
				strcat(SMTP_RECP, "remote|");
				strcat(SMTP_RECP, recp);
				strcat(SMTP_RECP, "|0|\n");
				return;
			}
			return;
	}

	cprintf("599 Unknown error\r\n");
}





/*
 * Back end for smtp_data()  ... this does the actual delivery of the message
 * Returns 0 on success, nonzero on failure
 */
int smtp_message_delivery(struct CtdlMessage *msg) {
	char user[1024];
	char node[1024];
	char name[1024];
	char buf[1024];
	char dtype[1024];
	char room[1024];
	int successful_saves = 0;	/* number of successful local saves */
	int failed_saves = 0;		/* number of failed deliveries */
	int remote_spools = 0;		/* number of copies to send out */
	long msgid = (-1L);
	int i;
	struct usersupp userbuf;
	char *instr;			/* Remote delivery instructions */
	struct CtdlMessage *imsg;

	lprintf(9, "smtp_message_delivery() called\n");

	/* Fill in 'from' fields with envelope information if missing */
	process_rfc822_addr(SMTP->from, user, node, name);
	if (msg->cm_fields['A']==NULL) msg->cm_fields['A'] = strdoop(user);
	if (msg->cm_fields['N']==NULL) msg->cm_fields['N'] = strdoop(node);
	if (msg->cm_fields['H']==NULL) msg->cm_fields['H'] = strdoop(name);

	/* Save the message in the queue */
	msgid = CtdlSaveMsg(msg,
		"",
		SMTP_SPOOLOUT_ROOM,
		MES_LOCAL,
		1);
	++successful_saves;

	instr = mallok(1024);
	sprintf(instr, "Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n",
		SPOOLMIME, msgid, time(NULL) );

	for (i=0; i<SMTP->number_of_recipients; ++i) {
		extract_token(buf, SMTP_RECP, i, '\n');
		extract(dtype, buf, 0);

		/* Stuff local mailboxes */
		if (!strcasecmp(dtype, "local")) {
			extract(user, buf, 1);
			if (getuser(&userbuf, user) == 0) {
				MailboxName(room, &userbuf, MAILROOM);
				CtdlSaveMsgPointerInRoom(room, msgid, 0);
				++successful_saves;
			}
			else {
				++failed_saves;
			}
		}

		/* Delivery to local non-mailbox rooms */
		if (!strcasecmp(dtype, "room")) {
			extract(room, buf, 1);
			CtdlSaveMsgPointerInRoom(room, msgid, 0);
			++successful_saves;
		}

		/* Remote delivery */
		if (!strcasecmp(dtype, "remote")) {
			extract(user, buf, 1);
			instr = reallok(instr, strlen(instr) + 1024);
			sprintf(&instr[strlen(instr)],
				"remote|%s|0\n",
				user);
			++remote_spools;
		}

	}

	/* If there are remote spools to be done, save the instructions */
	if (remote_spools > 0) {
        	imsg = mallok(sizeof(struct CtdlMessage));
		memset(imsg, 0, sizeof(struct CtdlMessage));
		imsg->cm_magic = CTDLMESSAGE_MAGIC;
		imsg->cm_anon_type = MES_NORMAL;
		imsg->cm_format_type = FMT_RFC822;
		imsg->cm_fields['M'] = instr;
		CtdlSaveMsg(imsg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL, 1);
		CtdlFreeMessage(imsg);
	}

	/* If there are no remote spools, delete the message */	
	else {
		phree(instr);	/* only needed here, because CtdlSaveMsg()
				 * would free this buffer otherwise */
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, msgid, NULL); 
	}

	return(failed_saves);
}



/*
 * Implements the DATA command
 */
void smtp_data(void) {
	char *body;
	struct CtdlMessage *msg;
	int retval;
	char nowstamp[256];

	if (strlen(SMTP->from) == 0) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (SMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now; terminate with '.' by itself\r\n");
	
	generate_rfc822_datestamp(nowstamp, time(NULL));
	body = mallok(4096);
	if (body != NULL) sprintf(body,
		"Received: from %s\n"
		"	by %s;\n"
		"	%s\n",
			"FIX.FIX.com",
			config.c_fqdn,
			nowstamp);
	
	body = CtdlReadMessageBody(".", config.c_maxmsglen, body);
	if (body == NULL) {
		cprintf("550 Unable to save message text: internal error.\r\n");
		return;
	}

	lprintf(9, "Converting message...\n");
	msg = convert_internet_message(body);

	/* If the user is locally authenticated, FORCE the From: header to
	 * show up as the real sender
	 */
	if (CC->logged_in) {
		if (msg->cm_fields['A'] != NULL) phree(msg->cm_fields['A']);
		if (msg->cm_fields['N'] != NULL) phree(msg->cm_fields['N']);
		if (msg->cm_fields['H'] != NULL) phree(msg->cm_fields['H']);
		msg->cm_fields['A'] = strdoop(CC->usersupp.fullname);
		msg->cm_fields['N'] = strdoop(config.c_nodename);
		msg->cm_fields['H'] = strdoop(config.c_humannode);
	}

	retval = smtp_message_delivery(msg);
	CtdlFreeMessage(msg);

	if (!retval) {
		cprintf("250 Message accepted for delivery.\r\n");
	}
	else {
		cprintf("550 Internal delivery errors: %d\r\n", retval);
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
		cprintf("250 This command successfully did nothing.\r\n");
	}

	else if (!strncasecmp(cmdbuf, "QUIT", 4)) {
		cprintf("221 Goodbye...\r\n");
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
		cprintf("502 I'm sorry Dave, I'm afraid I can't do that.\r\n");
	}

}




/*****************************************************************************/
/*               SMTP CLIENT (OUTBOUND PROCESSING) STUFF                     */
/*****************************************************************************/



/*
 * smtp_try()
 *
 * Called by smtp_do_procmsg() to attempt delivery to one SMTP host
 *
 */
void smtp_try(char *key, char *addr, int *status, char *dsn, long msgnum)
{
	char buf[256];
	int sock = (-1);
	char mxhosts[1024];
	int num_mxhosts;
	int mx;
	char user[256], node[256], name[256];

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(addr, user, node, name);
	lprintf(9, "Attempting SMTP delivery to <%s> @ <%s> (%s)\n",
		user, node, name);

	num_mxhosts = getmx(mxhosts, node);
	lprintf(9, "Number of MX hosts for <%s> is %d\n", node, num_mxhosts);
	if (num_mxhosts < 1) {
		*status = 5;
		sprintf(dsn, "No MX hosts found for <%s>", node);
		return;
	}

	for (mx=0; mx<num_mxhosts; ++mx) {
		extract(buf, mxhosts, mx);
		lprintf(9, "Trying <%s>\n", buf);
		sock = sock_connect(buf, "25", "tcp");
		sprintf(dsn, "Could not connect: %s", strerror(errno));
		if (sock >= 0) lprintf(9, "Connected!\n");
		if (sock < 0) sprintf(dsn, "%s", strerror(errno));
		if (sock >= 0) break;
	}

	if (sock < 0) {
		*status = 4;	/* dsn is already filled in */
		return;
	}

	/* Process the SMTP greeting from the server */
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}

	/* At this point we know we are talking to a real SMTP server */

	/* Do a HELO command */
	sprintf(buf, "HELO %s", config.c_fqdn);
	sock_puts(sock, buf);
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}


	/* HELO succeeded, now try the MAIL From: command */
	sprintf(buf, "MAIL From: FIX@uncnsrd.mt-kisco.ny.us"); /* FIX */
	sock_puts(sock, buf);
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}


	/* MAIL succeeded, now try the RCPT To: command */
	sprintf(buf, "RCPT To: %s", addr);
	sock_puts(sock, buf);
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}


	/* RCPT succeeded, now try the DATA command */
	sock_puts(sock, "DATA");
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '3') {
		if (buf[0] == '4') {
			*status = 3;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}

	/* If we reach this point, the server is expecting data */
	CtdlOutputMsg(msgnum, MT_RFC822, 0, 0, NULL, sock, 1);
	sock_puts(sock, ".");
	if (sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		sock_close(sock);
		return;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
		else {
			*status = 5;
			strcpy(dsn, &buf[4]);
			sock_close(sock);
			return;
		}
	}

	/* We did it! */
	strcpy(dsn, &buf[4]);
	*status = 2;

	sock_puts(sock, "QUIT");
	sock_gets(sock, buf);
	lprintf(9, "%s\n", buf);
	sock_close(sock);
}




/*
 * smtp_do_procmsg()
 *
 * Called by smtp_do_queue() to handle an individual message.
 */
void smtp_do_procmsg(long msgnum) {
	struct CtdlMessage *msg;
	char *instr = NULL;
	char *results = NULL;
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	long text_msgid = (-1);

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) {
		lprintf(3, "SMTP: tried %ld but no such message!\n", msgnum);
		return;
	}

	instr = strdoop(msg->cm_fields['M']);
	CtdlFreeMessage(msg);

	/* Strip out the headers amd any other non-instruction line */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		if (num_tokens(buf, '|') < 2) {
			lprintf(9, "removing <%s>\n", buf);
			remove_token(instr, i, '\n');
			--lines;
			--i;
		}
	}

	/* Learn the message ID */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		if (!strcasecmp(key, "msgid")) {
			text_msgid = extract_long(buf, 1);
		}
	}

	if (text_msgid < 0L) {
		lprintf(3, "SMTP: no 'msgid' directive found!\n");
		phree(instr);
		return;
	}

	/* Plow through the instructions looking for 'remote' directives and
	 * a status of 0 (no delivery yet attempted) or 3 (transient errors
	 * were experienced and it's time to try again)
	 */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		extract(addr, buf, 1);
		status = extract_int(buf, 2);
		extract(dsn, buf, 3);
		if ( (!strcasecmp(key, "remote"))
		   && ((status==0)||(status==3)) ) {
			remove_token(instr, i, '\n');
			--i;
			--lines;
			lprintf(9, "SMTP: Trying <%s>\n", addr);
			smtp_try(key, addr, &status, dsn, text_msgid);
			if (status != 2) {
				if (results == NULL) {
					results = mallok(1024);
					memset(results, 0, 1024);
				}
				else {
					results = reallok(results,
						strlen(results) + 1024);
				}
				sprintf(&results[strlen(results)],
					"%s|%s|%d|%s\n",
					key, addr, status, dsn);
			}
		}
	}

	if (results != NULL) {
		instr = reallok(instr, strlen(instr) + strlen(results) + 2);
		strcat(instr, results);
		phree(results);
	}

	/* Delete the instructions and replace with the updated ones */
	CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, msgnum, NULL);    
        msg = mallok(sizeof(struct CtdlMessage));
	memset(msg, 0, sizeof(struct CtdlMessage));
	msg->cm_magic = CTDLMESSAGE_MAGIC;
	msg->cm_anon_type = MES_NORMAL;
	msg->cm_format_type = FMT_RFC822;
	msg->cm_fields['M'] = malloc(strlen(instr)+256);
	sprintf(msg->cm_fields['M'],
		"Content-type: %s\n\n%s\n", SPOOLMIME, instr);
	phree(instr);
	CtdlSaveMsg(msg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL, 1);
	CtdlFreeMessage(msg);
}



/*
 * smtp_do_queue()
 * 
 * Run through the queue sending out messages.
 */
void smtp_do_queue(void) {
	lprintf(5, "SMTP: processing outbound queue\n");

	if (getroom(&CC->quickroom, SMTP_SPOOLOUT_ROOM) != 0) {
		lprintf(3, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L, SPOOLMIME, NULL, smtp_do_procmsg);

	lprintf(5, "SMTP: queue run completed\n");
}


/**** FIX  temporary hack to run the queue *****/
void cmd_qqqq(char *argbuf) {
	smtp_do_queue();
	cprintf("%d ok\n", OK);
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *Dynamic_Module_Init(void)
{
	SYM_SMTP = CtdlGetDynamicSymbol();
	SYM_SMTP_RECP = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(SMTP_PORT,
				smtp_greeting,
				smtp_command_loop);

	/****  FIX ... temporary hack to run the queue ******/
	CtdlRegisterProtoHook(cmd_qqqq, "QQQQ", "run the queue");  

	create_room(SMTP_SPOOLOUT_ROOM, 3, "", 0);
	return "$Id$";
}

