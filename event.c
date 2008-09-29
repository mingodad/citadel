/*
 * $Id$
 *
 * Editing calendar events.
 */

#include "webcit.h"
#include "webserver.h"

/*
 * Display an event by itself (for editing)
 * supplied_vevent	the event to edit
 * msgnum		reference on the citserver
 */
void display_edit_individual_event(icalcomponent *supplied_vevent, long msgnum, char *from, int unread) {
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
	int i, j = 0;
	int sequence = 0;
	char weekday_labels[7][32];

	lprintf(9, "display_edit_individual_event(%ld) calview=%s year=%s month=%s day=%s\n",
		msgnum, bstr("calview"), bstr("year"), bstr("month"), bstr("day")
	);

	/* populate the weekday names - begin */
	now = time(NULL);
	localtime_r(&now, &tm_now);
	while (tm_now.tm_wday != 0) {
		now -= 86400L;
		localtime_r(&now, &tm_now);
	}
	for (i=0; i<7; ++i) {
		localtime_r(&now, &tm_now);
		wc_strftime(weekday_labels[i], 32, "%A", &tm_now);
		now += 86400L;
	}
	/* populate the weekday names - end */

	now = time(NULL);
	strcpy(organizer_string, "");
	strcpy(attendee_string, "");

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
		/*
		 * If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VEVENT component, attempt to use the first
		 * relevant VEVENT subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
			display_edit_individual_event(
				icalcomponent_get_first_component(
					vevent, ICAL_VEVENT_COMPONENT), 
				msgnum, from, unread
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
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Add or edit an event"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">");

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

	wprintf("<FORM NAME=\"EventForm\" METHOD=\"POST\" action=\"save_event\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

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

	char *tabnames[] = {
		_("Event"),
		_("Attendees"),
		_("Recurrence")
	};

	tabbed_dialog(3, tabnames);
	begin_tab(0, 3);

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
		if (havebstr("year")) {
			tm_now.tm_year = ibstr("year") - 1900;
			tm_now.tm_mon = ibstr("month") - 1;
			tm_now.tm_mday = ibstr("day");
		}
		if (havebstr("hour")) {
			tm_now.tm_hour = ibstr("hour");
			tm_now.tm_min = ibstr("minute");
			tm_now.tm_sec = 0;
		}
		else {
			tm_now.tm_hour = 0;
			tm_now.tm_min = 0;
			tm_now.tm_sec = 0;
		}

		t_start = icaltime_from_timet_with_zone(
			mktime(&tm_now),
			((yesbstr("alldayevent")) ? 1 : 0),
			icaltimezone_get_utc_timezone()
		);
		t_start.is_utc = 1;

	}
	display_icaltimetype_as_webform(&t_start, "dtstart");

	wprintf("<INPUT TYPE=\"checkbox\" id=\"alldayevent\" NAME=\"alldayevent\" "
		"VALUE=\"yes\" onclick=\"eventEditAllDay();\""
		" %s >%s",
		(t_start.is_date ? "CHECKED=\"CHECKED\"" : "" ),
		_("All day event")
	);

	wprintf("</TD></TR>\n");

	/*
	 * If this is an all-day-event, set the end time to be identical to
	 * the start time (the hour/minute/second will be set to midnight).
	 * Otherwise extract or create it.
	 */
	wprintf("<TR><TD><B>");
	wprintf(_("End"));
	wprintf("</B></TD><TD id=\"dtendcell\">\n");
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
			/*
			 * If this is not an all-day event and there is no
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

	/**
	 * For a new event, the user creating the event should be the
	 * organizer.  Set this field accordingly.
	 */
	if (icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY)
	   == NULL) {
		sprintf(organizer_string, "MAILTO:%s", WC->cs_inet_email);
		icalcomponent_add_property(vevent,
			icalproperty_new_organizer(organizer_string)
		);
	}

	/**
	 * Determine who is the organizer of this event.
	 * We need to determine "me" or "not me."
	 */
	organizer = icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY);
	if (organizer != NULL) {
		strcpy(organizer_string, icalproperty_get_organizer(organizer));
		if (!strncasecmp(organizer_string, "MAILTO:", 7)) {
			strcpy(organizer_string, &organizer_string[7]);
			striplt(organizer_string);
			serv_printf("ISME %s", organizer_string);
			serv_getln(buf, sizeof buf);
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

	/**
	 * Transmit the organizer as a hidden field.   We don't want the user
	 * to be able to change it, but we do want it fed back to the server,
	 * especially if this is a new event and there is no organizer already
	 * in the calendar object.
	 */
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"organizer\" VALUE=\"");
	escputs(organizer_string);
	wprintf("\">");

	wprintf("</TD></TR>\n");

	/** Transparency */
	wprintf("<TR><TD><B>");
	wprintf(_("Show time as:"));
	wprintf("</B></TD><TD>");

	p = icalcomponent_get_first_property(vevent, ICAL_TRANSP_PROPERTY);
	if (p == NULL) {
		/** No transparency found.  Default to opaque (busy). */
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


	/** Done with properties. */
	wprintf("</TABLE>\n");

	end_tab(0, 3);

	/* Attendees tab (need to move things here) */
	begin_tab(1, 3);
	wprintf("<TABLE border=0 width=100%%>\n");	/* same table style as the event tab */
	wprintf("<TR><TD><B>");
	wprintf(_("Attendees"));
	wprintf("</B><br />"
		"<font size=-2>");
	wprintf(_("(One per line)"));
	wprintf("</font>\n");

	/** Pop open an address book -- begin **/
	wprintf(
		"&nbsp;<a href=\"javascript:PopOpenAddressBook('attendees_box|%s');\" "
		"title=\"%s\">"
		"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
		"</a>",
		_("Attendees"),
		_("Contacts")
	);
	/* Pop open an address book -- end **/

	wprintf("</TD><TD>"
		"<TEXTAREA %s NAME=\"attendees\" id=\"attendees_box\" wrap=soft "
		"ROWS=3 COLS=80 WIDTH=80>\n",
		(organizer_is_me ? "" : "DISABLED ")
	);
	i = 0;
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
		strcpy(attendee_string, icalproperty_get_attendee(attendee));
		if (!strncasecmp(attendee_string, "MAILTO:", 7)) {

			/** screen name or email address */
			strcpy(attendee_string, &attendee_string[7]);
			striplt(attendee_string);
			if (i++) wprintf("\n");
			escputs(attendee_string);
			wprintf(" ");

			/** participant status */
			partstat_as_string(buf, attendee);
			escputs(buf);
		}
	}
	wprintf("</TEXTAREA></TD></TR>\n");
	wprintf("</TABLE>\n");
	end_tab(1, 3);

	/* Recurrence tab */
	begin_tab(2, 3);
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;

	rrule = icalcomponent_get_first_property(vevent, ICAL_RRULE_PROPERTY);
	if (rrule) {
		recur = icalproperty_get_rrule(rrule);
	}
	else {
		/* blank recurrence with some sensible defaults */
		memset(&recur, 0, sizeof(struct icalrecurrencetype));
		recur.count = 3;
		recur.interval = 1;
		recur.freq = ICAL_WEEKLY_RECURRENCE;
	}

	wprintf("<INPUT TYPE=\"checkbox\" id=\"is_recur\" NAME=\"is_recur\" "
		"VALUE=\"yes\" "
		"onclick=\"RecurrenceShowHide();\""
		" %s >%s",
		(rrule ? "CHECKED=\"CHECKED\"" : "" ),
		_("This is a repeating event")
	);

	wprintf("<div id=\"rrule\">\n");		/* begin 'rrule' div */

	wprintf("<table border=0 width=100%%>\n");	/* same table style as the event tab */

	/* Table row displaying raw RRULE data, FIXME remove when finished */
	if (rrule) {
		wprintf("<tr><td><b>");
		wprintf("Raw data");
		wprintf("</b></td><td>");
		wprintf("<tt>%s</tt>", icalrecurrencetype_as_string(&recur));
		wprintf("</td></tr>\n");
	}

	char *frequency_units[] = {
		_("seconds"),
		_("minutes"),
		_("hours"),
		_("days"),
		_("weeks"),
		_("months"),
		_("years"),
		_("never")
	};

	wprintf("<tr><td><b>");
	wprintf(_("Repeats"));
	wprintf("</b></td><td>");
	if ((recur.freq < 0) || (recur.freq > 6)) recur.freq = 4;
	wprintf("%s ", _("every"));

	wprintf("<input type=\"text\" name=\"interval\" maxlength=\"3\" size=\"3\" ");
	wprintf("value=\"%d\"> ", recur.interval);

	wprintf("<select name=\"freq\" id=\"freq_selector\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (i=0; i<(sizeof frequency_units / sizeof(char *)); ++i) {
		wprintf("<option %s%svalue=\"%d\">%s</option>\n",
			((i == recur.freq) ? "selected " : ""),
			(((i == recur.freq) || ((i>=3)&&(i<=5))) ? "" : "disabled "),
			i,
			frequency_units[i]
		);
	}
	wprintf("</select>\n");

	wprintf("<div id=\"weekday_selector\">");	/* begin 'weekday_selector' div */
	wprintf("%s<br>", _("on these weekdays:"));

	char weekday_is_selected[7];
	memset(weekday_is_selected, 0, 7);

	for (i=0; i<ICAL_BY_DAY_SIZE; ++i) {
		if (recur.by_day[i] == ICAL_RECURRENCE_ARRAY_MAX) {
			i = ICAL_RECURRENCE_ARRAY_MAX;			/* all done */
		}
		else {
			for (j=0; j<7; ++j) {
				if (icalrecurrencetype_day_day_of_week(recur.by_day[i]) == j+1) {
					weekday_is_selected[j] = 1;
				}
			}
		}
	}

	for (i=0; i<7; ++i) {
		wprintf("<input type=\"checkbox\" name=\"weekday%d\" value=\"yes\"", i);
		if (weekday_is_selected[i]) wprintf(" checked");
		wprintf(">%s</input>\n", weekday_labels[i]);
	}
	wprintf("</div>\n");				/* end 'weekday_selector' div */

	wprintf("</td></tr>\n");

	wprintf("</table>\n");
	wprintf("</div>\n");				/* end 'rrule' div */

	end_tab(2, 3);

	/* submit buttons (common area beneath the tabs) */
	begin_tab(3, 3);
	wprintf("<CENTER>"
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
	end_tab(3, 3);

	wprintf("</div>\n");

	wprintf("<script type=\"text/javascript\">	\n"
		"eventEditAllDay();	\n"
		"RecurrenceShowHide();	\n"
		"</script>	\n"
	);
	address_book_popup();
	wDumpContent(1);

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}
}

