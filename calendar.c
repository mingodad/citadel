/*
 * $Id$
 *
 * Functions which handle calendar objects and their processing/display.
 */

#include "webcit.h"
#include "webserver.h"


/*
 * Process a calendar object.  At this point it's already been deserialized by cal_process_attachment()
 *
 * cal:			the calendar object
 * recursion_level:	Number of times we've recursed into this function
 * msgnum:		Message number on the Citadel server
 * cal_partnum:		MIME part number within that message containing the calendar object
 */
void cal_process_object(StrBuf *Target,
			icalcomponent *cal,
			int recursion_level,
			long msgnum,
			const char *cal_partnum) 
{
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
	char divname[32];
	static int divcount = 0;

	sprintf(divname, "rsvp%04x", ++divcount);

	/* Convert timezones to something easy to display.
	 * It's safe to do this in memory because we're only changing it on the
	 * display side -- when we tell the server to do something with the object,
	 * the server will be working with its original copy in the database.
	 */
	if ((cal) && (recursion_level == 0)) {
		ical_dezonify(cal);
	}

	/* Leading HTML for the display of this object */
	if (recursion_level == 0) {
		StrBufAppendPrintf(Target, "<div class=\"mimepart\">\n");
	}

	/* Look for a method */
	method = icalcomponent_get_first_property(cal, ICAL_METHOD_PROPERTY);

	/* See what we need to do with this */
	if (method != NULL) {
		char *title;
		the_method = icalproperty_get_method(method);

		StrBufAppendPrintf(Target, "<div id=\"%s_title\">", divname);
		StrBufAppendPrintf(Target, "<img src=\"static/calarea_48x.gif\">");
		StrBufAppendPrintf(Target, "<span>");
		switch(the_method) {
		case ICAL_METHOD_REQUEST:
			title = _("Meeting invitation");
			break;
		case ICAL_METHOD_REPLY:
			title = _("Attendee's reply to your invitation");
			break;
		case ICAL_METHOD_PUBLISH:
			title = _("Published event");
			break;
		default:
			title = _("This is an unknown type of calendar item.");
			break;
		}
		StrBufAppendPrintf(Target, "</span>");

		StrBufAppendPrintf(Target, "&nbsp;&nbsp;%s",title);
		StrBufAppendPrintf(Target, "</div>");
	}

	StrBufAppendPrintf(Target, "<dl>");
      	p = icalcomponent_get_first_property(cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		StrBufAppendPrintf(Target, "<dt>");
		StrBufAppendPrintf(Target, _("Summary:"));
		StrBufAppendPrintf(Target, "</dt><dd>");
		StrEscAppend(Target, NULL, (char *)icalproperty_get_comment(p), 0, 0);
		StrBufAppendPrintf(Target, "</dd>\n");
	}

      	p = icalcomponent_get_first_property(cal, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		StrBufAppendPrintf(Target, "<dt>");
		StrBufAppendPrintf(Target, _("Location:"));
		StrBufAppendPrintf(Target, "</dt><dd>");
		StrEscAppend(Target, NULL, (char *)icalproperty_get_comment(p), 0, 0);
		StrBufAppendPrintf(Target, "</dd>\n");
	}

	/*
	 * Only show start/end times if we're actually looking at the VEVENT
	 * component.  Otherwise it shows bogus dates for things like timezone.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {

      		p = icalcomponent_get_first_property(cal, ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);

			if (t.is_date) {
				struct tm d_tm;
				char d_str[32];
				memset(&d_tm, 0, sizeof d_tm);
				d_tm.tm_year = t.year - 1900;
				d_tm.tm_mon = t.month - 1;
				d_tm.tm_mday = t.day;
				wc_strftime(d_str, sizeof d_str, "%x", &d_tm);
				StrBufAppendPrintf(Target, "<dt>");
				StrBufAppendPrintf(Target, _("Date:"));
				StrBufAppendPrintf(Target, "</dt><dd>%s</dd>", d_str);
			}
			else {
				tt = icaltime_as_timet(t);
				webcit_fmt_date(buf, tt, 0);
				StrBufAppendPrintf(Target, "<dt>");
				StrBufAppendPrintf(Target, _("Starting date/time:"));
				StrBufAppendPrintf(Target, "</dt><dd>%s</dd>", buf);
			}
		}
	
      		p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtend(p);
			tt = icaltime_as_timet(t);
			webcit_fmt_date(buf, tt, 0);
			StrBufAppendPrintf(Target, "<dt>");
			StrBufAppendPrintf(Target, _("Ending date/time:"));
			StrBufAppendPrintf(Target, "</dt><dd>%s</dd>", buf);
		}

	}

      	p = icalcomponent_get_first_property(cal, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		StrBufAppendPrintf(Target, "<dt>");
		StrBufAppendPrintf(Target, _("Description:"));
		StrBufAppendPrintf(Target, "</dt><dd>");
		StrEscAppend(Target, NULL, (char *)icalproperty_get_comment(p), 0, 0);
		StrBufAppendPrintf(Target, "</dd>\n");
	}

	if (icalcomponent_get_first_property(cal, ICAL_RRULE_PROPERTY)) {
		/* Unusual string syntax used here in order to re-use existing translations */
		StrBufAppendPrintf(Target, "<dt>%s:</dt><dd>%s.</dd>\n",
			_("Recurrence"),
			_("This is a recurring event")
		);
	}

	/* If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); 
	     (p != NULL); 
	     p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		StrBufAppendPrintf(Target, "<dt>");
		StrBufAppendPrintf(Target, _("Attendee:"));
		StrBufAppendPrintf(Target, "</dt><dd>");
		safestrncpy(buf, icalproperty_get_attendee(p), sizeof buf);
		if (!strncasecmp(buf, "MAILTO:", 7)) {

			/** screen name or email address */
			strcpy(buf, &buf[7]);
			striplt(buf);
			StrEscAppend(Target, NULL, buf, 0, 0);
			StrBufAppendPrintf(Target, " ");

			/** participant status */
			partstat_as_string(buf, p);
			StrEscAppend(Target, NULL, buf, 0, 0);
		}
		StrBufAppendPrintf(Target, "</dd>\n");
	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	     (c != 0);
	     c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		cal_process_object(Target, c, recursion_level+1, msgnum, cal_partnum);
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

				StrBufAppendPrintf(Target, "<dt>%s",
					(is_update ?
					 _("Update:") :
					 _("CONFLICT:")
						)
					);
				StrBufAppendPrintf(Target, "</dt><dd>");
				StrEscAppend(Target, NULL, conflict_message, 0, 0);
				StrBufAppendPrintf(Target, "</dd>\n");
			}
		}
		lprintf(9, "...done.\n");

		StrBufAppendPrintf(Target, "</dl>");

		/* Display the Accept/Decline buttons */
		StrBufAppendPrintf(Target, "<p id=\"%s_question\">"
			"%s "
			"&nbsp;&nbsp;&nbsp;<span class=\"button_link\"> "
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Accept');\">%s</a>"
			"</span>&nbsp;&nbsp;&nbsp;<span class=\"button_link\">"
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Tentative');\">%s</a>"
			"</span>&nbsp;&nbsp;&nbsp;<span class=\"button_link\">"
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Decline');\">%s</a>"
			"</span></p>\n",
			divname,
			_("How would you like to respond to this invitation?"),
			divname, divname, msgnum, cal_partnum, _("Accept"),
			divname, divname, msgnum, cal_partnum, _("Tentative"),
			divname, divname, msgnum, cal_partnum, _("Decline")
			);

	}

	/* If this is a REPLY, display update button */
	if (the_method == ICAL_METHOD_REPLY) {

		/* Display the update buttons */
		StrBufAppendPrintf(Target, "<p id=\"%s_question\" >"
			"%s "
			"&nbsp;&nbsp;&nbsp;<span class=\"button_link\"> "
			"<a href=\"javascript:HandleRSVP('%s_question','%s_title','%ld','%s','Update');\">%s</a>"
			"</span>&nbsp;&nbsp;&nbsp;<span class=\"button_link\">"
			"<a href=\"javascript:HandleRSVP('%s_question','%s_title','%ld','%s','Ignore');\">%s</a>"
			"</span></p>\n",
			divname,
			_("Click <i>Update</i> to accept this reply and update your calendar."),
			divname, divname, msgnum, cal_partnum, _("Update"),
			divname, divname, msgnum, cal_partnum, _("Ignore")
			);
	
	}
	
	/* Trailing HTML for the display of this object */
	if (recursion_level == 0) {
		StrBufAppendPrintf(Target, "<p>&nbsp;</p></div>\n");
	}
}


