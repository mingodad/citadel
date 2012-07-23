/*
 * This module is an SMTP and ESMTP server for the Citadel system.
 * It is compliant with all of the following:
 *
 * RFC  821 - Simple Mail Transfer Protocol
 * RFC  876 - Survey of SMTP Implementations
 * RFC 1047 - Duplicate messages and SMTP
 * RFC 1652 - 8 bit MIME
 * RFC 1869 - Extended Simple Mail Transfer Protocol
 * RFC 1870 - SMTP Service Extension for Message Size Declaration
 * RFC 2033 - Local Mail Transfer Protocol
 * RFC 2197 - SMTP Service Extension for Command Pipelining
 * RFC 2476 - Message Submission
 * RFC 2487 - SMTP Service Extension for Secure SMTP over TLS
 * RFC 2554 - SMTP Service Extension for Authentication
 * RFC 2821 - Simple Mail Transfer Protocol
 * RFC 2822 - Internet Message Format
 * RFC 2920 - SMTP Service Extension for Command Pipelining
 *  
 * The VRFY and EXPN commands have been removed from this implementation
 * because nobody uses these commands anymore, except for spammers.
 *
 * Copyright (c) 1998-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <termios.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <syslog.h>

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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"



#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


#include "ctdl_module.h"

#include "smtp_util.h"
enum {				/* Command states for login authentication */
	smtp_command,
	smtp_user,
	smtp_password,
	smtp_plain
};


/*
 * Here's where our SMTP session begins its happy day.
 */
void smtp_greeting(int is_msa)
{
	citsmtp *sSMTP;
	char message_to_spammer[1024];

	strcpy(CC->cs_clientname, "SMTP session");
	CC->internal_pgm = 1;
	CC->cs_flags |= CS_STEALTH;
	CC->session_specific_data = malloc(sizeof(citsmtp));
	memset(SMTP, 0, sizeof(citsmtp));
	sSMTP = SMTP;
	sSMTP->is_msa = is_msa;

	/* If this config option is set, reject connections from problem
	 * addresses immediately instead of after they execute a RCPT
	 */
	if ( (config.c_rbl_at_greeting) && (sSMTP->is_msa == 0) ) {
		if (rbl_check(message_to_spammer)) {
			if (server_shutting_down)
				cprintf("421 %s\r\n", message_to_spammer);
			else
				cprintf("550 %s\r\n", message_to_spammer);
			CC->kill_me = KILLME_SPAMMER;
			/* no need to free_recipients(valid), it's not allocated yet */
			return;
		}
	}

	/* Otherwise we're either clean or we check later. */

	if (CC->nologin==1) {
		cprintf("451 Too many connections are already open; please try again later.\r\n");
		CC->kill_me = KILLME_MAX_SESSIONS_EXCEEDED;
		/* no need to free_recipients(valid), it's not allocated yet */
		return;
	}

	/* Note: the FQDN *must* appear as the first thing after the 220 code.
	 * Some clients (including citmail.c) depend on it being there.
	 */
	cprintf("220 %s ESMTP Citadel server ready.\r\n", config.c_fqdn);
}


/*
 * SMTPS is just like SMTP, except it goes crypto right away.
 */
void smtps_greeting(void) {
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) CC->kill_me = KILLME_NO_CRYPTO;		/* kill session if no crypto */
#endif
	smtp_greeting(0);
}


/*
 * SMTP MSA port requires authentication.
 */
void smtp_msa_greeting(void) {
	smtp_greeting(1);
}


/*
 * LMTP is like SMTP but with some extra bonus footage added.
 */
void lmtp_greeting(void) {

	smtp_greeting(0);
	SMTP->is_lmtp = 1;
}


/* 
 * Generic SMTP MTA greeting
 */
void smtp_mta_greeting(void) {
	smtp_greeting(0);
}


/*
 * We also have an unfiltered LMTP socket that bypasses spam filters.
 */
