/*
 * POP3 service for the Citadel system
 *
 * Copyright (c) 1998-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Current status of standards conformance:
 *
 * -> All required POP3 commands described in RFC1939 are implemented.
 * -> All optional POP3 commands described in RFC1939 are also implemented.
 * -> The deprecated "LAST" command is included in this implementation, because
 *    there exist mail clients which insist on using it (such as Bynari
 *    TradeMail, and certain versions of Eudora).
 * -> Capability detection via the method described in RFC2449 is implemented.
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
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_pop3.h"
#include "md5.h"



#include "ctdl_module.h"

int POP3DebugEnabled = 0;

#define POP3DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (POP3DebugEnabled != 0))
#define CCCID CCC->cs_pid
#define POP3_syslog(LEVEL, FORMAT, ...)			\
	POP3DBGLOG(LEVEL) syslog(LEVEL,			\
				 "POP3CC[%d] " FORMAT,	\
				 CCCID, __VA_ARGS__)

#define POP3M_syslog(LEVEL, FORMAT)			\
	POP3DBGLOG(LEVEL) syslog(LEVEL,			\
				 "POP3CC[%d] " FORMAT,	\
				 CCCID)

/*
 * This cleanup function blows away the temporary memory and files used by
 * the POP3 server.
 */
void pop3_cleanup_function(void)
{
	struct CitContext *CCC = CC;

	/* Don't do this stuff if this is not a POP3 session! */
	if (CCC->h_command_function != pop3_command_loop) return;

	struct citpop3 *pop3 = ((struct citpop3 *)CCC->session_specific_data);
	POP3M_syslog(LOG_DEBUG, "Performing POP3 cleanup hook");
	if (pop3->msgs != NULL) {
		free(pop3->msgs);
	}

	free(pop3);
}



/*
 * Here's where our POP3 session begins its happy day.
 */
void pop3_greeting(void)
{
	struct CitContext *CCC = CC;

	strcpy(CCC->cs_clientname, "POP3 session");
	CCC->internal_pgm = 1;
	CCC->session_specific_data = malloc(sizeof(struct citpop3));
	memset(POP3, 0, sizeof(struct citpop3));

	cprintf("+OK Citadel POP3 server ready.\r\n");
}


/*
 * POP3S is just like POP3, except it goes crypto right away.
 */
void pop3s_greeting(void)
{
	struct CitContext *CCC = CC;
	CtdlModuleStartCryptoMsgs(NULL, NULL, NULL);

/* kill session if no crypto */
#ifdef HAVE_OPENSSL
	if (!CCC->redirect_ssl) CCC->kill_me = KILLME_NO_CRYPTO;
#else
	CCC->kill_me = KILLME_NO_CRYPTO;
#endif

	pop3_greeting();
}



/*
 * Specify user name (implements POP3 "USER" command)
 */
void pop3_user(char *argbuf)
{
	struct CitContext *CCC = CC;
	char username[SIZ];

	if (CCC->logged_in) {
		cprintf("-ERR You are already logged in.\r\n");
		return;
	}

	strcpy(username, argbuf);
	striplt(username);

	/* POP3_syslog(LOG_DEBUG, "Trying <%s>", username); */
	if (CtdlLoginExistingUser(NULL, username) == login_ok) {
		cprintf("+OK Password required for %s\r\n", username);
	}
	else {
		cprintf("-ERR No such user.\r\n");
	}
}



/*
 * Back end for pop3_grab_mailbox()
 */
void pop3_add_message(long msgnum, void *userdata)
{
	struct CitContext *CCC = CC;
	struct MetaData smi;

	++POP3->num_msgs;
	if (POP3->num_msgs < 2) POP3->msgs = malloc(sizeof(struct pop3msg));
	else POP3->msgs = realloc(POP3->msgs, 
		(POP3->num_msgs * sizeof(struct pop3msg)) ) ;
	POP3->msgs[POP3->num_msgs-1].msgnum = msgnum;
	POP3->msgs[POP3->num_msgs-1].deleted = 0;

	/* We need to know the length of this message when it is printed in
	 * RFC822 format.  Perhaps we have cached this length in the message's
	 * metadata record.  If so, great; if not, measure it and then cache
	 * it for next time.
	 */
	GetMetaData(&smi, msgnum);
	if (smi.meta_rfc822_length <= 0L) {
		CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);

		CtdlOutputMsg(msgnum,
			      MT_RFC822,
			      HEADERS_ALL,
			      0, 1, NULL,
			      SUPPRESS_ENV_TO,
			      NULL, NULL, NULL);

		smi.meta_rfc822_length = StrLength(CCC->redirect_buffer);
		FreeStrBuf(&CCC->redirect_buffer); /* TODO: WHEW, all this for just knowing the length???? */
		PutMetaData(&smi);
	}
	POP3->msgs[POP3->num_msgs-1].rfc822_length = smi.meta_rfc822_length;
}