/*
 * Deserialize a calendar object in a message so it can be displayed.
 *
 */
void cal_process_attachment(wc_mime_attachment *Mime) 
{
	icalcomponent *cal;

	cal = icalcomponent_new_from_string(ChrPtr(Mime->Data));
	FlushStrBuf(Mime->Data);
	if (cal == NULL) {
		StrBufAppendPrintf(Mime->Data, _("There was an error parsing this calendar item."));
		StrBufAppendPrintf(Mime->Data, "<br />\n");
		return;
	}

	cal_process_object(Mime->Data, cal, 0, Mime->msgnum, ChrPtr(Mime->PartNum));

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}




/**
 * \brief accept/decline meeting
 * Respond to a meeting request
 */
void respond_to_request(void) 
{
	char buf[1024];

	begin_ajax_response();

	serv_printf("ICAL respond|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		wprintf("<img src=\"static/calarea_48x.gif\"><span>");
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
		wprintf("</span>");
	} else {
		wprintf("<img align=\"center\" src=\"static/error.gif\"><span>");
		wprintf("%s\n", &buf[4]);
		wprintf("</span>");
	}

	end_ajax_response();
}



/**
 * \brief Handle an incoming RSVP
 */
void handle_rsvp(void) 
{
	char buf[1024];

	begin_ajax_response();

	serv_printf("ICAL handle_rsvp|%s|%s|%s|",
		bstr("msgnum"),
		bstr("cal_partnum"),
		bstr("sc")
	);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		wprintf("<img src=\"static/calarea_48x.gif\"><span>");
		if (!strcasecmp(bstr("sc"), "update")) {
			wprintf(_("Your calendar has been updated to reflect this RSVP."));
		} else if (!strcasecmp(bstr("sc"), "ignore")) {
			wprintf(_("You have chosen to ignore this RSVP. "
				  "Your calendar has <b>not</b> been updated.")
				);
		}
		wprintf("</span>");
	} else {
		wprintf("<img src=\"static/error.gif\"><span> %s\n", &buf[4]);
		wprintf("</span>");
	}

	end_ajax_response();
}




