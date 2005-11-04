/*
 * $Id$
 *
 * Functions which handle calendar objects and their processing/display.
 *
 */

#include "webcit.h"
#include "webserver.h"

#ifndef WEBCIT_WITH_CALENDAR_SERVICE

/*
 * Handler stubs for builds with no calendar library available
 */
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum) {

	wprintf(_("<I>This message contains calendaring/scheduling information,"
		" but support for calendars is not available on this "
		"particular system.  Please ask your system administrator to "
		"install a new version of the Citadel web service with "
		"calendaring enabled.</I><br />\n")
	);

}

void display_calendar(long msgnum) {
	wprintf(_("<i>"
		"Cannot display calendar item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br />\n"));
}

void display_task(long msgnum) {
	wprintf(_("<i>"
		"Cannot display to-do item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br />\n"));
}

#else /* WEBCIT_WITH_CALENDAR_SERVICE */


/******   End of handler stubs.  Everything below this line is real.   ******/




/*
 * Process a calendar object
 * ...at this point it's already been deserialized by cal_process_attachment()
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
	char buf[256];
	char conflict_name[256];
	char conflict_message[256];
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
			wprintf("<tr><td colspan=\"2\">\n"
				"<img align=\"center\" "
				"src=\"/static/calarea_48x.gif\">"
				"&nbsp;&nbsp;"	
				"<B>");
			wprintf(_("Meeting invitation"));
			wprintf("</B></TD></TR>\n");
			break;
		    case ICAL_METHOD_REPLY:
			wprintf("<TR><TD COLSPAN=2>\n"
				"<IMG ALIGN=CENTER "
				"src=\"/static/calarea_48x.gif\">"
				"&nbsp;&nbsp;"	
				"<B>");
			wprintf(_("Attendee's reply to your invitation"));
			wprintf("</B></TD></TR>\n");
			break;
		    case ICAL_METHOD_PUBLISH:
			wprintf("<TR><TD COLSPAN=2>\n"
				"<IMG ALIGN=CENTER "
				"src=\"/static/calarea_48x.gif\">"
				"&nbsp;&nbsp;"	
				"<B>");
			wprintf(_("Published event"));
			wprintf("</B></TD></TR>\n");
			break;
		    default:
			wprintf("<TR><TD COLSPAN=2>");
			wprintf(_("This is an unknown type of calendar item."));
			wprintf("</TD></TR>\n");
			break;
		}
	}

      	p = icalcomponent_get_first_property(cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>");
		wprintf(_("Summary:"));
		wprintf("</B></TD><TD>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</TD></TR>\n");
	}

      	p = icalcomponent_get_first_property(cal, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>");
		wprintf(_("Location:"));
		wprintf("</B></TD><TD>");
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
				wprintf("<TR><TD><B>");
				wprintf(_("Date:"));
				wprintf("</B></TD><TD>"
					"%s %d, %d</TD></TR>",
					months[t.month - 1],
					t.day, t.year
				);
			}
			else {
				tt = icaltime_as_timet(t);
				fmt_date(buf, tt, 0);
				wprintf("<TR><TD><B>");
				wprintf(_("Starting date/time:"));
				wprintf("</B></TD><TD>%s</TD></TR>", buf);
			}
		}
	
      		p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtend(p);
			tt = icaltime_as_timet(t);
			fmt_date(buf, tt, 0);
			wprintf("<TR><TD><B>");
			wprintf(_("Ending date/time:"));
			wprintf("</B></TD><TD>%s</TD></TR>", buf);
		}

	}

      	p = icalcomponent_get_first_property(cal, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		wprintf("<TR><TD><B>");
		wprintf(_("Description:"));
		wprintf("</B></TD><TD>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</TD></TR>\n");
	}

	/* If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); (p != NULL); p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		wprintf("<TR><TD><B>");
		wprintf(_("Attendee:"));
		wprintf("</B></TD><TD>");
		safestrncpy(buf, icalproperty_get_attendee(p), sizeof buf);
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
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') {
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(conflict_name, buf, 3, '|', sizeof conflict_name);
				is_update = extract_int(buf, 4);

				if (is_update) {
					snprintf(conflict_message, sizeof conflict_message,
						_("This is an update of '%s' which is already in your calendar."), conflict_name);
				}
				else {
					snprintf(conflict_message, sizeof conflict_message,
						_("This event would conflict with '%s' which is already in your calendar."), conflict_name);
				}

				wprintf("<TR><TD><B><I>%s</I></B></TD><td>",
					(is_update ?
						_("Update:") :
						_("CONFLICT:")
					)
				);
				escputs(conflict_message);
				wprintf("</TD></TR>\n");
			}
		}
		lprintf(9, "...done.\n");

		/* Display the Accept/Decline buttons */
		wprintf("<TR><TD>How would you like to respond to this invitation?</td>"
			"<td><FONT SIZE=+1>"
			"<a href=\"/respond_to_request?msgnum=%ld&cal_partnum=%s&sc=Accept\">%s</a>"
			" | "
			"<a href=\"/respond_to_request?msgnum=%ld&cal_partnum=%s&sc=Tentative\">%s</a>"
			" | "
			"<a href=\"/respond_to_request?msgnum=%ld&cal_partnum=%s&sc=Decline\">%s</a>"
			"</FONT></TD></TR>\n",
			msgnum, cal_partnum, _("Accept"),
			msgnum, cal_partnum, _("Tentative"),
			msgnum, cal_partnum, _("Decline")
		);

	}

	/* If this is a REPLY, display update button */
	if (the_method == ICAL_METHOD_REPLY) {

		/***********
		 * In the future, if we want to validate this object before
		 * continuing, we can do it this way:
		serv_printf("ICAL whatever|%ld|%s|", msgnum, cal_partnum);
		serv_getln(buf, sizeof buf);
		}
		 ***********/

		/* Display the update buttons */
		wprintf("<TR><TD>"
			"%s"
			"</td><td><font size=+1>"
			"<a href=\"/handle_rsvp?msgnum=%ld&cal_partnum=%s&sc=Update\">%s</a>"
			" | "
			"<a href=\"/handle_rsvp?msgnum=%ld&cal_partnum=%s&sc=Ignore\">%s</a>"
			"</font>"
			"</TD></TR>\n",
			_("Click <i>Update</i> to accept this reply and update your calendar."),
			msgnum, cal_partnum, _("Update"),
			msgnum, cal_partnum, _("Ignore")
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
 */
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum) {
	icalcomponent *cal;

	cal = icalcomponent_new_from_string(part_source);

	if (cal == NULL) {
		wprintf(_("There was an error parsing this calendar item."));
		wprintf("<br />\n");
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

	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Respond to meeting request"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
	);
	wprintf("</div>\n<div id=\"content\">\n");

	serv_printf("ICAL respond|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		wprintf("<TABLE BORDER=0><TR><TD>"
			"<img src=\"static/calarea_48x.gif\" ALIGN=CENTER>"
			"</TD><TD>"
		);
		if (!strcasecmp(bstr("sc"), "accept")) {
			wprintf(_("You have accepted this meeting invitation.  "
				"It has been entered into your calendar.")
			);
		} else if (!strcasecmp(bstr("sc"), "tentative")) {
			wprintf(_("You have tentatively accepted this meeting invitation.  "
				"It has been 'pencilled in' to your calendar.")
			);
		} else if (!strcasecmp(bstr("sc"), "decline")) {
			wprintf(_("You have declined this meeting invitation.  "
				"It has <b>not</b> been entered into your calendar.")
			);
		}
		wprintf(" ");
		wprintf(_("A reply has been sent to the meeting organizer."));
		wprintf("</TD></TR></TABLE>\n");
	} else {
		wprintf("<img src=\"static/error.gif\" ALIGN=CENTER>"
			"%s\n", &buf[4]);
	}

	wprintf("<a href=\"/dotskip?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\"><br />");
	wprintf(_("Return to messages"));
	wprintf("</A><br />\n");

	wDumpContent(1);
}



