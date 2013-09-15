/*
 * This file contains miscellaneous housekeeping tasks.
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <libcitadel.h>

#include "ctdl_module.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "internet_addressing.h"
#include "journaling.h"

void check_sched_shutdown(void) {
	if ((ScheduledShutdown == 1) && (ContextList == NULL)) {
		syslog(LOG_NOTICE, "Scheduled shutdown initiating.\n");
		server_shutting_down = 1;
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

	syslog(LOG_DEBUG, "Checking floor reference counts\n");
	for (a=0; a<MAXFLOORS; ++a) {
		new_refcounts[a] = 0;
	}

	cdb_begin_transaction();
	CtdlForEachRoom(check_ref_counts_backend, (void *)new_refcounts );
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
		syslog(LOG_DEBUG, "Floor %d: %d rooms\n", a, new_refcounts[a]);
	}
}	

/*
 * This is the housekeeping loop.  Worker threads come through here after
 * processing client requests but before jumping back into the pool.  We
 * only allow housekeeping to execute once per minute, and we only allow one
 * instance to run at a time.
 */
static int housekeeping_in_progress = 0;
static time_t last_timer = 0L;
void do_housekeeping(void) {
	int do_housekeeping_now = 0;
	int do_perminute_housekeeping_now = 0;
	time_t now;

	/*
	 * We do it this way instead of wrapping the whole loop in an
	 * S_HOUSEKEEPING critical section because it eliminates the need to
	 * potentially have multiple concurrent mutexes in progress.
	 */
	begin_critical_section(S_HOUSEKEEPING);
	if (housekeeping_in_progress == 0) {
		do_housekeeping_now = 1;
		housekeeping_in_progress = 1;
	}
	end_critical_section(S_HOUSEKEEPING);

	if (do_housekeeping_now == 0) {
		return;
	}

	/*
	 * Ok, at this point we've made the decision to run the housekeeping
	 * loop.  Everything below this point is real work.
	 */

	now = time(NULL);
	if ( (now - last_timer) > (time_t)60 ) {
		do_perminute_housekeeping_now = 1;
		last_timer = time(NULL);
	}

	/* First, do the "as often as needed" stuff... */
	JournalRunQueue();
	PerformSessionHooks(EVT_HOUSE);

	/* Then, do the "once per minute" stuff... */
	if (do_perminute_housekeeping_now) {
		cdb_check_handles();			/* suggested by Justin Case */
		PerformSessionHooks(EVT_TIMER);		/* Run any timer hooks */
	}

	/*
	 * All done.
	 */
	begin_critical_section(S_HOUSEKEEPING);
	housekeeping_in_progress = 0;
	end_critical_section(S_HOUSEKEEPING);
}
