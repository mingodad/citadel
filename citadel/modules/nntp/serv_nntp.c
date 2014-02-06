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
#include "serv_nntp.h"

extern long timezone;

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


void nntp_capabilities(void)
{
	cprintf("101 Capability list:\r\n");
	cprintf("IMPLEMENTATION Citadel v%d.%02d\r\n", (REV_LEVEL/100), (REV_LEVEL%100));
	cprintf("VERSION 2\r\n");
	cprintf("READER\r\n");
	cprintf("MODE-READER\r\n");
	cprintf("LIST ACTIVE NEWSGROUPS\r\n");
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
 * Utility function to fetch the current list of message numbers in a room
 */
struct nntp_msglist nntp_fetch_msglist(struct ctdlroom *qrbuf) {
	struct nntp_msglist nm;
	struct cdbdata *cdbfr;

	cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf->QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		nm.msgnums = (long*)cdbfr->ptr;
		cdbfr->ptr = NULL;
		nm.num_msgs = cdbfr->len / sizeof(long);
		cdbfr->len = 0;
		cdb_free(cdbfr);
	} else {
		nm.num_msgs = 0;
		nm.msgnums = NULL;
	}
	return(nm);
}



/*
 * Various output formats for the LIST commands
 */
enum {
	NNTP_LIST_ACTIVE,
	NNTP_LIST_ACTIVE_TIMES,
	NNTP_LIST_DISTRIB_PATS,
	NNTP_LIST_HEADERS,
	NNTP_LIST_NEWSGROUPS,
	NNTP_LIST_OVERVIEW_FMT
};


/*
 * Output a room name (newsgroup name) in formats required for LIST and NEWGROUPS command
 */
void output_roomname_in_list_format(struct ctdlroom *qrbuf, int which_format, char *wildmat_pattern) {
	char n_name[1024];
	struct nntp_msglist nm;
	long low_water_mark = 0;
	long high_water_mark = 0;

	room_to_newsgroup(n_name, qrbuf->QRname, sizeof n_name);

	if ((wildmat_pattern != NULL) && (!IsEmptyStr(wildmat_pattern))) {
		if (!wildmat(n_name, wildmat_pattern)) {
			return;
		}
	}

	nm = nntp_fetch_msglist(qrbuf);
	if ((nm.num_msgs > 0) && (nm.msgnums != NULL)) {
		low_water_mark = nm.msgnums[0];
		high_water_mark = nm.msgnums[nm.num_msgs - 1];
	}

	// Only the mandatory formats are supported
	switch(which_format) {
	case NNTP_LIST_ACTIVE:
		// FIXME we have hardcoded "n" for "no posting allowed" -- fix when we add posting
		cprintf("%s %ld %ld n\r\n", n_name, high_water_mark, low_water_mark);
		break;
	case NNTP_LIST_NEWSGROUPS:
		cprintf("%s %s\r\n", n_name, qrbuf->QRname);
		break;
	}

	if (nm.msgnums != NULL) {
		free(nm.msgnums);
	}
}



/*
 * Called once per room by nntp_newgroups() to qualify and possibly output a single room
 */
void nntp_newgroups_backend(struct ctdlroom *qrbuf, void *data)
{
	int ra;
	int view;
	time_t thetime = *(time_t *)data;

	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	/*
	 * The "created after <date/time>" heuristics depend on the happy coincidence
	 * that for a very long time we have used a unix timestamp as the room record's
	 * generation number (QRgen).  When this module is merged into the master
	 * source tree we should rename QRgen to QR_create_time or something like that.
	 */

	if (ra & UA_KNOWN) {
		if (qrbuf->QRgen >= thetime) {
			output_roomname_in_list_format(qrbuf, NNTP_LIST_ACTIVE, NULL);
		}
	}
}


/*
 * Implements the NEWGROUPS command
 */
