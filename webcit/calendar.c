/*
 * Functions which handle calendar objects and their processing/display.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"
#include "calendar.h"

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
	const char *ch;

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
		StrBufAppendPrintf(Target, "<img src=\"static/webcit_icons/essen/32x32/calendar.png\">");
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
				webcit_fmt_date(buf, 256, tt, DATEFMT_FULL);
				StrBufAppendPrintf(Target, "<dt>");
				StrBufAppendPrintf(Target, _("Starting date/time:"));
				StrBufAppendPrintf(Target, "</dt><dd>%s</dd>", buf);
			}
		}
	
      		p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtend(p);
			tt = icaltime_as_timet(t);
			webcit_fmt_date(buf, 256, tt, DATEFMT_FULL);
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
		ch = icalproperty_get_attendee(p);
		if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {

			/** screen name or email address */
			safestrncpy(buf, ch + 7, sizeof(buf));
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
		syslog(LOG_DEBUG, "Checking server calendar for conflicts...\n");
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
		syslog(LOG_DEBUG, "...done.\n");

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
 */
void cal_process_attachment(wc_mime_attachment *Mime) 
{
	icalcomponent *cal;

	cal = icalcomponent_new_from_string(ChrPtr(Mime->Data));
	FlushStrBuf(Mime->Data);
	if (cal == NULL) {
		StrBufAppendPrintf(Mime->Data, _("There was an error parsing this calendar item."));
		StrBufAppendPrintf(Mime->Data, "<br>\n");
		return;
	}

	cal_process_object(Mime->Data, cal, 0, Mime->msgnum, ChrPtr(Mime->PartNum));

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}




