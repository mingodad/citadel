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
 * Utility function to fetch a VFREEBUSY type of thing for
 * any specified user.
 */
icalcomponent *get_freebusy_for_user(char *who) {
	char buf[SIZ];
	char *serialized_fb = NULL;
	icalcomponent *fb = NULL;

	serv_printf("ICAL freebusy|%s", who);
	serv_gets(buf);
	if (buf[0] == '1') {
		serialized_fb = read_server_text();
	}

	if (serialized_fb == NULL) {
		return NULL;
	}
	
	fb = icalcomponent_new_from_string(serialized_fb);
	free(serialized_fb);
	if (fb == NULL) {
		return NULL;
	}

	return fb;
}



/*
 * Back end function for check_attendee_availability()
 * This one checks an individual attendee against a supplied
 * event start and end time.  All these fields have already been
 * broken out.  The result is placed in 'annotation'.
 */
void check_individual_attendee(char *attendee_string,
				struct icaltimetype event_start,
				struct icaltimetype event_end,
				char *annotation) {

	icalcomponent *fbc = NULL;
	icalcomponent *fb = NULL;
	icalproperty *thisfb = NULL;

	/* Set to 'unknown' right from the beginning.  Unless we learn
	 * something else, that's what we'll go with.
	 */
	strcpy(annotation, "availability unknown");

	fbc = get_freebusy_for_user(attendee_string);
	if (fbc == NULL) {
		return;
	}

	/* Make sure we're looking at a VFREEBUSY by itself.  What we're probably
	 * looking at initially is a VFREEBUSY encapsulated in a VCALENDAR.
	 */
	if (icalcomponent_isa(fbc) == ICAL_VCALENDAR_COMPONENT) {
		fb = icalcomponent_get_first_component(fbc, ICAL_VFREEBUSY_COMPONENT);
	}
	else if (icalcomponent_isa(fbc) == ICAL_VFREEBUSY_COMPONENT) {
		fb = fbc;
	}

	/* Iterate through all FREEBUSY's looking for conflicts. */
	if (fb != NULL) {

		strcpy(annotation, "free");

		for (thisfb = icalcomponent_get_first_property(fb, ICAL_FREEBUSY_PROPERTY);
		    thisfb != NULL;
		    thisfb = icalcomponent_get_next_property(fb, ICAL_FREEBUSY_PROPERTY) ) {
	
			/* FIXME ... do the check */

		}
	}

	icalcomponent_free(fbc);
}



/*
 * Check the availability of all attendees for an event (when possible)
 * and annotate accordingly.
 */
void check_attendee_availability(icalcomponent *vevent) {
	icalproperty *attendee = NULL;
	icalproperty *dtstart_p = NULL;
	icalproperty *dtend_p = NULL;
	struct icaltimetype dtstart_t;
	struct icaltimetype dtend_t;
	char attendee_string[SIZ];
	char annotated_attendee_string[SIZ];
	char annotation[SIZ];

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
	 * Learn the start and end times.
	 */
	dtstart_p = icalcomponent_get_first_property(vevent, ICAL_DTSTART_PROPERTY);
	if (dtstart_p != NULL) dtstart_t = icalproperty_get_dtstart(dtstart_p);

	dtend_p = icalcomponent_get_first_property(vevent, ICAL_DTEND_PROPERTY);
	if (dtend_p != NULL) dtend_t = icalproperty_get_dtend(dtend_p);

	/*
	 * Iterate through attendees.
	 */
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {

		strcpy(attendee_string, icalproperty_get_attendee(attendee));
		if (!strncasecmp(attendee_string, "MAILTO:", 7)) {

			/* screen name or email address */
			strcpy(attendee_string, &attendee_string[7]);
			striplt(attendee_string);

			check_individual_attendee(attendee_string,
						dtstart_t, dtend_t,
						annotation);

			/* Replace the attendee name with an annotated one. */
			snprintf(annotated_attendee_string, sizeof annotated_attendee_string,
				"MAILTO:%s (%s)", attendee_string, annotation);
			icalproperty_set_attendee(attendee, annotated_attendee_string);

		}
	}

}


#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