/*
 * Open the inbox and read its contents.
 * (This should be called only once, by pop3_pass(), and returns the number
 * of messages in the inbox, or -1 for error)
 */
int pop3_grab_mailbox(void)
{
	struct CitContext *CCC = CC;
        visit vbuf;
	int i;

	if (CtdlGetRoom(&CCC->room, MAILROOM) != 0) return(-1);

	/* Load up the messages */
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		pop3_add_message, NULL);

	/* Figure out which are old and which are new */
        CtdlGetRelationship(&vbuf, &CCC->user, &CCC->room);
	POP3->lastseen = (-1);
	if (POP3->num_msgs) for (i=0; i<POP3->num_msgs; ++i) {
		if (is_msg_in_sequence_set(vbuf.v_seen,
		   (POP3->msgs[POP3->num_msgs-1].msgnum) )) {
			POP3->lastseen = i;
		}
	}

	return(POP3->num_msgs);
}

void pop3_login(void)
{
	struct CitContext *CCC = CC;
	int msgs;
	
	msgs = pop3_grab_mailbox();
	if (msgs >= 0) {
		cprintf("+OK %s is logged in (%d messages)\r\n",
			CCC->user.fullname, msgs);
		POP3_syslog(LOG_DEBUG, "POP3 authenticated %s", CCC->user.fullname);
	}
	else {
		cprintf("-ERR Can't open your mailbox\r\n");
	}
	
}


/*
 * Authorize with password (implements POP3 "PASS" command)
 */
void pop3_pass(char *argbuf) {
	char password[SIZ];

	safestrncpy(password, argbuf, sizeof password);
	striplt(password);

	/* POP3_syslog(LOG_DEBUG, "Trying <%s>", password); */
	if (CtdlTryPassword(password, strlen(password)) == pass_ok) {
		pop3_login();
	}
	else {
		cprintf("-ERR That is NOT the password.\r\n");
	}
}



/*
 * list available msgs
 */
void pop3_list(char *argbuf) {
	int i;
	int which_one;

	which_one = atoi(argbuf);

	/* "list one" mode */
	if (which_one > 0) {
		if (which_one > POP3->num_msgs) {
			cprintf("-ERR no such message, only %d are here\r\n",
				POP3->num_msgs);
			return;
		}
		else if (POP3->msgs[which_one-1].deleted) {
			cprintf("-ERR Sorry, you deleted that message.\r\n");
			return;
		}
		else {
			cprintf("+OK %d %ld\r\n",
				which_one,
				(long)POP3->msgs[which_one-1].rfc822_length
				);
			return;
		}
	}

	/* "list all" (scan listing) mode */
	else {
		cprintf("+OK Here's your mail:\r\n");
		if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
			if (! POP3->msgs[i].deleted) {
				cprintf("%d %ld\r\n",
					i+1,
					(long)POP3->msgs[i].rfc822_length);
			}
		}
		cprintf(".\r\n");
	}
}


/*
 * STAT (tally up the total message count and byte count) command
 */
void pop3_stat(char *argbuf) {
	int total_msgs = 0;
	size_t total_octets = 0;
	int i;
	
	if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
		if (! POP3->msgs[i].deleted) {
			++total_msgs;
			total_octets += POP3->msgs[i].rfc822_length;
		}
	}

	cprintf("+OK %d %ld\r\n", total_msgs, (long)total_octets);
}



/*
 * RETR command (fetch a message)
 */
void pop3_retr(char *argbuf) {
	int which_one;

	which_one = atoi(argbuf);
	if ( (which_one < 1) || (which_one > POP3->num_msgs) ) {
		cprintf("-ERR No such message.\r\n");
		return;
	}

	if (POP3->msgs[which_one - 1].deleted) {
		cprintf("-ERR Sorry, you deleted that message.\r\n");
		return;
	}

	cprintf("+OK Message %d:\r\n", which_one);
	CtdlOutputMsg(POP3->msgs[which_one - 1].msgnum,
		      MT_RFC822, HEADERS_ALL, 0, 1, NULL,
		      (ESC_DOT|SUPPRESS_ENV_TO),
		      NULL, NULL, NULL);
	cprintf(".\r\n");
}


/*
 * TOP command (dumb way of fetching a partial message or headers-only)
 */