/*
 * Respond to a meeting request - accept/decline meeting
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
		wc_printf("<img src=\"static/webcit_icons/essen/32x32/calendar.png\"><span>");
		if (!strcasecmp(bstr("sc"), "accept")) {
			wc_printf(_("You have accepted this meeting invitation.  "
				"It has been entered into your calendar.")
			);
		} else if (!strcasecmp(bstr("sc"), "tentative")) {
			wc_printf(_("You have tentatively accepted this meeting invitation.  "
				"It has been 'pencilled in' to your calendar.")
			);
		} else if (!strcasecmp(bstr("sc"), "decline")) {
			wc_printf(_("You have declined this meeting invitation.  "
				  "It has <b>not</b> been entered into your calendar.")
				);
		}
		wc_printf(" ");
		wc_printf(_("A reply has been sent to the meeting organizer."));
		wc_printf("</span>");
	} else {
		wc_printf("<img align=\"center\" src=\"static/webcit_icons/error.gif\"><span>");
		wc_printf("%s\n", &buf[4]);
		wc_printf("</span>");
	}

	end_ajax_response();
}



/*
 * Handle an incoming RSVP
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
		wc_printf("<img src=\"static/webcit_icons/calendar.png\"><span>");
		if (!strcasecmp(bstr("sc"), "update")) {
			/// Translators: RSVP aka Répondez s'il-vous-plaît Is the term 
			/// that the recipient of an ical-invitation should please 
			/// answer this request.
			wc_printf(_("Your calendar has been updated to reflect this RSVP."));
		} else if (!strcasecmp(bstr("sc"), "ignore")) {
			wc_printf(_("You have chosen to ignore this RSVP. "
				  "Your calendar has <b>not</b> been updated.")
				);
		}
		wc_printf("</span>");
	} else {
		wc_printf("<img src=\"static/webcit_icons/error.gif\"><span> %s\n", &buf[4]);
		wc_printf("</span>");
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
void display_individual_cal(icalcomponent *event, long msgnum, char *from, int unread, calview *calv)
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

	/* first and foremost, check for bogosity.  bail if we see no DTSTART property */

	if (icalcomponent_get_first_property(icalcomponent_get_first_component(
		event, ICAL_VEVENT_COMPONENT), ICAL_DTSTART_PROPERTY) == NULL)
	{
		return;
	}

	/* ok, chances are we've got a live one here.  let's try to figure out where it goes. */

	dtstart = icaltime_null_time();
	dtend = icaltime_null_time();
	
	if (WCC->disp_cal_items == NULL) {
		WCC->disp_cal_items = NewHash(0, Flathash);
	}

	/* Note: anything we do here, we also have to do below for the recurrences. */
	Cal = (disp_cal*) malloc(sizeof(disp_cal));
	memset(Cal, 0, sizeof(disp_cal));
	Cal->cal = icalcomponent_new_clone(event);

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
	/* syslog(LOG_DEBUG, "INITIAL: %s", ctime(&Cal->event_start)); */
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
	else {
		dur = icaltime_subtract(dtstart, dtstart);
	}

	/*
	 * Just let libical iterate the recurrence, and keep looping back to the top of this function,
	 * adding new hash entries that all point back to the same msgnum, until either the iteration
	 * stops or some outer bound is reached.  The display code will automatically do the Right Thing.
	 */
	cptr = event;
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

			/* Note: anything we do here, we also have to do above for the root event. */
			Cal = (disp_cal*) malloc(sizeof(disp_cal));
			memset(Cal, 0, sizeof(disp_cal));
			Cal->cal = icalcomponent_new_clone(event);
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

				/* Remove any existing DTSTART properties */
				while (	ps = icalcomponent_get_first_property(cptr, ICAL_DTSTART_PROPERTY),
					ps != NULL
				) {
					icalcomponent_remove_property(cptr, ps);
				}

				/* Add our shiny new DTSTART property from the iteration */
				ps = icalproperty_new_dtstart(next);
				icalcomponent_add_property(cptr, ps);
				Cal->event_start = icaltime_as_timet(next);
				final_recurrence = Cal->event_start;

				/* Remove any existing DTEND properties */
				while (	ps = icalcomponent_get_first_property(cptr, ICAL_DTEND_PROPERTY),
					(ps != NULL)
				) {
					icalcomponent_remove_property(cptr, ps);
				}

				/* Add our shiny new DTEND property from the iteration */
				ps = icalproperty_new_dtend(icaltime_add(next, dur));
				icalcomponent_add_property(cptr, ps);

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

			if (	(Cal->event_start > calv->lower_bound)
				&& (Cal->event_start < calv->upper_bound)
			) {
				/* syslog(LOG_DEBUG, "REPEATS: %s", ctime(&Cal->event_start)); */
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
	/* syslog(LOG_DEBUG, "Performed %d recurrences; final one is %s", num_recur, ctime(&final_recurrence)); */
}