void nntp_newgroups(const char *cmd) {
	/*
	 * HACK: this works because the 5XX series error codes from citadel
	 * protocol will also be considered error codes by an NNTP client
	 */
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;


	char stringy_date[16];
	char stringy_time[16];
	char stringy_gmt[16];
	struct tm tm;
	time_t thetime;

	extract_token(stringy_date, cmd, 1, ' ', sizeof stringy_date);
	extract_token(stringy_time, cmd, 2, ' ', sizeof stringy_time);
	extract_token(stringy_gmt, cmd, 3, ' ', sizeof stringy_gmt);

	memset(&tm, 0, sizeof tm);
	if (strlen(stringy_date) == 6) {
		sscanf(stringy_date, "%2d%2d%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
		tm.tm_year += 100;
	}
	else {
		sscanf(stringy_date, "%4d%2d%2d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
		tm.tm_year -= 1900;
	}
	tm.tm_mon -= 1;		// tm_mon is zero based (0=January)
	tm.tm_isdst = (-1);	// let the C library figure out whether DST is in effect
	sscanf(stringy_time, "%2d%2d%2d", &tm.tm_hour, &tm.tm_min ,&tm.tm_sec);
	thetime = mktime(&tm);
	if (!strcasecmp(stringy_gmt, "GMT")) {
		tzset();
		thetime += timezone;
	}


	cprintf("231 list of new newsgroups follows\r\n");
	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlForEachRoom(nntp_newgroups_backend, &thetime);
	cprintf(".\r\n");
}


/*
 * Called once per room by nntp_list() to qualify and possibly output a single room
 */
void nntp_list_backend(struct ctdlroom *qrbuf, void *data)
{
	int ra;
	int view;
	struct nntp_list_data *nld = (struct nntp_list_data *)data;

	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);
	if (ra & UA_KNOWN) {
		output_roomname_in_list_format(qrbuf, nld->list_format, nld->wildmat_pattern);
	}
}


/*
 * Implements the LIST commands
 */
void nntp_list(const char *cmd) {
	/*
	 * HACK: this works because the 5XX series error codes from citadel
	 * protocol will also be considered error codes by an NNTP client
	 */
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	char list_format[64];
	char wildmat_pattern[1024];
	struct nntp_list_data nld;

	extract_token(list_format, cmd, 1, ' ', sizeof list_format);
	extract_token(wildmat_pattern, cmd, 2, ' ', sizeof wildmat_pattern);

	if (strlen(wildmat_pattern) > 0) {
		nld.wildmat_pattern = wildmat_pattern;
	}
	else {
		nld.wildmat_pattern = NULL;
	}

	if ( (strlen(cmd) < 6) || (!strcasecmp(list_format, "ACTIVE")) ) {
		nld.list_format = NNTP_LIST_ACTIVE;
	}
	else if (!strcasecmp(list_format, "NEWSGROUPS")) {
		nld.list_format = NNTP_LIST_NEWSGROUPS;
	}
	else {
		cprintf("501 syntax error , unsupported list format\r\n");
		return;
	}

	cprintf("231 list of newsgroups follows\r\n");
	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlForEachRoom(nntp_list_backend, &nld);
	cprintf(".\r\n");
}


/*
 * Implement HELP command.
 */
void nntp_help(void) {
	cprintf("100 This is the Citadel NNTP service.\r\n");
	cprintf("RTFM http://www.ietf.org/rfc/rfc3977.txt\r\n");
	cprintf(".\r\n");
}


/*
 * back end for the LISTGROUP command , called for each message number
 */
void nntp_listgroup_backend(long msgnum, void *userdata) {

	struct listgroup_range *lr = (struct listgroup_range *)userdata;

	// check range if supplied
	if (msgnum < lr->lo) return;
	if ((lr->hi != 0) && (msgnum > lr->hi)) return;

	cprintf("%ld\r\n", msgnum);
}


/*
 * Implements the GROUP and LISTGROUP commands
 */
void nntp_group(const char *cmd) {
	/*
	 * HACK: this works because the 5XX series error codes from citadel
	 * protocol will also be considered error codes by an NNTP client
	 */
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	char verb[16];
	char requested_group[1024];
	char message_range[256];
	char range_lo[256];
	char range_hi[256];
	char requested_room[ROOMNAMELEN];
	char augmented_roomname[ROOMNAMELEN];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct ctdlroom QRscratch;
	int msgs, new;
	long oldest,newest;
	struct listgroup_range lr;

	extract_token(verb, cmd, 0, ' ', sizeof verb);
	extract_token(requested_group, cmd, 1, ' ', sizeof requested_group);
	extract_token(message_range, cmd, 2, ' ', sizeof message_range);
	extract_token(range_lo, message_range, 0, '-', sizeof range_lo);
	extract_token(range_hi, message_range, 1, '-', sizeof range_hi);
	lr.lo = atoi(range_lo);
	lr.hi = atoi(range_hi);

	/* In LISTGROUP mode we can specify an empty name for 'currently selected' */
	if ((!strcasecmp(verb, "LISTGROUP")) && (IsEmptyStr(requested_group))) {
		room_to_newsgroup(requested_group, CC->room.QRname, sizeof requested_group);
	}

	/* First try a regular match */
	newsgroup_to_room(requested_room, requested_group, sizeof requested_room);
	c = CtdlGetRoom(&QRscratch, requested_room);

	/* Then try a mailbox name match */
	if (c != 0) {
		CtdlMailboxName(augmented_roomname, sizeof augmented_roomname, &CC->user, requested_room);
		c = CtdlGetRoom(&QRscratch, augmented_roomname);
		if (c == 0) {
			safestrncpy(requested_room, augmented_roomname, sizeof(requested_room));
		}
	}

	/* If the room exists, check security/access */
	if (c == 0) {
		/* See if there is an existing user/room relationship */
		CtdlRoomAccess(&QRscratch, &CC->user, &ra, NULL);

		/* normal clients have to pass through security */
		if (ra & UA_KNOWN) {
			ok = 1;
		}
	}

	/* Fail here if no such room */
	if (!ok) {
		cprintf("411 no such newsgroup\r\n");
		return;
	}


	/*
	 * CtdlUserGoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.
	 */
	memcpy(&CC->room, &QRscratch, sizeof(struct ctdlroom));
	CtdlUserGoto(NULL, 0, 0, &msgs, &new, &oldest, &newest);
	cprintf("211 %d %ld %ld %s\r\n", msgs, oldest, newest, requested_group);

	// If this is a GROUP command, we can stop here.
	if (!strcasecmp(verb, "GROUP")) {
		return;
	}

	// If we get to this point we are running a LISTGROUP command.  Fetch those message numbers.
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, nntp_listgroup_backend, &lr);
	cprintf(".\r\n");
}


