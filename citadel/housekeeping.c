/*
 * This file contains housekeeping tasks which periodically
 * need to be executed.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
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
	
	time(&now);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (  (ccptr!=CC)
		   && (config.c_sleeping > 0)
		   && (now - (ccptr->lastcmd) > config.c_sleeping) ) {
			lprintf(3, "Session %d timed out\n", ccptr->cs_pid);
			kill_session(ccptr->cs_pid);
			ccptr = ContextList;
			}
		}
	}


/*
 * Main housekeeping function.  This gets run whenever a session terminates.
 */
void do_housekeeping(void) {

	begin_critical_section(S_HOUSEKEEPING);
	/*
	 * Terminate idle sessions.
	 */
	lprintf(7, "Calling terminate_idle_sessions()\n");
	terminate_idle_sessions();

	/*
	 * If the server is scheduled to shut down the next time all
	 * users are logged out, now's the time to do it.
	 */
	if ((ScheduledShutdown == 1) && (ContextList == NULL)) {
		lprintf(3, "Scheduled shutdown initiating.\n");
		master_cleanup();
		}
	end_critical_section(S_HOUSEKEEPING);
	}



/*
 * Check (and fix) floor reference counts.  This doesn't need to be done
 * very often, since the counts should remain correct during normal operation.
 * NOTE: this function pair should ONLY be called during startup.  It is NOT
 * thread safe.
 */
void check_ref_counts_backend(struct quickroom *qrbuf) {
	struct floor flbuf;

	lgetfloor(&flbuf, qrbuf->QRfloor);
	++flbuf.f_ref_count;
	flbuf.f_flags = flbuf.f_flags | QR_INUSE;
	lputfloor(&flbuf, qrbuf->QRfloor);
	}

void check_ref_counts(void) {
	struct floor flbuf;
	int a;

	for (a=0; a<MAXFLOORS; ++a) {
		lgetfloor(&flbuf, a);
		flbuf.f_ref_count = 0;
		flbuf.f_flags = flbuf.f_flags & ~QR_INUSE;
		lputfloor(&flbuf, a);
		}

	ForEachRoom(check_ref_counts_backend);
	}	

