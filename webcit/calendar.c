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
#include <time.h>
#include "webcit.h"
#include "webserver.h"

#ifndef WEBCIT_WITH_CALENDAR_SERVICE

/*
 * Handler stubs for builds with no calendar library available
 */
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum) {

	wprintf("<I>This message contains calendaring/scheduling information,"
		" but support for calendars is not available on this "
		"particular system.  Please ask your system administrator to "
		"install a new version of the Citadel web service with "
		"calendaring enabled.</I><BR>\n"
	);

}

void display_calendar(long msgnum) {
	wprintf("<i>"
		"Cannot display calendar item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br>\n");
}

void display_task(long msgnum) {
	wprintf("<i>"
		"Cannot display to-do item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br>\n");
}

#else /* WEBCIT_WITH_CALENDAR_SERVICE */


/******   End of handler stubs.  Everything below this line is real.   ******/




/*
 * Process a calendar object
 * ...at this point it's already been deserialized by cal_process_attachment()
 *
 * ok for complete vcalendar objects
 *
 */
void cal_process_object(icalcomponent *cal,
			int recursion_level,
			long msgnum,
			char *cal_partnum
) {
	icalcomponent *c;
	icalproperty *method = NULL;
	icalproperty_method the_method = ICAL_METHOD_NONE;
	icalproperty *p;
	struct icaltimetype t;
	time_t tt;
	char buf[SIZ];
	char conflict_name[SIZ];
	int is_update = 0;

	/* Leading HTML for the display of this object */
	if (recursion_level == 0) {
		wprintf("<CENTER><TABLE border=0>\n");
	}

	/* Look for a method */
	method = icalcomponent_get_first_property(cal, ICAL_METHOD_PROPERTY);

	/* See what we need to do with this */
	if (method != NULL) {
		the_method = icalproperty_get_method(method);
		switch(the_method) {
		    case ICAL_METHOD_REQUEST:
			wprintf("<TR><TD COLSPAN=2>\n"
				"<IMG ALIGN=CENTER "
				"SRC=\"/static/vcalendar.gif\">"
				"&nbsp;&nbsp;"	
				"<B>Meeting invitation</B>
				</TD></TR>\n"
			);
			break;
		    case ICAL_METHOD_REPLY:
			wprintf("<TR><TD COLSPAN=2>\n"
				"<IMG ALIGN=CENTER "
				"SRC=\"/static/vcalendar.gif\">"
				"&nbsp;&nbsp;"	
				"<B>Attendee's reply to your invitation</B>
				</TD></TR>\n"
			);
			break;
		    case ICAL_METHOD_PUBLISH:
			wprintf("<TR><TD COLSPAN=2>\n"
				"<IMG ALIGN=CENTER "
				"SRC=\"/static/vcalendar.gif\">"
				"&nbsp;&nbsp;"	
				"<B>Published event</B>
				</TD></TR>\n"
			);
			break;
		    default:
			wprintf("<TR><TD COLSPAN=2>"
				"I don't know what to do with this.</TD></TR>"
				"\n");
			break;
		}
	}

      	p = icalcomponent_get_first_property(cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>Summary:</B></TD><TD>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</TD></TR>\n");
	}

      	p = icalcomponent_get_first_property(cal, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>Location:</B></TD><TD>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</TD></TR>\n");
	}

	/*
	 * Only show start/end times if we're actually looking at the VEVENT
	 * component.  Otherwise it shows bogus dates for things like timezone.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {

      		p = icalcomponent_get_first_property(cal,
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);

			if (t.is_date) {
				wprintf("<TR><TD><B>Date:"
					"</B></TD><TD>"
					"%s %d, %d</TD></TR>",
					months[t.month - 1],
					t.day, t.year
				);
			}
			else {
				tt = icaltime_as_timet(t);
				fmt_date(buf, tt);
				wprintf("<TR><TD><B>Starting date/time:"
					"</B></TD><TD>"
					"%s</TD></TR>", buf
				);
			}
		}
	
      		p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtend(p);
			tt = icaltime_as_timet(t);
			fmt_date(buf, tt);
			wprintf("<TR><TD><B>Ending date/time:</B></TD><TD>"
				"%s</TD></TR>", buf
			);
		}

	}

      	p = icalcomponent_get_first_property(cal, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>Description:</B></TD><TD>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</TD></TR>\n");
	}

	/* If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); (p != NULL); p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		wprintf("<TR><TD><B>Attendee:</B></TD><TD>");
		strcpy(buf, icalproperty_get_attendee(p));
		if (!strncasecmp(buf, "MAILTO:", 7)) {

			/* screen name or email address */
			strcpy(buf, &buf[7]);
			striplt(buf);
			escputs(buf);
			wprintf(" ");

			/* participant status */
			partstat_as_string(buf, p);
			escputs(buf);
		}
		wprintf("</TD></TR>\n");
	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		cal_process_object(c, recursion_level+1, msgnum, cal_partnum);
	}

	/* If this is a REQUEST, display conflicts and buttons */
	if (the_method == ICAL_METHOD_REQUEST) {

		/* Check for conflicts */
		lprintf(9, "Checking server calendar for conflicts...\n");
		serv_printf("ICAL conflicts|%ld|%s|", msgnum, cal_partnum);
		serv_gets(buf);
		if (buf[0] == '1') {
			while (serv_gets(buf), strcmp(buf, "000")) {
				extract(conflict_name, buf, 3);
				is_update = extract_int(buf, 4);
				wprintf("<TR><TD><B><I>%s</I></B></TD>"
					"<TD>"
					"%s "
					"<I>&quot;",

					(is_update ?
						"Update:" :
						"CONFLICT:"
					),

					(is_update ?
						"This is an update of" :
						"This event would conflict with"
					)
		
				);
				escputs(conflict_name);
				wprintf("&quot;</I> "
					"which is already in your calendar."
					"</TD></TR>\n");
			}
		}
		lprintf(9, "...done.\n");

		/* Display the Accept/Decline buttons */
		wprintf("<TR><TD COLSPAN=2>"
			"<FORM METHOD=\"GET\" "
			"ACTION=\"/respond_to_request\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Accept\">\n"
			"&nbsp;&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Tentative\">\n"
			"&nbsp;&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Decline\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"msgnum\" "
				"VALUE=\"%ld\">"
			"<INPUT TYPE=\"hidden\" NAME=\"cal_partnum\" "
				"VALUE=\"%s\">"
			"</FORM>"
			"</TD></TR>\n",
			msgnum, cal_partnum
		);

	}

	/* If this is a REPLY, display update button */
	if (the_method == ICAL_METHOD_REPLY) {

		/***********
		 * In the future, if we want to validate this object before
		 * continuing, we can do it this way:
		serv_printf("ICAL whatever|%ld|%s|", msgnum, cal_partnum);
		serv_gets(buf);
		}
		 ***********/

		/* Display the update buttons */
		wprintf("<TR><TD COLSPAN=2>"
			"Click <i>Update</i> to accept this reply and "
			"update your calendar."
			"<FORM METHOD=\"GET\" "
			"ACTION=\"/handle_rsvp\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Update\">\n"
			"&nbsp;&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Ignore\">\n"
			"<INPUT TYPE=\"hidden\" NAME=\"msgnum\" "
				"VALUE=\"%ld\">"
			"<INPUT TYPE=\"hidden\" NAME=\"cal_partnum\" "
				"VALUE=\"%s\">"
			"</FORM>"
			"</TD></TR>\n",
			msgnum, cal_partnum
		);

	}

	/* Trailing HTML for the display of this object */
	if (recursion_level == 0) {

		wprintf("</TR></TABLE></CENTER>\n");
	}
}


