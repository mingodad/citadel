/*
 * $Id$
 *
 * Consolidate mail from remote POP3 accounts.
 *
 * Copyright (c) 2007-2009 by the citadel.org team
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

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

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "ctdl_module.h"
#include "clientsocket.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "database.h"
#include "citadel_dirs.h"

struct pop3aggr {
	struct pop3aggr *next;
	char roomname[ROOMNAMELEN];
	char pop3host[128];
	char pop3user[128];
	char pop3pass[128];
	int keep;
	time_t interval;
};

struct pop3aggr *palist = NULL;


void pop3_do_fetching(char *roomname, char *pop3host, char *pop3user, char *pop3pass, int keep)
{
	int sock;
	char buf[SIZ];
	int msg_to_fetch = 0;
	int *msglist = NULL;
	int num_msgs = 0;
	int alloc_msgs = 0;
	int i;
	char *body = NULL;
	struct CtdlMessage *msg = NULL;
	long msgnum = 0;
	char this_uidl[64];
	char utmsgid[SIZ];
	struct cdbdata *cdbut;
	struct UseTable ut;

	CtdlLogPrintf(CTDL_DEBUG, "POP3: %s %s %s <password>\n", roomname, pop3host, pop3user);
	CtdlLogPrintf(CTDL_NOTICE, "Connecting to <%s>\n", pop3host);
	
	if (CtdlThreadCheckStop())
		return;
		
	sock = sock_connect(pop3host, "110", "tcp");
	if (sock < 0) {
		CtdlLogPrintf(CTDL_ERR, "Could not connect: %s\n", strerror(errno));
		return;
	}
	
	if (CtdlThreadCheckStop())
		goto bail;

	CtdlLogPrintf(CTDL_DEBUG, "Connected!\n");

	/* Read the server greeting */
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	if (CtdlThreadCheckStop())
		goto bail;

	/* Identify ourselves.  NOTE: we have to append a CR to each command.  The LF will
	 * automatically be appended by sock_puts().  Believe it or not, leaving out the CR
	 * will cause problems if the server happens to be Exchange, which is so b0rken it
	 * actually barfs on LF-terminated newlines.
	 */
	snprintf(buf, sizeof buf, "USER %s\r", pop3user);
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	if (CtdlThreadCheckStop())
		goto bail;

	/* Password */
	snprintf(buf, sizeof buf, "PASS %s\r", pop3pass);
	CtdlLogPrintf(CTDL_DEBUG, "<PASS <password>\n");
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	if (CtdlThreadCheckStop())
		goto bail;

	/* Get the list of messages */
	snprintf(buf, sizeof buf, "LIST\r");
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	if (CtdlThreadCheckStop())
		goto bail;

	do {
		if (CtdlThreadCheckStop())
			goto bail;

		if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
		CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
		msg_to_fetch = atoi(buf);
		if (msg_to_fetch > 0) {
			if (alloc_msgs == 0) {
				alloc_msgs = 100;
				msglist = malloc((alloc_msgs * (sizeof(int))));
			}
			else if (num_msgs >= alloc_msgs) {
				alloc_msgs = alloc_msgs * 2;
				msglist = realloc(msglist, (alloc_msgs * sizeof(int)));
			}
			if (msglist == NULL) goto bail;
			msglist[num_msgs++] = msg_to_fetch;
		}
	} while (buf[0] != '.');

	if (num_msgs) for (i=0; i<num_msgs; ++i) {

		/* Find out the UIDL of the message, to determine whether we've already downloaded it */
		snprintf(buf, sizeof buf, "UIDL %d\r", msglist[i]);
		CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
		if (sock_puts(sock, buf) <0) goto bail;
		if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
		CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
		if (strncasecmp(buf, "+OK", 3)) goto bail;
		extract_token(this_uidl, buf, 2, ' ', sizeof this_uidl);

		snprintf(utmsgid, sizeof utmsgid, "pop3/%s/%s@%s", roomname, this_uidl, pop3host);

		if (CtdlThreadCheckStop())
			goto bail;

		cdbut = cdb_fetch(CDB_USETABLE, utmsgid, strlen(utmsgid));
		if (cdbut != NULL) {
			/* message has already been seen */
			CtdlLogPrintf(CTDL_DEBUG, "%s has already been seen\n", utmsgid);
			cdb_free(cdbut);

			/* rewrite the record anyway, to update the timestamp */
			strcpy(ut.ut_msgid, utmsgid);
			ut.ut_timestamp = time(NULL);
			cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid), &ut, sizeof(struct UseTable) );
		}
		else {
			/* Message has not been seen. Tell the server to fetch the message... */
			snprintf(buf, sizeof buf, "RETR %d\r", msglist[i]);
			CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
			if (sock_puts(sock, buf) <0) goto bail;
			if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
			CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
			if (strncasecmp(buf, "+OK", 3)) goto bail;
	
			if (CtdlThreadCheckStop())
				goto bail;

			/* If we get to this point, the message is on its way.  Read it. */
			body = CtdlReadMessageBody(".", config.c_maxmsglen, NULL, 1, sock);
			if (body == NULL) goto bail;
	
			CtdlLogPrintf(CTDL_DEBUG, "Converting message...\n");
			msg = convert_internet_message(body);
			body = NULL;	/* yes, this should be dereferenced, NOT freed */
	
			/* Do Something With It (tm) */
			msgnum = CtdlSubmitMsg(msg, NULL, roomname, 0);
			if (msgnum > 0L) {
				/* Message has been committed to the store */
	
				if (!keep) {
					snprintf(buf, sizeof buf, "DELE %d\r", msglist[i]);
					CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
					if (sock_puts(sock, buf) <0) goto bail;
					if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
					CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf); /* errors here are non-fatal */
				}

				/* write the uidl to the use table so we don't fetch this message again */
				strcpy(ut.ut_msgid, utmsgid);
				ut.ut_timestamp = time(NULL);
				cdb_store(CDB_USETABLE, utmsgid, strlen(utmsgid),
						&ut, sizeof(struct UseTable) );
			}
			CtdlFreeMessage(msg);
		}
	}

	/* Log out */
	snprintf(buf, sizeof buf, "QUIT\r");
	CtdlLogPrintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_getln(sock, buf, sizeof buf) < 0) goto bail;
	CtdlLogPrintf(CTDL_DEBUG, ">%s\n", buf);