/*
 * free memory allocated using libical
 */
void delete_cal(void *vCal)
{
	disp_cal *Cal = (disp_cal*) vCal;
	icalcomponent_free(Cal->cal);
	free(Cal->from);
	free(Cal);
}

/*
 * This is the meat-and-bones of the first part of our two-phase calendar display.
 * As we encounter calendar items in messages being read from the server, we break out
 * any iCalendar objects and store them in a hash table.  Later on, the second phase will
 * use this hash table to render the calendar for display.
 */
void display_individual_cal(icalcomponent *cal, long msgnum, char *from, int unread, struct calview *calv)
{
	icalproperty *ps = NULL;
	struct icaltimetype dtstart, dtend;
	struct icaldurationtype dur;
	wcsession *WCC = WC;
	disp_cal *Cal;
	size_t len;
	time_t final_recurrence = 0;
	icalcomponent *cptr = NULL;

	/* recur variables */
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;
	icalrecur_iterator *ritr = NULL;
	struct icaltimetype next;
	int num_recur = 0;
	int stop_rr = 0;

	dtstart = icaltime_null_time();
	dtend = icaltime_null_time();
	
	if (WCC->disp_cal_items == NULL)
		WCC->disp_cal_items = NewHash(0, Flathash);

	/* Note: anything we do here, we also have to do below for the recurrences. */
	Cal = (disp_cal*) malloc(sizeof(disp_cal));
	memset(Cal, 0, sizeof(disp_cal));
	Cal->cal = icalcomponent_new_clone(cal);

	/* Dezonify and decapsulate at the very last moment */
	/* lprintf(9, "INITIAL: %s\n", icaltime_as_ical_string(icalproperty_get_dtstart(
		icalcomponent_get_first_property(icalcomponent_get_first_component(
		Cal->cal, ICAL_VEVENT_COMPONENT), ICAL_DTSTART_PROPERTY)))
	); */
	ical_dezonify(Cal->cal);
	if (icalcomponent_isa(Cal->cal) != ICAL_VEVENT_COMPONENT) {
		cptr = icalcomponent_get_first_component(Cal->cal, ICAL_VEVENT_COMPONENT);
		if (cptr) {
			cptr = icalcomponent_new_clone(cptr);
			icalcomponent_free(Cal->cal);
			Cal->cal = cptr;
		}
	}

	Cal->unread = unread;
	len = strlen(from);
	Cal->from = (char*)malloc(len+ 1);
	memcpy(Cal->from, from, len + 1);
	Cal->cal_msgnum = msgnum;

	/* Precalculate the starting date and time of this event, and store it in our top-level
	 * structure.  Later, when we are rendering the calendar, we can just peek at these values
	 * without having to break apart every calendar item.
	 */
	ps = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
	if (ps != NULL) {
		dtstart = icalproperty_get_dtstart(ps);
		Cal->event_start = icaltime_as_timet(dtstart);
	}

	/* Do the same for the ending date and time.  It makes the day view much easier to render. */
	ps = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
	if (ps != NULL) {
		dtend = icalproperty_get_dtend(ps);
		Cal->event_end = icaltime_as_timet(dtend);
	}

	/* Store it in the hash list. */
	Put(WCC->disp_cal_items, 
	    (char*) &Cal->event_start,
	    sizeof(Cal->event_start), 
	    Cal, 
	    delete_cal);

	/****************************** handle recurring events ******************************/

	if (icaltime_is_null_time(dtstart)) return;	/* Can't recur without a start time */

	if (!icaltime_is_null_time(dtend)) {		/* Need duration for recurrences */
		dur = icaltime_subtract(dtend, dtstart);
	}

	/*
	 * Just let libical iterate the recurrence, and keep looping back to the top of this function,
	 * adding new hash entries that all point back to the same msgnum, until either the iteration
	 * stops or some outer bound is reached.  The display code will automatically do the Right Thing.
	 */
	cptr = cal;
	if (icalcomponent_isa(cptr) != ICAL_VEVENT_COMPONENT) {
		cptr = icalcomponent_get_first_component(cptr, ICAL_VEVENT_COMPONENT);
	}
	if (!cptr) return;
	ps = icalcomponent_get_first_property(cptr, ICAL_DTSTART_PROPERTY);
	if (ps == NULL) return;
	dtstart = icalproperty_get_dtstart(ps);
	rrule = icalcomponent_get_first_property(cptr, ICAL_RRULE_PROPERTY);
	if (!rrule) return;
	recur = icalproperty_get_rrule(rrule);
	ritr = icalrecur_iterator_new(recur, dtstart);
	if (!ritr) return;

	while (next = icalrecur_iterator_next(ritr), ((!icaltime_is_null_time(next))&&(!stop_rr)) ) {
		++num_recur;
		if (num_recur > 1) {		/* Skip the first one.  We already did it at the root. */
			icalcomponent *cptr;
			/* lprintf(9, "REPEATS: %s\n", icaltime_as_ical_string(next)); */

			/* Note: anything we do here, we also have to do above for the root event. */
			Cal = (disp_cal*) malloc(sizeof(disp_cal));
			memset(Cal, 0, sizeof(disp_cal));
			Cal->cal = icalcomponent_new_clone(cal);
			Cal->unread = unread;
			len = strlen(from);
			Cal->from = (char*)malloc(len+ 1);
			memcpy(Cal->from, from, len + 1);
			Cal->cal_msgnum = msgnum;

			if (icalcomponent_isa(Cal->cal) == ICAL_VEVENT_COMPONENT) {
				cptr = Cal->cal;
			}
			else {
				cptr = icalcomponent_get_first_component(Cal->cal, ICAL_VEVENT_COMPONENT);
			}
			if (cptr) {
				ps = icalcomponent_get_first_property(cptr, ICAL_DTSTART_PROPERTY);
				if (ps != NULL) {
					icalcomponent_remove_property(cptr, ps);
					ps = icalproperty_new_dtstart(next);
					icalcomponent_add_property(cptr, ps);
	
					Cal->event_start = icaltime_as_timet(next);
					final_recurrence = Cal->event_start;
				}

				ps = icalcomponent_get_first_property(cptr, ICAL_DTEND_PROPERTY);
				if (ps != NULL) {
					icalcomponent_remove_property(cptr, ps);
	
					/* Make a new dtend */
					ps = icalproperty_new_dtend(icaltime_add(next, dur));
		
					/* and stick it somewhere */
					icalcomponent_add_property(cptr, ps);
				}

			}

			/* Dezonify and decapsulate at the very last moment */
			ical_dezonify(Cal->cal);
			if (icalcomponent_isa(Cal->cal) != ICAL_VEVENT_COMPONENT) {
				cptr = icalcomponent_get_first_component(Cal->cal, ICAL_VEVENT_COMPONENT);
				if (cptr) {
					cptr = icalcomponent_new_clone(cptr);
					icalcomponent_free(Cal->cal);
					Cal->cal = cptr;
				}
			}

			if ( (Cal->event_start > calv->lower_bound)
			   && (Cal->event_start < calv->upper_bound) ) {
				Put(WCC->disp_cal_items, 
					(char*) &Cal->event_start,
					sizeof(Cal->event_start), 
					Cal, 
					delete_cal
				);
			}
			else {
				delete_cal(Cal);
			}

			/* If an upper bound is set, stop when we go out of scope */
			if (final_recurrence > calv->upper_bound) stop_rr = 1;
		}
	}
	icalrecur_iterator_free(ritr);
	/* lprintf(9, "Performed %d recurrences; final one is %s", num_recur, ctime(&final_recurrence)); */

}