void process_ical_object(long msgnum, int unread,
			 char *from, 
			 char *FlatIcal, 
			 icalcomponent_kind which_kind,
			 IcalCallbackFunc CallBack,
			 calview *calv
	) 
{
	icalcomponent *cal, *c;

	cal = icalcomponent_new_from_string(FlatIcal);
	if (cal != NULL) {

		/* A which_kind of (-1) means just load the whole thing */
		if (which_kind == (-1)) {
			CallBack(cal, msgnum, from, unread, calv);
		}
		
		/* Otherwise recurse and hunt */
		else {
			
			/* Simple components of desired type */
			if (icalcomponent_isa(cal) == which_kind) {
				CallBack(cal, msgnum, from, unread, calv);
			}
			
			/* Subcomponents of desired type */
			for (c = icalcomponent_get_first_component(cal, which_kind);
			     (c != 0);
			     c = icalcomponent_get_next_component(cal, which_kind)) {
				CallBack(c, msgnum, from, unread, calv);
			}
			
		}
		
		icalcomponent_free(cal);
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
		      IcalCallbackFunc CallBack,
		      calview *calv,
		      int RenderAsync
	) 
{
	StrBuf *Buf;
	StrBuf *Data = NULL;
	const char *bptr;
	int Done = 0;
	char from[128] = "";
	char mime_partnum[256];
	char mime_filename[256];
	char mime_content_type[256];
	char mime_disposition[256];
	char relevant_partnum[256];
	char *relevant_source = NULL;
	int phase = 0;				/* 0 = citadel headers, 1 = mime headers, 2 = body */
	char msg4_content_type[256] = "";
	char msg4_content_encoding[256] = "";
	int msg4_content_length = 0;

	relevant_partnum[0] = '\0';
	serv_printf("MSG4 %ld", msgnum);	/* we need the mime headers */
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		FreeStrBuf (&Buf);
		return;
	}
	while (!Done && (StrBuf_ServGetln(Buf)>=0)) {
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			Done = 1;
			break;
		}
		bptr = ChrPtr(Buf);
		switch (phase) {
		case 0:
			if (!strncasecmp(bptr, "part=", 5)) {
				extract_token(mime_filename, &bptr[5], 1, '|', sizeof mime_filename);
				extract_token(mime_partnum, &bptr[5], 2, '|', sizeof mime_partnum);
				extract_token(mime_disposition, &bptr[5], 3, '|', sizeof mime_disposition);
				extract_token(mime_content_type, &bptr[5], 4, '|', sizeof mime_content_type);
				/* do we care? mime_length = */extract_int(&bptr[5], 5);

				if (  (!strcasecmp(mime_content_type, "text/calendar"))
				      || (!strcasecmp(mime_content_type, "application/ics"))
				      || (!strcasecmp(mime_content_type, "text/vtodo"))
				      || (!strcasecmp(mime_content_type, "text/todo"))
					) {
					strcpy(relevant_partnum, mime_partnum);
				}
			}
			else if (!strncasecmp(bptr, "from=", 4)) {
				extract_token(from, bptr, 1, '=', sizeof(from));
			}
			else if ((phase == 0) && (!strncasecmp(bptr, "text", 4))) {
				phase = 1;
			}
		break;
		case 1:
			if (!IsEmptyStr(bptr)) {
				if (!strncasecmp(bptr, "Content-type: ", 14)) {
					safestrncpy(msg4_content_type, &bptr[14], sizeof msg4_content_type);
					striplt(msg4_content_type);
				}
				else if (!strncasecmp(bptr, "Content-transfer-encoding: ", 27)) {
					safestrncpy(msg4_content_encoding, &bptr[27], sizeof msg4_content_encoding);
					striplt(msg4_content_type);
				}
				else if ((!strncasecmp(bptr, "Content-length: ", 16))) {
					msg4_content_length = atoi(&bptr[16]);
				}
				break;
			}
			else {
				phase++;
				
				if ((msg4_content_length > 0)
				    && ( !strcasecmp(msg4_content_encoding, "7bit"))
				    && ((!strcasecmp(mime_content_type, "text/calendar"))
					|| (!strcasecmp(mime_content_type, "application/ics"))
					|| (!strcasecmp(mime_content_type, "text/vtodo"))
					|| (!strcasecmp(mime_content_type, "text/todo"))
					    )
					) 
				{
				}
			}
		case 2:
			if (Data == NULL)
				Data = NewStrBufPlain(NULL, msg4_content_length * 2);
			if (msg4_content_length > 0) {
				StrBuf_ServGetBLOBBuffered(Data, msg4_content_length);
				phase ++;
			}
			else {
				StrBufAppendBuf(Data, Buf, 0);
				StrBufAppendBufPlain(Data, "\r\n", 1, 0);
			}
		case 3:
			StrBufAppendBuf(Data, Buf, 0);
		}
	}
	FreeStrBuf(&Buf);

	/* If MSG4 didn't give us the part we wanted, but we know that we can find it
	 * as one of the other MIME parts, attempt to load it now.
	 */
	if ((Data == NULL) && (!IsEmptyStr(relevant_partnum))) {
		Data = load_mimepart(msgnum, relevant_partnum);
	}

	if (Data != NULL) {
		relevant_source = (char*) ChrPtr(Data);
		process_ical_object(msgnum, unread,
				    from, 
				    relevant_source, 
				    which_kind,
				    CallBack,
				    calv);
	}
	FreeStrBuf (&Data);

	icalmemory_free_ring();
}