/*
 * Deserialize a calendar object in a message so it can be processed.
 * (This is the main entry point for these things)
 * ok for complete vcalendar objects
 */
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum) {
	icalcomponent *cal;

	cal = icalcomponent_new_from_string(part_source);

	if (cal == NULL) {
		wprintf("Error parsing calendar object<BR>\n");
		return;
	}

	ical_dezonify(cal);
	cal_process_object(cal, 0, msgnum, cal_partnum);

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}




/*
 * Respond to a meeting request
 */
void respond_to_request(void) {
	char buf[SIZ];

	output_headers(3);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Respond to meeting request</SPAN>"
		"</TD></TR></TABLE><BR>\n"
	);

	serv_printf("ICAL respond|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_gets(buf);

	if (buf[0] == '2') {
		wprintf("<TABLE BORDER=0><TR><TD>"
			"<IMG SRC=\"static/vcalendar.gif\" ALIGN=CENTER>"
			"</TD><TD>"
		);
		if (!strcasecmp(bstr("sc"), "accept")) {
			wprintf("You have accepted this meeting invitation.  "
				"It has been entered into your calendar, "
			);
		} else if (!strcasecmp(bstr("sc"), "tentative")) {
			wprintf("You have tentatively accepted this meeting invitation.  "
				"It has been 'pencilled in' to your calendar, "
			);
		} else if (!strcasecmp(bstr("sc"), "decline")) {
			wprintf("You have declined this meeting invitation.  "
				"It has <b>not</b> been entered into your calendar, "
			);
		}
		wprintf("and a reply has been sent to the meeting organizer."
			"</TD></TR></TABLE>\n"
		);
	} else {
		wprintf("<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
			"%s\n", &buf[4]);
	}

	wprintf("<A HREF=\"/dotskip?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\"><BR>Return to messages</A><BR>\n");

	wDumpContent(1);
}



