/*
 * $Id$
 *
 * An implementation of RFC821 (Simple Mail Transfer Protocol) for the
 * Citadel system.
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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "serv_extensions.h"
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
#include "locate_host.h"


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

struct citsmtp {		/* Information about the current session */
	int command_state;
	char helo_node[SIZ];
	struct usersupp vrfy_buffer;
	int vrfy_count;
	char vrfy_match[SIZ];
	char from[SIZ];
	char recipients[SIZ];
	int number_of_recipients;
	int number_of_rooms;
	int delivery_mode;
	int message_originated_locally;
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
#define SMTP_RECPS	((char *)CtdlGetUserData(SYM_SMTP_RECPS))
#define SMTP_ROOMS	((char *)CtdlGetUserData(SYM_SMTP_ROOMS))

long SYM_SMTP;
long SYM_SMTP_RECPS;
long SYM_SMTP_ROOMS;

int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */



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
	CtdlAllocUserData(SYM_SMTP_RECPS, SIZ);
	CtdlAllocUserData(SYM_SMTP_ROOMS, SIZ);
	snprintf(SMTP_RECPS, SIZ, "%s", "");
	snprintf(SMTP_ROOMS, SIZ, "%s", "");

	cprintf("220 %s ESMTP Citadel/UX server ready.\r\n", config.c_fqdn);
}


/*
 * Implement HELO and EHLO commands.
 */
void smtp_hello(char *argbuf, int is_esmtp) {

	safestrncpy(SMTP->helo_node, argbuf, sizeof SMTP->helo_node);

	if (!is_esmtp) {
		cprintf("250 Greetings and joyous salutations.\r\n");
	}
	else {
		cprintf("250-Greetings and joyous salutations.\r\n");
		cprintf("250-HELP\r\n");
		cprintf("250-SIZE %ld\r\n", config.c_maxmsglen);
		cprintf("250-PIPELINING\r\n");
		cprintf("250 AUTH=LOGIN\r\n");
	}
}


/*
 * Implement HELP command.
 */
void smtp_help(void) {
	cprintf("214-Commands accepted:\r\n");
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
	cprintf("214     \r\n");
}


/*
 *
 */
