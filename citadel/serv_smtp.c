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
#include <sys/time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
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
	char helo_node[SIZ];
	struct usersupp vrfy_buffer;
	int vrfy_count;
	char vrfy_match[SIZ];
	char from[SIZ];
	int number_of_recipients;
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
	CtdlAllocUserData(SYM_SMTP_RECP, SIZ);
	sprintf(SMTP_RECP, "%s", "");

	cprintf("220 Welcome to the Citadel/UX ESMTP server at %s\r\n",
		config.c_fqdn);
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
	char buf[SIZ];
	char username[SIZ];

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
	char password[SIZ];

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
	char buf[SIZ];

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
	if (SMTP_RECP != NULL) strcpy(SMTP_RECP, "");
	if (CC->logged_in) logout(CC);
	cprintf("250 Zap!\r\n");
}

/*
 * Clear out the portions of the state buffer that need to be cleared out
 * after the DATA command finishes.
 */
void smtp_data_clear(void) {
	strcpy(SMTP->from, "");
	SMTP->number_of_recipients = 0;
	SMTP->delivery_mode = 0;
	SMTP->message_originated_locally = 0;
	if (SMTP_RECP != NULL) strcpy(SMTP_RECP, "");
}



/*
 * Implements the "MAIL From:" command
 */
void smtp_mail(char *argbuf) {
	char user[SIZ];
	char node[SIZ];
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
		else {
			SMTP->message_originated_locally = 1;
		}
	}

	/* Otherwise, make sure outsiders aren't trying to forge mail from
	 * this system.
	 */
	else {
		TRACE;
		cvt = convert_internet_address(user, node, SMTP->from);
		TRACE;
		lprintf(9, "cvt=%d, citaddr=<%s@%s>\n", cvt, user, node);
		if (CtdlHostAlias(node) == hostalias_localhost) {
			TRACE;
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
	int cvt;
	char user[SIZ];
	char node[SIZ];
	char recp[SIZ];

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
	TRACE;
	alias(recp);
	TRACE;

	cvt = convert_internet_address(user, node, recp);
	snprintf(recp, sizeof recp, "%s@%s", user, node);
	lprintf(9, "cvt=%d, citaddr=<%s@%s>\n", cvt, user, node);

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

		case rfc822_address_on_citadel_network:
			cprintf("250 %s is on the local network\r\n", recp);
			++SMTP->number_of_recipients;
			CtdlReallocUserData(SYM_SMTP_RECP,
				strlen(SMTP_RECP) + 1024 );
			strcat(SMTP_RECP, "ignet|");
			strcat(SMTP_RECP, user);
			strcat(SMTP_RECP, "|");
			strcat(SMTP_RECP, node);
			strcat(SMTP_RECP, "|0|\n");
			return;

		case rfc822_address_nonlocal:
			if (SMTP->message_originated_locally == 0) {
				cprintf("551 Relaying denied\r\n");
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
 * Send a message out through the local network
 * (This is kind of ugly.  IGnet should be done using clean server-to-server
 * code instead of the old style spool.)
 */
void smtp_deliver_ignet(struct CtdlMessage *msg, char *user, char *dest) {
	struct ser_ret smr;
	char *hold_R, *hold_D, *hold_O;
	FILE *fp;
	char filename[SIZ];
	static int seq = 0;

	lprintf(9, "smtp_deliver_ignet(msg, %s, %s)\n", user, dest);

	hold_R = msg->cm_fields['R'];
	hold_D = msg->cm_fields['D'];
	hold_O = msg->cm_fields['O'];
	msg->cm_fields['R'] = user;
	msg->cm_fields['D'] = dest;
	msg->cm_fields['O'] = MAILROOM;

	serialize_message(&smr, msg);

	msg->cm_fields['R'] = hold_R;
	msg->cm_fields['D'] = hold_D;
	msg->cm_fields['O'] = hold_O;

	if (smr.len != 0) {
		snprintf(filename, sizeof filename,
			"./network/spoolin/%s.%04x.%04x",
			dest, getpid(), ++seq);
		lprintf(9, "spool file name is <%s>\n", filename);
		fp = fopen(filename, "wb");
		if (fp != NULL) {
			fwrite(smr.ser, smr.len, 1, fp);
			fclose(fp);
		}
		phree(smr.ser);
	}

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
	if (msg->cm_fields['O']==NULL) msg->cm_fields['O'] = strdoop(MAILROOM);

	/* Save the message in the queue */
	msgid = CtdlSaveMsg(msg,
		"",
		SMTP_SPOOLOUT_ROOM,
		MES_LOCAL);
	++successful_saves;

	instr = mallok(1024);
	snprintf(instr, 1024,
			"Content-type: %s\n\nmsgid|%ld\nsubmitted|%ld\n"
			"bounceto|%s\n",
		SPOOLMIME, msgid, time(NULL),
		SMTP->from );

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

		/* Delivery over the local Citadel network (IGnet) */
		if (!strcasecmp(dtype, "ignet")) {
			extract(user, buf, 1);
			extract(node, buf, 2);
			smtp_deliver_ignet(msg, user, node);
		}

		/* Remote delivery */
		if (!strcasecmp(dtype, "remote")) {
			extract(user, buf, 1);
			instr = reallok(instr, strlen(instr) + 1024);
			snprintf(&instr[strlen(instr)],
				strlen(instr) + 1024,
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
		CtdlSaveMsg(imsg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL);
		CtdlFreeMessage(imsg);
	}

	/* If there are no remote spools, delete the message */	
	else {
		phree(instr);	/* only needed here, because CtdlSaveMsg()
				 * would free this buffer otherwise */
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, msgid, ""); 
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
	char nowstamp[SIZ];

	if (strlen(SMTP->from) == 0) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (SMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now; terminate with '.' by itself\r\n");
	
	datestring(nowstamp, time(NULL), DATESTRING_RFC822);
	body = mallok(4096);

	if (body != NULL) snprintf(body, 4096,
		"Received: from %s\n"
		"	by %s;\n"
		"	%s\n",
			SMTP->helo_node,
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

	smtp_data_clear();	/* clear out the buffers now */
}




/* 
 * Main command loop for SMTP sessions.
 */
void smtp_command_loop(void) {
	char *icmdbuf;

	time(&CC->lastcmd);
	if (client_gets(&icmdbuf) < 1) {
		lprintf(3, "SMTP socket is broken.  Ending session.\n");
		CC->kill_me = 1;
		return;
	}
	/* Rather than fix the dynamic buffer a zillion places in here... */
	if (strlen(icmdbuf) >= SIZ)
	  *(icmdbuf+SIZ)= '\0'; /* no SMTP command should be this big */
	lprintf(5, "citserver[%3d]: %s\n", CC->cs_pid, icmdbuf);
	while (strlen(icmdbuf) < 5) strcat(icmdbuf, " ");

	if (SMTP->command_state == smtp_user) {
		smtp_get_user(icmdbuf);
	}

	else if (SMTP->command_state == smtp_password) {
		smtp_get_pass(icmdbuf);
	}

	else if (!strncasecmp(icmdbuf, "AUTH", 4)) {
		smtp_auth(&icmdbuf[5]);
	}

	else if (!strncasecmp(icmdbuf, "DATA", 4)) {
		smtp_data();
	}

	else if (!strncasecmp(icmdbuf, "EHLO", 4)) {
		smtp_hello(&icmdbuf[5], 1);
	}

	else if (!strncasecmp(icmdbuf, "EXPN", 4)) {
		smtp_expn(&icmdbuf[5]);
	}

	else if (!strncasecmp(icmdbuf, "HELO", 4)) {
		smtp_hello(&icmdbuf[5], 0);
	}

	else if (!strncasecmp(icmdbuf, "HELP", 4)) {
		smtp_help();
	}

	else if (!strncasecmp(icmdbuf, "MAIL", 4)) {
		smtp_mail(&icmdbuf[5]);
	}

	else if (!strncasecmp(icmdbuf, "NOOP", 4)) {
		cprintf("250 This command successfully did nothing.\r\n");
	}

	else if (!strncasecmp(icmdbuf, "QUIT", 4)) {
		cprintf("221 Goodbye...\r\n");
		CC->kill_me = 1;
		return;
		}

	else if (!strncasecmp(icmdbuf, "RCPT", 4)) {
		smtp_rcpt(&icmdbuf[5]);
	}

	else if (!strncasecmp(icmdbuf, "RSET", 4)) {
		smtp_rset();
	}

	else if (!strncasecmp(icmdbuf, "VRFY", 4)) {
		smtp_vrfy(&icmdbuf[5]);
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
		sprintf(dsn, "Error creating temporary file");
		return;
	}
	else {
		CtdlRedirectOutput(msg_fp, -1);
		CtdlOutputMsg(msgnum, MT_RFC822, 0, 0, 1);
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
			if ((lp>=0)&&(rp>lp)) {
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
	snprintf(buf, sizeof buf, "HELO %s", config.c_fqdn);
	lprintf(9, ">%s\n", buf);
	sock_puts_crlf(sock, buf);
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


	/* HELO succeeded, now try the MAIL From: command */
	snprintf(buf, sizeof buf, "MAIL From: <%s>", mailfrom);
	lprintf(9, ">%s\n", buf);
	sock_puts_crlf(sock, buf);
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


	/* MAIL succeeded, now try the RCPT To: command */
	snprintf(buf, sizeof buf, "RCPT To: <%s>", addr);
	lprintf(9, ">%s\n", buf);
	sock_puts_crlf(sock, buf);
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


	/* RCPT succeeded, now try the DATA command */
	lprintf(9, ">DATA\n");
	sock_puts_crlf(sock, "DATA");
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP conversation");
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
		strcpy(dsn, "Connection broken during SMTP conversation");
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
	sock_puts_crlf(sock, "QUIT");
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
	int mes_type = 0;

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
		TRACE;
		if (strlen(bounceto) == 0) {
			lprintf(7, "No bounce address specified\n");
			bounce_msgid = (-1L);
		}
		else if (mes_type = alias(bounceto), mes_type == MES_ERROR) {
			lprintf(7, "Invalid bounce address <%s>\n", bounceto);
			bounce_msgid = (-1L);
		}
		else {
			bounce_msgid = CtdlSaveMsg(bmsg,
				bounceto,
				"", mes_type);
		}
		TRACE;

		/* Otherwise, go to the Aide> room */
		lprintf(9, "bounce to room?\n");
		if (bounce_msgid < 0L) bounce_msgid = CtdlSaveMsg(bmsg,
			"", AIDEROOM,
			MES_LOCAL);
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
	if ( (time(NULL) - last_attempted) < retry) {
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
			SPOOLMIME, instr, time(NULL), retry );
		phree(instr);
		CtdlSaveMsg(msg, "", SMTP_SPOOLOUT_ROOM, MES_LOCAL);
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
	CtdlForEachMessage(MSGS_ALL, 0L, (-127),
		SPOOLMIME, NULL, smtp_do_procmsg, NULL);

	lprintf(7, "SMTP: queue run completed\n");
	doing_queue = 0;
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *Dynamic_Module_Init(void)
{
	SYM_SMTP = CtdlGetDynamicSymbol();
	SYM_SMTP_RECP = CtdlGetDynamicSymbol();

	CtdlRegisterServiceHook(config.c_smtp_port,	/* On the net... */
				NULL,
				smtp_greeting,
				smtp_command_loop);

	CtdlRegisterServiceHook(0,			/* ...and locally */
				"smtp.socket",
				smtp_greeting,
				smtp_command_loop);

	create_room(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1);
	CtdlRegisterSessionHook(smtp_do_queue, EVT_TIMER);
	return "$Id$";
}