/*
 * Handle an incoming RSVP
 */
void handle_rsvp(void) {
	char buf[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Update your calendar with this RSVP"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	serv_printf("ICAL handle_rsvp|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		wprintf("<TABLE BORDER=0><TR><TD>"
			"<img src=\"static/calarea_48x.gif\" ALIGN=CENTER>"
			"</TD><TD>"
		);
		if (!strcasecmp(bstr("sc"), "update")) {
			wprintf(_("Your calendar has been updated to reflect this RSVP."));
		} else if (!strcasecmp(bstr("sc"), "ignore")) {
			wprintf(_("You have chosen to ignore this RSVP. "
				"Your calendar has <b>not</b> been updated.")
			);
		}
		wprintf("</TD></TR></TABLE>\n"
		);
	} else {
		wprintf("<img src=\"static/error.gif\" ALIGN=CENTER>"
			"%s\n", &buf[4]);
	}

	wprintf("<a href=\"/dotskip?room=");
	urlescputs(WC->wc_roomname);
	wprintf("\"><br />");
	wprintf(_("Return to messages"));
	wprintf("</A><br />\n");

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
			(sizeof(struct disp_cal) * WC->num_cal) );
	WC->disp_cal[WC->num_cal - 1].cal = icalcomponent_new_clone(cal);

	WC->disp_cal[WC->num_cal - 1].cal_msgnum = msgnum;
}