void smtp_get_user(char *argbuf) {
	char buf[SIZ];
	char username[SIZ];

	CtdlDecodeBase64(username, argbuf, SIZ);
	lprintf(9, "Trying <%s>\n", username);
	if (CtdlLoginExistingUser(username) == login_ok) {
		CtdlEncodeBase64(buf, "Password:", 9);
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
	char password[SIZ];

	CtdlDecodeBase64(password, argbuf, SIZ);
	lprintf(9, "Trying <%s>\n", password);
	if (CtdlTryPassword(password) == pass_ok) {
		cprintf("235 Hello, %s\r\n", CC->usersupp.fullname);
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
	char buf[SIZ];

	if (strncasecmp(argbuf, "login", 5) ) {
		cprintf("550 We only support LOGIN authentication.\r\n");
		return;
	}

	if (strlen(argbuf) >= 7) {
		smtp_get_user(&argbuf[6]);
	}

	else {
		CtdlEncodeBase64(buf, "Username:", 9);
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

	/*
	 * It is somewhat ambiguous whether we want to log out when a RSET
	 * command is issued.  Here's the code to do it.  It is commented out
	 * because some clients (such as Pine) issue RSET commands before
	 * each message, but still expect to be logged in.
	 *
	 * if (CC->logged_in) {
	 *	logout(CC);
	 * }
	 */

	cprintf("250 Zap!\r\n");
}

/*
 * Clear out the portions of the state buffer that need to be cleared out
 * after the DATA command finishes.
 */
void smtp_data_clear(void) {
	strcpy(SMTP->from, "");
	strcpy(SMTP->recipients, "");
	SMTP->number_of_recipients = 0;
	SMTP->delivery_mode = 0;
	SMTP->message_originated_locally = 0;
}



/*
 * Implements the "MAIL From:" command
 */
void smtp_mail(char *argbuf) {
	char user[SIZ];
	char node[SIZ];
	char name[SIZ];

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
	stripallbut(SMTP->from, '<', '>');

	if (strlen(SMTP->from) == 0) {
		cprintf("501 Empty sender name is not permitted\r\n");
		return;
	}

	/* If this SMTP connection is from a logged-in user, force the 'from'
	 * to be the user's Internet e-mail address as Citadel knows it.
	 */
	if (CC->logged_in) {
		strcpy(SMTP->from, CC->cs_inet_email);
		cprintf("250 Sender ok <%s>\r\n", SMTP->from);
		SMTP->message_originated_locally = 1;
		return;
	}

	/* Otherwise, make sure outsiders aren't trying to forge mail from
	 * this system.
	 */
	else {
		process_rfc822_addr(SMTP->from, user, node, name);
		if (CtdlHostAlias(node) != hostalias_nomatch) {
			cprintf("550 You must log in to send mail from %s\r\n",
				node);
			strcpy(SMTP->from, "");
			return;
		}
	}

	cprintf("250 Sender ok\r\n");
}



/*
 * Implements the "RCPT To:" command
 */
void smtp_rcpt(char *argbuf) {
	char recp[SIZ];
	char message_to_spammer[SIZ];
	struct recptypes *valid = NULL;

	if (strlen(SMTP->from) == 0) {
		cprintf("503 Need MAIL before RCPT\r\n");
		return;
	}

	if (strncasecmp(argbuf, "To:", 3)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	strcpy(recp, &argbuf[3]);
	striplt(recp);
	stripallbut(recp, '<', '>');

	if ( (strlen(recp) + strlen(SMTP->recipients) + 1 ) >= SIZ) {
		cprintf("452 Too many recipients\r\n");
		return;
	}

	/* RBL check */
	if ( (!CC->logged_in) && (!CC->is_local_socket) ) {
		if (rbl_check(message_to_spammer)) {
			cprintf("550 %s\r\n", message_to_spammer);
			/* no need to phree(valid), it's not allocated yet */
			return;
		}
	}

	valid = validate_recipients(recp);
	if (valid->num_error > 0) {
		cprintf("599 Error: %s\r\n", valid->errormsg);
		phree(valid);
		return;
	}

	if (valid->num_internet > 0) {
		if (SMTP->message_originated_locally == 0) {
			cprintf("551 Relaying denied <%s>\r\n", recp);
			phree(valid);
			return;
		}
	}

	cprintf("250 RCPT ok <%s>\r\n", recp);
	if (strlen(SMTP->recipients) > 0) {
		strcat(SMTP->recipients, ",");
	}
	strcat(SMTP->recipients, recp);
	SMTP->number_of_recipients += 1;
}




/*
 * Implements the DATA command
 */
void smtp_data(void) {
	char *body;
	struct CtdlMessage *msg;
	long msgnum;
	char nowstamp[SIZ];
	struct recptypes *valid;
	int scan_errors;

	if (strlen(SMTP->from) == 0) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (SMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now; terminate with '.' by itself\r\n");
	
	datestring(nowstamp, sizeof nowstamp, time(NULL), DATESTRING_RFC822);
	body = mallok(4096);

	/* FIXME
	   it should be Received: from %s (real.name.dom [w.x.y.z])
	 */
	if (body != NULL) snprintf(body, 4096,
		"Received: from %s (%s)\n"
		"	by %s; %s\n",
			SMTP->helo_node,
			CC->cs_host,
			config.c_fqdn,
			nowstamp);
	
	body = CtdlReadMessageBody(".", config.c_maxmsglen, body, 1);
	if (body == NULL) {
		cprintf("550 Unable to save message: internal error.\r\n");
		return;
	}

	lprintf(9, "Converting message...\n");
	msg = convert_internet_message(body);

	/* If the user is locally authenticated, FORCE the From: header to
	 * show up as the real sender.  Yes, this violates the RFC standard,
	 * but IT MAKES SENSE.  Comment it out if you don't like this behavior.
	 *
	 * We also set the "message room name" ('O' field) to MAILROOM
	 * (which is Mail> on most systems) to prevent it from getting set
	 * to something ugly like "0000058008.Sent Items>" when the message
	 * is read with a Citadel client.
	 */
	if (CC->logged_in) {
		if (msg->cm_fields['A'] != NULL) phree(msg->cm_fields['A']);
		if (msg->cm_fields['N'] != NULL) phree(msg->cm_fields['N']);
		if (msg->cm_fields['H'] != NULL) phree(msg->cm_fields['H']);
		if (msg->cm_fields['F'] != NULL) phree(msg->cm_fields['F']);
		if (msg->cm_fields['O'] != NULL) phree(msg->cm_fields['O']);
		msg->cm_fields['A'] = strdoop(CC->usersupp.fullname);
		msg->cm_fields['N'] = strdoop(config.c_nodename);
		msg->cm_fields['H'] = strdoop(config.c_humannode);
		msg->cm_fields['F'] = strdoop(CC->cs_inet_email);
        	msg->cm_fields['O'] = strdoop(MAILROOM);
	}

	/* Submit the message into the Citadel system. */
	valid = validate_recipients(SMTP->recipients);

	/* If there are modules that want to scan this message before final
	 * submission (such as virus checkers or spam filters), call them now
	 * and give them an opportunity to reject the message.
	 */
	scan_errors = PerformMessageHooks(msg, EVT_SMTPSCAN);

	if (scan_errors > 0) {	/* We don't want this message! */

		if (msg->cm_fields['0'] == NULL) {
			msg->cm_fields['0'] = strdoop(
				"Message rejected by filter");
		}

		cprintf("550 %s\r\n", msg->cm_fields['0']);
	}
	
	else {			/* Ok, we'll accept this message. */
		msgnum = CtdlSubmitMsg(msg, valid, "");
		if (msgnum > 0L) {
			cprintf("250 Message accepted.\r\n");
		}
		else {
			cprintf("550 Internal delivery error\r\n");
		}
	}

	CtdlFreeMessage(msg);
	phree(valid);
	smtp_data_clear();	/* clear out the buffers now */
}




/* 
 * Main command loop for SMTP sessions.
 */
void smtp_command_loop(void) {
	char cmdbuf[SIZ];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "SMTP socket is broken.  Ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(5, "SMTP: %s\n", cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	lprintf(9, "CC->logged_in = %d\n", CC->logged_in);

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
		cprintf("250 NOOP\r\n");
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
		cprintf("502 I'm afraid I can't do that.\r\n");
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
void smtp_try(const char *key, const char *addr, int *status,
	      char *dsn, size_t n, long msgnum)
{
	int sock = (-1);
	char mxhosts[1024];
	int num_mxhosts;
	int mx;
	int i;
	char user[SIZ], node[SIZ], name[SIZ];
	char buf[1024];
	char mailfrom[1024];
	int lp, rp;
	FILE *msg_fp = NULL;
	size_t msg_size;
	size_t blocksize = 0;
	int scan_done;

	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(addr, user, node, name);

	lprintf(9, "Attempting SMTP delivery to <%s> @ <%s> (%s)\n",
		user, node, name);

	/* Load the message out of the database into a temp file */
	msg_fp = tmpfile();
	if (msg_fp == NULL) {
		*status = 4;
		snprintf(dsn, n, "Error creating temporary file");
		return;
	}
	else {
		CtdlRedirectOutput(msg_fp, -1);
		CtdlOutputMsg(msgnum, MT_RFC822, HEADERS_ALL, 0, 1);
		CtdlRedirectOutput(NULL, -1);
		fseek(msg_fp, 0L, SEEK_END);
		msg_size = ftell(msg_fp);
	}


	/* Extract something to send later in the 'MAIL From:' command */
	strcpy(mailfrom, "");
	rewind(msg_fp);
	scan_done = 0;
	do {
		if (fgets(buf, sizeof buf, msg_fp)==NULL) scan_done = 1;
		if (!strncasecmp(buf, "From:", 5)) {
			safestrncpy(mailfrom, &buf[5], sizeof mailfrom);
			striplt(mailfrom);
			for (i=0; i<strlen(mailfrom); ++i) {
				if (!isprint(mailfrom[i])) {
					strcpy(&mailfrom[i], &mailfrom[i+1]);
					i=0;
				}
			}

			/* Strip out parenthesized names */
			lp = (-1);
			rp = (-1);
			for (i=0; i<strlen(mailfrom); ++i) {
				if (mailfrom[i] == '(') lp = i;
				if (mailfrom[i] == ')') rp = i;
			}
			if ((lp>0)&&(rp>lp)) {
				strcpy(&mailfrom[lp-1], &mailfrom[rp+1]);
			}

			/* Prefer brokketized names */
			lp = (-1);
			rp = (-1);
			for (i=0; i<strlen(mailfrom); ++i) {
				if (mailfrom[i] == '<') lp = i;
				if (mailfrom[i] == '>') rp = i;
			}
			if ( (lp>=0) && (rp>lp) ) {
				mailfrom[rp] = 0;
				strcpy(mailfrom, &mailfrom[lp]);
			}

			scan_done = 1;
		}
	} while (scan_done == 0);
	if (strlen(mailfrom)==0) strcpy(mailfrom, "someone@somewhere.org");

	/* Figure out what mail exchanger host we have to connect to */
	num_mxhosts = getmx(mxhosts, node);
	lprintf(9, "Number of MX hosts for <%s> is %d\n", node, num_mxhosts);
	if (num_mxhosts < 1) {
		*status = 5;
		snprintf(dsn, SIZ, "No MX hosts found for <%s>", node);
		return;
	}

	for (mx=0; mx<num_mxhosts; ++mx) {
		extract(buf, mxhosts, mx);
		lprintf(9, "Trying <%s>\n", buf);
		sock = sock_connect(buf, "25", "tcp");
		snprintf(dsn, SIZ, "Could not connect: %s", strerror(errno));
		if (sock >= 0) lprintf(9, "Connected!\n");
		if (sock < 0) snprintf(dsn, SIZ, "%s", strerror(errno));
		if (sock >= 0) break;
	}

	if (sock < 0) {
		*status = 4;	/* dsn is already filled in */
		return;
	}

	/* Process the SMTP greeting from the server */
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
		goto bail;
	}
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* At this point we know we are talking to a real SMTP server */

	/* Do a HELO command */
	snprintf(buf, sizeof buf, "HELO %s\r\n", config.c_fqdn);
	lprintf(9, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP HELO");
		goto bail;
	}
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}


	/* HELO succeeded, now try the MAIL From: command */
	snprintf(buf, sizeof buf, "MAIL From: <%s>\r\n", mailfrom);
	lprintf(9, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP MAIL");
		goto bail;
	}
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}


	/* MAIL succeeded, now try the RCPT To: command */
	snprintf(buf, sizeof buf, "RCPT To: <%s>\r\n", addr);
	lprintf(9, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP RCPT");
		goto bail;
	}
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}


	/* RCPT succeeded, now try the DATA command */
	lprintf(9, ">DATA\n");
	sock_write(sock, "DATA\r\n", 6);
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP DATA");
		goto bail;
	}
	lprintf(9, "<%s\n", buf);
	if (buf[0] != '3') {
		if (buf[0] == '4') {
			*status = 3;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* If we reach this point, the server is expecting data */
	rewind(msg_fp);
	while (msg_size > 0) {
		blocksize = sizeof(buf);
		if (blocksize > msg_size) blocksize = msg_size;
		fread(buf, blocksize, 1, msg_fp);
		sock_write(sock, buf, blocksize);
		msg_size -= blocksize;
	}
	if (buf[blocksize-1] != 10) {
		lprintf(5, "Possible problem: message did not correctly "
			"terminate. (expecting 0x10, got 0x%02x)\n",
				buf[blocksize-1]);
	}

	sock_write(sock, ".\r\n", 3);
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP message transmit");
		goto bail;
	}
	lprintf(9, "%s\n", buf);
	if (buf[0] != '2') {
		if (buf[0] == '4') {
			*status = 4;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
		else {
			*status = 5;
			safestrncpy(dsn, &buf[4], 1023);
			goto bail;
		}
	}

	/* We did it! */
	safestrncpy(dsn, &buf[4], 1023);
	*status = 2;

	lprintf(9, ">QUIT\n");
	sock_write(sock, "QUIT\r\n", 6);
	ml_sock_gets(sock, buf);
	lprintf(9, "<%s\n", buf);

bail:	if (msg_fp != NULL) fclose(msg_fp);
	sock_close(sock);
	return;
}



/*
 * smtp_do_bounce() is caled by smtp_do_procmsg() to scan a set of delivery
 * instructions for "5" codes (permanent fatal errors) and produce/deliver
 * a "bounce" message (delivery status notification).
 */
void smtp_do_bounce(char *instr) {
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	char bounceto[1024];
	int num_bounces = 0;
	int bounce_this = 0;
	long bounce_msgid = (-1);
	time_t submitted = 0L;
	struct CtdlMessage *bmsg = NULL;
	int give_up = 0;
	struct recptypes *valid;
	int successful_bounce = 0;

	lprintf(9, "smtp_do_bounce() called\n");
	strcpy(bounceto, "");

	lines = num_tokens(instr, '\n');


	/* See if it's time to give up on delivery of this message */
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		extract(addr, buf, 1);
		if (!strcasecmp(key, "submitted")) {
			submitted = atol(addr);
		}
	}

	if ( (time(NULL) - submitted) > SMTP_GIVE_UP ) {
		give_up = 1;
	}



	bmsg = (struct CtdlMessage *) mallok(sizeof(struct CtdlMessage));
	if (bmsg == NULL) return;
	memset(bmsg, 0, sizeof(struct CtdlMessage));

        bmsg->cm_magic = CTDLMESSAGE_MAGIC;
        bmsg->cm_anon_type = MES_NORMAL;
        bmsg->cm_format_type = 1;
        bmsg->cm_fields['A'] = strdoop("Citadel");
        bmsg->cm_fields['O'] = strdoop(MAILROOM);
        bmsg->cm_fields['N'] = strdoop(config.c_nodename);

	if (give_up) bmsg->cm_fields['M'] = strdoop(
"A message you sent could not be delivered to some or all of its recipients\n"
"due to prolonged unavailability of its destination(s).\n"
"Giving up on the following addresses:\n\n"
);

        else bmsg->cm_fields['M'] = strdoop(
"A message you sent could not be delivered to some or all of its recipients.\n"
"The following addresses were undeliverable:\n\n"
);

	/*
	 * Now go through the instructions checking for stuff.
	 */
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		extract(addr, buf, 1);
		status = extract_int(buf, 2);
		extract(dsn, buf, 3);
		bounce_this = 0;

		lprintf(9, "key=<%s> addr=<%s> status=%d dsn=<%s>\n",
			key, addr, status, dsn);

		if (!strcasecmp(key, "bounceto")) {
			strcpy(bounceto, addr);
		}

		if (
		   (!strcasecmp(key, "local"))
		   || (!strcasecmp(key, "remote"))
		   || (!strcasecmp(key, "ignet"))
		   || (!strcasecmp(key, "room"))
		) {
			if (status == 5) bounce_this = 1;
			if (give_up) bounce_this = 1;
		}

		if (bounce_this) {
			++num_bounces;

			if (bmsg->cm_fields['M'] == NULL) {
				lprintf(2, "ERROR ... M field is null "
					"(%s:%d)\n", __FILE__, __LINE__);
			}

			bmsg->cm_fields['M'] = reallok(bmsg->cm_fields['M'],
				strlen(bmsg->cm_fields['M']) + 1024 );
			strcat(bmsg->cm_fields['M'], addr);
			strcat(bmsg->cm_fields['M'], ": ");
			strcat(bmsg->cm_fields['M'], dsn);
			strcat(bmsg->cm_fields['M'], "\n");

			remove_token(instr, i, '\n');
			--i;
			--lines;
		}
	}

	/* Deliver the bounce if there's anything worth mentioning */
	lprintf(9, "num_bounces = %d\n", num_bounces);
	if (num_bounces > 0) {

		/* First try the user who sent the message */
		lprintf(9, "bounce to user? <%s>\n", bounceto);
		if (strlen(bounceto) == 0) {
			lprintf(7, "No bounce address specified\n");
			bounce_msgid = (-1L);
		}

		/* Can we deliver the bounce to the original sender? */
		valid = validate_recipients(bounceto);
		if (valid != NULL) {
			if (valid->num_error == 0) {
				CtdlSubmitMsg(bmsg, valid, "");
				successful_bounce = 1;
			}
		}

		/* If not, post it in the Aide> room */
		if (successful_bounce == 0) {
			CtdlSubmitMsg(bmsg, NULL, config.c_aideroom);
		}

		/* Free up the memory we used */
		if (valid != NULL) {
			phree(valid);
		}
	}

	CtdlFreeMessage(bmsg);
	lprintf(9, "Done processing bounces\n");
}


/*
 * smtp_purge_completed_deliveries() is caled by smtp_do_procmsg() to scan a
 * set of delivery instructions for completed deliveries and remove them.
 *
 * It returns the number of incomplete deliveries remaining.
 */
int smtp_purge_completed_deliveries(char *instr) {
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	int completed;
	int incomplete = 0;

	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		extract(addr, buf, 1);
		status = extract_int(buf, 2);
		extract(dsn, buf, 3);

		completed = 0;

		if (
		   (!strcasecmp(key, "local"))
		   || (!strcasecmp(key, "remote"))
		   || (!strcasecmp(key, "ignet"))
		   || (!strcasecmp(key, "room"))
		) {
			if (status == 2) completed = 1;
			else ++incomplete;
		}

		if (completed) {
			remove_token(instr, i, '\n');
			--i;
			--lines;
		}
	}

	return(incomplete);
}