/*
 * Handle an incoming RSVP
 */
void handle_rsvp(void) {
	char buf[SIZ];

	output_headers(3);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">"
		"Update your calendar with this RSVP</SPAN>"
		"</TD></TR></TABLE><BR>\n"
	);

	serv_printf("ICAL handle_rsvp|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_gets(buf);

	if (buf[0] == '2') {
		wprintf("<TABLE BORDER=0><TR><TD>"
			"<IMG SRC=\"static/vcalendar.gif\" ALIGN=CENTER>"
			"</TD><TD>"
		);
		if (!strcasecmp(bstr("sc"), "update")) {
			wprintf("Your calendar has been updated "
				"to reflect this RSVP."
			);
		} else if (!strcasecmp(bstr("sc"), "ignore")) {
			wprintf("You have chosen to ignore this RSVP. "
				"Your calendar has <b>not</b> been updated."
			);
		}
		wprintf("</TD></TR></TABLE>\n"
		);
	} else {
		wprintf("<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
			"%s\n", &buf[4]);
	}

	wprintf("<A HREF=\"/dotskip?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\"><BR>Return to messages</A><BR>\n");

	wDumpContent(1);
}




/*****************************************************************************/



/*
 * Display handlers for message reading
 */



/*
 * If we're reading calendar items, just store them for now.  We have to
 * sort and re-output them later when we draw the calendar.
 */
void display_individual_cal(icalcomponent *cal, long msgnum) {

	WC->num_cal += 1;

	WC->disp_cal = realloc(WC->disp_cal,
			(sizeof(icalcomponent *) * WC->num_cal) );
	WC->disp_cal[WC->num_cal - 1] = icalcomponent_new_clone(cal);

	WC->cal_msgnum = realloc(WC->cal_msgnum,
			(sizeof(long) * WC->num_cal) );
	WC->cal_msgnum[WC->num_cal - 1] = msgnum;
}



/*
 * Display a task in the task list
 */
void display_individual_task(icalcomponent *vtodo, long msgnum) {
	icalproperty *p;

	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	wprintf("<A HREF=\"/display_edit_task?msgnum=%ld&taskrm=", msgnum);
	urlescputs(WC->wc_roomname);
	wprintf("\">");
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</A><BR>\n");
}


/*
 * Display a task by itself (for editing)
 *
 * ok for complete vcalendar objects
 */
