/*
 * $Id$
 */
/**
 * \defgroup calav Functions which handle calendar objects and their processing/display.
 * \ingroup Calendaring
 */
/* @{ */

#include "webcit.h"
#include "webserver.h"

#ifndef WEBCIT_WITH_CALENDAR_SERVICE

/**
 * \brief get around non existing types
 * Handler stubs for builds with no calendar library available
 * \param part_source dummy pointer to the source
 * \param msgnum number of the mesage in the db
 * \param cal_partnum number of the calendar part
 */
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum) {

	wprintf(_("<I>This message contains calendaring/scheduling information,"
		" but support for calendars is not available on this "
		"particular system.  Please ask your system administrator to "
		"install a new version of the Citadel web service with "
		"calendaring enabled.</I><br />\n")
	);

}

/**
 * \brief say we can't display calendar items
 * \param msgnum number of the mesage in our db
 */
void display_calendar(long msgnum) {
	wprintf(_("<i>"
		"Cannot display calendar item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br />\n"));
}

/**
 * \brief say we can't display task items
 * \param msgnum number of the mesage in our db
 */
void display_task(long msgnum) {
	wprintf(_("<i>"
		"Cannot display to-do item.  You are seeing this error "
		"because your WebCit service has not been installed with "
		"calendar support.  Please contact your system administrator."
		"</i><br />\n"));
}
/** ok, we have calendaring available */
#else /* WEBCIT_WITH_CALENDAR_SERVICE */


/******   End of handler stubs.  Everything below this line is real.   ******/