/*
 * Implements the MODE command
 */
void nntp_mode(const char *cmd) {

	char which_mode[16];

	extract_token(which_mode, cmd, 1, ' ', sizeof which_mode);

	if (!strcasecmp(which_mode, "reader")) {
		cprintf("201 Reader mode FIXME implement posting and change to 200\r\n");
	}
	else {
		cprintf("501 unknown mode\r\n");
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

	else if (!strcasecmp(cmdname, "help")) {
		nntp_help();
	}

	else if (!strcasecmp(cmdname, "capabilities")) {
		nntp_capabilities();
	}

	else if (!strcasecmp(cmdname, "starttls")) {
		nntp_starttls();
	}

	else if (!strcasecmp(cmdname, "authinfo")) {
		nntp_authinfo(ChrPtr(Cmd));
	}

	else if (!strcasecmp(cmdname, "newgroups")) {
		nntp_newgroups(ChrPtr(Cmd));
	}

	else if (!strcasecmp(cmdname, "list")) {
		nntp_list(ChrPtr(Cmd));
	}

	else if (!strcasecmp(cmdname, "group")) {
		nntp_group(ChrPtr(Cmd));
	}

	else if (!strcasecmp(cmdname, "listgroup")) {
		nntp_group(ChrPtr(Cmd));
	}

	else if (!strcasecmp(cmdname, "mode")) {
		nntp_mode(ChrPtr(Cmd));
	}

	else {
		cprintf("500 I'm afraid I can't do that.\r\n");
	}

	FreeStrBuf(&Cmd);
}


/*****************************************************************************/
/*		      MODULE INITIALIZATION STUFF			  */
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
