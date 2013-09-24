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
 * Copyright (c) 1998-2013 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *  
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
#include "room_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"
#include "clientsocket.h"
#include "locate_host.h"
#include "citadel_dirs.h"
#include "ctdl_module.h"

#include "smtp_util.h"
enum {				/* Command states for login authentication */
	smtp_command,
	smtp_user,
	smtp_password,
	smtp_plain
};

enum SMTP_FLAGS {
	HELO,
	EHLO,
	LHLO
};

typedef void (*smtp_handler)(long offest, long Flags);

typedef struct _smtp_handler_hook {
	smtp_handler h;
	int Flags;
} smtp_handler_hook;

HashList *SMTPCmds = NULL;
#define MaxSMTPCmdLen 10

#define RegisterSmtpCMD(First, H, Flags) \
	registerSmtpCMD(HKEY(First), H, Flags)
void registerSmtpCMD(const char *First, long FLen, 
		     smtp_handler H,
		     int Flags)
{
	smtp_handler_hook *h;

	if (FLen >= MaxSMTPCmdLen)
		cit_panic_backtrace (0);

	h = (smtp_handler_hook*) malloc(sizeof(smtp_handler_hook));
	memset(h, 0, sizeof(smtp_handler_hook));

	h->Flags = Flags;
	h->h = H;
	Put(SMTPCmds, First, FLen, h, NULL);
}