/*
 * Save an edited event
 *
 * supplied_vevent:	the event to save
 * msgnum:		the index on the citserver
 */
void save_individual_event(icalcomponent *supplied_vevent, long msgnum, char *from, int unread) {
	char buf[SIZ];
	icalproperty *prop;
	icalcomponent *vevent, *encaps;
	int created_new_vevent = 0;
	int all_day_event = 0;
	struct icaltimetype event_start, t;
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	int i, j;
	int foundit;
	char form_attendees[SIZ];
	char organizer_string[SIZ];
	int sequence = 0;
	enum icalproperty_transp formtransp = ICAL_TRANSP_NONE;

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;
		/**
		 * If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VEVENT component, attempt to use the first
		 * relevant VEVENT subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
			save_individual_event(
				icalcomponent_get_first_component(
					vevent, ICAL_VEVENT_COMPONENT), 
				msgnum, from, unread
			);
			return;
		}
	}
	else {
		vevent = icalcomponent_new(ICAL_VEVENT_COMPONENT);
		created_new_vevent = 1;
	}

	if ( (havebstr("save_button"))
	   || (havebstr("check_button")) ) {

		/** Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

	 	if (havebstr("summary")) {
	
		 	icalcomponent_add_property(vevent,
				  	icalproperty_new_summary(bstr("summary")));
	 	} else {
		 	icalcomponent_add_property(vevent,
					icalproperty_new_summary("Untitled Event"));
	 	}
	
	 	while (prop = icalcomponent_get_first_property(vevent,
				     	ICAL_LOCATION_PROPERTY), prop != NULL) {
		 	icalcomponent_remove_property(vevent, prop);
		 	icalproperty_free(prop);
	 	}
	 	if (havebstr("location")) {
		 	icalcomponent_add_property(vevent,
					icalproperty_new_location(bstr("location")));
	 	}
	 	while (prop = icalcomponent_get_first_property(vevent,
				  ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
		 	icalcomponent_remove_property(vevent, prop);
		 	icalproperty_free(prop);
	 	}
	 	if (havebstr("description")) {
		 	icalcomponent_add_property(vevent,
			  	icalproperty_new_description(bstr("description")));
	 	}

		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

		if (yesbstr("alldayevent")) {
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

		/**
		 * The following odd-looking snippet of code looks like it
		 * takes some unnecessary steps.  It is done this way because
		 * libical incorrectly turns an "all day event" into a normal
		 * event starting at midnight (i.e. it serializes as date/time
		 * instead of just date) unless icalvalue_new_date() is used.
		 * So we force it, if this is an all day event.
		 */
		prop = icalproperty_new_dtstart(event_start);
		if (all_day_event) {
			icalproperty_set_value(prop, icalvalue_new_date(event_start));
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

		/** See if transparency is indicated */
		if (havebstr("transp")) {
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

			icalcomponent_add_property(vevent, icalproperty_new_transp(formtransp));
		}

		/** Give this event a UID if it doesn't have one. */
		if (icalcomponent_get_first_property(vevent,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vevent, icalproperty_new_uid(buf));
		}

		/** Increment the sequence ID */
		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_SEQUENCE_PROPERTY), (prop != NULL) ) {
			i = icalproperty_get_sequence(prop);
			if (i > sequence) sequence = i;
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}
		++sequence;
		icalcomponent_add_property(vevent,
			icalproperty_new_sequence(sequence)
		);
		
		/**
		 * Set the organizer, only if one does not already exist *and*
		 * the form is supplying one
		 */
		strcpy(buf, bstr("organizer"));
		if ( (icalcomponent_get_first_property(vevent,
		   ICAL_ORGANIZER_PROPERTY) == NULL) 
		   && (!IsEmptyStr(buf)) ) {

			/** set new organizer */
			sprintf(organizer_string, "MAILTO:%s", buf);
			icalcomponent_add_property(vevent,
				icalproperty_new_organizer(organizer_string)
			);

		}

		/**
		 * Add any new attendees listed in the web form
		 */

		/* First, strip out the parenthesized partstats.  */
		strcpy(form_attendees, bstr("attendees"));
		stripout(form_attendees, '(', ')');

		/* Next, change any commas to newlines, because we want newline-separated attendees. */
		j = strlen(form_attendees);
		for (i=0; i<j; ++i) {
			if (form_attendees[i] == ',') {
				form_attendees[i] = '\n';
				while (isspace(form_attendees[i+1])) {
					strcpy(&form_attendees[i+1], &form_attendees[i+2]);
				}
			}
		}

		/** Now iterate! */
		for (i=0; i<num_tokens(form_attendees, '\n'); ++i) {
			extract_token(buf, form_attendees, i, '\n', sizeof buf);
			striplt(buf);
			if (!IsEmptyStr(buf)) {
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

		/**
		 * Remove any attendees *not* listed in the web form
		 */
STARTOVER:	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
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

		/**
		 * Encapsulate event into full VCALENDAR component.  Clone it first,
		 * for two reasons: one, it's easier to just free the whole thing
		 * when we're done instead of unbundling, but more importantly, we
		 * can't encapsulate something that may already be encapsulated
		 * somewhere else.
		 */
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vevent));

		/* Set the method to PUBLISH */
		icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

		/** If the user clicked 'Save' then save it to the server. */
		if ( (encaps != NULL) && (havebstr("save_button")) ) {
			serv_puts("ENT0 1|||4|||1|");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '8') {
				serv_puts("Content-type: text/calendar");
				serv_puts("");
				serv_puts(icalcomponent_as_ical_string(encaps));
				serv_puts("000");
			}
			if ( (buf[0] == '8') || (buf[0] == '4') ) {
				while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				}
			}
			if (buf[0] == '2') {
				strcpy(WC->ImportantMessage, &buf[4]);
			}
			icalmemory_free_ring ();
			icalcomponent_free(encaps);
			encaps = NULL;
		}

		/** Or, check attendee availability if the user asked for that. */
		if ( (encaps != NULL) && (havebstr("check_button")) ) {

			/* Call this function, which does the real work */
			check_attendee_availability(encaps);

			/** This displays the form again, with our annotations */
			display_edit_individual_event(encaps, msgnum, from, unread);

			icalcomponent_free(encaps);
			encaps = NULL;
		}
		if (encaps != NULL) {
			icalcomponent_free(encaps);
			encaps = NULL;
		}

	}

	/*
	 * If the user clicked 'Delete' then delete it.
	 */
	if ( (havebstr("delete_button")) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", lbstr("msgnum"));
		serv_getln(buf, sizeof buf);
	}

	if (created_new_vevent) {
		icalcomponent_free(vevent);
	}

	/* If this was a save or delete, go back to the calendar view. */
	if (!havebstr("check_button")) {
		readloop("readfwd");
	}
}
