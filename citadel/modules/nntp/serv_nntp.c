/*
 * NNTP server module FIXME THIS IS NOT FINISHED
 *
 * Copyright (c) 2014 by the citadel.org team
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




/****************** BEGIN UTILITY FUNCTIONS THAT COULD BE MOVED ELSEWHERE LATER **************/


/*
 * Tests whether the supplied string is a valid newsgroup name
 * Returns true (nonzero) or false (0)
 */
int is_valid_newsgroup_name(char *name) {
	char *ptr = name;
	int has_a_letter = 0;

	if (!ptr) return(0);
	if (!strncasecmp(name, "ctdl.", 5)) return(0);

	while (*ptr != 0) {

		if (isalpha(ptr[0])) {
			has_a_letter = 1;
		}

		if (	(isalnum(ptr[0]))
			|| (ptr[0] == '.')
			|| (ptr[0] == '+')
			|| (ptr[0] == '-')
		) {
			++ptr;
		}
		else {
			return(0);
		}
	}
	return(has_a_letter);
}



/*
 * Convert a Citadel room name to a valid newsgroup name
 */
void room_to_newsgroup(char *target, char *source, size_t target_size) {

	if (!target) return;
	if (!source) return;

	if (is_valid_newsgroup_name(source)) {
		strncpy(target, source, target_size);
		return;
	}

	strcpy(target, "ctdl.");
	int len = 5;
	char *ptr = source;
	char ch;

	while (ch=*ptr++, ch!=0) {
		if (len >= target_size) return;
		if (	(isalnum(ch))
			|| (ch == '.')
			|| (ch == '-')
		) {
			target[len++] = ch;
			target[len] = 0;
		}
		else {
			target[len++] = '+' ;
			sprintf(&target[len], "%02x", ch);
			len += 2;
			target[len] = 0;
		}
	}
}


/*
 * Convert a newsgroup name to a Citadel room name.
 * This function recognizes names converted with room_to_newsgroup() and restores them with full fidelity.
 */
void newsgroup_to_room(char *target, char *source, size_t target_size) {

	if (!target) return;
	if (!source) return;

	if (strncasecmp(source, "ctdl.", 5)) {			// not a converted room name; pass through as-is
		strncpy(target, source, target_size);
		return;
	}

	target[0] = 0;
	int len = 0;
	char *ptr = &source[5];
	char ch;

	while (ch=*ptr++, ch!=0) {
		if (len >= target_size) return;
		if (ch == '+') {
			char hex[3];
			long digit;
			hex[0] = *ptr++;
			hex[1] = *ptr++;
			hex[2] = 0;
			digit = strtol(hex, NULL, 16);
			ch = (char)digit;
		}
		target[len++] = ch;
		target[len] = 0;
	}
}


/******************  END  UTILITY FUNCTIONS THAT COULD BE MOVED ELSEWHERE LATER **************/



/*
 * Here's where our NNTP session begins its happy day.
 */
void nntp_greeting(void)
{
	strcpy(CC->cs_clientname, "NNTP session");
	CC->cs_flags |= CS_STEALTH;

	/* CC->session_specific_data = malloc(sizeof(citnntp));
	memset(NNTP, 0, sizeof(citnntp));
	*/

	if (CC->nologin==1) {
		cprintf("451 Too many connections are already open; please try again later.\r\n");
		CC->kill_me = KILLME_MAX_SESSIONS_EXCEEDED;
		return;
	}

	/* Note: the FQDN *must* appear as the first thing after the 220 code.
	 * Some clients (including citmail.c) depend on it being there.
	 */
	cprintf("200 %s NNTP Citadel server is not finished yet\r\n", config.c_fqdn);
}


/*
 * NNTPS is just like NNTP, except it goes crypto right away.
 */
void nntps_greeting(void) {
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) CC->kill_me = KILLME_NO_CRYPTO;		/* kill session if no crypto */
#endif
	nntp_greeting();
}



/*
 * implements the STARTTLS command
 */
void nntp_starttls(void)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response, "382 Begin TLS negotiation now\r\n");
	sprintf(nosup_response, "502 Can not initiate TLS negotiation\r\n");
	sprintf(error_response, "580 Internal error\r\n");
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
}


void nntp_noop(void)
{
	cprintf("250 NOOP\r\n");
}


void nntp_capabilities(void)
{
	cprintf("101 Capability list:\r\n");
#ifdef HAVE_OPENSSL
	cprintf("STARTTLS\r\n");
#endif
	if (!CC->logged_in) {
		cprintf("AUTHINFO USER\r\n");
	}
	cprintf(".\r\n");
}