/*
 * smtp_do_procmsg()
 *
 * Called by smtp_do_queue() to handle an individual message.
 */
void smtp_do_procmsg(long msgnum, void *userdata) {
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
	int incomplete_deliveries_remaining;
	time_t attempted = 0L;
	time_t last_attempted = 0L;
	time_t retry = SMTP_RETRY_INTERVAL;

	lprintf(9, "smtp_do_procmsg(%ld)\n", msgnum);

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
			remove_token(instr, i, '\n');
			--lines;
			--i;
		}
	}

	/* Learn the message ID and find out about recent delivery attempts */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n');
		extract(key, buf, 0);
		if (!strcasecmp(key, "msgid")) {
			text_msgid = extract_long(buf, 1);
		}
		if (!strcasecmp(key, "retry")) {
			/* double the retry interval after each attempt */
			retry = extract_long(buf, 1) * 2L;
			if (retry > SMTP_RETRY_MAX) {
				retry = SMTP_RETRY_MAX;
			}
			remove_token(instr, i, '\n');
		}
		if (!strcasecmp(key, "attempted")) {
			attempted = extract_long(buf, 1);
			if (attempted > last_attempted)
				last_attempted = attempted;
		}
	}

	/*
	 * Postpone delivery if we've already tried recently.
	 */
	if (((time(NULL) - last_attempted) < retry) && (run_queue_now == 0)) {
		lprintf(7, "Retry time not yet reached.\n");
		phree(instr);
		return;
	}


	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (text_msgid < 0L) {
		lprintf(3, "SMTP: no 'msgid' directive found!\n");
		phree(instr);
		return;
	}

	/* Plow through the instructions looking for 'remote' directives and
	 * a status of 0 (no delivery yet attempted) or 3/4 (transient errors
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
		   && ((status==0)||(status==3)||(status==4)) ) {

			/* Remove this "remote" instruction from the set,
			 * but replace the set's final newline if
			 * remove_token() stripped it.  It has to be there.
			 */
			remove_token(instr, i, '\n');
			if (instr[strlen(instr)-1] != '\n') {
				strcat(instr, "\n");
			}

			--i;
			--lines;
			lprintf(9, "SMTP: Trying <%s>\n", addr);
			smtp_try(key, addr, &status, dsn, sizeof dsn, text_msgid);
			if (status != 2) {
				if (results == NULL) {
					results = mallok(1024);
					memset(results, 0, 1024);
				}
				else {
					results = reallok(results,
						strlen(results) + 1024);
				}
				snprintf(&results[strlen(results)], 1024,
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


	/* Generate 'bounce' messages */
	smtp_do_bounce(instr);

	/* Go through the delivery list, deleting completed deliveries */
	incomplete_deliveries_remaining = 
		smtp_purge_completed_deliveries(instr);


	/*
	 * No delivery instructions remain, so delete both the instructions
	 * message and the message message.
	 */
	if (incomplete_deliveries_remaining <= 0) {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, msgnum, "");
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, text_msgid, "");
	}


	/*
	 * Uncompleted delivery instructions remain, so delete the old
	 * instructions and replace with the updated ones.
	 */
	if (incomplete_deliveries_remaining > 0) {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, msgnum, "");
        	msg = mallok(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		msg->cm_fields['M'] = malloc(strlen(instr)+SIZ);
		snprintf(msg->cm_fields['M'],
			strlen(instr)+SIZ,
			"Content-type: %s\n\n%s\n"
			"attempted|%ld\n"
			"retry|%ld\n",
			SPOOLMIME, instr, (long)time(NULL), (long)retry );
		phree(instr);
		CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM);
		CtdlFreeMessage(msg);
	}

}