/*
 * Display a calendar item
 */
int calendar_LoadMsgFromServer(SharedMessageStatus *Stat, 
			       void **ViewSpecific, 
			       message_summary* Msg, 
			       int is_new, 
			       int i)
{
	calview *c = (calview*) *ViewSpecific;
	load_ical_object(Msg->msgnum, is_new, (-1), display_individual_cal, c, 1);
	return 0;
}

/*
 * display the editor component for an event
 */
void display_edit_event(void) {
	long msgnum = 0L;

	msgnum = lbstr("msgnum");
	if (msgnum > 0L) {
		/* existing event */
		load_ical_object(msgnum, 0, ICAL_VEVENT_COMPONENT, display_edit_individual_event, NULL, 0);
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
		load_ical_object(msgnum, 0, (-1), save_individual_event, NULL, 0);
	}
	else {
		save_individual_event(NULL, 0L, "", 0, NULL);
	}
}





/*
 * Anonymous request of freebusy data for a user
 */
void do_freebusy(void)
{
	const char *req = ChrPtr(WC->Hdr->HR.ReqLine);
	char who[SIZ];
	char buf[SIZ];
	int len;
	long lines;

	extract_token(who, req, 0, ' ', sizeof who);
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

	syslog(LOG_INFO, "freebusy requested for <%s>\n", who);
	serv_printf("ICAL freebusy|%s", who);
	serv_getln(buf, sizeof buf);

	if (buf[0] != '1') {
		hprintf("HTTP/1.1 404 %s\n", &buf[4]);
		output_headers(0, 0, 0, 0, 0, 0);
		hprintf("Content-Type: text/plain\r\n");
		wc_printf("%s\n", &buf[4]);
		end_burst();
		return;
	}

	read_server_text(WC->WBuf, &lines);
	http_transmit_thing("text/calendar", 0);
}



int calendar_Cleanup(void **ViewSpecific)
{
	calview *c;
	
	c = (calview *) *ViewSpecific;

	wDumpContent(1);
	free (c);
	*ViewSpecific = NULL;

	return 0;
}

int __calendar_Cleanup(void **ViewSpecific)
{
	calview *c;
	
	c = (calview *) *ViewSpecific;

	free (c);
	*ViewSpecific = NULL;

	return 0;
}


void 
InitModule_CALENDAR
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_CALENDAR,
		calendar_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL,
		calendar_LoadMsgFromServer,
		calendar_RenderView_or_Tail,
		calendar_Cleanup);

	RegisterReadLoopHandlerset(
		VIEW_CALBRIEF,
		calendar_GetParamsGetServerCall,
		NULL,
		NULL,
		NULL,
		calendar_LoadMsgFromServer,
		calendar_RenderView_or_Tail,
		calendar_Cleanup);



	RegisterPreference("daystart", _("Calendar day view begins at:"), PRF_INT, NULL);
	RegisterPreference("dayend", _("Calendar day view ends at:"), PRF_INT, NULL);
	RegisterPreference("weekstart", _("Week starts on:"), PRF_INT, NULL);

	WebcitAddUrlHandler(HKEY("freebusy"), "", 0, do_freebusy, COOKIEUNNEEDED|ANONYMOUS|FORCE_SESSIONCLOSE);
	WebcitAddUrlHandler(HKEY("display_edit_task"), "", 0, display_edit_task, 0);
	WebcitAddUrlHandler(HKEY("display_edit_event"), "", 0, display_edit_event, 0);
	WebcitAddUrlHandler(HKEY("save_event"), "", 0, save_event, 0);
	WebcitAddUrlHandler(HKEY("respond_to_request"), "", 0, respond_to_request, 0);
	WebcitAddUrlHandler(HKEY("handle_rsvp"), "", 0, handle_rsvp, 0);
}
