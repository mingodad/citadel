/*
 * Editing calendar events.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "webcit.h"
#include "webserver.h"
#include "calendar.h"

/*
 * Display an event by itself (for editing)
 * supplied_vevent	the event to edit
 * msgnum		reference on the citserver
 */
void display_edit_individual_event(icalcomponent *supplied_vevent, long msgnum, char *from,
	int unread, calview *calv)
{
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
	/************************************************************
	 * Uncomment this to see the UID in calendar events for debugging
	int sequence = 0;
	*/
	char weekday_labels[7][32];
	char month_labels[12][32];
	long weekstart = 0;
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;
	char weekday_is_selected[7];
	int which_rrmonthtype_is_preselected = 0;

	int rrmday;
	int rrmweekday;

	icaltimetype day1;
	int weekbase;
	int rrmweek;
	int rrymweek;
	int rrymweekday;
	int rrymonth;
	int which_rrend_is_preselected;
	int which_rryeartype_is_preselected;

	const char *ch;
	char *tabnames[3];
	const char *frequency_units[8];
	const char *ordinals[6];

	frequency_units[0] = _("seconds");
	frequency_units[1] = _("minutes");
	frequency_units[2] = _("hours");
	frequency_units[3] = _("days");
	frequency_units[4] = _("weeks");
	frequency_units[5] = _("months");
	frequency_units[6] = _("years");
	frequency_units[7] = _("never");


	ordinals[0] = "0";
	ordinals[1] = _("first");
	ordinals[2] = _("second");
	ordinals[3] = _("third");
	ordinals[4] = _("fourth");
	ordinals[5] = _("fifth");


	tabnames[0] = _("Event");
	tabnames[1] = _("Attendees");
	tabnames[2] = _("Recurrence");

	get_pref_long("weekstart", &weekstart, 17);
	if (weekstart > 6) weekstart = 0;

	syslog(9, "display_edit_individual_event(%ld) calview=%s year=%s month=%s day=%s\n",
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

	/* populate the month names - begin */
	now = 259200L;	/* 1970-jan-04 is the first Sunday ever */
	localtime_r(&now, &tm_now);
	for (i=0; i<12; ++i) {
		localtime_r(&now, &tm_now);
		wc_strftime(month_labels[i], 32, "%B", &tm_now);
		now += 2678400L;
	}
	/* populate the month names - end */

	now = time(NULL);
	strcpy(organizer_string, "");
	strcpy(attendee_string, "");

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;

		/* Convert all timestamps to UTC to make them easier to process. */
		ical_dezonify(vevent);

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
				msgnum, from, unread, NULL
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
	/************************************************************
	 * Uncomment this to see the UID in calendar events for debugging
	if (p != NULL) {
		sequence = icalproperty_get_sequence(p);
	}
	*/
	/* Begin output */
	output_headers(1, 1, 2, 0, 0, 0);
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<h1>");
	wc_printf(_("Add or edit an event"));
	wc_printf("</h1>");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	/************************************************************
	 * Uncomment this to see the UID in calendar events for debugging
	wc_printf("UID == ");
	p = icalcomponent_get_first_property(vevent, ICAL_UID_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("<br>\n");
	wc_printf("SEQUENCE == %d<br>\n", sequence);
	*************************************************************/

	wc_printf("<form name=\"EventForm\" method=\"POST\" action=\"save_event\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wc_printf("<input type=\"hidden\" name=\"msgnum\" value=\"%ld\">\n",
		msgnum);
	wc_printf("<input type=\"hidden\" name=\"calview\" value=\"%s\">\n",
		bstr("calview"));
	wc_printf("<input type=\"hidden\" name=\"year\" value=\"%s\">\n",
		bstr("year"));
	wc_printf("<input type=\"hidden\" name=\"month\" value=\"%s\">\n",
		bstr("month"));
	wc_printf("<input type=\"hidden\" name=\"day\" value=\"%s\">\n",
		bstr("day"));


	tabbed_dialog(3, tabnames);
	begin_tab(0, 3);

	/* Put it in a borderless table so it lines up nicely */
	wc_printf("<table border='0' width='100%%'>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("Summary"));
	wc_printf("</b></td><td>\n"
		"<input type=\"text\" name=\"summary\" "
		"maxlength=\"64\" size=\"64\" value=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("\"></td></tr>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("Location"));
	wc_printf("</b></td><td>\n"
		"<input type=\"text\" name=\"location\" "
		"maxlength=\"64\" size=\"64\" value=\"");
	p = icalcomponent_get_first_property(vevent, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("\"></td></tr>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("Start"));
	wc_printf("</b></td><td>\n");
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
	display_icaltimetype_as_webform(&t_start, "dtstart", 0);

	wc_printf("<input type=\"checkbox\" id=\"alldayevent\" name=\"alldayevent\" "
		"value=\"yes\" onclick=\"eventEditAllDay();\""
		" %s >%s",
		(t_start.is_date ? "checked=\"checked\"" : "" ),
		_("All day event")
	);

	wc_printf("</td></tr>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("End"));
	wc_printf("</b></td><td id=\"dtendcell\">\n");
	p = icalcomponent_get_first_property(vevent,
						ICAL_DTEND_PROPERTY);
	if (p != NULL) {
		t_end = icalproperty_get_dtend(p);

		/*
		 * If this is an all-day-event, the end time is set to real end
		 * day + 1, so we have to adjust accordingly.
		 */
		if (t_start.is_date) {
			icaltime_adjust(&t_end, -1, 0, 0, 0);
		}
	}
	else {
		if (created_new_vevent == 1) {
			/* set default duration */
			if (t_start.is_date) {
				/*
				 * If this is an all-day-event, set the end time to be identical to
				 * the start time (the hour/minute/second will be set to midnight).
				 */
				t_end = t_start;
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
		else {
			/*
			 * If an existing event has no end date/time this is
			 * supposed to mean end = start.
			 */
			t_end = t_start;
		}
	}
	display_icaltimetype_as_webform(&t_end, "dtend", 0);
	wc_printf("</td></tr>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("Notes"));
	wc_printf("</b></td><td>\n"
		"<textarea name=\"description\" "
		"rows='5' cols='72' style='width:72'>\n"
	);
	p = icalcomponent_get_first_property(vevent, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		escputs((char *)icalproperty_get_comment(p));
	}
	wc_printf("</textarea></td></tr>");

	/*
	 * For a new event, the user creating the event should be the
	 * organizer.  Set this field accordingly.
	 */
	if (icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY)
	   == NULL) {
		sprintf(organizer_string, "mailto:%s", ChrPtr(WC->cs_inet_email));
		icalcomponent_add_property(vevent,
			icalproperty_new_organizer(organizer_string)
		);
	}

	/*
	 * Determine who is the organizer of this event.
	 * We need to determine "me" or "not me."
	 */
	organizer = icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY);
	if (organizer != NULL) {
		strcpy(organizer_string, icalproperty_get_organizer(organizer));
		if (!strncasecmp(organizer_string, "mailto:", 7)) {
			strcpy(organizer_string, &organizer_string[7]);
			striplt(organizer_string);
			serv_printf("ISME %s", organizer_string);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				organizer_is_me = 1;
			}
		}
	}

	wc_printf("<tr><td><b>");
	wc_printf(_("Organizer"));
	wc_printf("</b></td><td>");
	escputs(organizer_string);
	if (organizer_is_me) {
		wc_printf(" <font size='-1'><i>");
		wc_printf(_("(you are the organizer)"));
		wc_printf("</i></font>\n");
	}

	/*
	 * Transmit the organizer as a hidden field.   We don't want the user
	 * to be able to change it, but we do want it fed back to the server,
	 * especially if this is a new event and there is no organizer already
	 * in the calendar object.
	 */
	wc_printf("<input type=\"hidden\" name=\"organizer\" value=\"");
	escputs(organizer_string);
	wc_printf("\">");

	wc_printf("</td></tr>\n");

	/* Transparency */
	wc_printf("<tr><td><b>");
	wc_printf(_("Show time as:"));
	wc_printf("</b></td><td>");

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

	wc_printf("<input type=\"radio\" name=\"transp\" value=\"transparent\"");
	if ((v != NULL) && (icalvalue_get_transp(v) == ICAL_TRANSP_TRANSPARENT)) {
		wc_printf(" CHECKED");
	}
	wc_printf(">");
	wc_printf(_("Free"));
	wc_printf("&nbsp;&nbsp;");

	wc_printf("<input type=\"radio\" name=\"transp\" value=\"opaque\"");
	if ((v != NULL) && (icalvalue_get_transp(v) == ICAL_TRANSP_OPAQUE)) {
		wc_printf(" CHECKED");
	}
	wc_printf(">");
	wc_printf(_("Busy"));

	wc_printf("</td></tr>\n");


	/* Done with properties. */
	wc_printf("</TABLE>\n");

	end_tab(0, 3);

	/* Attendees tab (need to move things here) */
	begin_tab(1, 3);
	wc_printf("<table border='0' width='100%%'>\n");	/* same table style as the event tab */
	wc_printf("<tr><td><b>");
	wc_printf(_("Attendees"));
	wc_printf("</b><br>"
		"<font size='-2'>");
	wc_printf(_("(One per line)"));
	wc_printf("</font>\n");

	/* Pop open an address book -- begin */
	wc_printf(
		"&nbsp;<a href=\"javascript:PopOpenAddressBook('attendees_box|%s');\" "
		"title=\"%s\">"
		"<img alt='' align='middle' border='0' width='16' height='16' src=\"static/webcit_icons/essen/16x16/contact.png\">"
		"</a>",
		_("Attendees"),
		_("Contacts")
	);
	/* Pop open an address book -- end */

	wc_printf("</td><td>"
		"<textarea %s name=\"attendees\" id=\"attendees_box\" "
		"onchange=\"EnableOrDisableCheckButton();\" "
		"onKeyPress=\"EnableOrDisableCheckButton();\" "
		"rows='10' cols='72' style='width:72'>\n",
		(organizer_is_me ? "" : "disabled='disabled' ")
	);
	i = 0;
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
		ch = icalproperty_get_attendee(attendee);
		if ((ch != NULL) && !strncasecmp(ch, "mailto:", 7)) {

			/* screen name or email address */
			safestrncpy(attendee_string, ch + 7, sizeof(attendee_string));
			striplt(attendee_string);
			if (i++) wc_printf("\n");
			escputs(attendee_string);
			wc_printf(" ");

			/* participant status */
			partstat_as_string(buf, attendee);
			escputs(buf);
		}
	}
	wc_printf("</textarea></td></tr>\n");
	wc_printf("</table>\n");
	end_tab(1, 3);

	/* Recurrence tab */
	begin_tab(2, 3);

	rrule = icalcomponent_get_first_property(vevent, ICAL_RRULE_PROPERTY);
	if (rrule) {
		recur = icalproperty_get_rrule(rrule);
	}
	else {
		/* blank recurrence with some sensible defaults */
		memset(&recur, 0, sizeof(struct icalrecurrencetype));
		recur.count = 3;
		recur.until = icaltime_null_time();
		recur.interval = 1;
		recur.freq = ICAL_WEEKLY_RECURRENCE;
	}

	wc_printf("<input type=\"checkbox\" id=\"is_recur\" name=\"is_recur\" "
		"value=\"yes\" "
		"onclick=\"RecurrenceShowHide();\""
		" %s >%s",
		(rrule ? "checked=\"checked\"" : "" ),
		_("This is a recurring event")
	);

	wc_printf("<div id=\"rrule_div\">\n");		/* begin 'rrule_div' div */

	wc_printf("<table border='0' cellspacing='10' width='100%%'>\n");

	wc_printf("<tr><td><b>");
	wc_printf(_("Recurrence rule"));
	wc_printf("</b></td><td>");

	if ((recur.freq < 0) || (recur.freq > 6)) recur.freq = 4;
	wc_printf("%s ", _("Repeats every"));

	wc_printf("<input type=\"text\" name=\"interval\" maxlength=\"3\" size=\"3\" ");
	wc_printf("value=\"%d\">&nbsp;", recur.interval);

	wc_printf("<select name=\"freq\" id=\"freq_selector\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (i=0; i<(sizeof frequency_units / sizeof(char *)); ++i) {
		wc_printf("<option %s%svalue=\"%d\">%s</option>\n",
			((i == recur.freq) ? "selected='selected' " : ""),
			(((i == recur.freq) || ((i>=3)&&(i<=6))) ? "" : "disabled='disabled' "),
			i,
			frequency_units[i]
		);
	}
	wc_printf("</select>\n");

	wc_printf("<div id=\"weekday_selector\">");	/* begin 'weekday_selector' div */
	wc_printf("%s<br>", _("on these weekdays:"));

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

	for (j=0; j<7; ++j) {
		i = ((j + (int)weekstart) % 7);
		wc_printf("<input type=\"checkbox\" name=\"weekday%d\" value=\"yes\"", i);
		if (weekday_is_selected[i]) wc_printf(" checked='checked'");
		wc_printf(">%s\n", weekday_labels[i]);
	}
	wc_printf("</div>\n");				/* end 'weekday_selector' div */





	wc_printf("<div id=\"monthday_selector\">");	/* begin 'monthday_selector' div */

	wc_printf("<input type=\"radio\" name=\"rrmonthtype\" id=\"rrmonthtype_mday\" "
		"value=\"rrmonthtype_mday\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rrmonthtype_is_preselected == 0) ? "checked='checked'" : "")
	);

	rrmday = t_start.day;
	rrmweekday = icaltime_day_of_week(t_start) - 1;

	/* Figure out what week of the month we're in */
	day1 = t_start;
	day1.day = 1;
	weekbase = icaltime_week_number(day1);
	rrmweek = icaltime_week_number(t_start) - weekbase + 1;

	/* Are we going by day of the month or week/day? */

	if (recur.by_month_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
		which_rrmonthtype_is_preselected = 0;
		rrmday = recur.by_month_day[0];
	}
	else if (recur.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX) {
		which_rrmonthtype_is_preselected = 1;
		rrmweek = icalrecurrencetype_day_position(recur.by_day[0]);
		rrmweekday = icalrecurrencetype_day_day_of_week(recur.by_day[0]) - 1;
	}

	wc_printf(_("on day %s%d%s of the month"), "<span id=\"rrmday\">", rrmday, "</span>");
	wc_printf("<br>\n");

	wc_printf("<input type=\"radio\" name=\"rrmonthtype\" id=\"rrmonthtype_wday\" "
		"value=\"rrmonthtype_wday\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rrmonthtype_is_preselected == 1) ? "checked='checked'" : "")
	);

	wc_printf(_("on the "));
	wc_printf("<select name=\"rrmweek\" id=\"rrmweek\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (i=1; i<=5; ++i) {
		wc_printf("<option %svalue=\"%d\">%s</option>\n",
			((i==rrmweek) ? "selected='selected' " : ""),
			i,
			ordinals[i]
		);
	}
	wc_printf("</select> \n");

	wc_printf("<select name=\"rrmweekday\" id=\"rrmweekday\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (j=0; j<7; ++j) {
		i = ((j + (int)weekstart) % 7);
		wc_printf("<option %svalue=\"%d\">%s</option>\n",
			((i==rrmweekday) ? "selected='selected' " : ""),
			i,
			weekday_labels[i]
		);
	}
	wc_printf("</select>");

	wc_printf(" %s<br>\n", _("of the month"));

	wc_printf("</div>\n");				/* end 'monthday_selector' div */


	rrymweek = rrmweek;
	rrymweekday = rrmweekday;
	rrymonth = t_start.month;
	which_rryeartype_is_preselected = 0;

	if (
		(recur.by_day[0] != ICAL_RECURRENCE_ARRAY_MAX)
		&& (recur.by_day[0] != 0)
		&& (recur.by_month[0] != ICAL_RECURRENCE_ARRAY_MAX)
		&& (recur.by_month[0] != 0)
	) {
		which_rryeartype_is_preselected = 1;
		rrymweek = icalrecurrencetype_day_position(recur.by_day[0]);
		rrymweekday = icalrecurrencetype_day_day_of_week(recur.by_day[0]) - 1;
		rrymonth = recur.by_month[0];
	}

	wc_printf("<div id=\"yearday_selector\">");	/* begin 'yearday_selector' div */

	wc_printf("<input type=\"radio\" name=\"rryeartype\" id=\"rryeartype_ymday\" "
		"value=\"rryeartype_ymday\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rryeartype_is_preselected == 0) ? "checked='checked'" : "")
	);
	wc_printf(_("every "));
	wc_printf("<span id=\"ymday\">%s</span><br>", _("year on this date"));

	wc_printf("<input type=\"radio\" name=\"rryeartype\" id=\"rryeartype_ywday\" "
		"value=\"rryeartype_ywday\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rryeartype_is_preselected == 1) ? "checked='checked'" : "")
	);

	wc_printf(_("on the "));
	wc_printf("<select name=\"rrymweek\" id=\"rrymweek\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (i=1; i<=5; ++i) {
		wc_printf("<option %svalue=\"%d\">%s</option>\n",
			((i==rrymweek) ? "selected='selected' " : ""),
			i,
			ordinals[i]
		);
	}
	wc_printf("</select> \n");

	wc_printf("<select name=\"rrymweekday\" id=\"rrymweekday\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (j=0; j<7; ++j) {
		i = ((j + (int)weekstart) % 7);
		wc_printf("<option %svalue=\"%d\">%s</option>\n",
			((i==rrymweekday) ? "selected='selected' " : ""),
			i,
			weekday_labels[i]
		);
	}
	wc_printf("</select>");

	wc_printf(" %s ", _("of"));

	wc_printf("<select name=\"rrymonth\" id=\"rrymonth\" size=\"1\" "
		"onChange=\"RecurrenceShowHide();\">\n");
	for (i=1; i<=12; ++i) {
		wc_printf("<option %svalue=\"%d\">%s</option>\n",
			((i==rrymonth) ? "selected='selected' " : ""),
			i,
			month_labels[i-1]
		);
	}
	wc_printf("</select>");
	wc_printf("<br>\n");

	wc_printf("</div>\n");				/* end 'yearday_selector' div */

	wc_printf("</td></tr>\n");


	which_rrend_is_preselected = 0;
	if (!icaltime_is_null_time(recur.until)) which_rrend_is_preselected = 2;
	if (recur.count > 0) which_rrend_is_preselected = 1;

	wc_printf("<tr><td><b>");
	wc_printf(_("Recurrence range"));
	wc_printf("</b></td><td>\n");

	wc_printf("<input type=\"radio\" name=\"rrend\" id=\"rrend_none\" "
		"value=\"rrend_none\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rrend_is_preselected == 0) ? "checked='checked'" : "")
	);
	wc_printf("%s<br>\n", _("No ending date"));

	wc_printf("<input type=\"radio\" name=\"rrend\" id=\"rrend_count\" "
		"value=\"rrend_count\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rrend_is_preselected == 1) ? "checked='checked'" : "")
	);
	wc_printf(_("Repeat this event"));
	wc_printf(" <input type=\"text\" name=\"rrcount\" id=\"rrcount\" maxlength=\"3\" size=\"3\" ");
	wc_printf("value=\"%d\"> ", recur.count);
	wc_printf(_("times"));
	wc_printf("<br>\n");

	wc_printf("<input type=\"radio\" name=\"rrend\" id=\"rrend_until\" "
		"value=\"rrend_until\" "
		"%s onChange=\"RecurrenceShowHide();\">",
		((which_rrend_is_preselected == 2) ? "checked='checked'" : "")
	);
	wc_printf(_("Repeat this event until "));

	if (icaltime_is_null_time(recur.until)) {
		recur.until = icaltime_add(t_start, icaldurationtype_from_int(604800));
	}
	display_icaltimetype_as_webform(&recur.until, "rruntil", 1);
	wc_printf("<br>\n");

	wc_printf("</td></tr>\n");

	wc_printf("</table>\n");
	wc_printf("</div>\n");				/* end 'rrule' div */

	end_tab(2, 3);

	/* submit buttons (common area beneath the tabs) */
	begin_tab(3, 3);
	wc_printf("<center>"
		"<input type=\"submit\" name=\"save_button\" value=\"%s\">"
		"&nbsp;&nbsp;"
		"<input type=\"submit\" name=\"delete_button\" value=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<input type=\"submit\" id=\"check_button\" name=\"check_button\" value=\"%s\">\n"
		"&nbsp;&nbsp;"
		"<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n"
		"</center>\n",
		_("Save"),
		_("Delete"),
		_("Check attendee availability"),
		_("Cancel")
	);
	end_tab(3, 3);
	wc_printf("</form>\n");

	StrBufAppendPrintf(WC->trailing_javascript,
		"eventEditAllDay();		\n"
		"RecurrenceShowHide();		\n"
		"EnableOrDisableCheckButton();	\n"
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
void save_individual_event(icalcomponent *supplied_vevent, long msgnum, char *from,
			int unread, calview *calv) {
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
	const char *ch;

	if (supplied_vevent != NULL) {
		vevent = supplied_vevent;

		/* Convert all timestamps to UTC to make them easier to process. */
		ical_dezonify(vevent);

		/*
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
				msgnum, from, unread, NULL
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

		/* Replace values in the component with ones from the form */

		while (prop = icalcomponent_get_first_property(vevent,
		      ICAL_SUMMARY_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

		/* Add NOW() to the calendar object... */
		icalcomponent_set_dtstamp(vevent,
					  icaltime_from_timet(
						  time(NULL), 0));

	 	if (havebstr("summary")) {
		 	icalcomponent_add_property(vevent,
				  	icalproperty_new_summary(bstr("summary")));
	 	} else {
		 	icalcomponent_add_property(vevent,
					icalproperty_new_summary(_("Untitled Event")));
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

		prop = icalproperty_new_dtstart(event_start);

		if (all_day_event) {
			/* Force it to serialize as a date-only rather than date/time */
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

		if (all_day_event) {
			icaltime_from_webform_dateonly(&t, "dtend");

			/* with this field supposed to be non-inclusive we have to add one day */
			icaltime_adjust(&t, 1, 0, 0, 0);
		}
		else {
			icaltime_from_webform(&t, "dtend");
		}

		icalcomponent_add_property(vevent,
			icalproperty_new_dtend(icaltime_normalize(t)
			)
		);

		/* recurrence rules -- begin */

		/* remove any existing rule */
		while (prop = icalcomponent_get_first_property(vevent, ICAL_RRULE_PROPERTY), prop != NULL) {
			icalcomponent_remove_property(vevent, prop);
			icalproperty_free(prop);
		}

		if (yesbstr("is_recur")) {
			struct icalrecurrencetype recur;
			icalrecurrencetype_clear(&recur);

			recur.interval = atoi(bstr("interval"));
			recur.freq = atoi(bstr("freq"));

			switch(recur.freq) {

				/* These can't happen; they're disabled. */
				case ICAL_SECONDLY_RECURRENCE:
					break;
				case ICAL_MINUTELY_RECURRENCE:
					break;
				case ICAL_HOURLY_RECURRENCE:
					break;

				/* Daily is valid but there are no further inputs. */
				case ICAL_DAILY_RECURRENCE:
					break;

				/* These are the real options. */

				case ICAL_WEEKLY_RECURRENCE:
					j=0;
					for (i=0; i<7; ++i) {
						snprintf(buf, sizeof buf, "weekday%d", i);
						if (YESBSTR(buf)) recur.by_day[j++] =
							icalrecurrencetype_day_day_of_week(i+1);
					}
					recur.by_day[j++] = ICAL_RECURRENCE_ARRAY_MAX;
					break;

				case ICAL_MONTHLY_RECURRENCE:
					if (!strcasecmp(bstr("rrmonthtype"), "rrmonthtype_mday")) {
						recur.by_month_day[0] = event_start.day;
						recur.by_month_day[1] = ICAL_RECURRENCE_ARRAY_MAX;
					}
					else if (!strcasecmp(bstr("rrmonthtype"), "rrmonthtype_wday")) {
						recur.by_day[0] = (atoi(bstr("rrmweek")) * 8)
								+ atoi(bstr("rrmweekday")) + 1;
						recur.by_day[1] = ICAL_RECURRENCE_ARRAY_MAX;
					}
					break;

				case ICAL_YEARLY_RECURRENCE:
					if (!strcasecmp(bstr("rryeartype"), "rryeartype_ymday")) {
						/* no further action is needed here */
					}
					else if (!strcasecmp(bstr("rryeartype"), "rryeartype_ywday")) {
						recur.by_month[0] = atoi(bstr("rrymonth"));
						recur.by_month[1] = ICAL_RECURRENCE_ARRAY_MAX;
						recur.by_day[0] = (atoi(bstr("rrymweek")) * 8)
								+ atoi(bstr("rrymweekday")) + 1;
						recur.by_day[1] = ICAL_RECURRENCE_ARRAY_MAX;
					}
					break;

				/* This one can't happen either. */
				case ICAL_NO_RECURRENCE:
					break;
			}

			if (!strcasecmp(bstr("rrend"), "rrend_count")) {
				recur.count = atoi(bstr("rrcount"));
			}
			else if (!strcasecmp(bstr("rrend"), "rrend_until")) {
				icaltime_from_webform_dateonly(&recur.until, "rruntil");
			}

			icalcomponent_add_property(vevent, icalproperty_new_rrule(recur));
		}

		/* recurrence rules -- end */

		/* See if transparency is indicated */
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

		/* Give this event a UID if it doesn't have one. */
		if (icalcomponent_get_first_property(vevent,
		   ICAL_UID_PROPERTY) == NULL) {
			generate_uuid(buf);
			icalcomponent_add_property(vevent, icalproperty_new_uid(buf));
		}

		/* Increment the sequence ID */
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

		/*
		 * Set the organizer, only if one does not already exist *and*
		 * the form is supplying one
		 */
		strcpy(buf, bstr("organizer"));
		if ( (icalcomponent_get_first_property(vevent,
		   ICAL_ORGANIZER_PROPERTY) == NULL)
		   && (!IsEmptyStr(buf)) ) {

			/* set new organizer */
			sprintf(organizer_string, "MAILTO:%s", buf);
			icalcomponent_add_property(vevent,
				icalproperty_new_organizer(organizer_string)
			);

		}

		/*
		 * Add any new attendees listed in the web form
		 */

		/* First, strip out the parenthesized partstats.  */
		strcpy(form_attendees, bstr("attendees"));
		while (	stripout(form_attendees, '(', ')') != 0);

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

		/* Now iterate! */
		for (i=0; i<num_tokens(form_attendees, '\n'); ++i) {
			extract_token(buf, form_attendees, i, '\n', sizeof buf);
			striplt(buf);
			if (!IsEmptyStr(buf)) {
				sprintf(attendee_string, "MAILTO:%s", buf);
				foundit = 0;

				for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
					ch = icalproperty_get_attendee(attendee);
					if ((ch != NULL) && !strcasecmp(attendee_string, ch))
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
STARTOVER:	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
			ch = icalproperty_get_attendee(attendee);
			if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {
				safestrncpy(attendee_string, ch + 7, sizeof(attendee_string));
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
		encaps = ical_encapsulate_subcomponent(icalcomponent_new_clone(vevent));

		/* Set the method to PUBLISH */
		icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

		/* If the user clicked 'Save' then save it to the server. */
		if ( (encaps != NULL) && (havebstr("save_button")) ) {
			serv_puts("ENT0 1|||4|||1|");
			serv_getln(buf, sizeof buf);
			switch (buf[0]) {
			case '8':
				serv_puts("Content-type: text/calendar");
				serv_puts("Content-Transfer-Encoding: quoted-printable");
				serv_puts("");
				text_to_server_qp(icalcomponent_as_ical_string(encaps));
//				serv_puts(icalcomponent_as_ical_string(encaps));
				serv_puts("000");
			case '4':
				while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {}
				break;
			case '2':
				AppendImportantMessage(buf + 4, - 1);
				break;
			default:
				break;
			}
			icalmemory_free_ring ();
			icalcomponent_free(encaps);
			encaps = NULL;
		}

		/* Or, check attendee availability if the user asked for that. */
		if ( (encaps != NULL) && (havebstr("check_button")) ) {

			/* Call this function, which does the real work */
			check_attendee_availability(encaps);

			/* This displays the form again, with our annotations */
			display_edit_individual_event(encaps, msgnum, from, unread, NULL);

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

	/* If this was a save or delete, go back to the calendar or summary view. */
	if (!havebstr("check_button")) {
		if (!strcasecmp(bstr("calview"), "summary")) {
			display_summary_page();
		}
		else {
			readloop(readfwd, eUseDefault);
		}
	}
}