/*
 * Display a task by itself (for editing)
 */
void display_edit_individual_task(icalcomponent *supplied_vtodo, long msgnum, char *from,
			int unread, struct calview *calv)
{
	icalcomponent *vtodo;
	icalproperty *p;
	struct icaltimetype IcalTime;
	time_t now;
	int created_new_vtodo = 0;
	icalproperty_status todoStatus;

	now = time(NULL);

	if (supplied_vtodo != NULL) {
		vtodo = supplied_vtodo;

		/**
		 * If we're looking at a fully encapsulated VCALENDAR
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
					), 
				msgnum, from, unread, calv
				);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}
	
	/*/ TODO: Can we take all this and move it into a template?	 */
	output_headers(1, 1, 1, 0, 0, 0);
	wprintf("<!-- start task edit form -->");
	p = icalcomponent_get_first_property(vtodo, ICAL_SUMMARY_PROPERTY);
	/* Get summary early for title */
	wprintf("<div class=\"box\">\n");
	wprintf("<div class=\"boxlabel\">");
	wprintf(_("Edit task"));
	wprintf("- ");
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</div>");
	
	wprintf("<div class=\"boxcontent\">\n");
	wprintf("<FORM METHOD=\"POST\" action=\"save_task\">\n");
	wprintf("<div style=\"display: none;\">\n	");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"msgnum\" VALUE=\"%ld\">\n", msgnum);
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"return_to_summary\" VALUE=\"%d\">\n",
		ibstr("return_to_summary"));
	wprintf("</div>");
	wprintf("<table class=\"calendar_background\"><tr><td>");
	wprintf("<TABLE STYLE=\"border: none;\">\n");

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
	wprintf("<INPUT TYPE=\"CHECKBOX\" NAME=\"nodtstart\" ID=\"nodtstart\" VALUE=\"NODTSTART\" ");
	if (p == NULL) {
		wprintf("CHECKED=\"CHECKED\"");
	}
	wprintf(">");
	wprintf(_("No date"));
	
	wprintf(" ");
	wprintf(_("or"));
	wprintf(" ");
	if (p != NULL) {
		IcalTime = icalproperty_get_dtstart(p);
	}
	else
		IcalTime = icaltime_current_time_with_zone(get_default_icaltimezone());
	display_icaltimetype_as_webform(&IcalTime, "dtstart", 0);
	wprintf("</TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("Due date:"));
	wprintf("</TD><TD>");
	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	wprintf("<INPUT TYPE=\"CHECKBOX\" NAME=\"nodue\" ID=\"nodue\" VALUE=\"NODUE\"");
	if (p == NULL) {
		wprintf("CHECKED=\"CHECKED\"");
	}
	wprintf(">");
	wprintf(_("No date"));
	wprintf(" ");
	wprintf(_("or"));
	wprintf(" ");
	if (p != NULL) {
		IcalTime = icalproperty_get_due(p);
	}
	else
		IcalTime = icaltime_current_time_with_zone(get_default_icaltimezone());
	display_icaltimetype_as_webform(&IcalTime, "due", 0);
		
	wprintf("</TD></TR>\n");
	todoStatus = icalcomponent_get_status(vtodo);
	wprintf("<TR><TD>\n");
	wprintf(_("Completed:"));
	wprintf("</TD><TD>");
	wprintf("<INPUT TYPE=\"CHECKBOX\" NAME=\"status\" VALUE=\"COMPLETED\"");
	if (todoStatus == ICAL_STATUS_COMPLETED) {
		wprintf(" CHECKED=\"CHECKED\"");
	} 
	wprintf(" >");
	wprintf("</TD></TR>");
	/* start category field */
	p = icalcomponent_get_first_property(vtodo, ICAL_CATEGORIES_PROPERTY);
	wprintf("<TR><TD>");
	wprintf(_("Category:"));
	wprintf("</TD><TD>");
	wprintf("<INPUT TYPE=\"text\" NAME=\"category\" MAXLENGTH=\"32\" SIZE=\"32\" VALUE=\"");
	if (p != NULL) {
		escputs((char *)icalproperty_get_categories(p));
	}
	wprintf("\">");
	wprintf("</TD></TR>\n	");
	/* end category field */
	wprintf("<TR><TD>");
	wprintf(_("Description:"));
	wprintf("</TD><TD>");
	wprintf("<TEXTAREA NAME=\"description\" "
		"ROWS=\"10\" COLS=\"80\">\n"
		);
	p = icalcomponent_get_first_property(vtodo, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wprintf("</TEXTAREA></TD></TR></TABLE>\n");

	wprintf("<SPAN STYLE=\"text-align: center;\">"
		"<INPUT TYPE=\"submit\" NAME=\"save_button\" VALUE=\"%s\">"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"delete_button\" VALUE=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">\n"
		"</SPAN>\n",
		_("Save"),
		_("Delete"),
		_("Cancel")
		);
	wprintf("</td></tr></table>");
	wprintf("</FORM>\n");
	wprintf("</div></div></div>\n");
	wprintf("<!-- end task edit form -->");
	wDumpContent(1);

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}
}