/**
 * \brief Process a calendar object
 * ...at this point it's already been deserialized by cal_process_attachment()
 * \param cal the calendar object
 * \param recursion_level call stack depth ??????
 * \param msgnum number of the mesage in our db
 * \param cal_partnum of the calendar object ???? 
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
	char divname[32];
	static int divcount = 0;

	sprintf(divname, "rsvp%04x", ++divcount);

	/** Leading HTML for the display of this object */
	if (recursion_level == 0) {
		wprintf("<div class=\"mimepart\">\n");
	}

	/** Look for a method */
	method = icalcomponent_get_first_property(cal, ICAL_METHOD_PROPERTY);

	/** See what we need to do with this */
	if (method != NULL) {
		the_method = icalproperty_get_method(method);
		char *title;

		wprintf("<div id=\"%s_title\">", divname);
		wprintf("<img src=\"static/calarea_48x.gif\">");
		wprintf("<span>");
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
		wprintf("</span>");

		wprintf("&nbsp;&nbsp;%s",title);
		wprintf("</div>");
	}

	wprintf("<dl>");
      	p = icalcomponent_get_first_property(cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		wprintf("<dt>");
		wprintf(_("Summary:"));
		wprintf("</dt><dd>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</dd>\n");
	}

      	p = icalcomponent_get_first_property(cal, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		wprintf("<dt>");
		wprintf(_("Location:"));
		wprintf("</dt><dd>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</dd>\n");
	}

	/**
	 * Only show start/end times if we're actually looking at the VEVENT
	 * component.  Otherwise it shows bogus dates for things like timezone.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {

      		p = icalcomponent_get_first_property(cal,
						ICAL_DTSTART_PROPERTY);
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
				wprintf("<dt>");
				wprintf(_("Date:"));
				wprintf("</dt><dd>%s</dd>", d_str);
			}
			else {
				tt = icaltime_as_timet(t);
				fmt_date(buf, tt, 0);
				wprintf("<dt>");
				wprintf(_("Starting date/time:"));
				wprintf("</dt><dd>%s</dd>", buf);
			}
		}
	
      		p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtend(p);
			tt = icaltime_as_timet(t);
			fmt_date(buf, tt, 0);
			wprintf("<dt>");
			wprintf(_("Ending date/time:"));
			wprintf("</dt><dd>%s</dd>", buf);
		}

	}

      	p = icalcomponent_get_first_property(cal, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		wprintf("<dt>");
		wprintf(_("Description:"));
		wprintf("</dt><dd>");
		escputs((char *)icalproperty_get_comment(p));
		wprintf("</dd>\n");
	}

	/** If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); (p != NULL); p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		wprintf("<dt>");
		wprintf(_("Attendee:"));
		wprintf("</dt><dd>");
		safestrncpy(buf, icalproperty_get_attendee(p), sizeof buf);
		if (!strncasecmp(buf, "MAILTO:", 7)) {

			/** screen name or email address */
			strcpy(buf, &buf[7]);
			striplt(buf);
			escputs(buf);
			wprintf(" ");

			/** participant status */
			partstat_as_string(buf, p);
			escputs(buf);
		}
		wprintf("</dd>\n");
	}

	/** If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		cal_process_object(c, recursion_level+1, msgnum, cal_partnum);
	}

	/** If this is a REQUEST, display conflicts and buttons */
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

				wprintf("<dt>%s",
					(is_update ?
						_("Update:") :
						_("CONFLICT:")
					)
				);
				wprintf("</dt><dd>");
				escputs(conflict_message);
				wprintf("</dd>\n");
			}
		}
		lprintf(9, "...done.\n");

		wprintf("</dl>");

		/** Display the Accept/Decline buttons */
		wprintf("<p id=\"%s_question\" class=\"buttons\">"
			"%s "
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Accept');\">%s</a>"
			"<span> | </span>"
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Tentative');\">%s</a>"
			"<span> | </span>"
			"<a href=\"javascript:RespondToInvitation('%s_question','%s_title','%ld','%s','Decline');\">%s</a>"
			"</p>\n",
			divname,
			_("How would you like to respond to this invitation?"),
			divname, divname, msgnum, cal_partnum, _("Accept"),
			divname, divname, msgnum, cal_partnum, _("Tentative"),
			divname, divname, msgnum, cal_partnum, _("Decline")
		);

	}

	/** If this is a REPLY, display update button */
	if (the_method == ICAL_METHOD_REPLY) {

		/** \todo  In the future, if we want to validate this object before \
		 * continuing, we can do it this way:
		serv_printf("ICAL whatever|%ld|%s|", msgnum, cal_partnum);
		serv_getln(buf, sizeof buf);
		}
		 ***********/

		/** Display the update buttons */
		wprintf("<p id=\"%s_question\" class=\"buttons\">"
			"%s"
			"<a href=\"javascript:HandleRSVP('%s_question','%s_title','%ld','%s','Update');\">%s</a>"
			"<span> | </span>"
			"<a href=\"javascript:HandleRSVP('%s_question','%s_title','%ld','%s','Ignore');\">%s</a>"
			"</p>\n",
			divname,
			_("Click <i>Update</i> to accept this reply and update your calendar."),
			divname, divname, msgnum, cal_partnum, _("Update"),
			divname, divname, msgnum, cal_partnum, _("Ignore")
		);

	}

	/** Trailing HTML for the display of this object */
	if (recursion_level == 0) {
		wprintf("<p>&nbsp;</p></div>\n");
	}
}


/**
 * \brief process calendar mail atachment
 * Deserialize a calendar object in a message so it can be processed.
 * (This is the main entry point for these things)
 * \param part_source the part of the message we want to parse
 * \param msgnum number of the mesage in our db
 * \param cal_partnum the number of the calendar item
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




/**
 * \brief accept/decline meeting
 * Respond to a meeting request
 */
