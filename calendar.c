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

#else /* HAVE_ICAL_H */


/******   End of handler stubs.  Everything below this line is real.   ******/




/*
 * Process a calendar object
 * ...at this point it's already been deserialized by cal_process_attachment()
 */
void cal_process_object(icalcomponent *cal,
			int recursion_level
) {
	icalcomponent *c;
	icalproperty *method = NULL;
	icalproperty_method the_method;
	icalproperty *p;
	struct icaltimetype t;
	time_t tt;
	char buf[SIZ];

	/* Leading HTML for the display of this object */
	if (recursion_level == 0) {
		wprintf("<CENTER><TABLE border=0 cellpadding=5>\n");
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
			tt = icaltime_as_timet(t);
			fmt_date(buf, tt);
			wprintf("<TR><TD><B>Starting date/time:</B></TD><TD>"
				"%s</TD></TR>", buf
			);
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

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		cal_process_object(c, recursion_level+1);
	}

	/* Trailing HTML for the display of this object */
	if (recursion_level == 0) {
		wprintf("<TR><TD COLSPAN=2>"
			"<FORM METHOD=\"GET\" "
			"ACTION=\"/respond_to_request\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Accept\">"
			"&nbsp;&nbsp;"
			"<INPUT TYPE=\"submit\" NAME=\"sc\" "
				"VALUE=\"Decline\">"
			"</FORM>"
			"</TD></TR></TABLE></CENTER>\n"
		);
	}
}


/*
 * Deserialize a calendar object in a message so it can be processed.
 * (This is the main entry point for these things)
 */
void cal_process_attachment(char *part_source) {
	icalcomponent *cal;

	cal = icalcomponent_new_from_string(part_source);

	if (cal == NULL) {
		wprintf("Error parsing calendar object: %s<BR>\n",
			icalerror_strerror(icalerrno));
		return;
	}

	cal_process_object(cal, 0);

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
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
	wprintf("<LI><A HREF=\"/display_edit_task?msgnum=%ld\">", msgnum);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</A>\n");
}


/*
 * Display a task by itself (for editing)
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
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>Edit task</B>"
		"</FONT></TD></TR></TABLE><BR>\n"
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
 */
void save_individual_task(icalcomponent *supplied_vtodo, long msgnum) {
	char buf[SIZ];
	int delete_existing = 0;
	icalproperty *prop;
	icalcomponent *vtodo;
	int created_new_vtodo = 0;

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;
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
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_summary(bstr("summary")));
		
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_description(bstr("description")));
	
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
		}
		icalcomponent_add_property(vtodo,
			icalproperty_new_dtstart(
				icaltime_from_webform("dtstart")
			)
		);
	
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
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

#endif /* HAVE_ICAL_H */
