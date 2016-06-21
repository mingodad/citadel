//
// NNTP server module (RFC 3977)
//
// Copyright (c) 2014-2015 by the citadel.org team
//
// This program is open source software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License version 3.
//  
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//

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

//	***************** BEGIN UTILITY FUNCTIONS THAT COULD BE MOVED ELSEWHERE LATER **************


//
// Tests whether the supplied string is a valid newsgroup name
// Returns true (nonzero) or false (0)
//
int is_valid_newsgroup_name(char *name) {
	char *ptr = name;
	int has_a_letter = 0;
	int num_dots = 0;

	if (!ptr) return(0);
	if (!strncasecmp(name, "ctdl.", 5)) return(0);

	while (*ptr != 0) {

		if (isalpha(ptr[0])) {
			has_a_letter = 1;
		}

		if (ptr[0] == '.') {
			++num_dots;
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
	return( (has_a_letter) && (num_dots >= 1) ) ;
}


//
// Convert a Citadel room name to a valid newsgroup name
//
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
			target[len++] = tolower(ch);
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


//
// Convert a newsgroup name to a Citadel room name.
// This function recognizes names converted with room_to_newsgroup() and restores them with full fidelity.
//
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


//	*****************  END  UTILITY FUNCTIONS THAT COULD BE MOVED ELSEWHERE LATER **************



//
// Here's where our NNTP session begins its happy day.
//
void nntp_greeting(void)
{
	strcpy(CC->cs_clientname, "NNTP session");
	CC->cs_flags |= CS_STEALTH;

	CC->session_specific_data = malloc(sizeof(citnntp));
	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
	memset(nntpstate, 0, sizeof(citnntp));

	if (CC->nologin==1) {
		cprintf("451 Too many connections are already open; please try again later.\r\n");
		CC->kill_me = KILLME_MAX_SESSIONS_EXCEEDED;
		return;
	}

	// Display the standard greeting
	cprintf("200 %s NNTP Citadel server is not finished yet\r\n", CtdlGetConfigStr("c_fqdn"));
}


//
// NNTPS is just like NNTP, except it goes crypto right away.
//
void nntps_greeting(void) {
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);
#ifdef HAVE_OPENSSL
	if (!CC->redirect_ssl) CC->kill_me = KILLME_NO_CRYPTO;		/* kill session if no crypto */
#endif
	nntp_greeting();
}


//
// implements the STARTTLS command
//
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


//
// Implements the CAPABILITY command
//
void nntp_capabilities(void)
{
	cprintf("101 Capability list:\r\n");
	cprintf("IMPLEMENTATION Citadel %d\r\n", REV_LEVEL);
	cprintf("VERSION 2\r\n");
	cprintf("READER\r\n");
	cprintf("MODE-READER\r\n");
	cprintf("LIST ACTIVE NEWSGROUPS\r\n");
	cprintf("OVER\r\n");
#ifdef HAVE_OPENSSL
	cprintf("STARTTLS\r\n");
#endif
	if (!CC->logged_in) {
		cprintf("AUTHINFO USER\r\n");
	}
	cprintf(".\r\n");
}


// 
// Implements the QUIT command
//
void nntp_quit(void)
{
	cprintf("221 Goodbye...\r\n");
	CC->kill_me = KILLME_CLIENT_LOGGED_OUT;
}


//
// Cleanup hook for this module
//
void nntp_cleanup(void)
{
	/* nothing here yet */
}


//
// Implements the AUTHINFO USER command (RFC 4643)
//
void nntp_authinfo_user(const char *username)
{
	int a = CtdlLoginExistingUser(NULL, username);
	switch (a) {
	case login_already_logged_in:
		cprintf("482 Already logged in\r\n");
		return;
	case login_too_many_users:
		cprintf("481 Too many users are already online (maximum is %d)\r\n", CtdlGetConfigInt("c_maxsessions"));
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


//
// Implements the AUTHINFO PASS command (RFC 4643)
//
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


//
// Implements the AUTHINFO extension (RFC 4643) in USER/PASS mode
//
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


//
// Utility function to fetch the current list of message numbers in a room
//
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


//
// Output a room name (newsgroup name) in formats required for LIST and NEWGROUPS command
//
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


//
// Called once per room by nntp_newgroups() to qualify and possibly output a single room
//
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


//
// Implements the NEWGROUPS command
//
void nntp_newgroups(const char *cmd) {
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


//
// Called once per room by nntp_list() to qualify and possibly output a single room
//
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


//
// Implements the LIST commands
//
void nntp_list(const char *cmd) {
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
	else if (!strcasecmp(list_format, "OVERVIEW.FMT")) {
		nld.list_format = NNTP_LIST_OVERVIEW_FMT;
	}
	else {
		cprintf("501 syntax error , unsupported list format\r\n");
		return;
	}

	// OVERVIEW.FMT delivers a completely different type of data than all of the
	// other LIST commands.  It's a stupid place to put it.  But that's how it's
	// written into RFC3977, so we have to handle it here.
	if (nld.list_format == NNTP_LIST_OVERVIEW_FMT) {
		cprintf("215 Order of fields in overview database.\r\n");
		cprintf("Subject:\r\n");
		cprintf("From:\r\n");
		cprintf("Date:\r\n");
		cprintf("Message-ID:\r\n");
		cprintf("References:\r\n");
		cprintf("Bytes:\r\n");
		cprintf("Lines:\r\n");
		cprintf(".\r\n");
		return;
	}

	cprintf("215 list of newsgroups follows\r\n");
	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlForEachRoom(nntp_list_backend, &nld);
	cprintf(".\r\n");
}


//
// Implement HELP command.
//
void nntp_help(void) {
	cprintf("100 This is the Citadel NNTP service.\r\n");
	cprintf("RTFM http://www.ietf.org/rfc/rfc3977.txt\r\n");
	cprintf(".\r\n");
}


//
// Implement DATE command.
//
void nntp_date(void) {
	time_t now;
	struct tm nowLocal;
	struct tm nowUtc;
	char tsFromUtc[32];

	now = time(NULL);
	localtime_r(&now, &nowLocal);
	gmtime_r(&now, &nowUtc);

	strftime(tsFromUtc, sizeof(tsFromUtc), "%Y%m%d%H%M%S", &nowUtc);

	cprintf("111 %s\r\n", tsFromUtc);
}


//
// back end for the LISTGROUP command , called for each message number
//
void nntp_listgroup_backend(long msgnum, void *userdata) {

	struct listgroup_range *lr = (struct listgroup_range *)userdata;

	// check range if supplied
	if (msgnum < lr->lo) return;
	if ((lr->hi != 0) && (msgnum > lr->hi)) return;

	cprintf("%ld\r\n", msgnum);
}


//
// Implements the GROUP and LISTGROUP commands
//
void nntp_group(const char *cmd) {
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
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

	// If this is a GROUP command, set the "current article number" to zero, and then stop here.
	if (!strcasecmp(verb, "GROUP")) {
		nntpstate->current_article_number = oldest;
		return;
	}

	// If we get to this point we are running a LISTGROUP command.  Fetch those message numbers.
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, nntp_listgroup_backend, &lr);
	cprintf(".\r\n");
}


//
// Implements the MODE command
//
void nntp_mode(const char *cmd) {

	char which_mode[16];

	extract_token(which_mode, cmd, 1, ' ', sizeof which_mode);

	if (!strcasecmp(which_mode, "reader")) {
		// FIXME implement posting and change to 200
		cprintf("201 Reader mode activated\r\n");
	}
	else {
		cprintf("501 unknown mode\r\n");
	}
}


//
// Implements the ARTICLE, HEAD, BODY, and STAT commands.
// (These commands all accept the same parameters; they differ only in how they output the retrieved message.)
//
void nntp_article(const char *cmd) {
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
	char which_command[16];
	int acmd = 0;
	char requested_article[256];
	long requested_msgnum = 0;
	char *lb, *rb = NULL;
	int must_change_currently_selected_article = 0;

	// We're going to store one of these values in the variable 'acmd' so that
	// we can quickly check later which version of the output we want.
	enum {
		ARTICLE,
		HEAD,
		BODY,
		STAT
	};

	extract_token(which_command, cmd, 0, ' ', sizeof which_command);

	if (!strcasecmp(which_command, "article")) {
		acmd = ARTICLE;
	}
	else if (!strcasecmp(which_command, "head")) {
		acmd = HEAD;
	}
	else if (!strcasecmp(which_command, "body")) {
		acmd = BODY;
	}
	else if (!strcasecmp(which_command, "stat")) {
		acmd = STAT;
	}
	else {
		cprintf("500 I'm afraid I can't do that.\r\n");
		return;
	}

	// Which NNTP command was issued, determines whether we will fetch headers, body, or both.
	int			headers_only = HEADERS_ALL;
	if (acmd == HEAD)	headers_only = HEADERS_FAST;
	else if (acmd == BODY)	headers_only = HEADERS_NONE;
	else if (acmd == STAT)	headers_only = HEADERS_FAST;

	// now figure out what the client is asking for.
	extract_token(requested_article, cmd, 1, ' ', sizeof requested_article);
	lb = strchr(requested_article, '<');
	rb = strchr(requested_article, '>');
	requested_msgnum = atol(requested_article);

	// If no article number or message-id is specified, the client wants the "currently selected article"
	if (IsEmptyStr(requested_article)) {
		if (nntpstate->current_article_number < 1) {
			cprintf("420 No current article selected\r\n");
			return;
		}
		requested_msgnum = nntpstate->current_article_number;
		must_change_currently_selected_article = 1;
		// got it -- now fall through and keep going
	}

	// If the requested article is numeric, it maps directly to a message number.  Good.
	else if (requested_msgnum > 0) {
		must_change_currently_selected_article = 1;
		// good -- fall through and keep going
	}

	// If the requested article has angle brackets, the client wants a specific message-id.
	// We don't know how to do that yet.
	else if ( (lb != NULL) && (rb != NULL) && (lb < rb) ) {
		must_change_currently_selected_article = 0;
		cprintf("500 I don't know how to fetch by message-id yet.\r\n");	// FIXME
		return;
	}

	// Anything else is noncompliant gobbledygook and should die in a car fire.
	else {
		must_change_currently_selected_article = 0;
		cprintf("500 syntax error\r\n");
		return;
	}

	// At this point we know the message number of the "article" being requested.
	// We have an awesome API call that does all the heavy lifting for us.
	char *fetched_message_id = NULL;
	CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	int fetch = CtdlOutputMsg(requested_msgnum,
			MT_RFC822,		// output in RFC822 format ... sort of
			headers_only,		// headers, body, or both?
			0,			// don't do Citadel protocol responses
			1,			// CRLF newlines
			NULL,			// teh whole thing, not just a section
			0,			// no flags yet ... maybe new ones for Path: etc ?
			NULL,
			NULL,
			&fetched_message_id	// extract the message ID from the message as we go...
	);
	StrBuf *msgtext = CC->redirect_buffer;
	CC->redirect_buffer = NULL;

	if (fetch != om_ok) {
		cprintf("423 no article with that number\r\n");
		FreeStrBuf(&msgtext);
		return;
	}

	// RFC3977 6.2.1.2 specifes conditions under which the "currently selected article"
	// MUST or MUST NOT be set to the message we just referenced.
	if (must_change_currently_selected_article) {
		nntpstate->current_article_number = requested_msgnum;
	}

	// Now give the client what it asked for.
	if (acmd == ARTICLE) {
		cprintf("220 %ld <%s>\r\n", requested_msgnum, fetched_message_id);
	}
	if (acmd == HEAD) {
		cprintf("221 %ld <%s>\r\n", requested_msgnum, fetched_message_id);
	}
	if (acmd == BODY) {
		cprintf("222 %ld <%s>\r\n", requested_msgnum, fetched_message_id);
	}
	if (acmd == STAT) {
		cprintf("223 %ld <%s>\r\n", requested_msgnum, fetched_message_id);
		FreeStrBuf(&msgtext);
		return;
	}

	client_write(SKEY(msgtext));
	cprintf(".\r\n");			// this protocol uses a dot terminator
	FreeStrBuf(&msgtext);
	if (fetched_message_id) free(fetched_message_id);
}


//
// Utility function for nntp_last_next() that turns a msgnum into a message ID.
// The memory for the returned string is pwnz0red by the caller.
//
char *message_id_from_msgnum(long msgnum) {

	char *fetched_message_id = NULL;
	CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputMsg(msgnum,
			MT_RFC822,		// output in RFC822 format ... sort of
			HEADERS_FAST,		// headers, body, or both?
			0,			// don't do Citadel protocol responses
			1,			// CRLF newlines
			NULL,			// teh whole thing, not just a section
			0,			// no flags yet ... maybe new ones for Path: etc ?
			NULL,
			NULL,
			&fetched_message_id	// extract the message ID from the message as we go...
	);
	StrBuf *msgtext = CC->redirect_buffer;
	CC->redirect_buffer = NULL;

	FreeStrBuf(&msgtext);
	return(fetched_message_id);
}


//
// The LAST and NEXT commands are so similar that they are handled by a single function.
//
void nntp_last_next(const char *cmd) {
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
	char which_command[16];
	int acmd = 0;

	// We're going to store one of these values in the variable 'acmd' so that
	// we can quickly check later which version of the output we want.
	enum {
		NNTP_LAST,
		NNTP_NEXT
	};

	extract_token(which_command, cmd, 0, ' ', sizeof which_command);

	if (!strcasecmp(which_command, "last")) {
		acmd = NNTP_LAST;
	}
	else if (!strcasecmp(which_command, "next")) {
		acmd = NNTP_NEXT;
	}
	else {
		cprintf("500 I'm afraid I can't do that.\r\n");
		return;
	}

	// ok, here we go ... fetch the msglist so we can figure out our place in the universe
	struct nntp_msglist nm;
	int i = 0;
	long selected_msgnum = 0;
	char *message_id = NULL;

	nm = nntp_fetch_msglist(&CC->room);
	if ((nm.num_msgs < 0) || (nm.msgnums == NULL)) {
		cprintf("500 something bad happened\r\n");
		return;
	}

	if ( (acmd == NNTP_LAST) && (nm.num_msgs == 0) ) {
			cprintf("422 no previous article in this group\r\n");	// nothing here
	}

	else if ( (acmd == NNTP_LAST) && (nntpstate->current_article_number <= nm.msgnums[0]) ) {
			cprintf("422 no previous article in this group\r\n");	// already at the beginning
	}

	else if (acmd == NNTP_LAST) {
		for (i=0; ((i<nm.num_msgs)&&(selected_msgnum<=0)); ++i) {
			if ( (nm.msgnums[i] >= nntpstate->current_article_number) && (i > 0) ) {
				selected_msgnum = nm.msgnums[i-1];
			}
		}
		if (selected_msgnum > 0) {
			nntpstate->current_article_number = selected_msgnum;
			message_id = message_id_from_msgnum(nntpstate->current_article_number);
			cprintf("223 %ld <%s>\r\n", nntpstate->current_article_number, message_id);
			if (message_id) free(message_id);
		}
		else {
			cprintf("422 no previous article in this group\r\n");
		}
	}

	else if ( (acmd == NNTP_NEXT) && (nm.num_msgs == 0) ) {
			cprintf("421 no next article in this group\r\n");	// nothing here
	}

	else if ( (acmd == NNTP_NEXT) && (nntpstate->current_article_number >= nm.msgnums[nm.num_msgs-1]) ) {
			cprintf("421 no next article in this group\r\n");	// already at the end
	}

	else if (acmd == NNTP_NEXT) {
		for (i=0; ((i<nm.num_msgs)&&(selected_msgnum<=0)); ++i) {
			if (nm.msgnums[i] > nntpstate->current_article_number) {
				selected_msgnum = nm.msgnums[i];
			}
		}
		if (selected_msgnum > 0) {
			nntpstate->current_article_number = selected_msgnum;
			message_id = message_id_from_msgnum(nntpstate->current_article_number);
			cprintf("223 %ld <%s>\r\n", nntpstate->current_article_number, message_id);
			if (message_id) free(message_id);
		}
		else {
			cprintf("421 no next article in this group\r\n");
		}
	}

	// should never get here
	else {
		cprintf("500 internal error\r\n");
	}

	if (nm.msgnums != NULL) {
		free(nm.msgnums);
	}

}


//
// back end for the XOVER command , called for each message number
//
void nntp_xover_backend(long msgnum, void *userdata) {

	struct listgroup_range *lr = (struct listgroup_range *)userdata;

	// check range if supplied
	if (msgnum < lr->lo) return;
	if ((lr->hi != 0) && (msgnum > lr->hi)) return;

	struct CtdlMessage *msg = CtdlFetchMessage(msgnum, 0, 1);
	if (msg == NULL) {
		return;
	}

	// Teh RFC says we need:
	// -------------------------
	// Subject header content
	// From header content
	// Date header content
	// Message-ID header content
	// References header content
	// :bytes metadata item
	// :lines metadata item

	time_t msgtime = atol(msg->cm_fields[eTimestamp]);
	char strtimebuf[26];
	ctime_r(&msgtime, strtimebuf);

	// here we go -- print the line o'data
	cprintf("%ld\t%s\t%s <%s>\t%s\t%s\t%s\t100\t10\r\n",
		msgnum,
		msg->cm_fields[eMsgSubject],
		msg->cm_fields[eAuthor],
		msg->cm_fields[erFc822Addr],
		strtimebuf,
		msg->cm_fields[emessageId],
		msg->cm_fields[eWeferences]
	);

	CM_Free(msg);
}


//
//
// XOVER is used by some clients, even if we don't offer it
//
void nntp_xover(const char *cmd) {
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
	char range[256];
	struct listgroup_range lr;

	extract_token(range, cmd, 1, ' ', sizeof range);
	lr.lo = atol(range);
	if (lr.lo <= 0) {
		lr.lo = nntpstate->current_article_number;
		lr.hi = nntpstate->current_article_number;
	}
	else {
		char *dash = strchr(range, '-');
		if (dash != NULL) {
			++dash;
			lr.hi = atol(dash);
			if (lr.hi == 0) {
				lr.hi = LONG_MAX;
			}
			if (lr.hi < lr.lo) {
				lr.hi = lr.lo;
			}
		}
		else {
			lr.hi = lr.lo;
		}
	}

	cprintf("224 Overview information follows\r\n");
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, nntp_xover_backend, &lr);
	cprintf(".\r\n");
}


// 
// Main command loop for NNTP server sessions.
//
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
	syslog(LOG_DEBUG, "NNTP: %s\n", ((!strncasecmp(ChrPtr(Cmd), "AUTHINFO", 8)) ? "AUTHINFO ..." : ChrPtr(Cmd)));
	extract_token(cmdname, ChrPtr(Cmd), 0, ' ', sizeof cmdname);

	// Rumpelstiltskin lookups are *awesome*

	if (!strcasecmp(cmdname, "quit")) {
		nntp_quit();
	}

	else if (!strcasecmp(cmdname, "help")) {
		nntp_help();
	}

	else if (!strcasecmp(cmdname, "date")) {
		nntp_date();
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

	else if (
			(!strcasecmp(cmdname, "article"))
			|| (!strcasecmp(cmdname, "head"))
			|| (!strcasecmp(cmdname, "body"))
			|| (!strcasecmp(cmdname, "stat"))
		)
	{
		nntp_article(ChrPtr(Cmd));
	}

	else if (
			(!strcasecmp(cmdname, "last"))
			|| (!strcasecmp(cmdname, "next"))
		)
	{
		nntp_last_next(ChrPtr(Cmd));
	}

	else if (
			(!strcasecmp(cmdname, "xover"))
			|| (!strcasecmp(cmdname, "over"))
		)
	{
		nntp_xover(ChrPtr(Cmd));
	}

	else {
		cprintf("500 I'm afraid I can't do that.\r\n");
	}

	FreeStrBuf(&Cmd);
}


//	****************************************************************************
//			      MODULE INITIALIZATION STUFF
//	****************************************************************************


//
// This cleanup function blows away the temporary memory used by
// the NNTP server.
//
void nntp_cleanup_function(void)
{
	/* Don't do this stuff if this is not an NNTP session! */
	if (CC->h_command_function != nntp_command_loop) return;

	syslog(LOG_DEBUG, "Performing NNTP cleanup hook\n");
	citnntp *nntpstate = (citnntp *) CC->session_specific_data;
	if (nntpstate != NULL) {
		free(nntpstate);
		nntpstate = NULL;
	}
}

const char *CitadelServiceNNTP="NNTP";
const char *CitadelServiceNNTPS="NNTPS";

CTDL_MODULE_INIT(nntp)
{
	if (!threading)
	{
		CtdlRegisterServiceHook(CtdlGetConfigInt("c_nntp_port"),
					NULL,
					nntp_greeting,
					nntp_command_loop,
					NULL, 
					CitadelServiceNNTP);

#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(CtdlGetConfigInt("c_nntps_port"),
					NULL,
					nntps_greeting,
					nntp_command_loop,
					NULL,
					CitadelServiceNNTPS);
#endif

		CtdlRegisterCleanupHook(nntp_cleanup);
		CtdlRegisterSessionHook(nntp_cleanup_function, EVT_STOP, PRIO_STOP + 250);
	}
	
	/* return our module name for the log */
	return "nntp";
}