bail:	sock_close(sock);
	if (msglist) free(msglist);
}


/*
 * Scan a room's netconfig to determine whether it requires POP3 aggregation
 */
void pop3client_scan_room(struct ctdlroom *qrbuf, void *data)
{
	char filename[PATH_MAX];
	char buf[1024];
	char instr[32];
	FILE *fp;
	struct pop3aggr *pptr;

	if (CtdlThreadCheckStop())
		return;

	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);

	/* Only do net processing for rooms that have netconfigs */
	fp = fopen(filename, "r");
	if (fp == NULL) {
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;

		extract_token(instr, buf, 0, '|', sizeof instr);
		if (!strcasecmp(instr, "pop3client")) {
			pptr = (struct pop3aggr *) malloc(sizeof(struct pop3aggr));
			if (pptr != NULL) {
				safestrncpy(pptr->roomname, qrbuf->QRname, sizeof pptr->roomname);
				extract_token(pptr->pop3host, buf, 1, '|', sizeof pptr->pop3host);
				extract_token(pptr->pop3user, buf, 2, '|', sizeof pptr->pop3user);
				extract_token(pptr->pop3pass, buf, 3, '|', sizeof pptr->pop3pass);
				pptr->keep = extract_int(buf, 4);
				pptr->interval = extract_long(buf, 5);
				pptr->next = palist;
				palist = pptr;
			}
		}

	}

	fclose(fp);

}


void pop3client_scan(void) {
	static time_t last_run = 0L;
	static int doing_pop3client = 0;
	struct pop3aggr *pptr;
	time_t fastest_scan;
	
	if (config.c_pop3_fastest < config.c_pop3_fetch)
		fastest_scan = config.c_pop3_fastest;
	else
		fastest_scan = config.c_pop3_fetch;

	/*
	 * Run POP3 aggregation no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < fastest_scan ) {
		return;
	}

	/*
	 * This is a simple concurrency check to make sure only one pop3client run
	 * is done at a time.  We could do this with a mutex, but since we
	 * don't really require extremely fine granularity here, we'll do it
	 * with a static variable instead.
	 */
	if (doing_pop3client) return;
	doing_pop3client = 1;

	CtdlLogPrintf(CTDL_DEBUG, "pop3client started\n");
	CtdlForEachRoom(pop3client_scan_room, NULL);

	while (palist != NULL && !CtdlThreadCheckStop()) {
		if ((palist->interval && time(NULL) > (last_run + palist->interval))
			|| (time(NULL) > last_run + config.c_pop3_fetch))
				pop3_do_fetching(palist->roomname, palist->pop3host,
					palist->pop3user, palist->pop3pass, palist->keep);
		pptr = palist;
		palist = palist->next;
		free(pptr);
	}

	CtdlLogPrintf(CTDL_DEBUG, "pop3client ended\n");
	last_run = time(NULL);
	doing_pop3client = 0;
}


CTDL_MODULE_INIT(pop3client)
{
	if (!threading)
	{
		CtdlRegisterSessionHook(pop3client_scan, EVT_TIMER);
	}
	
	/* return our Subversion id for the Log */
        return "$Id$";
}