void lmtp_unfiltered_greeting(void) {
	citsmtp *sSMTP;

	smtp_greeting(0);
	sSMTP = SMTP;
	sSMTP->is_lmtp = 1;
	sSMTP->is_unfiltered = 1;
}


/*
 * Login greeting common to all auth methods
 */
void smtp_auth_greeting(void) {
		cprintf("235 Hello, %s\r\n", CC->user.fullname);
		syslog(LOG_NOTICE, "SMTP authenticated %s\n", CC->user.fullname);
		CC->internal_pgm = 0;
		CC->cs_flags &= ~CS_STEALTH;
}


/*
 * Implement HELO and EHLO commands.
 *
 * which_command:  0=HELO, 1=EHLO, 2=LHLO
 */
void smtp_hello(char *argbuf, int which_command) {
	citsmtp *sSMTP = SMTP;

	safestrncpy(sSMTP->helo_node, argbuf, sizeof sSMTP->helo_node);

	if ( (which_command != 2) && (sSMTP->is_lmtp) ) {
		cprintf("500 Only LHLO is allowed when running LMTP\r\n");
		return;
	}

	if ( (which_command == 2) && (sSMTP->is_lmtp == 0) ) {
		cprintf("500 LHLO is only allowed when running LMTP\r\n");
		return;
	}

	if (which_command == 0) {
		cprintf("250 Hello %s (%s [%s])\r\n",
			sSMTP->helo_node,
			CC->cs_host,
			CC->cs_addr
		);
	}
	else {
		if (which_command == 1) {
			cprintf("250-Hello %s (%s [%s])\r\n",
				sSMTP->helo_node,
				CC->cs_host,
				CC->cs_addr
			);
		}
		else {
			cprintf("250-Greetings and joyous salutations.\r\n");
		}
		cprintf("250-HELP\r\n");
		cprintf("250-SIZE %ld\r\n", config.c_maxmsglen);

#ifdef HAVE_OPENSSL
		/*
		 * Offer TLS, but only if TLS is not already active.
		 * Furthermore, only offer TLS when running on
		 * the SMTP-MSA port, not on the SMTP-MTA port, due to
		 * questionable reliability of TLS in certain sending MTA's.
		 */
		if ( (!CC->redirect_ssl) && (sSMTP->is_msa) ) {
			cprintf("250-STARTTLS\r\n");
		}
#endif	/* HAVE_OPENSSL */

		cprintf("250-AUTH LOGIN PLAIN\r\n"
			"250-AUTH=LOGIN PLAIN\r\n"
			"250 8BITMIME\r\n"
		);
	}
}



/*
 * Implement HELP command.
 */
void smtp_help(void) {
	cprintf("214 RTFM http://www.ietf.org/rfc/rfc2821.txt\r\n");
}


/*
 *
 */
void smtp_get_user(char *argbuf) {
	char buf[SIZ];
	char username[SIZ];
	citsmtp *sSMTP = SMTP;

	CtdlDecodeBase64(username, argbuf, SIZ);
	/* syslog(LOG_DEBUG, "Trying <%s>\n", username); */
	if (CtdlLoginExistingUser(NULL, username) == login_ok) {
		CtdlEncodeBase64(buf, "Password:", 9, 0);
		cprintf("334 %s\r\n", buf);
		sSMTP->command_state = smtp_password;
	}
	else {
		cprintf("500 No such user.\r\n");
		sSMTP->command_state = smtp_command;
	}
}


/*
 *
 */
void smtp_get_pass(char *argbuf) {
	char password[SIZ];
	long len;

	memset(password, 0, sizeof(password));	
	len = CtdlDecodeBase64(password, argbuf, SIZ);
	/* syslog(LOG_DEBUG, "Trying <%s>\n", password); */
	if (CtdlTryPassword(password, len) == pass_ok) {
		smtp_auth_greeting();
	}
	else {
		cprintf("535 Authentication failed.\r\n");
	}
	SMTP->command_state = smtp_command;
}


