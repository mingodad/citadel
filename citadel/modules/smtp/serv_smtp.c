/*
 * $Id$
 *
 * This module is an SMTP and ESMTP implementation for the Citadel system.
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
 * Copyright (c) 1998-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "policy.h"
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



struct citsmtp {		/* Information about the current session */
	int command_state;
	char helo_node[SIZ];
	char from[SIZ];
	char recipients[SIZ];
	int number_of_recipients;
	int delivery_mode;
	int message_originated_locally;
	int is_lmtp;
	int is_unfiltered;
	int is_msa;
};

enum {				/* Command states for login authentication */
	smtp_command,
	smtp_user,
	smtp_password,
	smtp_plain
};

#define SMTP		((struct citsmtp *)CC->session_specific_data)


int run_queue_now = 0;	/* Set to 1 to ignore SMTP send retry times */

citthread_mutex_t smtp_send_lock;


/*****************************************************************************/
/*                      SMTP SERVER (INBOUND) STUFF                          */
/*****************************************************************************/


/*
 * Here's where our SMTP session begins its happy day.
 */
void smtp_greeting(int is_msa)
{
	char message_to_spammer[1024];

	strcpy(CC->cs_clientname, "SMTP session");
	CC->internal_pgm = 1;
	CC->cs_flags |= CS_STEALTH;
	CC->session_specific_data = malloc(sizeof(struct citsmtp));
	memset(SMTP, 0, sizeof(struct citsmtp));
	SMTP->is_msa = is_msa;

	/* If this config option is set, reject connections from problem
	 * addresses immediately instead of after they execute a RCPT
	 */
	if ( (config.c_rbl_at_greeting) && (SMTP->is_msa == 0) ) {
		if (rbl_check(message_to_spammer)) {
			if (CtdlThreadCheckStop())
				cprintf("421 %s\r\n", message_to_spammer);
			else
				cprintf("550 %s\r\n", message_to_spammer);
			CC->kill_me = 1;
			/* no need to free_recipients(valid), it's not allocated yet */
			return;
		}
	}

	/* Otherwise we're either clean or we check later. */

	if (CC->nologin==1) {
		cprintf("500 Too many users are already online (maximum is %d)\r\n",
			config.c_maxsessions
		);
		CC->kill_me = 1;
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
	if (!CC->redirect_ssl) CC->kill_me = 1;		/* kill session if no crypto */
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
	smtp_greeting(0);
	SMTP->is_lmtp = 1;
	SMTP->is_unfiltered = 1;
}


/*
 * Login greeting common to all auth methods
 */
void smtp_auth_greeting(void) {
		cprintf("235 Hello, %s\r\n", CC->user.fullname);
		CtdlLogPrintf(CTDL_NOTICE, "SMTP authenticated %s\n", CC->user.fullname);
		CC->internal_pgm = 0;
		CC->cs_flags &= ~CS_STEALTH;
}


/*
 * Implement HELO and EHLO commands.
 *
 * which_command:  0=HELO, 1=EHLO, 2=LHLO
 */
void smtp_hello(char *argbuf, int which_command) {

	safestrncpy(SMTP->helo_node, argbuf, sizeof SMTP->helo_node);

	if ( (which_command != 2) && (SMTP->is_lmtp) ) {
		cprintf("500 Only LHLO is allowed when running LMTP\r\n");
		return;
	}

	if ( (which_command == 2) && (SMTP->is_lmtp == 0) ) {
		cprintf("500 LHLO is only allowed when running LMTP\r\n");
		return;
	}

	if (which_command == 0) {
		cprintf("250 Hello %s (%s [%s])\r\n",
			SMTP->helo_node,
			CC->cs_host,
			CC->cs_addr
		);
	}
	else {
		if (which_command == 1) {
			cprintf("250-Hello %s (%s [%s])\r\n",
				SMTP->helo_node,
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
		if ( (!CC->redirect_ssl) && (SMTP->is_msa) ) {
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

	CtdlDecodeBase64(username, argbuf, SIZ);
	/* CtdlLogPrintf(CTDL_DEBUG, "Trying <%s>\n", username); */
	if (CtdlLoginExistingUser(NULL, username) == login_ok) {
		CtdlEncodeBase64(buf, "Password:", 9, 0);
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

	memset(password, 0, sizeof(password));	
	CtdlDecodeBase64(password, argbuf, SIZ);
	/* CtdlLogPrintf(CTDL_DEBUG, "Trying <%s>\n", password); */
	if (CtdlTryPassword(password) == pass_ok) {
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

	CtdlDecodeBase64(decoded_authstring, encoded_authstring, strlen(encoded_authstring) );
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);

	SMTP->command_state = smtp_command;

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass) == pass_ok) {
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

	/*
	 * Our entire SMTP state is discarded when a RSET command is issued,
	 * but we need to preserve this one little piece of information, so
	 * we save it for later.
	 */
	is_lmtp = SMTP->is_lmtp;
	is_unfiltered = SMTP->is_unfiltered;

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

	/*
	 * Reinstate this little piece of information we saved (see above).
	 */
	SMTP->is_lmtp = is_lmtp;
	SMTP->is_unfiltered = is_unfiltered;

	if (do_response) {
		cprintf("250 Zap!\r\n");
	}
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

const char *smtp_get_Recipients(void)
{
	if (SMTP == NULL)
		return NULL;
	else return SMTP->from;

}

/*
 * Implements the "MAIL FROM:" command
 */
void smtp_mail(char *argbuf) {
	char user[SIZ];
	char node[SIZ];
	char name[SIZ];

	if (!IsEmptyStr(SMTP->from)) {
		cprintf("503 Only one sender permitted\r\n");
		return;
	}

	if (strncasecmp(argbuf, "From:", 5)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	strcpy(SMTP->from, &argbuf[5]);
	striplt(SMTP->from);
	if (haschar(SMTP->from, '<') > 0) {
		stripallbut(SMTP->from, '<', '>');
	}

	/* We used to reject empty sender names, until it was brought to our
	 * attention that RFC1123 5.2.9 requires that this be allowed.  So now
	 * we allow it, but replace the empty string with a fake
	 * address so we don't have to contend with the empty string causing
	 * other code to fail when it's expecting something there.
	 */
	if (IsEmptyStr(SMTP->from)) {
		strcpy(SMTP->from, "someone@example.com");
	}

	/* If this SMTP connection is from a logged-in user, force the 'from'
	 * to be the user's Internet e-mail address as Citadel knows it.
	 */
	if (CC->logged_in) {
		safestrncpy(SMTP->from, CC->cs_inet_email, sizeof SMTP->from);
		cprintf("250 Sender ok <%s>\r\n", SMTP->from);
		SMTP->message_originated_locally = 1;
		return;
	}

	else if (SMTP->is_lmtp) {
		/* Bypass forgery checking for LMTP */
	}

	/* Otherwise, make sure outsiders aren't trying to forge mail from
	 * this system (unless, of course, c_allow_spoofing is enabled)
	 */
	else if (config.c_allow_spoofing == 0) {
		process_rfc822_addr(SMTP->from, user, node, name);
		if (CtdlHostAlias(node) != hostalias_nomatch) {
			cprintf("550 You must log in to send mail from %s\r\n", node);
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
	char recp[1024];
	char message_to_spammer[SIZ];
	struct recptypes *valid = NULL;

	if (IsEmptyStr(SMTP->from)) {
		cprintf("503 Need MAIL before RCPT\r\n");
		return;
	}

	if (strncasecmp(argbuf, "To:", 3)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	if ( (SMTP->is_msa) && (!CC->logged_in) ) {
		cprintf("550 You must log in to send mail on this port.\r\n");
		strcpy(SMTP->from, "");
		return;
	}

	safestrncpy(recp, &argbuf[3], sizeof recp);
	striplt(recp);
	stripallbut(recp, '<', '>');

	if ( (strlen(recp) + strlen(SMTP->recipients) + 1 ) >= SIZ) {
		cprintf("452 Too many recipients\r\n");
		return;
	}

	/* RBL check */
	if ( (!CC->logged_in)	/* Don't RBL authenticated users */
	   && (!SMTP->is_lmtp) ) {	/* Don't RBL LMTP clients */
		if (config.c_rbl_at_greeting == 0) {	/* Don't RBL again if we already did it */
			if (rbl_check(message_to_spammer)) {
				if (CtdlThreadCheckStop())
					cprintf("421 %s\r\n", message_to_spammer);
				else
					cprintf("550 %s\r\n", message_to_spammer);
				/* no need to free_recipients(valid), it's not allocated yet */
				return;
			}
		}
	}

	valid = validate_recipients(recp, 
				    smtp_get_Recipients (),
				    (SMTP->is_lmtp)? POST_LMTP:
				       (CC->logged_in)? POST_LOGGED_IN:
				                        POST_EXTERNAL);
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
		if ( (SMTP->message_originated_locally == 0)
		   && (SMTP->is_lmtp == 0) ) {
			cprintf("551 <%s> - relaying denied\r\n", recp);
			free_recipients(valid);
			return;
		}
	}

	cprintf("250 RCPT ok <%s>\r\n", recp);
	if (!IsEmptyStr(SMTP->recipients)) {
		strcat(SMTP->recipients, ",");
	}
	strcat(SMTP->recipients, recp);
	SMTP->number_of_recipients += 1;
	if (valid != NULL)  {
		free_recipients(valid);
	}
}




/*
 * Implements the DATA command
 */
void smtp_data(void) {
	char *body;
	struct CtdlMessage *msg = NULL;
	long msgnum = (-1L);
	char nowstamp[SIZ];
	struct recptypes *valid;
	int scan_errors;
	int i;
	char result[SIZ];

	if (IsEmptyStr(SMTP->from)) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (SMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now - terminate with '.' by itself\r\n");
	
	datestring(nowstamp, sizeof nowstamp, time(NULL), DATESTRING_RFC822);
	body = malloc(4096);

	if (body != NULL) {
		if (SMTP->is_lmtp && (CC->cs_UDSclientUID != -1)) {
			snprintf(body, 4096,
				 "Received: from %s (Citadel from userid %ld)\n"
				 "	by %s; %s\n",
				 SMTP->helo_node,
				 (long int) CC->cs_UDSclientUID,
				 config.c_fqdn,
				 nowstamp);
		}
		else {
			snprintf(body, 4096,
				 "Received: from %s (%s [%s])\n"
				 "	by %s; %s\n",
				 SMTP->helo_node,
				 CC->cs_host,
				 CC->cs_addr,
				 config.c_fqdn,
				 nowstamp);
		}
	}
	body = CtdlReadMessageBody(".", config.c_maxmsglen, body, 1, 0);
	if (body == NULL) {
		cprintf("550 Unable to save message: internal error.\r\n");
		return;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Converting message...\n");
	msg = convert_internet_message(body);

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
	msg->cm_fields['P'] = strdup(SMTP->from);

	/* Set the "envelope to" address */
	if (msg->cm_fields['V'] != NULL) {
		free(msg->cm_fields['V']);
	}
	msg->cm_fields['V'] = strdup(SMTP->recipients);

	/* Submit the message into the Citadel system. */
	valid = validate_recipients(SMTP->recipients, 
				    smtp_get_Recipients (),
				    (SMTP->is_lmtp)? POST_LMTP:
				       (CC->logged_in)? POST_LOGGED_IN:
				                        POST_EXTERNAL);

	/* If there are modules that want to scan this message before final
	 * submission (such as virus checkers or spam filters), call them now
	 * and give them an opportunity to reject the message.
	 */
	if (SMTP->is_unfiltered) {
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

	/* For SMTP and ESTMP, just print the result message.  For LMTP, we
	 * have to print one result message for each recipient.  Since there
	 * is nothing in Citadel which would cause different recipients to
	 * have different results, we can get away with just spitting out the
	 * same message once for each recipient.
	 */
	if (SMTP->is_lmtp) {
		for (i=0; i<SMTP->number_of_recipients; ++i) {
			cprintf("%s", result);
		}
	}
	else {
		cprintf("%s", result);
	}

	/* Write something to the syslog (which may or may not be where the
	 * rest of the Citadel logs are going; some sysadmins want LOG_MAIL).
	 */
	if (enable_syslog) {
		syslog((LOG_MAIL | LOG_INFO),
			"%ld: from=<%s>, nrcpts=%d, relay=%s [%s], stat=%s",
			msgnum,
			SMTP->from,
			SMTP->number_of_recipients,
			CC->cs_host,
			CC->cs_addr,
			result
		);
	}

	/* Clean up */
	CtdlFreeMessage(msg);
	free_recipients(valid);
	smtp_data_clear();	/* clear out the buffers now */
}


/*
 * implements the STARTTLS command (Citadel API version)
 */
void smtp_starttls(void)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response,
		"220 Begin TLS negotiation now\r\n");
	sprintf(nosup_response,
		"554 TLS not supported here\r\n");
	sprintf(error_response,
		"554 Internal error\r\n");
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
	smtp_rset(0);
}



/* 
 * Main command loop for SMTP sessions.
 */
void smtp_command_loop(void) {
	char cmdbuf[SIZ];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		CtdlLogPrintf(CTDL_CRIT, "Client disconnected: ending session.\n");
		CC->kill_me = 1;
		return;
	}
	CtdlLogPrintf(CTDL_INFO, "SMTP server: %s\n", cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	if (SMTP->command_state == smtp_user) {
		smtp_get_user(cmdbuf);
	}

	else if (SMTP->command_state == smtp_password) {
		smtp_get_pass(cmdbuf);
	}

	else if (SMTP->command_state == smtp_plain) {
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
		CC->kill_me = 1;
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
/*               SMTP CLIENT (OUTBOUND PROCESSING) STUFF                     */
/*****************************************************************************/



/*
 * smtp_try()
 *
 * Called by smtp_do_procmsg() to attempt delivery to one SMTP host
 *
 */
void smtp_try(const char *key, const char *addr, int *status,
	      char *dsn, size_t n, long msgnum, char *envelope_from)
{
	int sock = (-1);
	char mxhosts[1024];
	int num_mxhosts;
	int mx;
	int i;
	char user[1024], node[1024], name[1024];
	char buf[1024];
	char mailfrom[1024];
	char mx_user[256];
	char mx_pass[256];
	char mx_host[256];
	char mx_port[256];
	int lp, rp;
	char *msgtext;
	char *ptr;
	size_t msg_size;
	int scan_done;
	
	
	/* Parse out the host portion of the recipient address */
	process_rfc822_addr(addr, user, node, name);

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Attempting delivery to <%s> @ <%s> (%s)\n",
		user, node, name);

	/* Load the message out of the database */
	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputMsg(msgnum, MT_RFC822, HEADERS_ALL, 0, 1, NULL, ESC_DOT);
	msgtext = CC->redirect_buffer;
	msg_size = CC->redirect_len;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	/* If no envelope_from is supplied, extract one from the message */
	if ( (envelope_from == NULL) || (IsEmptyStr(envelope_from)) ) {
		strcpy(mailfrom, "");
		scan_done = 0;
		ptr = msgtext;
		do {
			if (ptr = memreadline(ptr, buf, sizeof buf), *ptr == 0) {
				scan_done = 1;
			}
			if (!strncasecmp(buf, "From:", 5)) {
				safestrncpy(mailfrom, &buf[5], sizeof mailfrom);
				striplt(mailfrom);
				for (i=0; mailfrom[i]; ++i) {
					if (!isprint(mailfrom[i])) {
						strcpy(&mailfrom[i], &mailfrom[i+1]);
						i=0;
					}
				}
	
				/* Strip out parenthesized names */
				lp = (-1);
				rp = (-1);
				for (i=0; mailfrom[i]; ++i) {
					if (mailfrom[i] == '(') lp = i;
					if (mailfrom[i] == ')') rp = i;
				}
				if ((lp>0)&&(rp>lp)) {
					strcpy(&mailfrom[lp-1], &mailfrom[rp+1]);
				}
	
				/* Prefer brokketized names */
				lp = (-1);
				rp = (-1);
				for (i=0; mailfrom[i]; ++i) {
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
		if (IsEmptyStr(mailfrom)) strcpy(mailfrom, "someone@somewhere.org");
		stripallbut(mailfrom, '<', '>');
		envelope_from = mailfrom;
	}

	/* Figure out what mail exchanger host we have to connect to */
	num_mxhosts = getmx(mxhosts, node);
	CtdlLogPrintf(CTDL_DEBUG, "Number of MX hosts for <%s> is %d [%s]\n", node, num_mxhosts, mxhosts);
	if (num_mxhosts < 1) {
		*status = 5;
		snprintf(dsn, SIZ, "No MX hosts found for <%s>", node);
		return;
	}

	sock = (-1);
	for (mx=0; (mx<num_mxhosts && sock < 0); ++mx) {
		char *endpart;
		extract_token(buf, mxhosts, mx, '|', sizeof buf);
		strcpy(mx_user, "");
		strcpy(mx_pass, "");
		if (num_tokens(buf, '@') > 1) {
			strcpy (mx_user, buf);
			endpart = strrchr(mx_user, '@');
			*endpart = '\0';
			strcpy (mx_host, endpart + 1);
			endpart = strrchr(mx_user, ':');
			if (endpart != NULL) {
				strcpy(mx_pass, endpart+1);
				*endpart = '\0';
			}
		}
		else
			strcpy (mx_host, buf);
		endpart = strrchr(mx_host, ':');
		if (endpart != 0){
			*endpart = '\0';
			strcpy(mx_port, endpart + 1);
		}		
		else {
			strcpy(mx_port, "25");
		}
		CtdlLogPrintf(CTDL_DEBUG, "SMTP client: connecting to %s : %s ...\n", mx_host, mx_port);
		sock = sock_connect(mx_host, mx_port, "tcp");
		snprintf(dsn, SIZ, "Could not connect: %s", strerror(errno));
		if (sock >= 0) CtdlLogPrintf(CTDL_DEBUG, "SMTP client: connected!\n");
		if (sock < 0) {
			if (errno > 0) {
				snprintf(dsn, SIZ, "%s", strerror(errno));
			}
			else {
				snprintf(dsn, SIZ, "Unable to connect to %s : %s\n", mx_host, mx_port);
			}
		}
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
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
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

	/* Do a EHLO command.  If it fails, try the HELO command. */
	snprintf(buf, sizeof buf, "EHLO %s\r\n", config.c_fqdn);
	CtdlLogPrintf(CTDL_DEBUG, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP HELO");
		goto bail;
	}
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (buf[0] != '2') {
		snprintf(buf, sizeof buf, "HELO %s\r\n", config.c_fqdn);
		CtdlLogPrintf(CTDL_DEBUG, ">%s", buf);
		sock_write(sock, buf, strlen(buf));
		if (ml_sock_gets(sock, buf) < 0) {
			*status = 4;
			strcpy(dsn, "Connection broken during SMTP HELO");
			goto bail;
		}
	}
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

	/* Do an AUTH command if necessary */
	if (!IsEmptyStr(mx_user)) {
		char encoded[1024];
		sprintf(buf, "%s%c%s%c%s", mx_user, '\0', mx_user, '\0', mx_pass);
		CtdlEncodeBase64(encoded, buf, strlen(mx_user) + strlen(mx_user) + strlen(mx_pass) + 2, 0);
		snprintf(buf, sizeof buf, "AUTH PLAIN %s\r\n", encoded);
		CtdlLogPrintf(CTDL_DEBUG, ">%s", buf);
		sock_write(sock, buf, strlen(buf));
		if (ml_sock_gets(sock, buf) < 0) {
			*status = 4;
			strcpy(dsn, "Connection broken during SMTP AUTH");
			goto bail;
		}
		CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
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
	}

	/* previous command succeeded, now try the MAIL FROM: command */
	snprintf(buf, sizeof buf, "MAIL FROM:<%s>\r\n", envelope_from);
	CtdlLogPrintf(CTDL_DEBUG, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP MAIL");
		goto bail;
	}
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
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
	snprintf(buf, sizeof buf, "RCPT TO:<%s@%s>\r\n", user, node);
	CtdlLogPrintf(CTDL_DEBUG, ">%s", buf);
	sock_write(sock, buf, strlen(buf));
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP RCPT");
		goto bail;
	}
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
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
	CtdlLogPrintf(CTDL_DEBUG, ">DATA\n");
	sock_write(sock, "DATA\r\n", 6);
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP DATA");
		goto bail;
	}
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
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

	/* If we reach this point, the server is expecting data.*/
	sock_write(sock, msgtext, msg_size);
	if (msgtext[msg_size-1] != 10) {
		CtdlLogPrintf(CTDL_WARNING, "Possible problem: message did not "
			"correctly terminate. (expecting 0x10, got 0x%02x)\n",
				buf[msg_size-1]);
	}

	sock_write(sock, ".\r\n", 3);
	if (ml_sock_gets(sock, buf) < 0) {
		*status = 4;
		strcpy(dsn, "Connection broken during SMTP message transmit");
		goto bail;
	}
	CtdlLogPrintf(CTDL_DEBUG, "%s\n", buf);
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

	CtdlLogPrintf(CTDL_DEBUG, ">QUIT\n");
	sock_write(sock, "QUIT\r\n", 6);
	ml_sock_gets(sock, buf);
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	CtdlLogPrintf(CTDL_INFO, "SMTP client: delivery to <%s> @ <%s> (%s) succeeded\n",
		user, node, name);

bail:	free(msgtext);
	sock_close(sock);

	/* Write something to the syslog (which may or may not be where the
	 * rest of the Citadel logs are going; some sysadmins want LOG_MAIL).
	 */
	if (enable_syslog) {
		syslog((LOG_MAIL | LOG_INFO),
			"%ld: to=<%s>, relay=%s, stat=%s",
			msgnum,
			addr,
			mx_host,
			dsn
		);
	}

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
	char boundary[64];
	int num_bounces = 0;
	int bounce_this = 0;
	long bounce_msgid = (-1);
	time_t submitted = 0L;
	struct CtdlMessage *bmsg = NULL;
	int give_up = 0;
	struct recptypes *valid;
	int successful_bounce = 0;
	static int seq = 0;
	char *omsgtext;
	size_t omsgsize;
	long omsgid = (-1);

	CtdlLogPrintf(CTDL_DEBUG, "smtp_do_bounce() called\n");
	strcpy(bounceto, "");
	sprintf(boundary, "=_Citadel_Multipart_%s_%04x%04x", config.c_fqdn, getpid(), ++seq);
	lines = num_tokens(instr, '\n');

	/* See if it's time to give up on delivery of this message */
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		if (!strcasecmp(key, "submitted")) {
			submitted = atol(addr);
		}
	}

	if ( (time(NULL) - submitted) > SMTP_GIVE_UP ) {
		give_up = 1;
	}

	/* Start building our bounce message */

	bmsg = (struct CtdlMessage *) malloc(sizeof(struct CtdlMessage));
	if (bmsg == NULL) return;
	memset(bmsg, 0, sizeof(struct CtdlMessage));

        bmsg->cm_magic = CTDLMESSAGE_MAGIC;
        bmsg->cm_anon_type = MES_NORMAL;
        bmsg->cm_format_type = FMT_RFC822;
        bmsg->cm_fields['A'] = strdup("Citadel");
        bmsg->cm_fields['O'] = strdup(MAILROOM);
        bmsg->cm_fields['N'] = strdup(config.c_nodename);
        bmsg->cm_fields['U'] = strdup("Delivery Status Notification (Failure)");
	bmsg->cm_fields['M'] = malloc(1024);

        strcpy(bmsg->cm_fields['M'], "Content-type: multipart/mixed; boundary=\"");
        strcat(bmsg->cm_fields['M'], boundary);
        strcat(bmsg->cm_fields['M'], "\"\r\n");
        strcat(bmsg->cm_fields['M'], "MIME-Version: 1.0\r\n");
        strcat(bmsg->cm_fields['M'], "X-Mailer: " CITADEL "\r\n");
        strcat(bmsg->cm_fields['M'], "\r\nThis is a multipart message in MIME format.\r\n\r\n");
        strcat(bmsg->cm_fields['M'], "--");
        strcat(bmsg->cm_fields['M'], boundary);
        strcat(bmsg->cm_fields['M'], "\r\n");
        strcat(bmsg->cm_fields['M'], "Content-type: text/plain\r\n\r\n");

	if (give_up) strcat(bmsg->cm_fields['M'],
"A message you sent could not be delivered to some or all of its recipients\n"
"due to prolonged unavailability of its destination(s).\n"
"Giving up on the following addresses:\n\n"
);

        else strcat(bmsg->cm_fields['M'],
"A message you sent could not be delivered to some or all of its recipients.\n"
"The following addresses were undeliverable:\n\n"
);

	/*
	 * Now go through the instructions checking for stuff.
	 */
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		extract_token(dsn, buf, 3, '|', sizeof dsn);
		bounce_this = 0;

		CtdlLogPrintf(CTDL_DEBUG, "key=<%s> addr=<%s> status=%d dsn=<%s>\n",
			key, addr, status, dsn);

		if (!strcasecmp(key, "bounceto")) {
			strcpy(bounceto, addr);
		}

		if (!strcasecmp(key, "msgid")) {
			omsgid = atol(addr);
		}

		if (!strcasecmp(key, "remote")) {
			if (status == 5) bounce_this = 1;
			if (give_up) bounce_this = 1;
		}

		if (bounce_this) {
			++num_bounces;

			if (bmsg->cm_fields['M'] == NULL) {
				CtdlLogPrintf(CTDL_ERR, "ERROR ... M field is null "
					"(%s:%d)\n", __FILE__, __LINE__);
			}

			bmsg->cm_fields['M'] = realloc(bmsg->cm_fields['M'],
				strlen(bmsg->cm_fields['M']) + 1024 );
			strcat(bmsg->cm_fields['M'], addr);
			strcat(bmsg->cm_fields['M'], ": ");
			strcat(bmsg->cm_fields['M'], dsn);
			strcat(bmsg->cm_fields['M'], "\r\n");

			remove_token(instr, i, '\n');
			--i;
			--lines;
		}
	}

	/* Attach the original message */
	if (omsgid >= 0) {
        	strcat(bmsg->cm_fields['M'], "--");
        	strcat(bmsg->cm_fields['M'], boundary);
        	strcat(bmsg->cm_fields['M'], "\r\n");
        	strcat(bmsg->cm_fields['M'], "Content-type: message/rfc822\r\n");
        	strcat(bmsg->cm_fields['M'], "Content-Transfer-Encoding: 7bit\r\n");
        	strcat(bmsg->cm_fields['M'], "Content-Disposition: inline\r\n");
        	strcat(bmsg->cm_fields['M'], "\r\n");
	
		CC->redirect_buffer = malloc(SIZ);
		CC->redirect_len = 0;
		CC->redirect_alloc = SIZ;
		CtdlOutputMsg(omsgid, MT_RFC822, HEADERS_ALL, 0, 1, NULL, 0);
		omsgtext = CC->redirect_buffer;
		omsgsize = CC->redirect_len;
		CC->redirect_buffer = NULL;
		CC->redirect_len = 0;
		CC->redirect_alloc = 0;
		bmsg->cm_fields['M'] = realloc(bmsg->cm_fields['M'],
				(strlen(bmsg->cm_fields['M']) + omsgsize + 1024) );
		strcat(bmsg->cm_fields['M'], omsgtext);
		free(omsgtext);
	}

	/* Close the multipart MIME scope */
        strcat(bmsg->cm_fields['M'], "--");
        strcat(bmsg->cm_fields['M'], boundary);
        strcat(bmsg->cm_fields['M'], "--\r\n");

	/* Deliver the bounce if there's anything worth mentioning */
	CtdlLogPrintf(CTDL_DEBUG, "num_bounces = %d\n", num_bounces);
	if (num_bounces > 0) {

		/* First try the user who sent the message */
		CtdlLogPrintf(CTDL_DEBUG, "bounce to user? <%s>\n", bounceto);
		if (IsEmptyStr(bounceto)) {
			CtdlLogPrintf(CTDL_ERR, "No bounce address specified\n");
			bounce_msgid = (-1L);
		}

		/* Can we deliver the bounce to the original sender? */
		valid = validate_recipients(bounceto, smtp_get_Recipients (), 0);
		if (valid != NULL) {
			if (valid->num_error == 0) {
				CtdlSubmitMsg(bmsg, valid, "", QP_EADDR);
				successful_bounce = 1;
			}
		}

		/* If not, post it in the Aide> room */
		if (successful_bounce == 0) {
			CtdlSubmitMsg(bmsg, NULL, config.c_aideroom, QP_EADDR);
		}

		/* Free up the memory we used */
		if (valid != NULL) {
			free_recipients(valid);
		}
	}

	CtdlFreeMessage(bmsg);
	CtdlLogPrintf(CTDL_DEBUG, "Done processing bounces\n");
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
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		extract_token(dsn, buf, 3, '|', sizeof dsn);

		completed = 0;

		if (!strcasecmp(key, "remote")) {
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
	struct CtdlMessage *msg = NULL;
	char *instr = NULL;
	char *results = NULL;
	int i;
	int lines;
	int status;
	char buf[1024];
	char key[1024];
	char addr[1024];
	char dsn[1024];
	char envelope_from[1024];
	long text_msgid = (-1);
	int incomplete_deliveries_remaining;
	time_t attempted = 0L;
	time_t last_attempted = 0L;
	time_t retry = SMTP_RETRY_INTERVAL;

	CtdlLogPrintf(CTDL_DEBUG, "SMTP client: smtp_do_procmsg(%ld)\n", msgnum);
	strcpy(envelope_from, "");

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		CtdlLogPrintf(CTDL_ERR, "SMTP client: tried %ld but no such message!\n", msgnum);
		return;
	}

	instr = strdup(msg->cm_fields['M']);
	CtdlFreeMessage(msg);

	/* Strip out the headers amd any other non-instruction line */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		if (num_tokens(buf, '|') < 2) {
			remove_token(instr, i, '\n');
			--lines;
			--i;
		}
	}

	/* Learn the message ID and find out about recent delivery attempts */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		if (!strcasecmp(key, "msgid")) {
			text_msgid = extract_long(buf, 1);
		}
		if (!strcasecmp(key, "envelope_from")) {
			extract_token(envelope_from, buf, 1, '|', sizeof envelope_from);
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
		CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Retry time not yet reached.\n");
		free(instr);
		return;
	}


	/*
	 * Bail out if there's no actual message associated with this
	 */
	if (text_msgid < 0L) {
		CtdlLogPrintf(CTDL_ERR, "SMTP client: no 'msgid' directive found!\n");
		free(instr);
		return;
	}

	/* Plow through the instructions looking for 'remote' directives and
	 * a status of 0 (no delivery yet attempted) or 3/4 (transient errors
	 * were experienced and it's time to try again)
	 */
	lines = num_tokens(instr, '\n');
	for (i=0; i<lines; ++i) {
		extract_token(buf, instr, i, '\n', sizeof buf);
		extract_token(key, buf, 0, '|', sizeof key);
		extract_token(addr, buf, 1, '|', sizeof addr);
		status = extract_int(buf, 2);
		extract_token(dsn, buf, 3, '|', sizeof dsn);
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
			CtdlLogPrintf(CTDL_DEBUG, "SMTP client: Trying <%s>\n", addr);
			smtp_try(key, addr, &status, dsn, sizeof dsn, text_msgid, envelope_from);
			if (status != 2) {
				if (results == NULL) {
					results = malloc(1024);
					memset(results, 0, 1024);
				}
				else {
					results = realloc(results, strlen(results) + 1024);
				}
				snprintf(&results[strlen(results)], 1024,
					"%s|%s|%d|%s\n",
					key, addr, status, dsn);
			}
		}
	}

	if (results != NULL) {
		instr = realloc(instr, strlen(instr) + strlen(results) + 2);
		strcat(instr, results);
		free(results);
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
		long delmsgs[2];
		delmsgs[0] = msgnum;
		delmsgs[1] = text_msgid;
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, delmsgs, 2, "");
	}

	/*
	 * Uncompleted delivery instructions remain, so delete the old
	 * instructions and replace with the updated ones.
	 */
	if (incomplete_deliveries_remaining > 0) {
		CtdlDeleteMessages(SMTP_SPOOLOUT_ROOM, &msgnum, 1, "");
        	msg = malloc(sizeof(struct CtdlMessage));
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
		CtdlSubmitMsg(msg, NULL, SMTP_SPOOLOUT_ROOM, QP_EADDR);
		CtdlFreeMessage(msg);
	}

	free(instr);
}




/*
 * smtp_do_queue()
 * 
 * Run through the queue sending out messages.
 */
void *smtp_do_queue(void *arg) {
	int num_processed = 0;
	struct CitContext smtp_queue_CC;

	CtdlLogPrintf(CTDL_INFO, "SMTP client: processing outbound queue\n");

	CtdlFillSystemContext(&smtp_queue_CC, "SMTP Send");
	citthread_setspecific(MyConKey, (void *)&smtp_queue_CC );

	if (CtdlGetRoom(&CC->room, SMTP_SPOOLOUT_ROOM) != 0) {
		CtdlLogPrintf(CTDL_ERR, "Cannot find room <%s>\n", SMTP_SPOOLOUT_ROOM);
	}
	else {
		num_processed = CtdlForEachMessage(MSGS_ALL, 0L, NULL, SPOOLMIME, NULL, smtp_do_procmsg, NULL);
	}

	citthread_mutex_unlock (&smtp_send_lock);
	CtdlLogPrintf(CTDL_INFO, "SMTP client: queue run completed; %d messages processed\n", num_processed);
	return(NULL);
}



/*
 * smtp_queue_thread
 *
 * Create a thread to run the SMTP queue
 *
 * This was created as a response to a situation seen on Uncensored where a bad remote was holding
 * up SMTP sending for long times.
 * Converting to a thread does not fix the problem caused by the bad remote but it does prevent
 * the SMTP sending from stopping housekeeping and the EVT_TIMER event system which in turn prevented
 * other things from happening.
 */
void smtp_queue_thread (void)
{
	if (citthread_mutex_trylock (&smtp_send_lock)) {
		CtdlLogPrintf(CTDL_DEBUG, "SMTP queue run already in progress\n");
	}
	else {
		CtdlThreadCreate("SMTP Send", CTDLTHREAD_BIGSTACK, smtp_do_queue, NULL);
	}
}



void smtp_server_going_down (void)
{
	CtdlLogPrintf(CTDL_DEBUG, "SMTP module clean up for shutdown.\n");

	citthread_mutex_destroy (&smtp_send_lock);
}



/*****************************************************************************/
/*                          SMTP UTILITY COMMANDS                            */
/*****************************************************************************/

void cmd_smtp(char *argbuf) {
	char cmd[64];
	char node[256];
	char buf[1024];
	int i;
	int num_mxhosts;

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);

	if (!strcasecmp(cmd, "mx")) {
		extract_token(node, argbuf, 1, '|', sizeof node);
		num_mxhosts = getmx(buf, node);
		cprintf("%d %d MX hosts listed for %s\n",
			LISTING_FOLLOWS, num_mxhosts, node);
		for (i=0; i<num_mxhosts; ++i) {
			extract_token(node, buf, i, '|', sizeof node);
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
		cprintf("%d Invalid command.\n", ERROR + ILLEGAL_VALUE);
	}

}


/*
 * Initialize the SMTP outbound queue
 */
void smtp_init_spoolout(void) {
	struct ctdlroom qrbuf;

	/*
	 * Create the room.  This will silently fail if the room already
	 * exists, and that's perfectly ok, because we want it to exist.
	 */
	CtdlCreateRoom(SMTP_SPOOLOUT_ROOM, 3, "", 0, 1, 0, VIEW_MAILBOX);

	/*
	 * Make sure it's set to be a "system room" so it doesn't show up
	 * in the <K>nown rooms list for Aides.
	 */
	if (CtdlGetRoomLock(&qrbuf, SMTP_SPOOLOUT_ROOM) == 0) {
		qrbuf.QRflags2 |= QR2_SYSTEM;
		CtdlPutRoomLock(&qrbuf);
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

	CtdlLogPrintf(CTDL_DEBUG, "Performing SMTP cleanup hook\n");
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

		smtp_init_spoolout();
		CtdlRegisterSessionHook(smtp_queue_thread, EVT_TIMER);
		CtdlRegisterSessionHook(smtp_cleanup_function, EVT_STOP);
		CtdlRegisterProtoHook(cmd_smtp, "SMTP", "SMTP utility commands");
		CtdlRegisterCleanupHook (smtp_server_going_down);
		citthread_mutex_init (&smtp_send_lock, NULL);
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
