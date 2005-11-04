/*
 * $Id$
 *
 * Editing calendar events.
 *
 */

#include "webcit.h"
#include "webserver.h"


#ifdef WEBCIT_WITH_CALENDAR_SERVICE

/*
 * Display an event by itself (for editing)
 */
void display_edit_individual_event(icalcomponent *supplied_vevent, long msgnum) {
	icalcomponent *vevent;
	icalproperty *p;
	icalvalue *v;
	struct icaltimetype t_start, t_end;
	time_t now;
	struct tm tm_now;
	int created_new_vevent = 0;
	icalproperty *organizer = NULL;
	char organizer_string[SIZ];
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	char buf[SIZ];
	int organizer_is_me = 0;
	int i;
	int sequence = 0;

	now = time(NULL);
	strcpy(organizer_string, "");
	strcpy(attendee_string, "");

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
		/* If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VEVENT component, attempt to use the first
		 * relevant VEVENT subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
			display_edit_individual_event(
				icalcomponent_get_first_component(
					vevent, ICAL_VEVENT_COMPONENT
				), msgnum
			);
			return;
		}
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	/* Learn the sequence */
	p = icalcomponent_get_first_property(vevent, ICAL_SEQUENCE_PROPERTY);
	if (p != NULL) {
		sequence = icalproperty_get_sequence(p);
	}

	/* Begin output */
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Add or edit an event"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<script type=\"text/javascript\">"
		"function grey_all_day() { "
			"if (document.EventForm.alldayevent.checked) {"
				"document.EventForm.dtstart_hour.value='0';"
				"document.EventForm.dtstart_hour.disabled = true;"
				"document.EventForm.dtstart_minute.value='0';"
				"document.EventForm.dtstart_minute.disabled = true;"
				"document.EventForm.dtend_hour.value='0';"
				"document.EventForm.dtend_hour.disabled = true;"
				"document.EventForm.dtend_minute.value='0';"
				"document.EventForm.dtend_minute.disabled = true;"
				"document.EventForm.dtend_month.disabled = true;"
				"document.EventForm.dtend_day.disabled = true;"
				"document.EventForm.dtend_year.disabled = true;"
			"}"
			"else {"
				"document.EventForm.dtstart_hour.disabled = false;"
				"document.EventForm.dtstart_minute.disabled = false;"
				"document.EventForm.dtend_hour.disabled = false;"
				"document.EventForm.dtend_minute.disabled = false;"
				"document.EventForm.dtend_month.disabled = false;"
				"document.EventForm.dtend_day.disabled = false;"
				"document.EventForm.dtend_year.disabled = false;"
			"}"
		"}"
		"</script>\n"
	);


	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	/************************************************************
	 * Uncomment this to see the UID in calendar events for debugging
	wprintf("UID == ");
	p = icalcomponent_get_first_property(vevent, ICAL_UID_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("<br />\n");
	wprintf("SEQUENCE == %d<br />\n", sequence);
	*************************************************************/

	wprintf("<FORM NAME=\"EventForm\" METHOD=\"POST\" action=\"/save_event\">\n");

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

	wprintf("<TR><TD><B>");
	wprintf(_("Summary"));
	wprintf("</B></TD><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"summary\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"></TD></TR>\n");

	wprintf("<TR><TD><B>");
	wprintf(_("Location"));
	wprintf("</B></TD><TD>\n"
		"<INPUT TYPE=\"text\" NAME=\"location\" "
		"MAXLENGTH=\"64\" SIZE=\"64\" VALUE=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("\"></TD></TR>\n");

	wprintf("<TR><TD><B>");
	wprintf(_("Start"));
	wprintf("</B></TD><TD>\n");
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
		localtime_r(&now, &tm_now);
		if (strlen(bstr("year")) > 0) {
			tm_now.tm_year = atoi(bstr("year")) - 1900;
			tm_now.tm_mon = atoi(bstr("month")) - 1;
			tm_now.tm_mday = atoi(bstr("day"));
		}
		if (strlen(bstr("hour")) > 0) {
			tm_now.tm_hour = atoi(bstr("hour"));
			tm_now.tm_min = atoi(bstr("minute"));
			tm_now.tm_sec = 0;
		}
		else {
			tm_now.tm_hour = 9;
			tm_now.tm_min = 0;
			tm_now.tm_sec = 0;
		}

		t_start = icaltime_from_timet_with_zone(
			mktime(&tm_now),
			((!strcasecmp(bstr("alldayevent"), "yes")) ? 1 : 0),
			icaltimezone_get_utc_timezone()
		);
		t_start.is_utc = 1;

	}
	display_icaltimetype_as_webform(&t_start, "dtstart");

	wprintf("<INPUT TYPE=\"checkbox\" NAME=\"alldayevent\" "
		"VALUE=\"yes\" onClick=\"grey_all_day();\""
		" %s >%s",
		(t_start.is_date ? "CHECKED" : "" ),
		_("All day event")
	);

	wprintf("</TD></TR>\n");

	/* If this is an all-day-event, set the end time to be identical to
	 * the start time (the hour/minute/second will be set to midnight).
	 * Otherwise extract or create it.
	 */
	wprintf("<TR><TD><B>");
	wprintf(_("End"));
	wprintf("</B></TD><TD>\n");
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
			t_end.second = 0;
			t_end = icaltime_normalize(t_end);
			/* t_end = icaltime_from_timet(now, 0); */
		}
	}
	display_icaltimetype_as_webform(&t_end, "dtend");
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD><B>");
	wprintf(_("Notes"));
	wprintf("</B></TD><TD>\n"
		"<TEXTAREA NAME=\"description\" wrap=soft "
		"ROWS=5 COLS=80 WIDTH=80>\n"
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
			lprintf(9, "ISME %s\n", organizer_string);
			serv_printf("ISME %s", organizer_string);
			serv_getln(buf, sizeof buf);
			lprintf(9, "%s\n", buf);
			if (buf[0] == '2') {
				organizer_is_me = 1;
			}
		}
	}

	wprintf("<TR><TD><B>");
	wprintf(_("Organizer"));
	wprintf("</B></TD><TD>");
	escputs(organizer_string);
	if (organizer_is_me) {
		wprintf(" <FONT SIZE=-1><I>");
		wprintf(_("(you are the organizer)"));
		wprintf("</I></FONT>\n");
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

	/* Transparency */
	wprintf("<TR><TD><B>");
	wprintf(_("Show time as:"));
	wprintf("</B></TD><TD>");

	p = icalcomponent_get_first_property(vevent, ICAL_TRANSP_PROPERTY);
	if (p == NULL) {
		/* No transparency found.  Default to opaque (busy). */
		p = icalproperty_new_transp(ICAL_TRANSP_OPAQUE);
		if (p != NULL) {
			icalcomponent_add_property(vevent, p);
		}
	}
	if (p != NULL) {
		v = icalproperty_get_value(p);
	}
	else {
		v = NULL;
	}

	wprintf("<INPUT TYPE=\"radio\" NAME=\"transp\" VALUE=\"transparent\"");
	if (v != NULL) if (icalvalue_get_transp(v) == ICAL_TRANSP_TRANSPARENT)
		wprintf(" CHECKED");
	wprintf(">");
	wprintf(_("Free"));
	wprintf("&nbsp;&nbsp;");

	wprintf("<INPUT TYPE=\"radio\" NAME=\"transp\" VALUE=\"opaque\"");
	if (v != NULL) if (icalvalue_get_transp(v) == ICAL_TRANSP_OPAQUE)
		wprintf(" CHECKED");
	wprintf(">");
	wprintf(_("Busy"));

	wprintf("</TD></TR>\n");

	/* Attendees */
	wprintf("<TR><TD><B>");
	wprintf(_("Attendees"));
	wprintf("</B><br />"
		"<FONT SIZE=-2>");
	wprintf(_("(One per line)"));
	wprintf("</FONT></TD><TD>"
		"<TEXTAREA %s NAME=\"attendees\" wrap=soft "
		"ROWS=3 COLS=80 WIDTH=80>\n",
		(organizer_is_me ? "" : "DISABLED ")
	);
	i = 0;
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
		strcpy(attendee_string, icalproperty_get_attendee(attendee));
		if (!strncasecmp(attendee_string, "MAILTO:", 7)) {

			/* screen name or email address */
			strcpy(attendee_string, &attendee_string[7]);
			striplt(attendee_string);
			if (i++) wprintf("\n");
			escputs(attendee_string);
			wprintf(" ");

			/* participant status */
			partstat_as_string(buf, attendee);
			escputs(buf);
		}
	}
	wprintf("</TEXTAREA></TD></TR>\n");

	/* Done with properties. */
	wprintf("</TABLE>\n<CENTER>"
		"<INPUT TYPE=\"submit\" NAME=\"save_button\" VALUE=\"%s\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"delete_button\" VALUE=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"check_button\" "
				"VALUE=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n"
		"</CENTER>\n",
		_("Save"),
		_("Delete"),
		_("Check attendee availability"),
		_("Cancel")
	);

	wprintf("</FORM>\n");
	
	wprintf("</td></tr></table></div>\n");
	wprintf("<script type=\"text/javascript\">"
		"grey_all_day();"
		"</script>\n"
	);
	wDumpContent(1);

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}
}