/*
 * Display a task by itself (for editing)
 *
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

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR>"
		"<TD><img src=\"/static/taskmanag_48x.gif\"></TD>"
		"<td><SPAN CLASS=\"titlebar\">");
	wprintf(_("Edit task"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>");
	
	wprintf("<FORM METHOD=\"POST\" action=\"/save_task\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgnum\" VALUE=\"%ld\">\n",
		msgnum);

	wprintf("<TABLE border=0>\n");

	wprintf("<TR><TD>");
	wprintf(_("Summary:"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"summary\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"></TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("Start date:"));
	wprintf("</TD><TD>");
	p = icalcomponent_get_first_property(vtodo, ICAL_DTSTART_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_dtstart(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "dtstart");
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("Due date:"));
	wprintf("</TD><TD>");
	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	if (p != NULL) {
		t = icalproperty_get_due(p);
	}
	else {
		t = icaltime_from_timet(now, 0);
	}
	display_icaltimetype_as_webform(&t, "due");
	wprintf("</TD></TR>\n");
	wprintf("<TR><TD>");
	wprintf(_("Description:"));
	wprintf("</TD><TD>");
	wprintf("<TEXTAREA NAME=\"description\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n"
	);
	p = icalcomponent_get_first_property(vtodo, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</TEXTAREA></TD></TR></TABLE>\n");

	wprintf("<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"save_button\" VALUE=\"%s\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"delete_button\" VALUE=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n"
		"</CENTER>\n",
		_("Save"),
		_("Delete"),
		_("Cancel")
	);

	wprintf("</FORM>\n");

	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}
}

/*
 * Save an edited task
 *
 */
void save_individual_task(icalcomponent *supplied_vtodo, long msgnum) {
	char buf[SIZ];
	int delete_existing = 0;
	icalproperty *prop;
	icalcomponent *vtodo, *encaps;
	int created_new_vtodo = 0;
	int i;
	int sequence = 0;
	struct icaltimetype t;

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

	if (strlen(bstr("save_button")) > 0) {

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
		icaltime_from_webform(&t, "dtstart");
		icalcomponent_add_property(vtodo,
			icalproperty_new_dtstart(t)
		);
	
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		icaltime_from_webform(&t, "due");
		icalcomponent_add_property(vtodo,
			icalproperty_new_due(t)
		);

		/* Give this task a UID if it doesn't have one. */
		lprintf(9, "Give this task a UID if it doesn't have one.\n");
		if (icalcomponent_get_first_property(vtodo,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vtodo,
				icalproperty_new_uid(buf)
			);
		}

		/* Increment the sequence ID */
		lprintf(9, "Increment the sequence ID\n");
		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_SEQUENCE_PROPERTY), (prop != NULL) ) {
			i = icalproperty_get_sequence(prop);
			lprintf(9, "Sequence was %d\n", i);
			if (i > sequence) sequence = i;
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		++sequence;
		lprintf(9, "New sequence is %d.  Adding...\n", sequence);
		icalcomponent_add_property(vtodo,
			icalproperty_new_sequence(sequence)
		);

		/*
		 * Encapsulate event into full VCALENDAR component.  Clone it first,
		 * for two reasons: one, it's easier to just free the whole thing
		 * when we're done instead of unbundling, but more importantly, we
		 * can't encapsulate something that may already be encapsulated
		 * somewhere else.
		 */
		lprintf(9, "Encapsulating into full VCALENDAR component\n");
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vtodo));

		/* Serialize it and save it to the message base */
		serv_puts("ENT0 1|||4");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(encaps));
			serv_puts("000");

			/* Probably not necessary; the server will see the UID
			 * of the object and delete the old one anyway, but
			 * just in case...
			 */
			delete_existing = 1;
		}
		icalcomponent_free(encaps);
	}

	/*
	 * If the user clicked 'Delete' then explicitly delete the message.
	 */
	if (strlen(bstr("delete_button")) > 0) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", atol(bstr("msgnum")));
		serv_getln(buf, sizeof buf);
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
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') return;

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "part=", 5)) {
			extract_token(mime_filename, &buf[5], 1, '|', sizeof mime_filename);
			extract_token(mime_partnum, &buf[5], 2, '|', sizeof mime_partnum);
			extract_token(mime_disposition, &buf[5], 3, '|', sizeof mime_disposition);
			extract_token(mime_content_type, &buf[5], 4, '|', sizeof mime_content_type);
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
				display_individual_cal);
}

void display_edit_task(void) {
	long msgnum = 0L;

	/* Force change the room if we have to */
	if (strlen(bstr("taskrm")) > 0) {
		gotoroom(bstr("taskrm"));
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





/*
 * freebusy display (for client software)
 */
void do_freebusy(char *req) {
	char who[SIZ];
	char buf[SIZ];
	char *fb;

	extract_token(who, req, 1, ' ', sizeof who);
	if (!strncasecmp(who, "/freebusy/", 10)) {
		strcpy(who, &who[10]);
	}
	unescape_input(who);

	if ( (!strcasecmp(&who[strlen(who)-4], ".vcf"))
	   || (!strcasecmp(&who[strlen(who)-4], ".ifb"))
	   || (!strcasecmp(&who[strlen(who)-4], ".vfb")) ) {
		who[strlen(who)-4] = 0;
	}

	lprintf(9, "freebusy requested for <%s>\n", who);
	serv_printf("ICAL freebusy|%s", who);
	serv_getln(buf, sizeof buf);

	if (buf[0] != '1') {
		wprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("Content-Type: text/plain\r\n");
		wprintf("\r\n");
		wprintf("%s\n", &buf[4]);
		return;
	}

	fb = read_server_text();
	http_transmit_thing(fb, strlen(fb), "text/calendar", 0);
	free(fb);
}



#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