void pop3_top(char *argbuf)
{
	struct CitContext *CCC = CC;
	int which_one;
	int lines_requested = 0;
	int lines_dumped = 0;
	char buf[1024];
	StrBuf *msgtext;
	const char *ptr;
	int in_body = 0;
	int done = 0;

	sscanf(argbuf, "%d %d", &which_one, &lines_requested);
	if ( (which_one < 1) || (which_one > POP3->num_msgs) ) {
		cprintf("-ERR No such message.\r\n");
		return;
	}

	if (POP3->msgs[which_one - 1].deleted) {
		cprintf("-ERR Sorry, you deleted that message.\r\n");
		return;
	}

	CCC->redirect_buffer = NewStrBufPlain(NULL, SIZ);

	CtdlOutputMsg(POP3->msgs[which_one - 1].msgnum,
		      MT_RFC822,
		      HEADERS_ALL,
		      0, 1, NULL,
		      SUPPRESS_ENV_TO,
		      NULL, NULL, NULL);

	msgtext = CCC->redirect_buffer;
	CCC->redirect_buffer = NULL;

	cprintf("+OK Message %d:\r\n", which_one);
	
	ptr = ChrPtr(msgtext);
	while (ptr = cmemreadline(ptr, buf, (sizeof buf - 2)),
	      ( (*ptr != 0) && (done == 0))) {
		strcat(buf, "\r\n");
		if (in_body == 1) {
			if (lines_dumped >= lines_requested) {
				done = 1;
			}
		}
		if ((in_body == 0) || (done == 0)) {
			client_write(buf, strlen(buf));
		}
		if (in_body) {
			++lines_dumped;
		}
		if ((buf[0]==13)||(buf[0]==10)) in_body = 1;
	}

	if (buf[strlen(buf)-1] != 10) cprintf("\n");
	FreeStrBuf(&msgtext);

	cprintf(".\r\n");
}


/*
 * DELE (delete message from mailbox)
 */
void pop3_dele(char *argbuf) {
	int which_one;

	which_one = atoi(argbuf);
	if ( (which_one < 1) || (which_one > POP3->num_msgs) ) {
		cprintf("-ERR No such message.\r\n");
		return;
	}

	if (POP3->msgs[which_one - 1].deleted) {
		cprintf("-ERR You already deleted that message.\r\n");
		return;
	}

	/* Flag the message as deleted.  Will expunge during QUIT command. */
	POP3->msgs[which_one - 1].deleted = 1;
	cprintf("+OK Message %d deleted.\r\n",
		which_one);
}


/* Perform "UPDATE state" stuff
 */
void pop3_update(void)
{
	struct CitContext *CCC = CC;
	int i;
        visit vbuf;

	long *deletemsgs = NULL;
	int num_deletemsgs = 0;

	/* Remove messages marked for deletion */
	if (POP3->num_msgs > 0) {
		deletemsgs = malloc(POP3->num_msgs * sizeof(long));
		for (i=0; i<POP3->num_msgs; ++i) {
			if (POP3->msgs[i].deleted) {
				deletemsgs[num_deletemsgs++] = POP3->msgs[i].msgnum;
			}
		}
		if (num_deletemsgs > 0) {
			CtdlDeleteMessages(MAILROOM, deletemsgs, num_deletemsgs, "");
		}
		free(deletemsgs);
	}

	/* Set last read pointer */
	if (POP3->num_msgs > 0) {
		CtdlLockGetCurrentUser();

		CtdlGetRelationship(&vbuf, &CCC->user, &CCC->room);
		snprintf(vbuf.v_seen, sizeof vbuf.v_seen, "*:%ld",
			POP3->msgs[POP3->num_msgs-1].msgnum);
		CtdlSetRelationship(&vbuf, &CCC->user, &CCC->room);

		CtdlPutCurrentUserLock();
	}

}


/* 
 * RSET (reset, i.e. undelete any deleted messages) command
 */
void pop3_rset(char *argbuf) {
	int i;

	if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
		if (POP3->msgs[i].deleted) {
			POP3->msgs[i].deleted = 0;
		}
	}
	cprintf("+OK Reset completed.\r\n");
}



/* 
 * LAST (Determine which message is the last unread message)
 */
void pop3_last(char *argbuf) {
	cprintf("+OK %d\r\n", POP3->lastseen + 1);
}


/*
 * CAPA is a command which tells the client which POP3 extensions
 * are supported.
 */
void pop3_capa(void) {
	cprintf("+OK Capability list follows\r\n"
		"TOP\r\n"
		"USER\r\n"
		"UIDL\r\n"
		"IMPLEMENTATION %s\r\n"
		".\r\n"
		,
		CITADEL
	);
}



/*
 * UIDL (Universal IDentifier Listing) is easy.  Our 'unique' message
 * identifiers are simply the Citadel message numbers in the database.
 */
