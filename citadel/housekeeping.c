/*
 * $Id$
 *
 * This file contains housekeeping tasks which periodically
 * need to be executed.  It keeps a nice little queue...
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/time.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include "tools.h"
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "config.h"
#include "housekeeping.h"
#include "sysdep_decls.h"
#include "room_ops.h"
#include "database.h"


int housepipe[2];	/* This is the queue for housekeeping tasks */


/*
 * Terminate idle sessions.  This function pounds through the session table
 * comparing the current time to each session's time-of-last-command.  If an
 * idle session is found it is terminated, then the search restarts at the
 * beginning because the pointer to our place in the list becomes invalid.
 */
void terminate_idle_sessions(void) {
	struct CitContext *ccptr;
	time_t now;
	int session_to_kill;

	now = time(NULL);
	session_to_kill = 0;
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (  (ccptr!=CC)
	   	&& (config.c_sleeping > 0)
	   	&& (now - (ccptr->lastcmd) > config.c_sleeping) ) {
			ccptr->kill_me = 1;
		}
	}
	end_critical_section(S_SESSION_TABLE);
}



void check_sched_shutdown(void) {
	if ((ScheduledShutdown == 1) && (ContextList == NULL)) {
		lprintf(3, "Scheduled shutdown initiating.\n");
		master_cleanup();
	}
}



/*
 * This is the main loop for the housekeeping thread.  It remains active
 * during the entire run of the server.
 */
void housekeeping_loop(void) {
	long flags;
        struct timeval tv;
        fd_set readfds;
        int did_something;
	char house_cmd[256];	/* Housekeep cmds are always 256 bytes long */
	char cmd[256];

	if (pipe(housepipe) != 0) {
		lprintf(1, "FATAL ERROR: can't create housekeeping pipe: %s\n",
			strerror(errno));
		exit(0);
	}

	flags = (long) fcntl(housepipe[1], F_GETFL);
	flags |= O_NONBLOCK;
	fcntl(housepipe[1], F_SETFL, flags);

	while(1) {
		do {
			did_something = 0;
			tv.tv_sec = HOUSEKEEPING_WAKEUP;
			tv.tv_usec = 0;
                	FD_ZERO(&readfds);
                	FD_SET(housepipe[0], &readfds);
                	select(housepipe[0] + 1, &readfds, NULL, NULL, &tv);
                	if (FD_ISSET(housepipe[0], &readfds)) {
                        	did_something = 1;
			}

			if (did_something) {
				read(housepipe[0], house_cmd, 256);
			}
			else {
				memset(house_cmd, 0, 256);
				strcpy(house_cmd, "MINUTE");
			}

			extract(cmd, house_cmd, 0);
			cdb_begin_transaction();

			/* Do whatever this cmd requires */

			/* Once-every-minute housekeeper */
			if (!strcmp(cmd, "MINUTE")) {
				terminate_idle_sessions();
			}

			/* Scheduled shutdown housekeeper */
			else if (!strcmp(cmd, "SCHED_SHUTDOWN")) {
				check_sched_shutdown();
			}

			/* Unknown */
			else {
				lprintf(7, "Unknown housekeeping command\n");
			}

			cdb_end_transaction();

		} while (did_something);
	}
}






void enter_housekeeping_cmd(char *cmd) {
	char cmdbuf[256];

	lprintf(9, "enter_housekeeping_cmd(%s)\n", cmd);
	safestrncpy(cmdbuf, cmd, 256);
	begin_critical_section(S_HOUSEKEEPING);
	write(housepipe[1], cmdbuf, 256);
	end_critical_section(S_HOUSEKEEPING);
	lprintf(9, "leaving enter_housekeeping_cmd()\n");
}
	


/*
 * Check (and fix) floor reference counts.  This doesn't need to be done
 * very often, since the counts should remain correct during normal operation.
 * NOTE: this function pair should ONLY be called during startup.  It is NOT
 * thread safe.
 */
void check_ref_counts_backend(struct quickroom *qrbuf, void *data) {
	struct floor flbuf;

	getfloor(&flbuf, qrbuf->QRfloor);
	++flbuf.f_ref_count;
	flbuf.f_flags = flbuf.f_flags | QR_INUSE;
	putfloor(&flbuf, qrbuf->QRfloor);
}

void check_ref_counts(void) {
	struct floor flbuf;
	int a;

	lprintf(7, "Checking floor reference counts\n");
	for (a=0; a<MAXFLOORS; ++a) {
		getfloor(&flbuf, a);
		flbuf.f_ref_count = 0;
		flbuf.f_flags = flbuf.f_flags & ~QR_INUSE;
		putfloor(&flbuf, a);
	}

	ForEachRoom(check_ref_counts_backend, NULL);
}	

