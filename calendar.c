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
 * Handler stubs for builds with no calendar library available
 */
void cal_process_attachment(char *part_source) {

	wprintf("<I>This message contains calendaring/scheduling information,"
		" but support for calendars is not available on this "
		"particular system.  Please ask your system administrator to "
		"install a new version of the Citadel web service with "
		"calendaring enabled.</I><BR>\n"
	);

}

void display_calendar(long msgnum) {
	wprintf("<i>Cannot display calendar item</i><br>\n");
}

void display_task(long msgnum) {
	wprintf("<i>Cannot display item from task list</i><br>\n");
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

/*****************************************************************************/





/*
 * Display handlers for message reading
 */
void display_individual_cal(icalcomponent *cal) {
	wprintf("display_individual_cal() called<BR>\n");
}

void display_individual_task(icalcomponent *vtodo) {
	icalproperty *p;

	wprintf("display_individual_task() called<BR>\n");

	for (
	    p = icalcomponent_get_first_property(vtodo,
						ICAL_ANY_PROPERTY);
	    p != 0;
	    p = icalcomponent_get_next_property(vtodo,
						ICAL_ANY_PROPERTY)
	) {

		/* Get a string representation of the property's value 
		wprintf("Prop value: %s<BR>\n",
					icalproperty_get_comment(p) ); */

		/* Spit out the property in its RFC 2445 representation */
		wprintf("<TT>%s</TT><BR>\n",
					icalproperty_as_ical_string(p) );


	}




}

/*
 * Code common to all display handlers.  Given a message number and a MIME
 * type, we load the message and hunt for that MIME type.  If found, we load
 * the relevant part, deserialize it into a libical component, filter it for
 * the requested object type, and feed it to the specified handler.
 */
void display_using_handler(long msgnum,
			char *mimetype,
			icalcomponent_kind which_kind,
			void (*callback)(icalcomponent *)
	) {
	char buf[SIZ];
	char mime_partnum[SIZ];
	char mime_filename[SIZ];
	char mime_content_type[SIZ];
	char mime_disposition[SIZ];
	int mime_length;
	char relevant_partnum[SIZ];
	char *relevant_source = NULL;
	icalcomponent *cal, *c;

	sprintf(buf, "MSG0 %ld|1", msgnum);	/* ask for headers only */
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '1') return;

	while (serv_gets(buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract(mime_filename, &buf[5], 1);
			extract(mime_partnum, &buf[5], 2);
			extract(mime_disposition, &buf[5], 3);
			extract(mime_content_type, &buf[5], 4);
			mime_length = extract_int(&buf[5], 5);

			if (!strcasecmp(mime_content_type, "text/calendar")) {
				strcpy(relevant_partnum, mime_partnum);
			}

		}
	}

	if (strlen(relevant_partnum) > 0) {
		relevant_source = load_mimepart(msgnum, relevant_partnum);
		if (relevant_source != NULL) {

			/* Display the task */
			cal = icalcomponent_new_from_string(relevant_source);
			if (cal != NULL) {
				for (c = icalcomponent_get_first_component(cal,
				    which_kind);
	    			    (c != 0);
	    			    c = icalcomponent_get_next_component(cal,
				    which_kind)) {
					callback(c);
				}
				icalcomponent_free(cal);
			}
			free(relevant_source);
		}
	}

}

void display_calendar(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_ANY_COMPONENT,
				display_individual_cal);
}

void display_task(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				display_individual_task);
}

#endif /* HAVE_ICAL_H */
