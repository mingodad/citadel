/*
 * $Id$
 *
 * Check attendee availability for scheduling a meeting.
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


#ifdef WEBCIT_WITH_CALENDAR_SERVICE

/*
 * Display an event by itself (for editing)
 */
void check_attendee_availability(icalcomponent *vevent) {
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];

	if (vevent == NULL) {
		return;
	}

	/* If we're looking at a fully encapsulated VCALENDAR
	 * rather than a VEVENT component, attempt to use the first
	 * relevant VEVENT subcomponent.  If there is none, the
	 * NULL returned by icalcomponent_get_first_component() will
	 * tell the next iteration of this function to create a
	 * new one.
	 */
	if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
		check_attendee_availability(
			icalcomponent_get_first_component(
				vevent, ICAL_VEVENT_COMPONENT
			)
		);
		return;
	}


	/*
	 * Iterate through attendees.  FIXME do something useful.
	 */
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {

		strcpy(attendee_string, icalproperty_get_attendee(attendee));
		if (!strncasecmp(attendee_string, "MAILTO:", 7)) {

			/* screen name or email address */
			strcpy(attendee_string, &attendee_string[7]);
			striplt(attendee_string);

			/* FIXME do something with attendee_string */
			lprintf(9, "FIXME with <%s>\n", attendee_string);

			/* participant status 
			partstat_as_string(buf, attendee); */
		}
	}

}


#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