/*
 * Back end for PLAIN auth method (either inline or multistate)
 */
void smtp_try_plain(char *encoded_authstring) {
	char decoded_authstring[1024];
	char ident[256];
	char user[256];
	char pass[256];
	int result;
	long len;

	CtdlDecodeBase64(decoded_authstring, encoded_authstring, strlen(encoded_authstring) );
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	len = safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);
	if (len == -1)
		len = sizeof(pass) - 1;

	SMTP->command_state = smtp_command;

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass, len) == pass_ok) {
			smtp_auth_greeting();
			return;
		}
	}
	cprintf("504 Authentication failed.\r\n");
}


/*
 * Attempt to perform authenticated SMTP
 */
void smtp_auth(char *argbuf) {
	char username_prompt[64];
	char method[64];
	char encoded_authstring[1024];

	if (CC->logged_in) {
		cprintf("504 Already logged in.\r\n");
		return;
	}

	extract_token(method, argbuf, 0, ' ', sizeof method);

	if (!strncasecmp(method, "login", 5) ) {
		if (strlen(argbuf) >= 7) {
			smtp_get_user(&argbuf[6]);
		}
		else {
			CtdlEncodeBase64(username_prompt, "Username:", 9, 0);
			cprintf("334 %s\r\n", username_prompt);
			SMTP->command_state = smtp_user;
		}
		return;
	}

	if (!strncasecmp(method, "plain", 5) ) {
		if (num_tokens(argbuf, ' ') < 2) {
			cprintf("334 \r\n");
			SMTP->command_state = smtp_plain;
			return;
		}

		extract_token(encoded_authstring, argbuf, 1, ' ', sizeof encoded_authstring);

		smtp_try_plain(encoded_authstring);
		return;
	}

	if (strncasecmp(method, "login", 5) ) {
		cprintf("504 Unknown authentication method.\r\n");
		return;
	}

}


/*
 * Implements the RSET (reset state) command.
 * Currently this just zeroes out the state buffer.  If pointers to data
 * allocated with malloc() are ever placed in the state buffer, we have to
 * be sure to free() them first!
 *
 * Set do_response to nonzero to output the SMTP RSET response code.
 */
void smtp_rset(int do_response) {
	int is_lmtp;
	int is_unfiltered;
	citsmtp *sSMTP = SMTP;

	/*
	 * Our entire SMTP state is discarded when a RSET command is issued,
	 * but we need to preserve this one little piece of information, so
	 * we save it for later.
	 */
	is_lmtp = sSMTP->is_lmtp;
	is_unfiltered = sSMTP->is_unfiltered;

	memset(sSMTP, 0, sizeof(citsmtp));

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

	/*
	 * Reinstate this little piece of information we saved (see above).
	 */
	sSMTP->is_lmtp = is_lmtp;
	sSMTP->is_unfiltered = is_unfiltered;

	if (do_response) {
		cprintf("250 Zap!\r\n");
	}
}

/*
 * Clear out the portions of the state buffer that need to be cleared out
 * after the DATA command finishes.
 */
void smtp_data_clear(void) {
	citsmtp *sSMTP = SMTP;

	strcpy(sSMTP->from, "");
	strcpy(sSMTP->recipients, "");
	sSMTP->number_of_recipients = 0;
	sSMTP->delivery_mode = 0;
	sSMTP->message_originated_locally = 0;
}

/*
 * Implements the "MAIL FROM:" command
 */