/*
 * smtp_do_queue()
 * 
 * Run through the queue sending out messages.
 */
void smtp_do_queue(void) {
	static int doing_queue = 0;

	/*
	 * This is a simple concurrency check to make sure only one queue run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_queue) return;
	doing_queue = 1;

	/* 
	 * Go ahead and run the queue
	 */
	lprintf(7, "SMTP: processing outbound queue\n");

	if (getroom(&CC->quickroom, SMTP_SPOOLOUT_ROOM) != 0) {
		lprintf(3, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
		return;
	}
	CtdlForEachMessage(MSGS_ALL, 0L,
		SPOOLMIME, NULL, smtp_do_procmsg, NULL);

	lprintf(7, "SMTP: queue run completed\n");
	run_queue_now = 0;
	doing_queue = 0;
}



/*****************************************************************************/
/*                          SMTP UTILITY COMMANDS                            */
/*****************************************************************************/

void cmd_smtp(char *argbuf) {
	char cmd[SIZ];
	char node[SIZ];
	char buf[SIZ];
	int i;
	int num_mxhosts;

	if (CtdlAccessCheck(ac_aide)) return;

	extract(cmd, argbuf, 0);

	if (!strcasecmp(cmd, "mx")) {
		extract(node, argbuf, 1);
		num_mxhosts = getmx(buf, node);
		cprintf("%d %d MX hosts listed for %s\n",
			LISTING_FOLLOWS, num_mxhosts, node);
		for (i=0; i<num_mxhosts; ++i) {
			extract(node, buf, i);
			cprintf("%s\n", node);
		}
		cprintf("000\n");
		return;
	}

	else if (!strcasecmp(cmd, "runqueue")) {
		run_queue_now = 1;
		cprintf("%d All outbound SMTP will be retried now.\n", CIT_OK);
		return;
	}

	else {
		cprintf("%d Invalid command.\n", ERROR+ILLEGAL_VALUE);
	}

}


/*
 * Initialize the SMTP outbound queue
 */
void smtp_init_spoolout(void) {
	struct quickroom qrbuf;

	/*
	 * Create the room.  This will silently fail if the room already
	 * exists, and that's perfectly ok, because we want it to exist.
	 */
	create_room(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1, 0);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (lgetroom(&qrbuf, SMTP_SPOOLOUT_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		lputroom(&qrbuf);
	}
}




/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *serv_smtp_init(void)
{
	SYM_SMTP = CtdlGetDynamicSymbol();

	CtdlRegisterServiceHook(config.c_smtp_port,	/* On the net... */
				NULL,
				smtp_greeting,
				smtp_command_loop);

	CtdlRegisterServiceHook(0,			/* ...and locally */
				"smtp.socket",
				smtp_greeting,
				smtp_command_loop);

	smtp_init_spoolout();
	CtdlRegisterSessionHook(smtp_do_queue, EVT_TIMER);
	CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
	return "$Id$";
}
