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
	struct icaltimetype t_start, t_end;
	time_t now;
	int created_new_vevent = 0;
	icalproperty *organizer = NULL;
	char organizer_string[SIZ];
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	char buf[SIZ];
	int i;
	int organizer_is_me = 0;

	now = time(NULL);
	strcpy(organizer_string, "");
	strcpy(attendee_string, "");

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>"
		"<IMG ALIGN=CENTER SRC=\"/static/vcalendar.gif\">"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>Edit event</B>"
		"</FONT></TD></TR></TABLE><BR>\n"
	);

	/************************************************************
	 * Uncomment this to see the UID in calendar events for debugging
	wprintf("UID == ");
	p = icalcomponent_get_first_property(vevent, ICAL_UID_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	*************************************************************/

	wprintf("<FORM NAME=\"EventForm\" METHOD=\"POST\" ACTION=\"/save_event\">\n");

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
		t_start = icalproperty_get_dtstart(p);
		if (t_start.is_date) {
			t_start.hour = 0;
			t_start.minute = 0;
			t_start.second = 0;
		}
	}
	else {
		memset(&t_start, 0, sizeof t_start);
		t_start.year = atoi(bstr("year"));
		t_start.month = atoi(bstr("month"));
		t_start.day = atoi(bstr("day"));
		if (strlen(bstr("hour")) > 0) {
			t_start.hour = atoi(bstr("hour"));
			t_start.minute = atoi(bstr("minute"));
		}
		else {
			t_start.hour = 9;
			t_start.minute = 0;
		}
		/* t_start = icaltime_from_timet(now, 0); */
	}
	display_icaltimetype_as_webform(&t_start, "dtstart");

	wprintf("<INPUT TYPE=\"checkbox\" NAME=\"alldayevent\" "
		"VALUE=\"yes\" onClick=\"

			if (this.checked) {
				this.form.dtstart_hour.value='0';
				this.form.dtstart_hour.disabled = true;
				this.form.dtstart_minute.value='0';
				this.form.dtstart_minute.disabled = true;
				this.form.dtend_hour.value='0';
				this.form.dtend_hour.disabled = true;
				this.form.dtend_minute.value='0';
				this.form.dtend_minute.disabled = true;
				this.form.dtend_month.disabled = true;
				this.form.dtend_day.disabled = true;
				this.form.dtend_year.disabled = true;
			}
			else {
				this.form.dtstart_hour.disabled = false;
				this.form.dtstart_minute.disabled = false;
				this.form.dtend_hour.disabled = false;
				this.form.dtend_minute.disabled = false;
				this.form.dtend_month.disabled = false;
				this.form.dtend_day.disabled = false;
				this.form.dtend_year.disabled = false;
			}


		\" %s >All day event",
		(t_start.is_date ? "CHECKED" : "" )
	);

	wprintf("</TD></TR>\n");

	/* If this is an all-day-event, set the end time to be identical to
	 * the start time (the hour/minute/second will be set to midnight).
	 * Otherwise extract or create it.
	 */
	wprintf("<TR><TD><B>End</B></TD><TD>\n");
	if (t_start.is_date) {
		t_end = t_start;
	}
	else {
		p = icalcomponent_get_first_property(vevent,
							ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t_end = icalproperty_get_dtend(p);
		}
		else {
			/* If this is not an all-day event and there is no
			 * end time specified, make the default one hour
			 * from the start time.
			 */
			t_end = t_start;
			t_end.hour += 1;
			t_end = icaltime_normalize(t_end);
			/* t_end = icaltime_from_timet(now, 0); */
		}
	}
	display_icaltimetype_as_webform(&t_end, "dtend");
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD><B>Notes</B></TD><TD>\n"
		"<TEXTAREA NAME=\"description\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n"
	);
	p = icalcomponent_get_first_property(vevent, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</TEXTAREA></TD></TR>");

	/* For a new event, the user creating the event should be the
	 * organizer.  Set this field accordingly.
	 */
	if (icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY)
	   == NULL) {
		sprintf(organizer_string, "MAILTO:%s", WC->cs_inet_email);
		icalcomponent_add_property(vevent,
			icalproperty_new_organizer(organizer_string)
		);
	}

	/* Determine who is the organizer of this event.
	 * We need to determine "me" or "not me."
	 */
	organizer = icalcomponent_get_first_property(vevent,
						ICAL_ORGANIZER_PROPERTY);
	if (organizer != NULL) {
		strcpy(organizer_string, icalproperty_get_organizer(organizer));
		if (!strncasecmp(organizer_string, "MAILTO:", 7)) {
			strcpy(organizer_string, &organizer_string[7]);
			striplt(organizer_string);
			serv_printf("ISME %s", organizer_string);
			serv_gets(buf);
			if (buf[0] == '2') {
				organizer_is_me = 1;
			}
		}
	}
	wprintf("<TR><TD><B>Organizer</B></TD><TD>");
	escputs(organizer_string);
	if (organizer_is_me) {
		wprintf(" <FONT SIZE=-1><I>"
			"(you are the organizer)</I></FONT>\n");
	}

	/*
	 * Transmit the organizer as a hidden field.   We don't want the user
	 * to be able to change it, but we do want it fed back to the server,
	 * especially if this is a new event and there is no organizer already
	 * in the calendar object.
	 */
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"organizer\" VALUE=\"");
	escputs(organizer_string);
	wprintf("\">");

	wprintf("</TD></TR>\n");

	/* Attendees (do more with this later) */
	wprintf("<TR><TD><B>Attendes</B><BR>"
		"<FONT SIZE=-2>(Separate multiple attendees with commas)"
		"</FONT></TD><TD>"
		"<TEXTAREA %s NAME=\"attendees\" wrap=soft "
		"ROWS=3 COLS=80 WIDTH=80>\n",
		(organizer_is_me ? "" : "DISABLED ")
	);
	i = 0;
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
		strcpy(attendee_string, icalproperty_get_attendee(attendee));
		if (!strncasecmp(attendee_string, "MAILTO:", 7)) {
			strcpy(attendee_string, &attendee_string[7]);
			striplt(attendee_string);
			if (i++) wprintf(", ");
			escputs(attendee_string);
		}
	}
	wprintf("</TEXTAREA></TD></TR>\n");

	/* Done with properties. */
	wprintf("</TABLE>\n<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Delete\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n"
		"</CENTER>\n"
	);

	wprintf("</FORM>\n");
	
	wprintf("<SCRIPT language=\"javascript\">
		<!--

			if (document.EventForm.alldayevent.checked) {
				document.EventForm.dtstart_hour.value='0';
				document.EventForm.dtstart_hour.disabled = true;
				document.EventForm.dtstart_minute.value='0';
				document.EventForm.dtstart_minute.disabled = true;
				document.EventForm.dtend_hour.value='0';
				document.EventForm.dtend_hour.disabled = true;
				document.EventForm.dtend_minute.value='0';
				document.EventForm.dtend_minute.disabled = true;
				document.EventForm.dtend_month.disabled = true;
				document.EventForm.dtend_day.disabled = true;
				document.EventForm.dtend_year.disabled = true;
			}
			else {
				document.EventForm.dtstart_hour.disabled = false;
				document.EventForm.dtstart_minute.disabled = false;
				document.EventForm.dtend_hour.disabled = false;
				document.EventForm.dtend_minute.disabled = false;
				document.EventForm.dtend_month.disabled = false;
				document.EventForm.dtend_day.disabled = false;
				document.EventForm.dtend_year.disabled = false;
			}
		//-->
		</SCRIPT>
	");

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
	icalproperty *prop;
	icalcomponent *vevent;
	int created_new_vevent = 0;
	int all_day_event = 0;
	struct icaltimetype event_start;
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	int i;
	int foundit;
	char form_attendees[SIZ];
	char organizer_string[SIZ];

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
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_summary(bstr("summary")));
		
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_LOCATION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_location(bstr("location")));
		
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vevent,
			icalproperty_new_description(bstr("description")));
	
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

		if (!strcmp(bstr("alldayevent"), "yes")) {
			all_day_event = 1;
		}
		else {
			all_day_event = 0;
		}

		event_start = icaltime_from_webform("dtstart");
		if (all_day_event) {
			event_start.is_date = 1;
			event_start.hour = 0;
			event_start.minute = 0;
			event_start.second = 0;
		}


		/* The following odd-looking snippet of code looks like it
		 * takes some unnecessary steps.  It is done this way because
		 * libical incorrectly turns an "all day event" into a normal
		 * event starting at midnight (i.e. it serializes as date/time
		 * instead of just date) unless icalvalue_new_date() is used.
		 * So we force it, if this is an all day event.
		 */
		prop = icalproperty_new_dtstart(event_start);
		if (all_day_event) {
			icalproperty_set_value(prop,
				icalvalue_new_date(event_start)
			);
		}

		if (prop) icalcomponent_add_property(vevent, prop);
		else icalproperty_free(prop);



		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DTEND_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DURATION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

		if (all_day_event == 0) {
			icalcomponent_add_property(vevent,
				icalproperty_new_dtend(icaltime_normalize(
					icaltime_from_webform("dtend"))
				)
			);
		}

		/* Give this event a UID if it doesn't have one. */
		if (icalcomponent_get_first_property(vevent,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_new_uid(buf);
			icalcomponent_add_property(vevent,
				icalproperty_new_uid(buf)
			);
		}

		/* Set the organizer, only if one does not already exist *and*
		 * the form is supplying one
		 */
		strcpy(buf, bstr("organizer"));
		if ( (icalcomponent_get_first_property(vevent,
		   ICAL_ORGANIZER_PROPERTY) == NULL) 
		   && (strlen(buf) > 0) ) {

			/* set new organizer */
			sprintf(organizer_string, "MAILTO:%s", buf);
			icalcomponent_add_property(vevent,
                        	icalproperty_new_organizer(organizer_string)
			);

		}

		/*
		 * Add any new attendees listed in the web form
		 */
		strcpy(form_attendees, bstr("attendees"));
		for (i=0; i<num_tokens(form_attendees, ','); ++i) {
			extract_token(buf, form_attendees, i, ',');
			striplt(buf);
			if (strlen(buf) > 0) {
				lprintf(9, "Attendee: <%s>\n", buf);
				sprintf(attendee_string, "MAILTO:%s", buf);
				foundit = 0;

				for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
					if (!strcasecmp(attendee_string,
					   icalproperty_get_attendee(attendee)))
						++foundit;
				}


				if (foundit == 0) {
					icalcomponent_add_property(vevent,
						icalproperty_new_attendee(attendee_string)
					);
				}
			}
		}

		/*
		 * Remove any attendees *not* listed in the web form
		 */
STARTOVER:
		for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
			strcpy(attendee_string, icalproperty_get_attendee(attendee));
			if (!strncasecmp(attendee_string, "MAILTO:", 7)) {
				strcpy(attendee_string, &attendee_string[7]);
				striplt(attendee_string);
				foundit = 0;
				for (i=0; i<num_tokens(form_attendees, ','); ++i) {
					extract_token(buf, form_attendees, i, ',');
					striplt(buf);
					if (!strcasecmp(buf, attendee_string)) ++foundit;
				}
				if (foundit == 0) {
					icalcomponent_remove_property(vevent, attendee);
					icalproperty_free(attendee);
					goto STARTOVER;
				}
			}
		}

		/*
		 * Serialize it and save it to the message base
		 */
		serv_puts("ENT0 1|||4");
		serv_gets(buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(vevent));
			serv_puts("000");
		}
	}

	/*
	 * If the user clicked 'Delete' then delete it.
	 */
	if ( (!strcasecmp(bstr("sc"), "Delete")) && (msgnum > 0L) ) {
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