void smtp_cleanup(void)
{
	DeleteHash(&SMTPCmds);
}

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
	sSMTP->Cmd = NewStrBufPlain(NULL, SIZ);
	sSMTP->helo_node = NewStrBuf();
	sSMTP->from = NewStrBufPlain(NULL, SIZ);
	sSMTP->recipients = NewStrBufPlain(NULL, SIZ);
	sSMTP->OneRcpt = NewStrBufPlain(NULL, SIZ);
	sSMTP->preferred_sender_email = NULL;
	sSMTP->preferred_sender_name = NULL;

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
void smtp_auth_greeting(long offset, long Flags) {
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
void smtp_hello(long offset, long which_command)
{
	citsmtp *sSMTP = SMTP;

	StrBufAppendBuf (sSMTP->helo_node, sSMTP->Cmd, offset);

	if ( (which_command != LHLO) && (sSMTP->is_lmtp) ) {
		cprintf("500 Only LHLO is allowed when running LMTP\r\n");
		return;
	}

	if ( (which_command == LHLO) && (sSMTP->is_lmtp == 0) ) {
		cprintf("500 LHLO is only allowed when running LMTP\r\n");
		return;
	}

	if (which_command == HELO) {
		cprintf("250 Hello %s (%s [%s])\r\n",
			ChrPtr(sSMTP->helo_node),
			CC->cs_host,
			CC->cs_addr
		);
	}
	else {
		if (which_command == EHLO) {
			cprintf("250-Hello %s (%s [%s])\r\n",
				ChrPtr(sSMTP->helo_node),
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
 * Backend function for smtp_webcit_preferences_hack().
 * Look at a message and determine if it's the preferences file.
 */
void smtp_webcit_preferences_hack_backend(long msgnum, void *userdata) {
	struct CtdlMessage *msg;
	char **webcit_conf = (char **) userdata;

	if (*webcit_conf) {
		return;	// already got it
	}

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		return;
	}

	if ( !CM_IsEmpty(msg, eMsgSubject) &&
	     (!strcasecmp(msg->cm_fields[eMsgSubject], "__ WebCit Preferences __")))
	{
		/* This is it!  Change ownership of the message text so it doesn't get freed. */
		*webcit_conf = (char *)msg->cm_fields[eMesageText];
		msg->cm_fields[eMesageText] = NULL;
	}
	CM_Free(msg);
}


/*
 * The configuration item for the user's preferred display name for outgoing email is, unfortunately,
 * stored in the account's WebCit configuration.  We have to fetch it now.
 */
void smtp_webcit_preferences_hack(void) {
	char config_roomname[ROOMNAMELEN];
	char *webcit_conf = NULL;
	citsmtp *sSMTP = SMTP;

	snprintf(config_roomname, sizeof config_roomname, "%010ld.%s", CC->user.usernum, USERCONFIGROOM);
	if (CtdlGetRoom(&CC->room, config_roomname) != 0) {
		return;
	}

	/*
	 * Find the WebCit configuration message
	 */

	CtdlForEachMessage(MSGS_ALL, 1, NULL, NULL, NULL, smtp_webcit_preferences_hack_backend, (void *)&webcit_conf);

	if (!webcit_conf) {
		return;
	}

	/* Parse the webcit configuration and attempt to do something useful with it */
	char *str = webcit_conf;
	char *saveptr = str;
	char *this_line = NULL;
	while (this_line = strtok_r(str, "\n", &saveptr), this_line != NULL) {
		str = NULL;
		if (!strncasecmp(this_line, "defaultfrom|", 12)) {
			sSMTP->preferred_sender_email = NewStrBufPlain(&this_line[12], -1);
		}
		if (!strncasecmp(this_line, "defaultname|", 12)) {
			sSMTP->preferred_sender_name = NewStrBufPlain(&this_line[12], -1);
		}
		if ((!strncasecmp(this_line, "defaultname|", 12)) && (sSMTP->preferred_sender_name == NULL)) {
			sSMTP->preferred_sender_name = NewStrBufPlain(&this_line[12], -1);
		}

	}
	free(webcit_conf);
}



/*
 * Implement HELP command.
 */
void smtp_help(long offset, long Flags) {
	cprintf("214 RTFM http://www.ietf.org/rfc/rfc2821.txt\r\n");
}


/*
 *
 */
void smtp_get_user(long offset)
{
	char buf[SIZ];
	char username[SIZ];
	citsmtp *sSMTP = SMTP;

	CtdlDecodeBase64(username, ChrPtr(sSMTP->Cmd) + offset, SIZ);
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
void smtp_get_pass(long offset, long Flags)
{
	citsmtp *sSMTP = SMTP;
	char password[SIZ];
	long len;

	memset(password, 0, sizeof(password));	
	len = CtdlDecodeBase64(password, ChrPtr(sSMTP->Cmd), SIZ);
	/* syslog(LOG_DEBUG, "Trying <%s>\n", password); */
	if (CtdlTryPassword(password, len) == pass_ok) {
		smtp_auth_greeting(offset, Flags);
	}
	else {
		cprintf("535 Authentication failed.\r\n");
	}
	sSMTP->command_state = smtp_command;
}


/*
 * Back end for PLAIN auth method (either inline or multistate)
 */
void smtp_try_plain(long offset, long Flags)
{
	citsmtp *sSMTP = SMTP;
	char decoded_authstring[1024];
	char ident[256];
	char user[256];
	char pass[256];
	int result;
	long len;

	CtdlDecodeBase64(decoded_authstring, ChrPtr(sSMTP->Cmd), StrLength(sSMTP->Cmd));
	safestrncpy(ident, decoded_authstring, sizeof ident);
	safestrncpy(user, &decoded_authstring[strlen(ident) + 1], sizeof user);
	len = safestrncpy(pass, &decoded_authstring[strlen(ident) + strlen(user) + 2], sizeof pass);
	if (len == -1)
		len = sizeof(pass) - 1;

	sSMTP->command_state = smtp_command;

	if (!IsEmptyStr(ident)) {
		result = CtdlLoginExistingUser(user, ident);
	}
	else {
		result = CtdlLoginExistingUser(NULL, user);
	}

	if (result == login_ok) {
		if (CtdlTryPassword(pass, len) == pass_ok) {
			smtp_webcit_preferences_hack();
			smtp_auth_greeting(offset, Flags);
			return;
		}
	}
	cprintf("504 Authentication failed.\r\n");
}


/*
 * Attempt to perform authenticated SMTP
 */
void smtp_auth(long offset, long Flags)
{
	citsmtp *sSMTP = SMTP;
	char username_prompt[64];
	char method[64];
	char encoded_authstring[1024];

	if (CC->logged_in) {
		cprintf("504 Already logged in.\r\n");
		return;
	}

	extract_token(method, ChrPtr(sSMTP->Cmd) + offset, 0, ' ', sizeof method);

	if (!strncasecmp(method, "login", 5) ) {
		if (StrLength(sSMTP->Cmd) - offset >= 7) {
			smtp_get_user(6);
		}
		else {
			CtdlEncodeBase64(username_prompt, "Username:", 9, 0);
			cprintf("334 %s\r\n", username_prompt);
			sSMTP->command_state = smtp_user;
		}
		return;
	}

	if (!strncasecmp(method, "plain", 5) ) {
		long len;
		if (num_tokens(ChrPtr(sSMTP->Cmd) + offset, ' ') < 2) {
			cprintf("334 \r\n");
			SMTP->command_state = smtp_plain;
			return;
		}

		len = extract_token(encoded_authstring, 
				    ChrPtr(sSMTP->Cmd) + offset,
				    1, ' ',
				    sizeof encoded_authstring);
		StrBufPlain(sSMTP->Cmd, encoded_authstring, len);
		smtp_try_plain(0, Flags);
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
void smtp_rset(long offset, long do_response) {
	citsmtp *sSMTP = SMTP;

	/*
	 * Our entire SMTP state is discarded when a RSET command is issued,
	 * but we need to preserve this one little piece of information, so
	 * we save it for later.
	 */

	FlushStrBuf(sSMTP->Cmd);
	FlushStrBuf(sSMTP->helo_node);
	FlushStrBuf(sSMTP->from);
	FlushStrBuf(sSMTP->recipients);
	FlushStrBuf(sSMTP->OneRcpt);

	sSMTP->command_state = 0;
	sSMTP->number_of_recipients = 0;
	sSMTP->delivery_mode = 0;
	sSMTP->message_originated_locally = 0;
	sSMTP->is_msa = 0;
	/*
	 * we must remember is_lmtp & is_unfiltered.
	 */

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

	if (do_response) {
		cprintf("250 Zap!\r\n");
	}
}

/*
 * Clear out the portions of the state buffer that need to be cleared out
 * after the DATA command finishes.
 */
void smtp_data_clear(long offset, long flags)
{
	citsmtp *sSMTP = SMTP;

	FlushStrBuf(sSMTP->from);
	FlushStrBuf(sSMTP->recipients);
	FlushStrBuf(sSMTP->OneRcpt);
	sSMTP->number_of_recipients = 0;
	sSMTP->delivery_mode = 0;
	sSMTP->message_originated_locally = 0;
}

/*
 * Implements the "MAIL FROM:" command
 */
void smtp_mail(long offset, long flags) {
	char user[SIZ];
	char node[SIZ];
	char name[SIZ];
	citsmtp *sSMTP = SMTP;

	if (StrLength(sSMTP->from) > 0) {
		cprintf("503 Only one sender permitted\r\n");
		return;
	}

	if (strncasecmp(ChrPtr(sSMTP->Cmd) + offset, "From:", 5)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	StrBufAppendBuf(sSMTP->from, sSMTP->Cmd, offset);
	StrBufTrim(sSMTP->from);
	if (strchr(ChrPtr(sSMTP->from), '<') != NULL) {
		StrBufStripAllBut(sSMTP->from, '<', '>');
	}

	/* We used to reject empty sender names, until it was brought to our
	 * attention that RFC1123 5.2.9 requires that this be allowed.  So now
	 * we allow it, but replace the empty string with a fake
	 * address so we don't have to contend with the empty string causing
	 * other code to fail when it's expecting something there.
	 */
	if (StrLength(sSMTP->from)) {
		StrBufPlain(sSMTP->from, HKEY("someone@example.com"));
	}

	/* If this SMTP connection is from a logged-in user, force the 'from'
	 * to be the user's Internet e-mail address as Citadel knows it.
	 */
	if (CC->logged_in) {
		StrBufPlain(sSMTP->from, CC->cs_inet_email, -1);
		cprintf("250 Sender ok <%s>\r\n", ChrPtr(sSMTP->from));
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
		process_rfc822_addr(ChrPtr(sSMTP->from), user, node, name);
		syslog(LOG_DEBUG, "Claimed envelope sender is '%s' == '%s' @ '%s' ('%s')",
			ChrPtr(sSMTP->from), user, node, name
		);
		if (CtdlHostAlias(node) != hostalias_nomatch) {
			cprintf("550 You must log in to send mail from %s\r\n", node);
			FlushStrBuf(sSMTP->from);
			syslog(LOG_DEBUG, "Rejecting unauthenticated mail from %s", node);
			return;
		}
	}

	cprintf("250 Sender ok\r\n");
}



/*
 * Implements the "RCPT To:" command
 */
void smtp_rcpt(long offset, long flags)
{
	struct CitContext *CCC = CC;
	char message_to_spammer[SIZ];
	recptypes *valid = NULL;
	citsmtp *sSMTP = SMTP;

	if (StrLength(sSMTP->from) == 0) {
		cprintf("503 Need MAIL before RCPT\r\n");
		return;
	}
	
	if (strncasecmp(ChrPtr(sSMTP->Cmd) + offset, "To:", 3)) {
		cprintf("501 Syntax error\r\n");
		return;
	}

	if ( (sSMTP->is_msa) && (!CCC->logged_in) ) {
		cprintf("550 You must log in to send mail on this port.\r\n");
		FlushStrBuf(sSMTP->from);
		return;
	}
	FlushStrBuf(sSMTP->OneRcpt);
	StrBufAppendBuf(sSMTP->OneRcpt, sSMTP->Cmd, offset + 3);
	StrBufTrim(sSMTP->OneRcpt);
	StrBufStripAllBut(sSMTP->OneRcpt, '<', '>');

	if ( (StrLength(sSMTP->OneRcpt) + StrLength(sSMTP->recipients)) >= SIZ) {
		cprintf("452 Too many recipients\r\n");
		return;
	}

	/* RBL check */
	if ( (!CCC->logged_in)	/* Don't RBL authenticated users */
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
		ChrPtr(sSMTP->OneRcpt), 
		smtp_get_Recipients(),
		(sSMTP->is_lmtp)? POST_LMTP: (CCC->logged_in)? POST_LOGGED_IN: POST_EXTERNAL
	);
	if (valid->num_error != 0) {
		cprintf("550 %s\r\n", valid->errormsg);
		free_recipients(valid);
		return;
	}

	if (valid->num_internet > 0) {
		if (CCC->logged_in) {
                        if (CtdlCheckInternetMailPermission(&CCC->user)==0) {
				cprintf("551 <%s> - you do not have permission to send Internet mail\r\n", 
					ChrPtr(sSMTP->OneRcpt));
                                free_recipients(valid);
                                return;
                        }
                }
	}

	if (valid->num_internet > 0) {
		if ( (sSMTP->message_originated_locally == 0)
		   && (sSMTP->is_lmtp == 0) ) {
			cprintf("551 <%s> - relaying denied\r\n", ChrPtr(sSMTP->OneRcpt));
			free_recipients(valid);
			return;
		}
	}

	cprintf("250 RCPT ok <%s>\r\n", ChrPtr(sSMTP->OneRcpt));
	if (StrLength(sSMTP->recipients) > 0) {
		StrBufAppendBufPlain(sSMTP->recipients, HKEY(","), 0);
	}
	StrBufAppendBuf(sSMTP->recipients, sSMTP->OneRcpt, 0);
	sSMTP->number_of_recipients ++;
	if (valid != NULL)  {
		free_recipients(valid);
	}
}




/*
 * Implements the DATA command
 */
void smtp_data(long offset, long flags)
{
	struct CitContext *CCC = CC;
	StrBuf *body;
	StrBuf *defbody; 
	struct CtdlMessage *msg = NULL;
	long msgnum = (-1L);
	char nowstamp[SIZ];
	recptypes *valid;
	int scan_errors;
	int i;
	citsmtp *sSMTP = SMTP;

	if (StrLength(sSMTP->from) == 0) {
		cprintf("503 Need MAIL command first.\r\n");
		return;
	}

	if (sSMTP->number_of_recipients < 1) {
		cprintf("503 Need RCPT command first.\r\n");
		return;
	}

	cprintf("354 Transmit message now - terminate with '.' by itself\r\n");
	
	datestring(nowstamp, sizeof nowstamp, time(NULL), DATESTRING_RFC822);
	defbody = NewStrBufPlain(NULL, SIZ);

	if (defbody != NULL) {
		if (sSMTP->is_lmtp && (CCC->cs_UDSclientUID != -1)) {
			StrBufPrintf(
				defbody,
				"Received: from %s (Citadel from userid %ld)\n"
				"	by %s; %s\n",
				ChrPtr(sSMTP->helo_node),
				(long int) CCC->cs_UDSclientUID,
				config.c_fqdn,
				nowstamp);
		}
		else {
			StrBufPrintf(
				defbody,
				"Received: from %s (%s [%s])\n"
				"	by %s; %s\n",
				ChrPtr(sSMTP->helo_node),
				CCC->cs_host,
				CCC->cs_addr,
				config.c_fqdn,
				nowstamp);
		}
	}
	body = CtdlReadMessageBodyBuf(HKEY("."), config.c_maxmsglen, defbody, 1, NULL);
	FreeStrBuf(&defbody);
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
	if ( (CCC->logged_in) && (config.c_rfc822_strict_from != CFG_SMTP_FROM_NOFILTER) ) {
		int validemail = 0;
		
		if (!CM_IsEmpty(msg, erFc822Addr)       &&
		    ((config.c_rfc822_strict_from == CFG_SMTP_FROM_CORRECT) || 
		     (config.c_rfc822_strict_from == CFG_SMTP_FROM_REJECT)    )  )
		{
			if (!IsEmptyStr(CCC->cs_inet_email))
				validemail = strcmp(CCC->cs_inet_email, msg->cm_fields[erFc822Addr]) == 0;
			if ((!validemail) && 
			    (!IsEmptyStr(CCC->cs_inet_other_emails)))
			{
				int num_secondary_emails = 0;
				int i;
				num_secondary_emails = num_tokens(CCC->cs_inet_other_emails, '|');
				for (i=0; i < num_secondary_emails && !validemail; ++i) {
					char buf[256];
					extract_token(buf, CCC->cs_inet_other_emails,i,'|',sizeof CCC->cs_inet_other_emails);
					validemail = strcmp(buf, msg->cm_fields[erFc822Addr]) == 0;
				}
			}
		}

		if (!validemail && (config.c_rfc822_strict_from == CFG_SMTP_FROM_REJECT)) {
			syslog(LOG_ERR, "invalid sender '%s' - rejecting this message", msg->cm_fields[erFc822Addr]);
			cprintf("550 Invalid sender '%s' - rejecting this message.\r\n", msg->cm_fields[erFc822Addr]);
			return;
		}

		CM_SetField(msg, eNodeName, config.c_nodename, strlen(config.c_nodename));
		CM_SetField(msg, eHumanNode, config.c_humannode, strlen(config.c_humannode));
        	CM_SetField(msg, eOriginalRoom, HKEY(MAILROOM));
		if (sSMTP->preferred_sender_name != NULL)
			CM_SetField(msg, eAuthor, SKEY(sSMTP->preferred_sender_name));
		else 
			CM_SetField(msg, eAuthor, CCC->user.fullname, strlen(CCC->user.fullname));

		if (!validemail) {
			if (sSMTP->preferred_sender_email != NULL)
				CM_SetField(msg, erFc822Addr, SKEY(sSMTP->preferred_sender_email));
			else
				CM_SetField(msg, erFc822Addr, CCC->cs_inet_email, strlen(CCC->cs_inet_email));
		}
	}

	/* Set the "envelope from" address */
	CM_SetField(msg, eMessagePath, SKEY(sSMTP->from));

	/* Set the "envelope to" address */
	CM_SetField(msg, eenVelopeTo, SKEY(sSMTP->recipients));

	/* Submit the message into the Citadel system. */
	valid = validate_recipients(
		ChrPtr(sSMTP->recipients),
		smtp_get_Recipients(),
		(sSMTP->is_lmtp)? POST_LMTP: (CCC->logged_in)? POST_LOGGED_IN: POST_EXTERNAL
	);

	/* If there are modules that want to scan this message before final
	 * submission (such as virus checkers or spam filters), call them now
	 * and give them an opportunity to reject the message.
	 */
	if (sSMTP->is_unfiltered) {
		scan_errors = 0;
	}
	else {
		scan_errors = PerformMessageHooks(msg, valid, EVT_SMTPSCAN);
	}

	if (scan_errors > 0) {	/* We don't want this message! */

		if (CM_IsEmpty(msg, eErrorMsg)) {
			CM_SetField(msg, eErrorMsg, HKEY("Message rejected by filter"));
		}

		StrBufPrintf(sSMTP->OneRcpt, "550 %s\r\n", msg->cm_fields[eErrorMsg]);
	}
	
	else {			/* Ok, we'll accept this message. */
		msgnum = CtdlSubmitMsg(msg, valid, "", 0);
		if (msgnum > 0L) {
			StrBufPrintf(sSMTP->OneRcpt, "250 Message accepted.\r\n");
		}
		else {
			StrBufPrintf(sSMTP->OneRcpt, "550 Internal delivery error\r\n");
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
			cputbuf(sSMTP->OneRcpt);
		}
	}
	else {
		cputbuf(sSMTP->OneRcpt);
	}

	/* Write something to the syslog(which may or may not be where the
	 * rest of the Citadel logs are going; some sysadmins want LOG_MAIL).
	 */
	syslog((LOG_MAIL | LOG_INFO),
	       "%ld: from=<%s>, nrcpts=%d, relay=%s [%s], stat=%s",
	       msgnum,
	       ChrPtr(sSMTP->from),
	       sSMTP->number_of_recipients,
	       CCC->cs_host,
	       CCC->cs_addr,
	       ChrPtr(sSMTP->OneRcpt)
	);

	/* Clean up */
	CM_Free(msg);
	free_recipients(valid);
	smtp_data_clear(0, 0);	/* clear out the buffers now */
}


/*
 * implements the STARTTLS command
 */
void smtp_starttls(long offset, long flags)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response, "220 Begin TLS negotiation now\r\n");
	sprintf(nosup_response, "554 TLS not supported here\r\n");
	sprintf(error_response, "554 Internal error\r\n");
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
	smtp_rset(0, 0);
}


/* 
 * Main command loop for SMTP server sessions.
 */
void smtp_command_loop(void)
{
	struct CitContext *CCC = CC;
	citsmtp *sSMTP = SMTP;
	const char *pch, *pchs;
	long i;
	char CMD[MaxSMTPCmdLen + 1];

	if (sSMTP == NULL) {
		syslog(LOG_EMERG, "Session SMTP data is null.  WTF?  We will crash now.\n");
		return cit_panic_backtrace (0);
	}

	time(&CCC->lastcmd);
	if (CtdlClientGetLine(sSMTP->Cmd) < 1) {
		syslog(LOG_CRIT, "SMTP: client disconnected: ending session.\n");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}
	syslog(LOG_DEBUG, "SMTP server: %s\n", ChrPtr(sSMTP->Cmd));

	if (sSMTP->command_state == smtp_user) {
		smtp_get_user(0);
	}

	else if (sSMTP->command_state == smtp_password) {
		smtp_get_pass(0, 0);
	}

	else if (sSMTP->command_state == smtp_plain) {
		smtp_try_plain(0, 0);
	}

	pchs = pch = ChrPtr(sSMTP->Cmd);
	i = 0;
	while ((*pch != '\0') &&
	       (!isblank(*pch)) && 
	       (pch - pchs <= MaxSMTPCmdLen))
	{
		CMD[i] = toupper(*pch);
		pch ++;
		i++;
	}
	CMD[i] = '\0';

	if ((*pch == '\0') ||
	    (isblank(*pch)))
	{
		void *v;

		if (GetHash(SMTPCmds, CMD, i, &v) &&
		    (v != NULL))
		{
			smtp_handler_hook *h = (smtp_handler_hook*) v;

			if (isblank(pchs[i]))
				i++;

			h->h(i, h->Flags);

			return;
		}
	}
	cprintf("502 I'm afraid I can't do that.\r\n");
}

void smtp_noop(long offest, long Flags)
{
	cprintf("250 NOOP\r\n");
}

void smtp_quit(long offest, long Flags)
{
	cprintf("221 Goodbye...\r\n");
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
}

/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/
/*
 * This cleanup function blows away the temporary memory used by
 * the SMTP server.
 */
void smtp_cleanup_function(void)
{
	citsmtp *sSMTP = SMTP;

	/* Don't do this stuff if this is not an SMTP session! */
	if (CC->h_command_function != smtp_command_loop) return;

	syslog(LOG_DEBUG, "Performing SMTP cleanup hook\n");

	FreeStrBuf(&sSMTP->Cmd);
	FreeStrBuf(&sSMTP->helo_node);
	FreeStrBuf(&sSMTP->from);
	FreeStrBuf(&sSMTP->recipients);
	FreeStrBuf(&sSMTP->OneRcpt);
	FreeStrBuf(&sSMTP->preferred_sender_email);
	FreeStrBuf(&sSMTP->preferred_sender_name);

	free(sSMTP);
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
		SMTPCmds = NewHash(1, NULL);
		
		RegisterSmtpCMD("AUTH", smtp_auth, 0);
		RegisterSmtpCMD("DATA", smtp_data, 0);
		RegisterSmtpCMD("HELO", smtp_hello, HELO);
		RegisterSmtpCMD("EHLO", smtp_hello, EHLO);
		RegisterSmtpCMD("LHLO", smtp_hello, LHLO);
		RegisterSmtpCMD("HELP", smtp_help, 0);
		RegisterSmtpCMD("MAIL", smtp_mail, 0);
		RegisterSmtpCMD("NOOP", smtp_noop, 0);
		RegisterSmtpCMD("QUIT", smtp_quit, 0);
		RegisterSmtpCMD("RCPT", smtp_rcpt, 0);
		RegisterSmtpCMD("RSET", smtp_rset, 1);
#ifdef HAVE_OPENSSL
		RegisterSmtpCMD("STARTTLS", smtp_starttls, 0);
#endif


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

		CtdlRegisterCleanupHook(smtp_cleanup);
		CtdlRegisterSessionHook(smtp_cleanup_function, EVT_STOP, PRIO_STOP + 250);
	}
	
	/* return our module name for the log */
	return "smtp";
}