void pop3_uidl(char *argbuf) {
	int i;
	int which_one;

	which_one = atoi(argbuf);

	/* "list one" mode */
	if (which_one > 0) {
		if (which_one > POP3->num_msgs) {
			cprintf("-ERR no such message, only %d are here\r\n",
				POP3->num_msgs);
			return;
		}
		else if (POP3->msgs[which_one-1].deleted) {
			cprintf("-ERR Sorry, you deleted that message.\r\n");
			return;
		}
		else {
			cprintf("+OK %d %ld\r\n",
				which_one,
				POP3->msgs[which_one-1].msgnum
				);
			return;
		}
	}

	/* "list all" (scan listing) mode */
	else {
		cprintf("+OK Here's your mail:\r\n");
		if (POP3->num_msgs > 0) for (i=0; i<POP3->num_msgs; ++i) {
			if (! POP3->msgs[i].deleted) {
				cprintf("%d %ld\r\n",
					i+1,
					POP3->msgs[i].msgnum);
			}
		}
		cprintf(".\r\n");
	}
}


/*
 * implements the STLS command (Citadel API version)
 */
void pop3_stls(void)
{
	char ok_response[SIZ];
	char nosup_response[SIZ];
	char error_response[SIZ];

	sprintf(ok_response,
		"+OK Begin TLS negotiation now\r\n");
	sprintf(nosup_response,
		"-ERR TLS not supported here\r\n");
	sprintf(error_response,
		"-ERR Internal error\r\n");
	CtdlModuleStartCryptoMsgs(ok_response, nosup_response, error_response);
}







/* 
 * Main command loop for POP3 sessions.
 */
void pop3_command_loop(void)
{
	struct CitContext *CCC = CC;
	char cmdbuf[SIZ];

	time(&CCC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		POP3M_syslog(LOG_INFO, "POP3 client disconnected: ending session.");
		CCC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}
	if (!strncasecmp(cmdbuf, "PASS", 4)) {
		POP3M_syslog(LOG_DEBUG, "POP3: PASS...");
	}
	else {
		POP3_syslog(LOG_DEBUG, "POP3: %s", cmdbuf);
	}
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");

	if (!strncasecmp(cmdbuf, "NOOP", 4)) {
		cprintf("+OK No operation.\r\n");
	}

	else if (!strncasecmp(cmdbuf, "CAPA", 4)) {
		pop3_capa();
	}

	else if (!strncasecmp(cmdbuf, "QUIT", 4)) {
		cprintf("+OK Goodbye...\r\n");
		pop3_update();
		CCC->kill_me = KILLME_CLIENT_LOGGED_OUT;
		return;
	}

	else if (!strncasecmp(cmdbuf, "USER", 4)) {
		pop3_user(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "PASS", 4)) {
		pop3_pass(&cmdbuf[5]);
	}

#ifdef HAVE_OPENSSL
	else if (!strncasecmp(cmdbuf, "STLS", 4)) {
		pop3_stls();
	}
#endif

	else if (!CCC->logged_in) {
		cprintf("-ERR Not logged in.\r\n");
	}
	
	else if (CCC->nologin) {
		cprintf("-ERR System busy, try later.\r\n");
		CCC->kill_me = KILLME_NOLOGIN;
	}

	else if (!strncasecmp(cmdbuf, "LIST", 4)) {
		pop3_list(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "STAT", 4)) {
		pop3_stat(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "RETR", 4)) {
		pop3_retr(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "DELE", 4)) {
		pop3_dele(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "RSET", 4)) {
		pop3_rset(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "UIDL", 4)) {
		pop3_uidl(&cmdbuf[5]);
	}

	else if (!strncasecmp(cmdbuf, "TOP", 3)) {
		pop3_top(&cmdbuf[4]);
	}

	else if (!strncasecmp(cmdbuf, "LAST", 4)) {
		pop3_last(&cmdbuf[4]);
	}

	else {
		cprintf("-ERR I'm afraid I can't do that.\r\n");
	}

}

const char *CitadelServicePop3="POP3";
const char *CitadelServicePop3S="POP3S";

void SetPOP3DebugEnabled(const int n)
{
	POP3DebugEnabled = n;
}


CTDL_MODULE_INIT(pop3)
{
	if(!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("pop3srv"), SetPOP3DebugEnabled, &POP3DebugEnabled);

		CtdlRegisterServiceHook(CtdlGetConfigInt("c_pop3_port"),
					NULL,
					pop3_greeting,
					pop3_command_loop,
					NULL,
					CitadelServicePop3);
#ifdef HAVE_OPENSSL
		CtdlRegisterServiceHook(CtdlGetConfigInt("c_pop3s_port"),
					NULL,
					pop3s_greeting,
					pop3_command_loop,
					NULL,
					CitadelServicePop3S);
#endif
		CtdlRegisterSessionHook(pop3_cleanup_function, EVT_STOP, PRIO_STOP + 30);
	}
	
	/* return our module name for the log */
	return "pop3";
}