/*
 * \brief Save an edited task
 * \param supplied_vtodo the task to save
 * \param msgnum number of the mesage in our db
 */
void save_individual_task(icalcomponent *supplied_vtodo, long msgnum, char* from, int unread,
				struct calview *calv)
{
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
		/**
		 * If we're looking at a fully encapsulated VCALENDAR
		 * rather than a VTODO component, attempt to use the first
		 * relevant VTODO subcomponent.  If there is none, the
		 * NULL returned by icalcomponent_get_first_component() will
		 * tell the next iteration of this function to create a
		 * new one.
		 */
		if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
			save_individual_task(
				icalcomponent_get_first_component(
					vtodo, ICAL_VTODO_COMPONENT), 
				msgnum, from, unread, calv
				);
			return;
		}
	}
	else {
		vtodo = icalcomponent_new(ICAL_VTODO_COMPONENT);
		created_new_vtodo = 1;
	}

	if (havebstr("save_button")) {

		/** Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (havebstr("summary")) {

			icalcomponent_add_property(vtodo,
						   icalproperty_new_summary(bstr("summary")));
		} else {
			icalcomponent_add_property(vtodo,
						   icalproperty_new_summary("Untitled Task"));
		}
	
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DESCRIPTION_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (havebstr("description")) {
			icalcomponent_add_property(vtodo,
						   icalproperty_new_description(bstr("description")));
		}
	
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DTSTART_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (IsEmptyStr(bstr("nodtstart"))) {
			icaltime_from_webform(&t, "dtstart");
			icalcomponent_add_property(vtodo,
						   icalproperty_new_dtstart(t)
				);
		}
		while(prop = icalcomponent_get_first_property(vtodo,
							      ICAL_STATUS_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}
		while(prop = icalcomponent_get_first_property(vtodo,
							      ICAL_PERCENTCOMPLETE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}

		if (havebstr("status")) {
			icalproperty_status taskStatus = icalproperty_string_to_status(bstr("status"));
			icalcomponent_set_status(vtodo, taskStatus);
			icalcomponent_add_property(vtodo,
				icalproperty_new_percentcomplete(
					(strcasecmp(bstr("status"), "completed") ? 0 : 100)
				)
			);
		}
		else {
			icalcomponent_add_property(vtodo, icalproperty_new_percentcomplete(0));
		}
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_CATEGORIES_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo,prop);
			icalproperty_free(prop);
		}
		if (!IsEmptyStr(bstr("category"))) {
			prop = icalproperty_new_categories(bstr("category"));
			icalcomponent_add_property(vtodo,prop);
		}
		while (prop = icalcomponent_get_first_property(vtodo,
							       ICAL_DUE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
		if (IsEmptyStr(bstr("nodue"))) {
			icaltime_from_webform(&t, "due");
			icalcomponent_add_property(vtodo,
						   icalproperty_new_due(t)
				);
		}
		/** Give this task a UID if it doesn't have one. */
		lprintf(9, "Give this task a UID if it doesn't have one.\n");
		if (icalcomponent_get_first_property(vtodo,
						     ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vtodo,
						   icalproperty_new_uid(buf)
				);
		}

		/** Increment the sequence ID */
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

		/**
		 * Encapsulate event into full VCALENDAR component.  Clone it first,
		 * for two reasons: one, it's easier to just free the whole thing
		 * when we're done instead of unbundling, but more importantly, we
		 * can't encapsulate something that may already be encapsulated
		 * somewhere else.
		 */
		lprintf(9, "Encapsulating into a full VCALENDAR component\n");
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vtodo));

		/* Serialize it and save it to the message base */
		serv_puts("ENT0 1|||4");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/calendar");
			serv_puts("");
			serv_puts(icalcomponent_as_ical_string(encaps));
			serv_puts("000");

			/**
			 * Probably not necessary; the server will see the UID
			 * of the object and delete the old one anyway, but
			 * just in case...
			 */
			delete_existing = 1;
		}
		icalcomponent_free(encaps);
	}

	/**
	 * If the user clicked 'Delete' then explicitly delete the message.
	 */
	if (havebstr("delete_button")) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", lbstr("msgnum"));
		serv_getln(buf, sizeof buf);
	}

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}

	/* Go back to wherever we came from */
	if (ibstr("return_to_summary") == 1) {
		summary();
	}
	else {
		readloop(readfwd);
	}
}



