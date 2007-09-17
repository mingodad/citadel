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


#ifdef POP3_AGGREGATION

void pop3client_do_room(struct ctdlroom *qrbuf, void *data)
{

	lprintf(CTDL_DEBUG, "POP3 aggregation for <%s>\n", qrbuf->QRname);

}


void pop3client_scan(void) {
	static time_t last_run = 0L;
	static int doing_pop3client = 0;

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
	ForEachRoom(pop3client_do_room, NULL);
	lprintf(CTDL_DEBUG, "pop3client ended\n");

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