/*
 * Save an edited event
 * 
 */
void save_individual_event(icalcomponent *supplied_vevent, long msgnum) {
	char buf[SIZ];
	icalproperty *prop;
	icalcomponent *vevent, *encaps;
	int created_new_vevent = 0;
	int all_day_event = 0;
	struct icaltimetype event_start, t;
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	int i;
	int foundit;
	char form_attendees[SIZ];
	char organizer_string[SIZ];
	int sequence = 0;
	enum icalproperty_transp formtransp = ICAL_TRANSP_NONE;

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
		/* If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VEVENT component, attempt to use the first
		 * relevant VEVENT subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
			save_individual_event(
				icalcomponent_get_first_component(
					vevent, ICAL_VEVENT_COMPONENT
				), msgnum
			);
			return;
		}
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	if ( (strlen(bstr("save_button")) > 0)
	   || (strlen(bstr("check_button")) > 0) ) {

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

		if (all_day_event) {
			icaltime_from_webform_dateonly(&event_start, "dtstart");
		}
		else {
			icaltime_from_webform(&event_start, "dtstart");
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
			icaltime_from_webform(&t, "dtend");	
			icalcomponent_add_property(vevent,
				icalproperty_new_dtend(icaltime_normalize(t)
				)
			);
		}

		/* See if transparency is indicated */
		if (strlen(bstr("transp")) > 0) {
			if (!strcasecmp(bstr("transp"), "opaque")) {
				formtransp = ICAL_TRANSP_OPAQUE;
			}
			else if (!strcasecmp(bstr("transp"), "transparent")) {
				formtransp = ICAL_TRANSP_TRANSPARENT;
			}

			while (prop = icalcomponent_get_first_property(vevent, ICAL_TRANSP_PROPERTY),
			      (prop != NULL)) {
				icalcomponent_remove_property(vevent, prop);
				icalproperty_free(prop);
			}

			lprintf(9, "adding new property...\n");
			icalcomponent_add_property(vevent, icalproperty_new_transp(formtransp));
			lprintf(9, "...added it.\n");
		}

		/* Give this event a UID if it doesn't have one. */
		lprintf(9, "Give this event a UID if it doesn't have one.\n");
		if (icalcomponent_get_first_property(vevent,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vevent,
				icalproperty_new_uid(buf)
			);
		}

		/* Increment the sequence ID */
		lprintf(9, "Increment the sequence ID\n");
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_SEQUENCE_PROPERTY), (prop != NULL) ) {
			i = icalproperty_get_sequence(prop);
			lprintf(9, "Sequence was %d\n", i);
			if (i > sequence) sequence = i;
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}
		++sequence;
		lprintf(9, "New sequence is %d.  Adding...\n", sequence);
		icalcomponent_add_property(vevent,
			icalproperty_new_sequence(sequence)
		);
		
		/* Set the organizer, only if one does not already exist *and*
		 * the form is supplying one
		 */
		lprintf(9, "Setting the organizer...\n");
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
		lprintf(9, "Add any new attendees\n");

		/* First, strip out the parenthesized partstats.  */
		strcpy(form_attendees, bstr("attendees"));
		stripout(form_attendees, '(', ')');

		/* Now iterate! */
		for (i=0; i<num_tokens(form_attendees, '\n'); ++i) {
			extract_token(buf, form_attendees, i, '\n', sizeof buf);
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
STARTOVER:	lprintf(9, "Remove unlisted attendees\n");
		for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
			strcpy(attendee_string, icalproperty_get_attendee(attendee));
			if (!strncasecmp(attendee_string, "MAILTO:", 7)) {
				strcpy(attendee_string, &attendee_string[7]);
				striplt(attendee_string);
				foundit = 0;
				for (i=0; i<num_tokens(form_attendees, '\n'); ++i) {
					extract_token(buf, form_attendees, i, '\n', sizeof buf);
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
		 * Encapsulate event into full VCALENDAR component.  Clone it first,
		 * for two reasons: one, it's easier to just free the whole thing
		 * when we're done instead of unbundling, but more importantly, we
		 * can't encapsulate something that may already be encapsulated
		 * somewhere else.
		 */
		lprintf(9, "Encapsulating into full VCALENDAR component\n");
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vevent));

		/* If the user clicked 'Save' then save it to the server. */
		lprintf(9, "Serializing it for saving\n");
		if ( (encaps != NULL) && (strlen(bstr("save_button")) > 0) ) {
			serv_puts("ENT0 1|||4|||1|");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '8') {
				serv_puts("Content-type: text/calendar");
				serv_puts("");
				serv_puts(icalcomponent_as_ical_string(encaps));
				serv_puts("000");
			}
			while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				lprintf(9, "ENT0 REPLY: %s\n", buf);
			}
			icalcomponent_free(encaps);
		}

		/* Or, check attendee availability if the user asked for that. */
		if ( (encaps != NULL) && (strlen(bstr("check_button")) > 0) ) {

			/* Call this function, which does the real work */
			check_attendee_availability(encaps);

			/* This displays the form again, with our annotations */
			display_edit_individual_event(encaps, msgnum);

			icalcomponent_free(encaps);
		}

	}

	/*
	 * If the user clicked 'Delete' then delete it.
	 */
	lprintf(9, "Checking to see if we have to delete an old event\n");
	if ( (strlen(bstr("delete_button")) > 0) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", atol(bstr("msgnum")));
		serv_getln(buf, sizeof buf);
	}

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}

	/* If this was a save or deelete, go back to the calendar view. */
	if (strlen(bstr("check_button")) == 0) {
		readloop("readfwd");
	}
}


#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