/*
 * Code common to all icalendar display handlers.  Given a message number and a MIME
 * type, we load the message and hunt for that MIME type.  If found, we load
 * the relevant part, deserialize it into a libical component, filter it for
 * the requested object type, and feed it to the specified handler.
 */
void load_ical_object(long msgnum, int unread,
			   icalcomponent_kind which_kind,
			   void (*callback)(icalcomponent *, long, char*, int, struct calview *),
			   struct calview *calv
	) 
{
	char buf[1024];
	char from[128] = "";
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	int mime_length;
	char relevant_partnum[256];
	char *relevant_source = NULL;
	icalcomponent *cal, *c;

	relevant_partnum[0] = '\0';
	sprintf(buf, "MSG4 %ld", msgnum);	/* we need the mime headers */
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

			if (  (!strcasecmp(mime_content_type, "text/calendar"))
			      || (!strcasecmp(mime_content_type, "application/ics"))
			      || (!strcasecmp(mime_content_type, "text/vtodo"))
				) {
				strcpy(relevant_partnum, mime_partnum);
			}
		}
		else if (!strncasecmp(buf, "from=", 4)) {
			extract_token(from, buf, 1, '=', sizeof(from));
		}
	}

	if (!IsEmptyStr(relevant_partnum)) {
		relevant_source = load_mimepart(msgnum, relevant_partnum);
		if (relevant_source != NULL) {

			cal = icalcomponent_new_from_string(relevant_source);
			if (cal != NULL) {

				/* A which_kind of (-1) means just load the whole thing */
				if (which_kind == (-1)) {
					callback(cal, msgnum, from, unread, calv);
				}

				/* Otherwise recurse and hunt */
				else {

					/* Simple components of desired type */
					if (icalcomponent_isa(cal) == which_kind) {
						callback(cal, msgnum, from, unread, calv);
					}
	
					/* Subcomponents of desired type */
					for (c = icalcomponent_get_first_component(cal, which_kind);
				     	(c != 0);
				     	c = icalcomponent_get_next_component(cal, which_kind)) {
						callback(c, msgnum, from, unread, calv);
					}

				}

				icalcomponent_free(cal);
			}
			free(relevant_source);
		}
	}
	icalmemory_free_ring();
}

