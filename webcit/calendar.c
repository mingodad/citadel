/*
 * $Id$
 *
 * Functions which handle calendar objects and their processing/display.
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
#include "webcit.h"
#include "webserver.h"

#ifdef HAVE_ICAL_H
#include <ical.h>
#endif


#ifndef HAVE_ICAL_H

/*
 * Handler stub for builds with no calendar library available
 */
void cal_process_attachment(char *part_source) {

	wprintf("<I>This message contains calendaring/scheduling information,"
		" but support for calendars is not available on this "
		"particular system.  Please ask your system administrator to "
		"install a new version of the Citadel web service with "
		"calendaring enabled.</I><BR>\n"
	);

}

#else /* HAVE_ICAL_H */



/*
 * Process a single calendar component.
 * It won't be a compound component at this point because those have
 * already been broken down by cal_process_object().
 */
void cal_process_subcomponent(icalcomponent *cal) {
	wprintf("cal_process_subcomponent() called<BR>\n");
	wprintf("cal_process_subcomponent() exiting<BR>\n");
}





/*
 * Process a calendar object
 * ...at this point it's already been deserialized by cal_process_attachment()
 */
void cal_process_object(icalcomponent *cal) {
	icalcomponent *c;
	int num_subcomponents = 0;

	wprintf("cal_process_object() called<BR>\n");

	/* Iterate through all subcomponents */
	wprintf("Iterating through all sub-components<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		cal_process_subcomponent(c);
		++num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VEVENTs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VEVENT_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VEVENT_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VTODOs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VTODO_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VTODO_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VJOURNALs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VJOURNAL_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VJOURNAL_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VCALENDARs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VCALENDAR_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VCALENDAR_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VFREEBUSYs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VFREEBUSY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VFREEBUSY_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VALARMs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VALARM_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VALARM_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VTIMEZONEs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VTIMEZONE_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VTIMEZONE_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VSCHEDULEs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VSCHEDULE_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VSCHEDULE_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VQUERYs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VQUERY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VQUERY_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VCARs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VCAR_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VCAR_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	/* Iterate through all subcomponents */
	wprintf("Iterating through VCOMMANDs<BR>\n");
	for (c = icalcomponent_get_first_component(cal, ICAL_VCOMMAND_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_VCOMMAND_COMPONENT)) {
		cal_process_subcomponent(c);
		--num_subcomponents;
	}

	if (num_subcomponents != 0) {
		wprintf("Warning: %d subcomponents unhandled<BR>\n",
			num_subcomponents);
	}

	wprintf("cal_process_object() exiting<BR>\n");
}


/*
 * Deserialize a calendar object in a message so it can be processed.
 * (This is the main entry point for these things)
 */
void cal_process_attachment(char *part_source) {
	icalcomponent *cal;

	wprintf("Processing calendar attachment<BR>\n");
	cal = icalcomponent_new_from_string(part_source);

	if (cal == NULL) {
		wprintf("Error parsing calendar object: %s<BR>\n",
			icalerror_strerror(icalerrno));
		return;
	}

	cal_process_object(cal);

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}

#endif /* HAVE_ICAL_H */