void respond_to_request(void) {
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
void handle_rsvp(void) {
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



/*@}*/
/*-----------------------------------------------------------------------**/



/**
 * \defgroup MsgDisplayHandlers Display handlers for message reading 
 * \ingroup Calendaring
 */

/*@{*/



/**
 * \brief get items, keep them.
 * If we're reading calendar items, just store them for now.  We have to
 * sort and re-output them later when we draw the calendar.
 * \param cal Our calendar to process
 * \param msgnum number of the mesage in our db
 */
void display_individual_cal(icalcomponent *cal, long msgnum)
{
	struct wcsession *WCC = WC;	/* stack this for faster access (WC is a function) */

	WCC->num_cal += 1;
	WCC->disp_cal = realloc(WC->disp_cal, (sizeof(struct disp_cal) * WCC->num_cal) );
	WCC->disp_cal[WCC->num_cal - 1].cal = icalcomponent_new_clone(cal);
	ical_dezonify(WCC->disp_cal[WCC->num_cal - 1].cal);
	WCC->disp_cal[WCC->num_cal - 1].cal_msgnum = msgnum;
}



/*
 * \brief edit a task
 * Display a task by itself (for editing)
 * \param supplied_vtodo the todo item we want to edit
 * \param msgnum number of the mesage in our db
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
	wprintf("<div id=\"banner\">\n");
	wprintf("<img src=\"static/taskmanag_48x.gif\">");
	wprintf("<h1>");
	wprintf(_("Edit task"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"calendar_background\"><tr><td>");
	
	wprintf("<FORM METHOD=\"POST\" action=\"save_task\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
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
 * \brief Save an edited task
 * \param supplied_vtodo the task to save
 * \param msgnum number of the mesage in our db
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

	if (!IsEmptyStr(bstr("save_button"))) {

		/** Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vtodo,
		      ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vtodo, prop);
			icalproperty_free(prop);
		}
	 	if (!IsEmptyStr(bstr("summary"))) {
	
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
	if (!IsEmptyStr(bstr("delete_button"))) {
		delete_existing = 1;
	}

	if ( (delete_existing) && (msgnum > 0L) ) {
		serv_printf("DELE %ld", atol(bstr("msgnum")));
		serv_getln(buf, sizeof buf);
	}

	if (created_new_vtodo) {
		icalcomponent_free(vtodo);
	}

	/** Go back to the task list */
	readloop("readfwd");
}



/**
 * \brief generic item handler
 * Code common to all display handlers.  Given a message number and a MIME
 * type, we load the message and hunt for that MIME type.  If found, we load
 * the relevant part, deserialize it into a libical component, filter it for
 * the requested object type, and feed it to the specified handler.
 * \param mimetype mimetyp of our object
 * \param which_kind sort of ical type
 * \param msgnum number of the mesage in our db
 * \param callback a funcion \todo
 *
 */
void display_using_handler(long msgnum,
			char *mimetype,
			icalcomponent_kind which_kind,
			void (*callback)(icalcomponent *, long)
	) {
	char buf[1024];
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	int mime_length;
	char relevant_partnum[256];
	char *relevant_source = NULL;
	icalcomponent *cal, *c;

	relevant_partnum[0] = '\0';
	sprintf(buf, "MSG0 %ld|0", msgnum);	/* unfortunately we need the mime headers */
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
			else if (!strcasecmp(mime_content_type, "text/vtodo")) {
				strcpy(relevant_partnum, mime_partnum);
			}

		}
	}

	if (!IsEmptyStr(relevant_partnum)) {
		relevant_source = load_mimepart(msgnum, relevant_partnum);
		if (relevant_source != NULL) {

			cal = icalcomponent_new_from_string(relevant_source);
			if (cal != NULL) {

				ical_dezonify(cal);

				/** Simple components of desired type */
				if (icalcomponent_isa(cal) == which_kind) {
					callback(cal, msgnum);
				}

				/** Subcomponents of desired type */
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

/**
 * \brief display whole calendar
 * \param msgnum number of the mesage in our db
 */
void display_calendar(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_VEVENT_COMPONENT,
				display_individual_cal);
}

/**
 * \brief display whole taksview
 * \param msgnum number of the mesage in our db
 */
void display_task(long msgnum) {
	display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				display_individual_cal);
}

/**
 * \brief display the editor component for a task
 */
void display_edit_task(void) {
	long msgnum = 0L;

	/** Force change the room if we have to */
	if (!IsEmptyStr(bstr("taskrm"))) {
		gotoroom(bstr("taskrm"));
	}

	msgnum = atol(bstr("msgnum"));
	if (msgnum > 0L) {
		/** existing task */
		display_using_handler(msgnum, "text/calendar",
				ICAL_VTODO_COMPONENT,
				display_edit_individual_task);
	}
	else {
		/** new task */
		display_edit_individual_task(NULL, 0L);
	}
}

/**
 *\brief save an edited task
 */
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

/**
 * \brief display the editor component for an event
 */
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

/**
 * \brief save an edited event
 */
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





/**
 * \brief freebusy display (for client software)
 * \param req dunno. ?????
 */
void do_freebusy(char *req) {
	char who[SIZ];
	char buf[SIZ];
	char *fb;
	int len;

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


/*@}*/