/*
 * Display a calendar item
 */
void load_calendar_item(message_summary *Msg, int unread, struct calview *c) {
	/*load_ical_object(Msg->msgnum, unread, ICAL_VEVENT_COMPONENT, display_individual_cal, c);*/
	load_ical_object(Msg->msgnum, unread, (-1), display_individual_cal, c);
}

/*
 * Display task view
 */
void display_task(message_summary *Msg, int unread) {
	load_ical_object(Msg->msgnum, unread, ICAL_VTODO_COMPONENT, display_individual_cal, NULL);
}

/*
 * Display the editor component for a task
 */
void display_edit_task(void) {
	long msgnum = 0L;
			
	/* Force change the room if we have to */
	if (havebstr("taskrm")) {
		gotoroom(sbstr("taskrm"));
	}

	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		/* existing task */
		load_ical_object(msgnum, 0,
				      ICAL_VTODO_COMPONENT,
				      display_edit_individual_task,
				      NULL
		);
	}
	else {
		/* new task */
		display_edit_individual_task(NULL, 0L, "", 0, NULL);
	}
}

/*
 * save an edited task
 */
void save_task(void) {
	long msgnum = 0L;
	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		load_ical_object(msgnum, 0, ICAL_VTODO_COMPONENT, save_individual_task, NULL);
	}
	else {
		save_individual_task(NULL, 0L, "", 0, NULL);
	}
}