void smtp_mail(char *argbuf) {
	char user[SIZ];
	char node[SIZ];
	char name[SIZ];
	citsmtp *sSMTP = SMTP;

	if (!IsEmptyStr(sSMTP->from)) {
		cprintf("503 Only one sender permitted\r\n");
		return;
	}

	if (strncasecmp(argbuf, "From:", 5)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	strcpy(sSMTP->from, &argbuf[5]);
	striplt(sSMTP->from);
	if (haschar(sSMTP->from, '<') > 0) {
		stripallbut(sSMTP->from, '<', '>');
	}

	/* We used to reject empty sender names, until it was brought to our
	 * attention that RFC1123 5.2.9 requires that this be allowed.  So now
	 * we allow it, but replace the empty string with a fake
	 * address so we don't have to contend with the empty string causing
	 * other code to fail when it's expecting something there.
	 */
	if (IsEmptyStr(sSMTP->from)) {
		strcpy(sSMTP->from, "someone@example.com");
	}

	/* If this SMTP connection is from a logged-in user, force the 'from'
	 * to be the user's Internet e-mail address as Citadel knows it.
	 */
	if (CC->logged_in) {
		safestrncpy(sSMTP->from, CC->cs_inet_email, sizeof sSMTP->from);
		cprintf("250 Sender ok <%s>\r\n", sSMTP->from);
		sSMTP->message_originated_locally = 1;
		return;
	}

	else if (sSMTP->is_lmtp) {
		/* Bypass forgery checking for LMTP */
	}

	/* Otherwise, make sure outsiders aren't trying to forge mail from
	 * this system (unless, of course, c_allow_spoofing is enabled)
	 */
	else if (config.c_allow_spoofing == 0) {
		process_rfc822_addr(sSMTP->from, user, node, name);
		if (CtdlHostAlias(node) != hostalias_nomatch) {
			cprintf("550 You must log in to send mail from %s\r\n", node);
			strcpy(sSMTP->from, "");
			return;
		}
	}

	cprintf("250 Sender ok\r\n");
}



/*
 * Implements the "RCPT To:" command
 */
void smtp_rcpt(char *argbuf) {
	char recp[1024];
	char message_to_spammer[SIZ];
	struct recptypes *valid = NULL;
	citsmtp *sSMTP = SMTP;

	if (IsEmptyStr(sSMTP->from)) {
		cprintf("503 Need MAIL before RCPT\r\n");
		return;
	}

	if (strncasecmp(argbuf, "To:", 3)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	if ( (sSMTP->is_msa) && (!CC->logged_in) ) {
		cprintf("550 You must log in to send mail on this port.\r\n");
		strcpy(sSMTP->from, "");
		return;
	}

	safestrncpy(recp, &argbuf[3], sizeof recp);
	striplt(recp);
	stripallbut(recp, '<', '>');

	if ( (strlen(recp) + strlen(sSMTP->recipients) + 1 ) >= SIZ) {
		cprintf("452 Too many recipients\r\n");
		return;
	}

	/* RBL check */
	if ( (!CC->logged_in)	/* Don't RBL authenticated users */
	   && (!sSMTP->is_lmtp) ) {	/* Don't RBL LMTP clients */
		if (config.c_rbl_at_greeting == 0) {	/* Don't RBL again if we already did it */
			if (rbl_check(message_to_spammer)) {
				if (server_shutting_down)
					cprintf("421 %s\r\n", message_to_spammer);
				else
					cprintf("550 %s\r\n", message_to_spammer);
				/* no need to free_recipients(valid), it's not allocated yet */
				return;
			}
		}
	}

	valid = validate_recipients(
		recp, 
		smtp_get_Recipients(),
		(sSMTP->is_lmtp)? POST_LMTP: (CC->logged_in)? POST_LOGGED_IN: POST_EXTERNAL
	);
	if (valid->num_error != 0) {
		cprintf("550 %s\r\n", valid->errormsg);
		free_recipients(valid);
		return;
	}

	if (valid->num_internet > 0) {
		if (CC->logged_in) {
                        if (CtdlCheckInternetMailPermission(&CC->user)==0) {
				cprintf("551 <%s> - you do not have permission to send Internet mail\r\n", recp);
                                free_recipients(valid);
                                return;
                        }
                }
	}

	if (valid->num_internet > 0) {
		if ( (sSMTP->message_originated_locally == 0)
		   && (sSMTP->is_lmtp == 0) ) {
			cprintf("551 <%s> - relaying denied\r\n", recp);
			free_recipients(valid);
			return;
		}
	}

	cprintf("250 RCPT ok <%s>\r\n", recp);
	if (!IsEmptyStr(sSMTP->recipients)) {
		strcat(sSMTP->recipients, ",");
	}
	strcat(sSMTP->recipients, recp);
	sSMTP->number_of_recipients += 1;
	if (valid != NULL)  {
		free_recipients(valid);
	}
}




/*
 * Implements the DATA command
 */
void smtp_data(void) {
	StrBuf *body;
	char *defbody; //TODO: remove me
	struct CtdlMessage *msg = NULL;
	long msgnum = (-1L);
	char nowstamp[SIZ];
	struct recptypes *valid;
	int scan_errors;
	int i;
	char result[SIZ];
	citsmtp *sSMTP = SMTP;

	if (IsEmptyStr(sSMTP->from)) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (sSMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now - terminate with '.' by itself\r\n");
	
	datestring(nowstamp, sizeof nowstamp, time(NULL), DATESTRING_RFC822);
	defbody = malloc(4096);

	if (defbody != NULL) {
		if (sSMTP->is_lmtp && (CC->cs_UDSclientUID != -1)) {
			snprintf(defbody, 4096,
			       "Received: from %s (Citadel from userid %ld)\n"
			       "	by %s; %s\n",
			       sSMTP->helo_node,
			       (long int) CC->cs_UDSclientUID,
			       config.c_fqdn,
			       nowstamp);
		}
		else {
			snprintf(defbody, 4096,
				 "Received: from %s (%s [%s])\n"
				 "	by %s; %s\n",
				 sSMTP->helo_node,
				 CC->cs_host,
				 CC->cs_addr,
				 config.c_fqdn,
				 nowstamp);
		}
	}
	body = CtdlReadMessageBodyBuf(HKEY("."), config.c_maxmsglen, defbody, 1, NULL);
	if (body == NULL) {
		cprintf("550 Unable to save message: internal error.\r\n");
		return;
	}

	syslog(LOG_DEBUG, "Converting message...\n");
	msg = convert_internet_message_buf(&body);

	/* If the user is locally authenticated, FORCE the From: header to
	 * show up as the real sender.  Yes, this violates the RFC standard,
	 * but IT MAKES SENSE.  If you prefer strict RFC adherence over
	 * common sense, you can disable this in the configuration.
	 *
	 * We also set the "message room name" ('O' field) to MAILROOM
	 * (which is Mail> on most systems) to prevent it from getting set
	 * to something ugly like "0000058008.Sent Items>" when the message
	 * is read with a Citadel client.
	 */
	if ( (CC->logged_in) && (config.c_rfc822_strict_from == 0) ) {

#ifdef SMTP_REJECT_INVALID_SENDER
		int validemail = 0;

		if (!IsEmptyStr(CC->cs_inet_email) && 
		    !IsEmptyStr(msg->cm_fields['F']))
			validemail = strcmp(CC->cs_inet_email, msg->cm_fields['F']) == 0;
		if ((!validemail) && 
		    (!IsEmptyStr(CC->cs_inet_other_emails)))
		{
			int num_secondary_emails = 0;
			int i;
			num_secondary_emails = num_tokens(CC->cs_inet_other_emails, '|');
			for (i=0; i<num_secondary_emails && !validemail; ++i) {
				char buf[256];
				extract_token(buf, CC->cs_inet_other_emails,i,'|',sizeof CC->cs_inet_other_emails);
				validemail = strcmp(buf, msg->cm_fields['F']) == 0;
			}
		}
		if (!validemail) {
			syslog(LOG_ERR, "invalid sender '%s' - rejecting this message", msg->cm_fields['F']);
			cprintf("550 Invalid sender '%s' - rejecting this message.\r\n", msg->cm_fields['F']);
			return;
		}
#endif /* SMTP_REJECT_INVALID_SENDER */

		if (msg->cm_fields['A'] != NULL) free(msg->cm_fields['A']);
		if (msg->cm_fields['N'] != NULL) free(msg->cm_fields['N']);
		if (msg->cm_fields['H'] != NULL) free(msg->cm_fields['H']);
		if (msg->cm_fields['F'] != NULL) free(msg->cm_fields['F']);
		if (msg->cm_fields['O'] != NULL) free(msg->cm_fields['O']);
		msg->cm_fields['A'] = strdup(CC->user.fullname);
		msg->cm_fields['N'] = strdup(config.c_nodename);
		msg->cm_fields['H'] = strdup(config.c_humannode);
		msg->cm_fields['F'] = strdup(CC->cs_inet_email);
        	msg->cm_fields['O'] = strdup(MAILROOM);
	}

	/* Set the "envelope from" address */
	if (msg->cm_fields['P'] != NULL) {
		free(msg->cm_fields['P']);
	}
	msg->cm_fields['P'] = strdup(sSMTP->from);

	/* Set the "envelope to" address */
	if (msg->cm_fields['V'] != NULL) {
		free(msg->cm_fields['V']);
	}
	msg->cm_fields['V'] = strdup(sSMTP->recipients);

	/* Submit the message into the Citadel system. */
	valid = validate_recipients(
		sSMTP->recipients,
		smtp_get_Recipients(),
		(sSMTP->is_lmtp)? POST_LMTP: (CC->logged_in)? POST_LOGGED_IN: POST_EXTERNAL
	);

	/* If there are modules that want to scan this message before final
	 * submission (such as virus checkers or spam filters), call them now
	 * and give them an opportunity to reject the message.
	 */
	if (sSMTP->is_unfiltered) {
		scan_errors = 0;
	}
	else {
		scan_errors = PerformMessageHooks(msg, EVT_SMTPSCAN);
	}

	if (scan_errors > 0) {	/* We don't want this message! */

		if (msg->cm_fields['0'] == NULL) {
			msg->cm_fields['0'] = strdup("Message rejected by filter");
		}

		sprintf(result, "550 %s\r\n", msg->cm_fields['0']);
	}
	
	else {			/* Ok, we'll accept this message. */
		msgnum = CtdlSubmitMsg(msg, valid, "", 0);
		if (msgnum > 0L) {
			sprintf(result, "250 Message accepted.\r\n");
		}
		else {
			sprintf(result, "550 Internal delivery error\r\n");
		}
	}

	/* For SMTP and ESMTP, just print the result message.  For LMTP, we
	 * have to print one result message for each recipient.  Since there
	 * is nothing in Citadel which would cause different recipients to
	 * have different results, we can get away with just spitting out the
	 * same message once for each recipient.
	 */
	if (sSMTP->is_lmtp) {
		for (i=0; i<sSMTP->number_of_recipients; ++i) {
			cprintf("%s", result);
		}
	}
	else {
		cprintf("%s", result);
	}

	/* Write something to the syslog(which may or may not be where the
	 * rest of the Citadel logs are going; some sysadmins want LOG_MAIL).
	 */
	syslog((LOG_MAIL | LOG_INFO),
		"%ld: from=<%s>, nrcpts=%d, relay=%s [%s], stat=%s",
		msgnum,
		sSMTP->from,
		sSMTP->number_of_recipients,
		CC->cs_host,
		CC->cs_addr,
		result
	);

	/* Clean up */
	CtdlFreeMessage(msg);
	free_recipients(valid);
	smtp_data_clear();	/* clear out the buffers now */
}


/*
 * implements the STARTTLS command
 */
void smtp_starttls(void)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response, "220 Begin TLS negotiation now\r\n");
	sprintf(nosup_response, "554 TLS not supported here\r\n");
	sprintf(error_response, "554 Internal error\r\n");
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
	smtp_rset(0);
}


/* 
 * Main command loop for SMTP server sessions.
 */
void smtp_command_loop(void) {
	char cmdbuf[SIZ];
	citsmtp *sSMTP = SMTP;

	if (sSMTP == NULL) {
		syslog(LOG_EMERG, "Session SMTP data is null.  WTF?  We will crash now.\n");
		return cit_panic_backtrace (0);
	}

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		syslog(LOG_CRIT, "SMTP: client disconnected: ending session.\n");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}
	syslog(LOG_INFO, "SMTP server: %s\n", cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	if (sSMTP->command_state == smtp_user) {
		smtp_get_user(cmdbuf);
	}

	else if (sSMTP->command_state == smtp_password) {
		smtp_get_pass(cmdbuf);
	}

	else if (sSMTP->command_state == smtp_plain) {
		smtp_try_plain(cmdbuf);
	}

	else if (!strncasecmp(cmdbuf, "AUTH", 4)) {
		smtp_auth(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "DATA", 4)) {
		smtp_data();
	}

	else if (!strncasecmp(cmdbuf, "HELO", 4)) {
		smtp_hello(&cmdbuf[5], 0);
	}

	else if (!strncasecmp(cmdbuf, "EHLO", 4)) {
		smtp_hello(&cmdbuf[5], 1);
	}

	else if (!strncasecmp(cmdbuf, "LHLO", 4)) {
		smtp_hello(&cmdbuf[5], 2);
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
		CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
		return;
	}

	else if (!strncasecmp(cmdbuf, "RCPT", 4)) {
		smtp_rcpt(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "RSET", 4)) {
		smtp_rset(1);
	}
#ifdef HAVE_OPENSSL
	else if (!strcasecmp(cmdbuf, "STARTTLS")) {
		smtp_starttls();
	}
#endif
	else {
		cprintf("502 I'm afraid I can't do that.\r\n");
	}


}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/
/*
 * This cleanup function blows away the temporary memory used by
 * the SMTP server.
 */
void smtp_cleanup_function(void) {

	/* Don't do this stuff if this is not an SMTP session! */
	if (CC->h_command_function != smtp_command_loop) return;

	syslog(LOG_DEBUG, "Performing SMTP cleanup hook\n");
	free(SMTP);
}



const char *CitadelServiceSMTP_MTA="SMTP-MTA";
const char *CitadelServiceSMTPS_MTA="SMTPs-MTA";
const char *CitadelServiceSMTP_MSA="SMTP-MSA";
const char *CitadelServiceSMTP_LMTP="LMTP";
const char *CitadelServiceSMTP_LMTP_UNF="LMTP-UnF";

CTDL_MODULE_INIT(smtp)
{
	if (!threading)
	{
		CtdlRegisterServiceHook(config.c_smtp_port,	/* SMTP MTA */
					NULL,
					smtp_mta_greeting,
					smtp_command_loop,
					NULL, 
					CitadelServiceSMTP_MTA);

#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(config.c_smtps_port,
					NULL,
					smtps_greeting,
					smtp_command_loop,
					NULL,
					CitadelServiceSMTPS_MTA);
#endif

		CtdlRegisterServiceHook(config.c_msa_port,	/* SMTP MSA */
					NULL,
					smtp_msa_greeting,
					smtp_command_loop,
					NULL,
					CitadelServiceSMTP_MSA);

		CtdlRegisterServiceHook(0,			/* local LMTP */
					file_lmtp_socket,
					lmtp_greeting,
					smtp_command_loop,
					NULL,
					CitadelServiceSMTP_LMTP);

		CtdlRegisterServiceHook(0,			/* local LMTP */
					file_lmtp_unfiltered_socket,
					lmtp_unfiltered_greeting,
					smtp_command_loop,
					NULL,
					CitadelServiceSMTP_LMTP_UNF);

		CtdlRegisterSessionHook(smtp_cleanup_function, EVT_STOP, PRIO_STOP + 250);
	}
	
	/* return our module name for the log */
	return "smtp";
}
