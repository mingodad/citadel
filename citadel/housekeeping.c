/*
 * This file contains housekeeping tasks which periodically
 * need to be executed.
 *
 * $Id$
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "config.h"
#include "housekeeping.h"
#include "sysdep_decls.h"
#include "room_ops.h"

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

	do {
		now = time(NULL);
		session_to_kill = 0;
		lprintf(9, "Scanning for timed out sessions...\n");
		begin_critical_section(S_SESSION_TABLE);
		for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
			if (  (ccptr!=CC)
		   	&& (config.c_sleeping > 0)
		   	&& (now - (ccptr->lastcmd) > config.c_sleeping) ) {
				session_to_kill = ccptr->cs_pid;
				}
			}
		end_critical_section(S_SESSION_TABLE);
		lprintf(9, "...done scanning.\n");
		if (session_to_kill > 0) {
			lprintf(3, "Session %d timed out.  Terminating it...\n",
				session_to_kill);
			kill_session(session_to_kill);
			lprintf(9, "...done terminating it.\n");
			}
		} while(session_to_kill > 0);
	}


/*
 * Main housekeeping function.  This gets run whenever a session terminates.
 */
void do_housekeeping(void) {

	lprintf(9, "--- begin housekeeping ---\n");
	begin_critical_section(S_HOUSEKEEPING);
	/*
	 * Terminate idle sessions.
	 */
	lprintf(7, "Calling terminate_idle_sessions()\n");
	terminate_idle_sessions();
	lprintf(9, "Done with terminate_idle_sessions()\n");

	/*
	 * If the server is scheduled to shut down the next time all
	 * users are logged out, now's the time to do it.
	 */
	if ((ScheduledShutdown == 1) && (ContextList == NULL)) {
		lprintf(3, "Scheduled shutdown initiating.\n");
		master_cleanup();
		}
	end_critical_section(S_HOUSEKEEPING);
	lprintf(9, "--- end housekeeping ---\n");
	}



/*
 * Check (and fix) floor reference counts.  This doesn't need to be done
 * very often, since the counts should remain correct during normal operation.
 * NOTE: this function pair should ONLY be called during startup.  It is NOT
 * thread safe.
 */
void check_ref_counts_backend(struct quickroom *qrbuf) {
	struct floor flbuf;

	getfloor(&flbuf, qrbuf->QRfloor);
	++flbuf.f_ref_count;
	flbuf.f_flags = flbuf.f_flags | QR_INUSE;
	putfloor(&flbuf, qrbuf->QRfloor);
	}

void check_ref_counts(void) {
	struct floor flbuf;
	int a;

	for (a=0; a<MAXFLOORS; ++a) {
		getfloor(&flbuf, a);
		flbuf.f_ref_count = 0;
		flbuf.f_flags = flbuf.f_flags & ~QR_INUSE;
		putfloor(&flbuf, a);
		}

	ForEachRoom(check_ref_counts_backend);
	}	