/*
 * display the editor component for an event
 */
void display_edit_event(void) {
	long msgnum = 0L;

	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		/* existing event */
		load_ical_object(msgnum, 0, ICAL_VEVENT_COMPONENT, display_edit_individual_event, NULL);
	}
	else {
		/* new event */
		display_edit_individual_event(NULL, 0L, "", 0, NULL);
	}
}

/*
 * save an edited event
 */
void save_event(void) {
	long msgnum = 0L;

	msgnum = lbstr("msgnum");

	if (msgnum > 0L) {
		/* load_ical_object(msgnum, 0, ICAL_VEVENT_COMPONENT, save_individual_event, NULL); */
		load_ical_object(msgnum, 0, (-1), save_individual_event, NULL);
	}
	else {
		save_individual_event(NULL, 0L, "", 0, NULL);
	}
}





/*
 * Anonymous request of freebusy data for a user
 */
void do_freebusy(const char *req) {
	char who[SIZ];
	char buf[SIZ];
	int len;
	long lines;

	extract_token(who, req, 1, ' ', sizeof who);
	if (!strncasecmp(who, "/freebusy/", 10)) {
		strcpy(who, &who[10]);
	}
	unescape_input(who);

	len = strlen(who);
	if ( (!strcasecmp(&who[len-4], ".vcf"))
	     || (!strcasecmp(&who[len-4], ".ifb"))
	     || (!strcasecmp(&who[len-4], ".vfb")) ) {
		who[len-4] = 0;
	}

	lprintf(9, "freebusy requested for <%s>\n", who);
	serv_printf("ICAL freebusy|%s", who);
	serv_getln(buf, sizeof buf);

	if (buf[0] != '1') {
		hprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wprintf("%s\n", &buf[4]);
		end_burst();
		return;
	}

	read_server_text(WC->WBuf, &lines);
	http_transmit_thing("text/calendar", 0);
}





void 
InitModule_CALENDAR
(void)
{
	RegisterPreference(HKEY("daystart"), _("Calendar day view begins at:"), PRF_INT, NULL);
	RegisterPreference(HKEY("dayend"), _("Calendar day view ends at:"), PRF_INT, NULL);
	RegisterPreference(HKEY("weekstart"), _("Week starts on:"), PRF_INT, NULL);

	WebcitAddUrlHandler(HKEY("display_edit_task"), display_edit_task, 0);
	WebcitAddUrlHandler(HKEY("save_task"), save_task, 0);
	WebcitAddUrlHandler(HKEY("display_edit_event"), display_edit_event, 0);
	WebcitAddUrlHandler(HKEY("save_event"), save_event, 0);
	WebcitAddUrlHandler(HKEY("respond_to_request"), respond_to_request, 0);
	WebcitAddUrlHandler(HKEY("handle_rsvp"), handle_rsvp, 0);
}
