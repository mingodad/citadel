/*
 * $Id$
 *
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "webcit.h"
#include "webserver.h"

#ifndef HAVE_ICAL_H

void do_calendar_view(void) {	/* stub for non-libical builds */
	wprintf("<CENTER><I>Calendar view not available</I></CENTER><BR>\n");
}

#else	/* HAVE_ICAL_H */

/****************************************************************************/

/*
 * We loaded calendar events into memory during a pass through the
 * messages in this room ... now display them.
 */
void do_calendar_view(void) {
	wprintf("<CENTER><I>Calendar view not available</I></CENTER><BR>\n");
}


#endif	/* HAVE_ICAL_H */