void nntp_quit(void)
{
	cprintf("221 Goodbye...\r\n");
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
}


void nntp_cleanup(void)
{
	/* nothing here yet */
}



/*
 * Implements the AUTHINFO USER command (RFC 4643)
 */
void nntp_authinfo_user(const char *username)
{
	int a = CtdlLoginExistingUser(NULL, username);
	switch (a) {
	case login_already_logged_in:
		cprintf("482 Already logged in\r\n");
		return;
	case login_too_many_users:
		cprintf("481 Too many users are already online (maximum is %d)\r\n", config.c_maxsessions);
		return;
	case login_ok:
		cprintf("381 Password required for %s\r\n", CC->curr_user);
		return;
	case login_not_found:
		cprintf("481 %s not found\r\n", username);
		return;
	default:
		cprintf("502 Internal error\r\n");
	}
}


/*
 * Implements the AUTHINFO PASS command (RFC 4643)
 */
void nntp_authinfo_pass(const char *buf)
{
	int a;

	a = CtdlTryPassword(buf, strlen(buf));

	switch (a) {
	case pass_already_logged_in:
		cprintf("482 Already logged in\r\n");
		return;
	case pass_no_user:
		cprintf("482 Authentication commands issued out of sequence\r\n");
		return;
	case pass_wrong_password:
		cprintf("481 Authentication failed\r\n");
		return;
	case pass_ok:
		cprintf("281 Authentication accepted\r\n");
		return;
	}
}



/*
 * Implements the AUTHINFO extension (RFC 4643) in USER/PASS mode
 */
void nntp_authinfo(const char *cmd) {

	if (!strncasecmp(cmd, "authinfo user ", 14)) {
		nntp_authinfo_user(&cmd[14]);
	}

	else if (!strncasecmp(cmd, "authinfo pass ", 14)) {
		nntp_authinfo_pass(&cmd[14]);
	}

	else {
		cprintf("502 command unavailable\r\n");
	}
}



/* 
 * Main command loop for NNTP server sessions.
 */
void nntp_command_loop(void)
{
	StrBuf *Cmd = NewStrBuf();
	char cmdname[16];

	time(&CC->lastcmd);
	if (CtdlClientGetLine(Cmd) < 1) {
		syslog(LOG_CRIT, "NNTP: client disconnected: ending session.\n");
		CC->kill_me = KILLME_CLIENT_DISCONNECTED;
		FreeStrBuf(&Cmd);
		return;
	}
	syslog(LOG_DEBUG, "NNTP server: %s\n", ChrPtr(Cmd));
	extract_token(cmdname, ChrPtr(Cmd), 0, ' ', sizeof cmdname);

	/*
	 * Rumpelstiltskin lookups are awesome
	 */

	if (!strcasecmp(cmdname, "quit")) {
		nntp_quit();
	}

	else if (!strcasecmp(cmdname, "capabilities")) {
		nntp_capabilities();
	}

	else if (!strcasecmp(cmdname, "starttls")) {
		nntp_starttls();
	}

	else if (!strcasecmp(cmdname, "noop")) {
		nntp_noop();
	}

	else if (!strcasecmp(cmdname, "authinfo")) {
		nntp_authinfo(ChrPtr(Cmd));
	}

	else {
		cprintf("500 I'm afraid I can't do that.\r\n");
	}

	FreeStrBuf(&Cmd);
}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


/*
 * This cleanup function blows away the temporary memory used by
 * the NNTP server.
 */
void nntp_cleanup_function(void)
{
	/* Don't do this stuff if this is not an NNTP session! */
	if (CC->h_command_function != nntp_command_loop) return;

	syslog(LOG_DEBUG, "Performing NNTP cleanup hook\n");
}

const char *CitadelServiceNNTP="NNTP";

CTDL_MODULE_INIT(nntp)
{
	if (!threading)
	{
		CtdlRegisterServiceHook(119,			// FIXME config.c_nntp_port,
					NULL,
					nntp_greeting,
					nntp_command_loop,
					NULL, 
					CitadelServiceNNTP);

#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(563,			// FIXME config.c_nntps_port,
					NULL,
					nntps_greeting,
					nntp_command_loop,
					NULL,
					CitadelServiceNNTP);
#endif

		CtdlRegisterCleanupHook(nntp_cleanup);
		CtdlRegisterSessionHook(nntp_cleanup_function, EVT_STOP, PRIO_STOP + 250);
	}
	
	/* return our module name for the log */
	return "nntp";
}
