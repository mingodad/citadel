/*
 * $Id$
 *
 * This file contains miscellaneous housekeeping tasks.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

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
#include <limits.h>
#include <sys/types.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "citserver.h"
#include "config.h"
#include "housekeeping.h"
#include "sysdep_decls.h"
#include "room_ops.h"
#include "database.h"
#include "msgbase.h"
#include "journaling.h"

#include "ctdl_module.h"


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
	int killed = 0;

	now = time(NULL);
	session_to_kill = 0;
	begin_critical_section(S_SESSION_TABLE);
	for (ccptr = ContextList; ccptr != NULL; ccptr = ccptr->next) {
		if (  (ccptr!=CC)
	   	&& (config.c_sleeping > 0)
	   	&& (now - (ccptr->lastcmd) > config.c_sleeping) ) {
			ccptr->kill_me = 1;
			++killed;
		}
	}
	end_critical_section(S_SESSION_TABLE);
	if (killed > 0)
		CtdlLogPrintf(CTDL_INFO, "Terminated %d idle sessions\n", killed);
}



void check_sched_shutdown(void) {
	if ((ScheduledShutdown == 1) && (ContextList == NULL)) {
		CtdlLogPrintf(CTDL_NOTICE, "Scheduled shutdown initiating.\n");
		CtdlThreadStopAll();
	}
}



/*
 * Check (and fix) floor reference counts.  This doesn't need to be done
 * very often, since the counts should remain correct during normal operation.
 */
void check_ref_counts_backend(struct ctdlroom *qrbuf, void *data) {

	int *new_refcounts;

	new_refcounts = (int *) data;

	++new_refcounts[(int)qrbuf->QRfloor];
}

void check_ref_counts(void) {
	struct floor flbuf;
	int a;

	int new_refcounts[MAXFLOORS];

	CtdlLogPrintf(CTDL_DEBUG, "Checking floor reference counts\n");
	for (a=0; a<MAXFLOORS; ++a) {
		new_refcounts[a] = 0;
	}

	cdb_begin_transaction();
	ForEachRoom(check_ref_counts_backend, (void *)new_refcounts );
	cdb_end_transaction();

	for (a=0; a<MAXFLOORS; ++a) {
		lgetfloor(&flbuf, a);
		flbuf.f_ref_count = new_refcounts[a];
		if (new_refcounts[a] > 0) {
			flbuf.f_flags = flbuf.f_flags | QR_INUSE;
		}
		else {
			flbuf.f_flags = flbuf.f_flags & ~QR_INUSE;
		}
		lputfloor(&flbuf, a);
		CtdlLogPrintf(CTDL_DEBUG, "Floor %d: %d rooms\n", a, new_refcounts[a]);
	}
}	

/*
 * This is the housekeeping loop.  Worker threads come through here after
 * processing client requests but before jumping back into the pool.  We
 * only allow housekeeping to execute once per minute, and we only allow one
 * instance to run at a time.
 */
void do_housekeeping(void) {
	static int housekeeping_in_progress = 0;
	static time_t last_timer = 0L;
	int do_housekeeping_now = 0;
	int do_perminute_housekeeping_now = 0;
	time_t now;
	const char *old_name;

	/*
	 * We do it this way instead of wrapping the whole loop in an
	 * S_HOUSEKEEPING critical section because it eliminates the need to
	 * potentially have multiple concurrent mutexes in progress.
	 */
	begin_critical_section(S_HOUSEKEEPING);
	if (housekeeping_in_progress == 0) {
		do_housekeeping_now = 1;
		housekeeping_in_progress = 1;
		now = time(NULL);
		if ( (now - last_timer) > (time_t)60 ) {
			do_perminute_housekeeping_now = 1;
			last_timer = time(NULL);
		}
	}
	end_critical_section(S_HOUSEKEEPING);

	if (do_housekeeping_now == 0) {
		return;
	}

	/*
	 * Ok, at this point we've made the decision to run the housekeeping
	 * loop.  Everything below this point is real work.
	 */

	/* First, do the "as often as needed" stuff... */
	old_name = CtdlThreadName("House Keeping - Journal");
	JournalRunQueue();

	CtdlThreadName("House Keeping - EVT_HOUSE");
	PerformSessionHooks(EVT_HOUSE);	/* perform as needed housekeeping */

	/* Then, do the "once per minute" stuff... */
	if (do_perminute_housekeeping_now) {
		cdb_check_handles();			/* suggested by Justin Case */
		CtdlThreadName("House Keeping - EVT_TIMER");
		PerformSessionHooks(EVT_TIMER);		/* Run any timer hooks */
	}

	/*
	 * All done.
	 */
	housekeeping_in_progress = 0;
	CtdlThreadName(old_name);
}
