/*
 * $Id$
 *
 * Editing calendar events.
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


#ifdef HAVE_ICAL_H


/*
 * Display an event by itself (for editing)
 */
void display_edit_individual_event(icalcomponent *supplied_vevent, long msgnum) {
	icalcomponent *vevent;
	icalproperty *p;
	struct icaltimetype t;
	time_t now;
	int created_new_vevent = 0;

	now = time(NULL);

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>Edit event</B>"
		"</FONT></TD></TR></TABLE><BR>\n"
	);

	wprintf("UID == ");
	p = icalcomponent_get_first_property(vevent, ICAL_UID_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf(" (FIXME remove this when done)<BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/save_event\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgnum\" VALUE=\"%ld\">\n",
		msgnum);
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"calview\" VALUE=\"%s\">\n",
		bstr("calview"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"year\" VALUE=\"%s\">\n",
		bstr("year"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"month\" VALUE=\"%s\">\n",
		bstr("month"));
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"day\" VALUE=\"%s\">\n",
		bstr("day"));

	/* Put it in a borderless table so it lines up nicely */
	wprintf("<TABLE border=0 width=100%%>\n");

	wprintf("<TR><TD><B>Summary</B></TD><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"summary\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"></TD></TR>\n");

	wprintf("<TR><TD><B>Location</B></TD><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"location\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"></TD></TR>\n");

	wprintf("<TR><TD><B>Start</B></TD><TD>\n");
	p = icalcomponent_get_first_property(vevent, ICAL_DTSTART_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_dtstart(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "dtstart");
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD><B>End</B></TD><TD>\n");
	p = icalcomponent_get_first_property(vevent, ICAL_DTEND_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_dtend(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "dtend");
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD><B>Notes</B></TD><TD>\n"
		"<TEXTAREA NAME=\"description\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n"
	);
	p = icalcomponent_get_first_property(vevent, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</TEXTAREA></TD></TR></TABLE>\n");

	wprintf("<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Delete\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n"
		"</CENTER>\n"
	);

	wprintf("</FORM>\n");

	wDumpContent(1);

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}
}

/*
 * Save an edited event
 */
void save_individual_event(icalcomponent *supplied_vevent, long msgnum) {
	char buf[SIZ];
	int delete_existing = 0;
	icalproperty *prop;
	icalcomponent *vevent;
	int created_new_vevent = 0;

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	if (!strcasecmp(bstr("sc"), "Save")) {

		/* Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_summary(bstr("summary")));
		
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_LOCATION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_location(bstr("location")));
		
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_description(bstr("description")));
	
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_dtstart(
				icaltime_from_webform("dtstart")
			)
		);
	
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_due(
				icaltime_from_webform("due")
			)
		);

		/* Give this event a UID if it doesn't have one. */
		if (icalcomponent_get_first_property(vevent,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_new_uid(buf);
			icalcomponent_add_property(vevent,
				icalproperty_new_uid(buf)
			);
		}
	
		/* Serialize it and save it to the message base */
		serv_puts("ENT0 1|||4");
		serv_gets(buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(vevent));
			serv_puts("000");
			delete_existing = 1;
		}
	}

	/*
	 * If the user clicked 'Delete' then delete it, period.
	 */
	if (!strcasecmp(bstr("sc"), "Delete")) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", atol(bstr("msgnum")));
		serv_gets(buf);
	}

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}

	/* Go back to the event list */
	readloop("readfwd");
}


#endif /* HAVE_ICAL_H */