void display_edit_individual_task(icalcomponent *supplied_vtodo, long msgnum) {
	icalcomponent *vtodo;
	icalproperty *p;
	struct icaltimetype t;
	time_t now;
	int created_new_vtodo = 0;

	now = time(NULL);

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;

		/* If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VTODO component, attempt to use the first
		 * relevant VTODO subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
			display_edit_individual_task(
				icalcomponent_get_first_component(
					vtodo, ICAL_VTODO_COMPONENT
				), msgnum
			);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Edit task</SPAN>"
		"</TD></TR></TABLE><BR>\n"
	);

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/save_task\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgnum\" VALUE=\"%ld\">\n",
		msgnum);

	wprintf("Summary: "
		"<INPUT TYPE=\"text\" NAME=\"summary\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"><BR>\n");

	wprintf("Start date: ");
	p = icalcomponent_get_first_property(vtodo, ICAL_DTSTART_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_dtstart(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "dtstart");
	wprintf("<BR>\n");

	wprintf("Due date: ");
	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_due(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "due");
	wprintf("<BR>\n");

	wprintf("<CENTER><TEXTAREA NAME=\"description\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n"
	);
	p = icalcomponent_get_first_property(vtodo, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</TEXTAREA><BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Delete\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">\n"
		"</CENTER>\n"
	);

	wprintf("</FORM>\n");

	wDumpContent(1);

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}
}

/*
 * Save an edited task
 *
 * ok 
 */
void save_individual_task(icalcomponent *supplied_vtodo, long msgnum) {
	char buf[SIZ];
	int delete_existing = 0;
	icalproperty *prop;
	icalcomponent *vtodo;
	int created_new_vtodo = 0;

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;
		/* If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VTODO component, attempt to use the first
		 * relevant VTODO subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
			save_individual_task(
				icalcomponent_get_first_component(
					vtodo, ICAL_VTODO_COMPONENT
				), msgnum
			);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}

	if (!strcasecmp(bstr("sc"), "Save")) {

		/* Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_summary(bstr("summary")));
		
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_description(bstr("description")));
	
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_dtstart(
				icaltime_from_webform("dtstart")
			)
		);
	
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_due(
				icaltime_from_webform("due")
			)
		);
	
		/* Serialize it and save it to the message base */
		serv_puts("ENT0 1|||4");
		serv_gets(buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(vtodo));
			serv_puts("000");

			/* Probably not necessary; the server will see the UID
			 * of the object and delete the old one anyway, but
			 * just in case...
			 */
			delete_existing = 1;
		}
	}

	/*
	 * If the user clicked 'Delete' then explicitly delete the message.
	 */
	if (!strcasecmp(bstr("sc"), "Delete")) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", atol(bstr("msgnum")));
		serv_gets(buf);
	}

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}

	/* Go back to the task list */
	readloop("readfwd");
}



/*
 * Code common to all display handlers.  Given a message number and a MIME
 * type, we load the message and hunt for that MIME type.  If found, we load
 * the relevant part, deserialize it into a libical component, filter it for
 * the requested object type, and feed it to the specified handler.
 *
 * ok
 */
void display_using_handler(long msgnum,
			char *mimetype,
			icalcomponent_kind which_kind,
			void (*callback)(icalcomponent *, long)
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

			cal = icalcomponent_new_from_string(relevant_source);
			if (cal != NULL) {

				ical_dezonify(cal);

				/* Simple components of desired type */
				if (icalcomponent_isa(cal) == which_kind) {
					callback(cal, msgnum);
				}

				/* Subcomponents of desired type */
				for (c = icalcomponent_get_first_component(cal,
				    which_kind);
	    			    (c != 0);
	    			    c = icalcomponent_get_next_component(cal,
				    which_kind)) {
					callback(c, msgnum);
				}
				icalcomponent_free(cal);
			}
			free(relevant_source);
		}
	}

}

void display_calendar(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_VEVENT_COMPONENT,
				display_individual_cal);
}

void display_task(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				display_individual_task);
}

void display_edit_task(void) {
	long msgnum = 0L;

	/* Force change the room if we have to */
	if (strlen(bstr("taskrm")) > 0) {
		gotoroom(bstr("taskrm"), 0);
	}

	msgnum = atol(bstr("msgnum"));
	if (msgnum > 0L) {
		/* existing task */
		display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				display_edit_individual_task);
	}
	else {
		/* new task */
		display_edit_individual_task(NULL, 0L);
	}
}

void save_task(void) {
	long msgnum = 0L;

	msgnum = atol(bstr("msgnum"));
	if (msgnum > 0L) {
		display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				save_individual_task);
	}
	else {
		save_individual_task(NULL, 0L);
	}
}

void display_edit_event(void) {
	long msgnum = 0L;

	msgnum = atol(bstr("msgnum"));
	if (msgnum > 0L) {
		/* existing event */
		display_using_handler(msgnum, "text/calendar",
				ICAL_VEVENT_COMPONENT,
				display_edit_individual_event);
	}
	else {
		/* new event */
		display_edit_individual_event(NULL, 0L);
	}
}

void save_event(void) {
	long msgnum = 0L;

	msgnum = atol(bstr("msgnum"));

	if (msgnum > 0L) {
		display_using_handler(msgnum, "text/calendar",
				ICAL_VEVENT_COMPONENT,
				save_individual_event);
	}
	else {
		save_individual_event(NULL, 0L);
	}
}

#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
