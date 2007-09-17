/*
 * Aggregate remote POP3 accounts
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
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "room_ops.h"
#include "ctdl_module.h"
#include "clientsocket.h"

struct pop3aggr {
	struct pop3aggr *next;
	char roomname[ROOMNAMELEN];
	char pop3host[128];
	char pop3user[128];
	char pop3pass[128];
};

struct pop3aggr *palist = NULL;

#ifdef POP3_AGGREGATION


void pop3_do_fetching(char *roomname, char *pop3host, char *pop3user, char *pop3pass)
{
	int sock;
	char buf[SIZ];

	lprintf(CTDL_DEBUG, "POP3: %s %s %s %s\n", roomname, pop3host, pop3user, pop3pass);
	lprintf(CTDL_NOTICE, "Connecting to <%s>\n", pop3host);
	sock = sock_connect(pop3host, "110", "tcp");
	if (sock < 0) {
		lprintf(CTDL_ERR, "Could not connect: %s\n", strerror(errno));
		return;
	}
	
	lprintf(CTDL_DEBUG, "Connected!\n");

	/* Read the server greeting */
	if (sock_gets(sock, buf) < 0) goto bail;
	lprintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	/* Identify ourselves */
	snprintf(buf, sizeof buf, "USER %s", pop3user);
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_gets(sock, buf) < 0) goto bail;
	lprintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	/* Password */
	snprintf(buf, sizeof buf, "PASS %s", pop3pass);
	lprintf(CTDL_DEBUG, "<%s\n", buf);
	if (sock_puts(sock, buf) <0) goto bail;
	if (sock_gets(sock, buf) < 0) goto bail;
	lprintf(CTDL_DEBUG, ">%s\n", buf);
	if (strncasecmp(buf, "+OK", 3)) goto bail;

	sock_puts(sock, "QUIT");
bail:	sock_close(sock);
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

	/*
	 * Run POP3 aggregation no more frequently than once every n seconds
	 */
	if ( (time(NULL) - last_run) < config.c_net_freq ) {
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

	lprintf(CTDL_DEBUG, "pop3client started\n");
	ForEachRoom(pop3client_scan_room, NULL);

	while (palist != NULL) {
		pop3_do_fetching(palist->roomname, palist->pop3host, palist->pop3user, palist->pop3pass);
		pptr = palist;
		palist = palist->next;
		free(pptr);
	}

	lprintf(CTDL_DEBUG, "pop3client ended\n");
	last_run = time(NULL);
	doing_pop3client = 0;
}

#endif

CTDL_MODULE_INIT(pop3client)
{
#ifdef POP3_AGGREGATION
	CtdlRegisterSessionHook(pop3client_scan, EVT_TIMER);
#endif

	/* return our Subversion id for the Log */
        return "$Id:  $";
}
