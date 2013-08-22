/*
 * Handles the HTML display of calendar items.
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

/* These define how high the hour rows are in the day view */
#define TIMELINE	30
#define EXTRATIMELINE	(TIMELINE / 2)

void embeddable_mini_calendar(int year, int month)
{
	struct tm starting_tm;
	struct tm tm;
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;
	time_t colheader_time;
	struct tm colheader_tm;
	char colheader_label[32];
	long weekstart = 0;
	char url[256];
	char div_id[256];

	snprintf(div_id, sizeof div_id, "mini_calendar_%d", rand() );

	/* Determine what day to start.  If an impossible value is found, start on Sunday.
	*/
	get_pref_long("weekstart", &weekstart, 17);
	if (weekstart > 6) weekstart = 0;

	/*
	* Now back up to the 1st of the month...
	*/
	memset(&starting_tm, 0, sizeof(struct tm));

	starting_tm.tm_year = year - 1900;
	starting_tm.tm_mon = month - 1;
	starting_tm.tm_mday = 1;
	thetime = mktime(&starting_tm);

	memcpy(&tm, &starting_tm, sizeof(struct tm));
	while (tm.tm_mday != 1) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/* Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/* Now back up until we're on the user's preferred start day */
	localtime_r(&thetime, &tm);
	while (tm.tm_wday != weekstart) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	wc_printf("<div class=\"mini_calendar\" id=\"%s\">\n", div_id);

	/* Previous month link */
	localtime_r(&previous_month, &tm);
	wc_printf("<a href=\"javascript:minical_change_month(%d,%d);\">&laquo;</a>",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);

	wc_strftime(colheader_label, sizeof colheader_label, "%B", &starting_tm);
	wc_printf("<span class=\"mini_calendar_month_label\">"
		"%s %d"
		"</span>", colheader_label, year);

	/* Next month link */
	localtime_r(&next_month, &tm);
	wc_printf("<a href=\"javascript:minical_change_month(%d,%d);\">&raquo;</a>",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);

	wc_printf("<table class=\"mini_calendar_days\">"
		"<tr>");
	colheader_time = thetime;
	for (i=0; i<7; ++i) {
		colheader_time = thetime + (i * 86400) ;
		localtime_r(&colheader_time, &colheader_tm);
		wc_strftime(colheader_label, sizeof colheader_label, "%A", &colheader_tm);
		wc_printf("<th>%c</th>", colheader_label[0]);

	}
	wc_printf("</tr>\n");


        /* Now do 35 or 42 days */
        for (i = 0; i < 42; ++i) {
                localtime_r(&thetime, &tm);

                if (i < 35) {

			/* Before displaying Sunday, start a new row */
			if ((i % 7) == 0) {
				wc_printf("<tr>");
			}

			if (tm.tm_mon == month-1) {
				snprintf(url, sizeof url, "readfwd?calview=day?year=%d?month=%d?day=%d",
					tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
				wc_printf("<td><a href=\"%s\">%d</a></td>", url, tm.tm_mday);
			}
			else {
				wc_printf("<td> </td>");
			}

			/* After displaying one week, end the row */
			if ((i % 7) == 6) {
				wc_printf("</tr>\n");
			}

		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wc_printf("</table>"			/* end of inner table */
		"</div>\n");

	StrBufAppendPrintf(WC->trailing_javascript,
		"	function minical_change_month(year, month) {					\n"
		"		p = 'year=' + year + '&month=' + month					\n"
		"			+ '&r=' + CtdlRandomString();			                \n"
		"		new Ajax.Updater('%s', 'mini_calendar', 				\n"
		"			{ method: 'get', parameters: p, evalScripts: true } );		\n"
		"	}										\n"
		"",
		div_id
	);

}

/*
 * ajax embedder for the above mini calendar
 */
void ajax_mini_calendar(void)
{
	embeddable_mini_calendar( ibstr("year"), ibstr("month"));
}


/*
 * Display one day of a whole month view of a calendar
 */
void calendar_month_view_display_events(int year, int month, int day)
{
	long hklen;
	const char *HashKey;
	void *vCal;
	HashPos *Pos;
	disp_cal *Cal;
	icalproperty *p = NULL;
	icalproperty *q = NULL;
	struct icaltimetype t;
	struct icaltimetype end_t;
	struct icaltimetype today_start_t;
	struct icaltimetype today_end_t;
	struct icaltimetype today_t;
	struct tm starting_tm;
	struct tm ending_tm;
	int all_day_event = 0;
	int show_event = 0;
	char buf[256];
	wcsession *WCC = WC;
	time_t tt;

	if (GetCount(WCC->disp_cal_items) == 0) {
		wc_printf("<br>\n");
		return;
	}

	/*
	 * Create an imaginary event which spans the 24 hours of today.  Any events which
	 * overlap with this one take place at least partially in this day.  We have to
	 * convert it from a struct tm in order to make it UTC.
	 */
	memset(&starting_tm, 0, sizeof(struct tm));
	starting_tm.tm_year = year - 1900;
	starting_tm.tm_mon = month - 1;
	starting_tm.tm_mday = day;
	starting_tm.tm_hour = 0;
	starting_tm.tm_min = 0;
	today_start_t = icaltime_from_timet_with_zone(mktime(&starting_tm), 0, icaltimezone_get_utc_timezone());
	today_start_t.is_utc = 1;

	memset(&ending_tm, 0, sizeof(struct tm));
	ending_tm.tm_year = year - 1900;
	ending_tm.tm_mon = month - 1;
	ending_tm.tm_mday = day;
	ending_tm.tm_hour = 23;
	ending_tm.tm_min = 59;
	today_end_t = icaltime_from_timet_with_zone(mktime(&ending_tm), 0, icaltimezone_get_utc_timezone());
	today_end_t.is_utc = 1;

	/*
	 * Create another one without caring about the timezone for all day events.
	 */
	today_t = icaltime_null_date();
	today_t.year = year;
	today_t.month = month;
	today_t.day = day;

	/*
	 * Now loop through our list of events to see which ones occur today.
	 */
	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		Cal = (disp_cal*)vCal;
		all_day_event = 0;
		q = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
		if (q != NULL) {
			t = icalproperty_get_dtstart(q);
		}
		else {
			memset(&t, 0, sizeof t);
		}
		q = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
		if (q != NULL) {
			end_t = icalproperty_get_dtend(q);
		}
		else {
			memset(&end_t, 0, sizeof end_t);
		}
		if (t.is_date) all_day_event = 1;

		if (all_day_event)
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_t, icaltime_null_time());
		}
		else
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_start_t, today_end_t);
		}

		/*
		 * If we determined that this event occurs today, then display it.
	 	 */
		if (show_event) {

			/* time_t logtt = icaltime_as_timet(t);
			syslog(LOG_DEBUG, "Match on %04d-%02d-%02d for event %x%s on %s",
				year, month, day,
				(int)Cal, ((all_day_event) ? " (all day)" : ""),
				ctime(&logtt)
			); */

			p = icalcomponent_get_first_property(Cal->cal, ICAL_SUMMARY_PROPERTY);
			if (p == NULL) {
				p = icalproperty_new_summary(_("Untitled Event"));
				icalcomponent_add_property(Cal->cal, p);
			}
			if (p != NULL) {

				if (all_day_event) {
					wc_printf("<table><tr>"
						"<td>"
						);
				}


				wc_printf("<a class=\"event%s\" href=\"display_edit_event?"
					"msgnum=%ld?calview=month?year=%d?month=%d?day=%d\">"
					,
					(Cal->unread)?"_unread":"_read",
					Cal->cal_msgnum,
					year, month, day
				);

				escputs((char *) icalproperty_get_comment(p));

				wc_printf("<span class=\"tooltip\"><span class=\"btttop\"></span><span class=\"bttmiddle\">");

				wc_printf("<span class=\"calendar_event_from\">%s: %s</span><br>", _("From"), Cal->from);
				wc_printf("<span class=\"calendar_event_summary\">%s</span> ", _("Summary:"));
				escputs((char *)icalproperty_get_comment(p));
				wc_printf("<br>");

				q = icalcomponent_get_first_property(
					Cal->cal,
					ICAL_LOCATION_PROPERTY);
				if (q) {
					wc_printf("<span class=\"calendar_event_location\">%s</span> ", _("Location:"));
					escputs((char *)icalproperty_get_comment(q));
					wc_printf("<br>");
				}

				/*
				 * Only show start/end times if we're actually looking at the VEVENT
				 * component.  Otherwise it shows bogus dates for e.g. timezones
				 */
				if (icalcomponent_isa(Cal->cal) == ICAL_VEVENT_COMPONENT) {

					q = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
					if (q != NULL) {
						int no_end = 0;

						t = icalproperty_get_dtstart(q);
						q = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
						if (q != NULL) {
							end_t = icalproperty_get_dtend(q);
						}
						else {
							/*
							 * events with starting date/time equal
							 * ending date/time might get only
							 * DTSTART but no DTEND
							 */
							no_end = 1;
						}

						if (t.is_date) {
							/* all day event */
							struct tm d_tm;

							if (!no_end) {
								/* end given, have to adjust it */
								icaltime_adjust(&end_t, -1, 0, 0, 0);
							}
							memset(&d_tm, 0, sizeof d_tm);
							d_tm.tm_year = t.year - 1900;
							d_tm.tm_mon = t.month - 1;
							d_tm.tm_mday = t.day;
							wc_strftime(buf, sizeof buf, "%x", &d_tm);

							if (no_end || !icaltime_compare(t, end_t)) {
								wc_printf("<span class=\"calendar_event_date\">%s</span> %s<br>",
									_("Date:"), buf);
							}
							else {
								wc_printf("<span class=\"calendar_event_starting_date\">%s</span> %s<br>",
									_("Starting date:"), buf);
								d_tm.tm_year = end_t.year - 1900;
								d_tm.tm_mon = end_t.month - 1;
								d_tm.tm_mday = end_t.day;
								wc_strftime(buf, sizeof buf, "%x", &d_tm);
								wc_printf("<span class=\"calendar_event_ending_date\">%s</span> %s<br>",
									_("Ending date:"), buf);
							}
						}
						else {
							tt = icaltime_as_timet(t);
							webcit_fmt_date(buf, 256, tt, DATEFMT_BRIEF);
							if (no_end || !icaltime_compare(t, end_t)) {
								wc_printf("<span class=\"calendar_event_date\">%s</span> %s<br>",
									_("Date/time:"), buf);
							}
							else {
								wc_printf("<span class=\"calendar_event_starting_date\">%s</span> %s<br>",
									_("Starting date/time:"), buf);
								tt = icaltime_as_timet(end_t);
								webcit_fmt_date(buf, 256, tt, DATEFMT_BRIEF);
								wc_printf("<span class=\"calendar_event_ending_date\">%s</span> %s<br>",
									_("Ending date/time:"), buf);
							}

						}
					}

				}

				q = icalcomponent_get_first_property(Cal->cal, ICAL_DESCRIPTION_PROPERTY);
				if (q) {
					wc_printf("<span class=\"calendar_event_notes\">%s</span> ", _("Notes:"));
					escputs((char *)icalproperty_get_comment(q));
					wc_printf("<br>");
				}

				wc_printf("</span><span class=\"bttbottom\"></span></span>");
				wc_printf("</a><br>\n");

				if (all_day_event) {
					wc_printf("</td></tr></table>");
				}

			}

		}


	}
	DeleteHashPos(&Pos);
}


/*
 * Display one day of a whole month view of a calendar
 */
void calendar_month_view_brief_events(time_t thetime, const char *daycolor) {
	long hklen;
	const char *HashKey;
	void *vCal;
	HashPos *Pos;
	time_t event_tt;
	time_t event_tts;
	time_t event_tte;
	wcsession *WCC = WC;
	struct tm event_tms;
	struct tm event_tme;
	struct tm today_tm;
	icalproperty *p;
	icalproperty *e;
	struct icaltimetype t;
	disp_cal *Cal;
	int all_day_event = 0;
	char *timeformat;
	int time_format;

	time_format = get_time_format_cached ();

	if (time_format == WC_TIMEFORMAT_24) timeformat="%k:%M";
	else timeformat="%I:%M %p";

	localtime_r(&thetime, &today_tm);

	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		Cal = (disp_cal*)vCal;
		p = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			event_tts=event_tt;
			if (t.is_date) all_day_event = 1;
			else all_day_event = 0;

			if (all_day_event) {
				gmtime_r(&event_tts, &event_tms);
			}
			else {
				localtime_r(&event_tts, &event_tms);
			}
			/* \todo epoch &! daymask */
			if ((event_tms.tm_year == today_tm.tm_year)
				&& (event_tms.tm_mon == today_tm.tm_mon)
			&& (event_tms.tm_mday == today_tm.tm_mday)) {


			char sbuf[255];
			char ebuf[255];

			p = icalcomponent_get_first_property(
				Cal->cal,
				ICAL_SUMMARY_PROPERTY);
			if (p == NULL) {
				p = icalproperty_new_summary(_("Untitled Event"));
				icalcomponent_add_property(Cal->cal, p);
			}
			e = icalcomponent_get_first_property(
				Cal->cal,
				ICAL_DTEND_PROPERTY);
			if ((p != NULL) && (e != NULL)) {
				time_t difftime;
				int hours, minutes;
				t = icalproperty_get_dtend(e);
				event_tte = icaltime_as_timet(t);
				localtime_r(&event_tte, &event_tme);
				difftime=(event_tte-event_tts)/60;
				hours=(int)(difftime / 60);
				minutes=difftime % 60;
				wc_printf("<tr><td bgcolor='%s'>%i:%2i</td><td bgcolor='%s'>"
					"<a class=\"event%s\" href=\"display_edit_event?msgnum=%ld?calview=calbrief?year=%s?month=%s?day=%s\">",
					daycolor,
					hours, minutes,
					(Cal->unread)?"_unread":"_read",
					daycolor,
					Cal->cal_msgnum,
					bstr("year"),
					bstr("month"),
					bstr("day")
					);

				escputs((char *)
					icalproperty_get_comment(p));
				/* \todo: allso ammitime format */
				wc_strftime(&sbuf[0], sizeof(sbuf), timeformat, &event_tms);
				wc_strftime(&ebuf[0], sizeof(sbuf), timeformat, &event_tme);

				wc_printf("</a></td>"
					"<td bgcolor='%s'>%s</td><td bgcolor='%s'>%s</td></tr>",
					daycolor,
					sbuf,
					daycolor,
					ebuf);
				}

			}


		}
	}
	DeleteHashPos(&Pos);
}


/*
 * view one month. pretty view
 */
void calendar_month_view(int year, int month, int day) {
	struct tm starting_tm;
	struct tm tm;
	struct tm today_tm;
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;
	time_t colheader_time;
	time_t today_timet;
	struct tm colheader_tm;
	char colheader_label[32];
	long weekstart = 0;

	/*
	 * Make sure we know which day is today.
	 */
	today_timet = time(NULL);
	localtime_r(&today_timet, &today_tm);

	/*
	 * Determine what day to start.  If an impossible value is found, start on Sunday.
	 */
	get_pref_long("weekstart", &weekstart, 17);
	if (weekstart > 6) weekstart = 0;

	/*
	 * Now back up to the 1st of the month...
	 */
	memset(&starting_tm, 0, sizeof(struct tm));

	starting_tm.tm_year = year - 1900;
	starting_tm.tm_mon = month - 1;
	starting_tm.tm_mday = day;
	thetime = mktime(&starting_tm);

	memcpy(&tm, &starting_tm, sizeof(struct tm));
	while (tm.tm_mday != 1) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/* Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/* Now back up until we're on the user's preferred start day */
	localtime_r(&thetime, &tm);
	while (tm.tm_wday != weekstart) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/* Outer (to get the background color) */
	wc_printf("<div class=\"calendar_outer_frame row\">");
	wc_printf("<div class=\"calendar_top_bar offset_6 grid_2\">\n");

	localtime_r(&previous_month, &tm);
	wc_printf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wc_printf("<img alt=\"%s\" src=\"static/webcit_icons/essen/16x16/back.png\"></a>\n", _("previous"));

	wc_strftime(colheader_label, sizeof colheader_label, "%B", &starting_tm);
	wc_printf("<span class=\"calendar_colheader\">"
		"%s %d"
		"</span>", colheader_label, year);

	localtime_r(&next_month, &tm);
	wc_printf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wc_printf("<img alt=\"%s\" src=\"static/webcit_icons/essen/16x16/forward.png\"></a>\n", _("next"));

	wc_printf("</div>\n");

	/* Inner table (the real one) */
	wc_printf("<table id=\"inner_month\" class=\"offset_2 grid_10\"><tr>");
	wc_printf("<th class=\"calendar_th_noday\"></th>");
	colheader_time = thetime;
	for (i=0; i<7; ++i) {
		colheader_time = thetime + (i * 86400) ;
		localtime_r(&colheader_time, &colheader_tm);
		wc_strftime(colheader_label, sizeof colheader_label, "%A", &colheader_tm);
		wc_printf("<th class=\"calendar_th_weekday\">%s</th>", colheader_label);

	}
	wc_printf("</tr>\n");


        /* Now do 35 or 42 days */
	localtime_r(&thetime, &tm);
        for (i = 0; i<42; ++i) {

		/* Before displaying the first day of the week, start a new row */
		if ((i % 7) == 0) {
			wc_printf("<tr><td class=\"week_of_year\">");
			wc_strftime(colheader_label, sizeof colheader_label, "%V", &tm);
                        wc_printf("%s ", colheader_label);
		}

		wc_printf("<td class=\"cal%s\"><div class=\"day\">",
			((tm.tm_mon != month-1) ? "out" :
				(((tm.tm_year == today_tm.tm_year) && (tm.tm_mon == today_tm.tm_mon) && (tm.tm_mday == today_tm.tm_mday)) ? "today" :
				((tm.tm_wday==0 || tm.tm_wday==6) ? "weekend" :
					"day")))
			);
		if ((i==0) || (tm.tm_mday == 1)) {
			wc_strftime(colheader_label, sizeof colheader_label, "%B", &tm);
			wc_printf("%s ", colheader_label);
		}
		wc_printf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">"
			"%d</a></div>",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_mday);

		/* put the data here, stupid */
		calendar_month_view_display_events(
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday
			);

		wc_printf("</td>");

		/* After displaying the last day of the week, end the row */
		if ((i % 7) == 6) {
			wc_printf("</tr>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
		localtime_r(&thetime, &tm);

		if ( ((i % 7) == 6) && (tm.tm_mon != month-1) && (tm.tm_mday < 15) ) {
			i = 100;	/* break out of the loop */
		}
	}

	wc_printf("</table>"			/* end of inner table */
		"</div>\n"			/* end of outer frame */
	);
}

/*
 * view one month. brief view
 */
void calendar_brief_month_view(int year, int month, int day) {
	struct tm starting_tm;
	struct tm tm;
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;
	char month_label[32];

	/* Determine what day to start.
	 * First, back up to the 1st of the month...
	 */
	memset(&starting_tm, 0, sizeof(struct tm));
	starting_tm.tm_year = year - 1900;
	starting_tm.tm_mon = month - 1;
	starting_tm.tm_mday = day;
	thetime = mktime(&starting_tm);

	memcpy(&tm, &starting_tm, sizeof(struct tm));
	while (tm.tm_mday != 1) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/* Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/* Now back up until we're on a Sunday */
	localtime_r(&thetime, &tm);
	while (tm.tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/* Outer frame (to get the background color) */
	wc_printf("<div class=\"calendar_outer_frame offset_2 grid_10\">\n");
	wc_printf("<div>\n");

	localtime_r(&previous_month, &tm);
	wc_printf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wc_printf("<img alt=\"%s\" src=\"static/webcit_icons/essen/16x16/back.png\"></a>\n", _("previous"));

	wc_strftime(month_label, sizeof month_label, "%B", &tm);
	wc_printf("<span class=\"calendar_month_label\">"
		"%s %d"
		"</span>", month_label, year);

	localtime_r(&next_month, &tm);
	wc_printf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wc_printf("<img alt=\"%s\" src=\"static/webcit_icons/essen/16x16/forward.png\"></a>\n", _("next"));

	wc_printf("</div>\n");

	/* Inner table (the real one) */
	wc_printf("<table id=\"calendar_inner_table\"><tr>");
	wc_printf("</tr>\n");
	wc_printf("<tr><td>\n");

	/* Now do 35 days */
	for (i = 0; i < 35; ++i) {
		char weeknumber[255];
		char weekday_name[32];
		char *daycolor;
		localtime_r(&thetime, &tm);


		/* Before displaying Sunday, start a new CELL */
		if ((i % 7) == 0) {
			wc_strftime(&weeknumber[0], sizeof(weeknumber), "%U", &tm);
			wc_printf("<table class=\"calendar_new_cell_before_sunday\"> <tr><th colspan='4'>%s %s</th></tr>"
				"   <tr><td>%s</td><td width='70%%'>%s</td><td>%s</td><td>%s</td></tr>\n",
				_("Week"),
				weeknumber,
				_("Hours"),
				_("Subject"),
				_("Start"),
				_("End")
				);
		}

		daycolor=((tm.tm_mon != month-1) ? "DDDDDD" :
			((tm.tm_wday==0 || tm.tm_wday==6) ? "EEEECC" :
				"FFFFFF"));

		/* Day Header */
		wc_strftime(weekday_name, sizeof weekday_name, "%A", &tm);
		wc_printf("<tr><td bgcolor='%s' colspan='1' align='left'> %s,%i."
			"</td><td bgcolor='%s' colspan='3'><hr></td></tr>\n",
			daycolor,
			weekday_name,tm.tm_mday,
			daycolor);

		/* put the data of one day  here, stupid */
		calendar_month_view_brief_events(thetime, daycolor);


		/* After displaying Saturday, end the row */
		if ((i % 7) == 6) {
			wc_printf("</td></tr></table>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wc_printf("</table>"			/* end of inner table */
		"</div>\n"		/* end of outer frame */
	);
}

/*
 * Calendar week view -- not implemented yet, this is a stub function
 */
void calendar_week_view(int year, int month, int day) {
	wc_printf("<span class=\"calendar_week_view\">week view FIXME</span>\n");
}


/*
 * display one day
 * Display events for a particular hour of a particular day.
 * (Specify hour < 0 to show "all day" events)
 *
 * dstart and dend indicate which hours our "daytime" begins and end
 */
void calendar_day_view_display_events(time_t thetime,
	int year,
	int month,
	int day,
	int notime_events,
	int dstart,
	int dend)
{
	long hklen;
	const char *HashKey;
	void *vCal;
	HashPos *Pos;
	icalproperty *p = NULL;
	icalproperty *q = NULL;
	time_t event_tt;
	time_t event_tte;
	struct tm event_te;
	struct tm event_tm;
	int show_event = 0;
	int all_day_event = 0;
	int ongoing_event = 0;
	wcsession *WCC = WC;
	disp_cal *Cal;
	struct icaltimetype t;
	struct icaltimetype end_t;
	struct icaltimetype today_start_t;
	struct icaltimetype today_end_t;
	struct icaltimetype today_t;
	struct tm starting_tm;
	struct tm ending_tm;
	int top = 0;
	int bottom = 0;
	int gap = 1;
	int startmin = 0;
	int diffmin = 0;
	int endmin = 0;

        char buf[256];

	if (GetCount(WCC->disp_cal_items) == 0) {
		/* nothing to display */
		return;
	}

	/* Create an imaginary event which spans the current day.  Any events which
	 * overlap with this one take place at least partially in this day.
	 */
	memset(&starting_tm, 0, sizeof(struct tm));
	starting_tm.tm_year = year - 1900;
	starting_tm.tm_mon = month - 1;
	starting_tm.tm_mday = day;
	starting_tm.tm_hour = 0;
	starting_tm.tm_min = 0;
	today_start_t = icaltime_from_timet_with_zone(mktime(&starting_tm), 0, icaltimezone_get_utc_timezone());
	today_start_t.is_utc = 1;

	memset(&ending_tm, 0, sizeof(struct tm));
	ending_tm.tm_year = year - 1900;
	ending_tm.tm_mon = month - 1;
	ending_tm.tm_mday = day;
	ending_tm.tm_hour = 23;
	ending_tm.tm_min = 59;
	today_end_t = icaltime_from_timet_with_zone(mktime(&ending_tm), 0, icaltimezone_get_utc_timezone());
	today_end_t.is_utc = 1;

	/*
	 * Create another one without caring about the timezone for all day events.
	 */
	today_t = icaltime_null_date();
	today_t.year = year;
	today_t.month = month;
	today_t.day = day;

	/* Now loop through our list of events to see which ones occur today.
	 */
	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		Cal = (disp_cal*)vCal;

		all_day_event = 0;
		ongoing_event=0;

		q = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
		if (q != NULL) {
			t = icalproperty_get_dtstart(q);
			event_tt = icaltime_as_timet(t);
			localtime_r(&event_tt, &event_te);
		}
		else {
			memset(&t, 0, sizeof t);
		}

		if (t.is_date) all_day_event = 1;

		q = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
		if (q != NULL) {
			end_t = icalproperty_get_dtend(q);
		}
		else {
			/* no end given means end = start */
			memcpy(&end_t, &t, sizeof(struct icaltimetype));
		}

		if (all_day_event)
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_t, icaltime_null_time());
			if (icaltime_compare(t, end_t)) {
				/*
				 * the end date is non-inclusive so adjust it by one
				 * day because our test is inclusive, note that a day is
				 * not to much because we are talking about all day
				 * events
				 */
				icaltime_adjust(&end_t, -1, 0, 0, 0);
			}
		}
		else
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_start_t, today_end_t);
		}

		event_tte = icaltime_as_timet(end_t);
		localtime_r(&event_tte, &event_tm);

		/* If we determined that this event occurs today, then display it.
	 	 */
		p = icalcomponent_get_first_property(Cal->cal,ICAL_SUMMARY_PROPERTY);
		if (p == NULL) {
			p = icalproperty_new_summary(_("Untitled Event"));
			icalcomponent_add_property(Cal->cal, p);
		}

		if ((show_event) && (p != NULL)) {

			if ((event_te.tm_mday != day) || (event_tm.tm_mday != day)) ongoing_event = 1;

			if (all_day_event && notime_events)
			{
				wc_printf("<li class=\"event_framed%s\"> "
					"<a href=\"display_edit_event?"
					"msgnum=%ld?calview=day?year=%d?month=%d?day=%d\" "
					" class=\"event_title\">"
					,
					(Cal->unread)?"_unread":"_read",
                                        Cal->cal_msgnum, year, month, day
				);
                                escputs((char *) icalproperty_get_comment(p));
				wc_printf("<span class=\"tooltip\"><span class=\"btttop\"></span><span class=\"bttmiddle\">");
                                wc_printf("<span class=\"calendar_event_allday\">%s</span>", _("All day event"));
				wc_printf("<span class=\"calendar_event_from\">%s: %s</span>",  _("From"), Cal->from);
                                wc_printf("<span class=\"calendar_event_summary\">%s</span> ", _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wc_printf("<br>");
				q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_location\">%s</span> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
				}
				if (!icaltime_compare(t, end_t)) { /* one day only */
					webcit_fmt_date(buf, 256, event_tt, DATEFMT_LOCALEDATE);
					wc_printf("<span class=\"calendar_event_date\">%s</span> %s<br>", _("Date:"), buf);
				}
				else {
					webcit_fmt_date(buf, 256, event_tt, DATEFMT_LOCALEDATE);
					wc_printf("<span class=\"calendar_event_starting_date\">%s</span> %s<br>", _("Starting date:"), buf);
					webcit_fmt_date(buf, 256, event_tte, DATEFMT_LOCALEDATE);
					wc_printf("<span class=\"calendar_event_ending_date\">%s</span> %s<br>", _("Ending date:"), buf);
				}
				q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_notes\">%s</span> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
                                }
				wc_printf("</span><span class=\"bttbottom\"></span></span>");
                                wc_printf("</a> <span>(");
                                wc_printf(_("All day event"));
                                wc_printf(")</span></li>\n");
			}
			else if (ongoing_event && notime_events)
			{
				wc_printf("<li class=\"event_framed%s\"> "
					"<a href=\"display_edit_event?"
					"msgnum=%ld&calview=day?year=%d?month=%d?day=%d\" "
					" class=\"event_title\">"
					,
					(Cal->unread)?"_unread":"_read",
					Cal->cal_msgnum, year, month, day
				);
				escputs((char *) icalproperty_get_comment(p));
				wc_printf("<span class=\"tooltip\"><span class=\"btttop\"></span><span class=\"bttmiddle\">");
                                wc_printf("<span class=\"calendar_event_ongoing\">%s</span>", _("Ongoing event"));
				wc_printf("<span class=\"calendar_event_from\">%s: %s</span>", _("From"), Cal->from);
                                wc_printf("<span class=\"calendar_event_summary\">%s</span> ", _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wc_printf("<br>");
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_location\">%s</span> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
								}
                                webcit_fmt_date(buf, 256, event_tt, DATEFMT_BRIEF);
                                wc_printf("<span class=\"calendar_event_starting_date\">%s</span> %s<br>", _("Starting date/time:"), buf);
                                webcit_fmt_date(buf, 256, event_tte, DATEFMT_BRIEF);
                                wc_printf("<span class=\"calendar_event_ending_date\">%s</span> %s<br>", _("Ending date/time:"), buf);
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_notes\">%s</span> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
                                }
                                wc_printf("</span><span class=\"bttbottom\"></span></span>");
				wc_printf("</a> <span>(");
				wc_printf(_("Ongoing event"));
				wc_printf(")</span></li>\n");
			}
			else if (!all_day_event && !notime_events)
			{
				gap++;

				if (event_te.tm_mday != day) event_te.tm_hour = 0;
				if (event_tm.tm_mday != day) event_tm.tm_hour = 24;

				/* Calculate the location of the top of the box */
				if (event_te.tm_hour < dstart) {
					startmin = diffmin = event_te.tm_min / 6;
					top = (event_te.tm_hour * EXTRATIMELINE) + startmin;
				}
				else if ((event_te.tm_hour >= dstart) && (event_te.tm_hour <= dend)) {
					startmin = diffmin = (event_te.tm_min / 2);
					top = (dstart * EXTRATIMELINE) + ((event_te.tm_hour - dstart) * TIMELINE) + startmin;
				}
				else if (event_te.tm_hour >dend) {
					startmin = diffmin = event_te.tm_min / 6;
					top = (dstart * EXTRATIMELINE) + ((dend - dstart - 1) * TIMELINE) + ((event_tm.tm_hour - dend + 1) * EXTRATIMELINE) + startmin ;
				}
				else {
					/* should never get here */
				}

				/* Calculate the location of the bottom of the box */
				if (event_tm.tm_hour < dstart) {
					endmin = diffmin = event_tm.tm_min / 6;
					bottom = (event_tm.tm_hour * EXTRATIMELINE) + endmin;
				}
				else if ((event_tm.tm_hour >= dstart) && (event_tm.tm_hour <= dend)) {
					endmin = diffmin = (event_tm.tm_min / 2);
					bottom = (dstart * EXTRATIMELINE) + ((event_tm.tm_hour - dstart) * TIMELINE) + endmin ;
				}
				else if (event_tm.tm_hour >dend) {
					endmin = diffmin = event_tm.tm_min / 6;
					bottom = (dstart * EXTRATIMELINE) + ((dend - dstart + 1) * TIMELINE) + ((event_tm.tm_hour - dend - 1) * EXTRATIMELINE) + endmin;
				}
				else {
					/* should never get here */
				}

				wc_printf("<dd class=\"event_framed%s\" "
					"style=\"position: absolute; "
					"top:%dpx; left:%dpx; "
					"height:%dpx; \" >",
					(Cal->unread)?"_unread":"_read",
					top, (gap * 40), (bottom-top)
					);
				wc_printf("<a href=\"display_edit_event?"
					"msgnum=%ld?calview=day?year=%d?month=%d?day=%d?hour=%d\" "
					"class=\"event_title\">"
					,
					Cal->cal_msgnum, year, month, day, t.hour
				);
				escputs((char *) icalproperty_get_comment(p));
				wc_printf("<span class=\"tooltip\"><span class=\"btttop\"></span><span class=\"bttmiddle\">");
				wc_printf("<span class=\"calendar_event_from\">%s: %s</span>", _("From"), Cal->from);
                                wc_printf("<span class=\"calendar_event_summary\">%s</span> ", _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wc_printf("<br>");
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_location\">%s</span> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
								}
				if (!icaltime_compare(t, end_t)) { /* one day only */
					webcit_fmt_date(buf, 256, event_tt, DATEFMT_BRIEF);
					wc_printf("<span class=\"calendar_event_date\">%s</span> %s<br>", _("Date/time:"), buf);
				}
				else {
					webcit_fmt_date(buf, 256, event_tt, DATEFMT_BRIEF);
					wc_printf("<span class=\"calendar_event_starting_date\">%s</span> %s<br>", _("Starting date/time:"), buf);
					webcit_fmt_date(buf, 256, event_tte, DATEFMT_BRIEF);
					wc_printf("<span class=\"calendar_event_ending_date\">%s</span> %s<br>", _("Ending date/time:"), buf);
				}
				q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wc_printf("<span class=\"calendar_event_notes\">%s</span> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wc_printf("<br>");
                                }
				wc_printf("</span><span class=\"bttbottom\"></span></span>");
				wc_printf("</a></dd>\n");
			}
		}
	}
	DeleteHashPos(&Pos);
}

/*
 * view one day
 */
void calendar_day_view(int year, int month, int day) {
	int hour;
	struct icaltimetype today, yesterday, tomorrow;
	long daystart;
	long dayend;
	struct tm d_tm;
	char d_str[160];
	int time_format;
	time_t today_t;
	int timeline = TIMELINE;
	int extratimeline = EXTRATIMELINE;
	int gap = 0;
	int hourlabel;
	int extrahourlabel;

	time_format = get_time_format_cached ();
	get_pref_long("daystart", &daystart, 8);
	get_pref_long("dayend", &dayend, 17);

	/* when loading daystart/dayend, replace missing, corrupt, or impossible values with defaults */
	if ((daystart < 0) || (dayend < 2) || (daystart >= 23) || (dayend > 23) || (dayend <= daystart)) {
		daystart = 9;
		dayend = 17;
	}

	/* Today's date */
	memset(&d_tm, 0, sizeof d_tm);
	d_tm.tm_year = year - 1900;
	d_tm.tm_mon = month - 1;
	d_tm.tm_mday = day;
	today_t = mktime(&d_tm);

	/* Figure out the dates for "yesterday" and "tomorrow" links */

	memset(&today, 0, sizeof(struct icaltimetype));
	today.year = year;
	today.month = month;
	today.day = day;
	today.is_date = 1;

	memcpy(&yesterday, &today, sizeof(struct icaltimetype));
	--yesterday.day;
	yesterday = icaltime_normalize(yesterday);

	memcpy(&tomorrow, &today, sizeof(struct icaltimetype));
	++tomorrow.day;
	tomorrow = icaltime_normalize(tomorrow);

	/* Inner table (the real one) */
	wc_printf("<table class=\"calendar\" id=\"inner_day\"><tr> \n");

	/* Innermost cell (contains hours etc.) */
	wc_printf("<td class=\"events_of_the_day\" >");
       	wc_printf("<dl class=\"events\" >");

	/* Now the middle of the day... */

	extrahourlabel = extratimeline - 2;
	hourlabel = extrahourlabel * 150 / 100;
	if (hourlabel > (timeline - 2)) hourlabel = timeline - 2;

	for (hour = 0; hour < daystart; ++hour) {	/* could do HEIGHT=xx */
		wc_printf("<dt class=\"extrahour\">"
			"<a href=\"display_edit_event?msgnum=0"
			"?calview=day?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
/* TODO: what have these been used for?
			(hour * extratimeline ),
			extratimeline,
			extrahourlabel,
*/
			year, month, day, hour
			);

		if (time_format == WC_TIMEFORMAT_24) {
			wc_printf("%2d:00</a> ", hour);
		}
		else {
			wc_printf("%d:00%s</a> ",
				((hour == 0) ? 12 : (hour <= 12 ? hour : hour-12)),
				(hour < 12 ? "am" : "pm")
				);
		}

		wc_printf("</dt>");
	}

	gap = daystart * extratimeline;

        for (hour = daystart; hour <= dayend; ++hour) {       /* could do HEIGHT=xx */
                wc_printf("<dt class=\"hour\">"
                        "<a href=\"display_edit_event?msgnum=0?calview=day"
                        "?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
/*TODO: what have these been used for?
                        gap + ((hour - daystart) * timeline ),
			timeline,
			hourlabel,
*/
                        year, month, day, hour
			);

                if (time_format == WC_TIMEFORMAT_24) {
                        wc_printf("%2d:00</a> ", hour);
                }
                else {
                        wc_printf("%d:00%s</a> ",
                                (hour <= 12 ? hour : hour-12),
                                (hour < 12 ? "am" : "pm")
						);
                }

                wc_printf("</dt>");
        }

	gap = gap + ((dayend - daystart + 1) * timeline);

        for (hour = (dayend + 1); hour < 24; ++hour) {       /* could do HEIGHT=xx */
                wc_printf("<dt class=\"extrahour\">"
                        "<a href=\"display_edit_event?msgnum=0?calview=day"
                        "?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
/*TODO: what have these been used for?
                        gap + ((hour - dayend - 1) * extratimeline ),
			extratimeline,
			extrahourlabel,
*/
                        year, month, day, hour
                );

                if (time_format == WC_TIMEFORMAT_24) {
                        wc_printf("%2d:00</a> ", hour);
                }
                else {
                        wc_printf("%d:00%s</a> ",
                                (hour <= 12 ? hour : hour-12),
                                (hour < 12 ? "am" : "pm")
                        );
                }

                wc_printf("</dt>");
        }

	/* Display events with start and end times on this day */
	calendar_day_view_display_events(today_t, year, month, day, 0, daystart, dayend);

       	wc_printf("</dl>");
	wc_printf("</td>");			/* end of innermost table */

	/* Display extra events (start/end times not present or not today) in the middle column */
        wc_printf("<td class=\"extra_events\">");

        wc_printf("<ul>");

        /* Display all-day events */
	calendar_day_view_display_events(today_t, year, month, day, 1, daystart, dayend);

        wc_printf("</ul>");

	wc_printf("</td>");	/* end extra on the middle */

	wc_printf("<td class=\"calendar_begin_stuff_on_the_right\">");	/* begin stuff-on-the-right */

	/* Begin todays-date-with-left-and-right-arrows */
	wc_printf("<div class=\"todays_date_with_left_and_right_arrows\">");

	/* Left arrow */
	wc_printf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">",
		yesterday.year, yesterday.month, yesterday.day);
	wc_printf("<img alt=\"previous\" src=\"static/webcit_icons/essen/16x16/back.png\"></a>");

	wc_strftime(d_str, sizeof d_str,
		"<div class=\"todays_date_sheet\">"
		"<div class=\"todays_date_weekday\">%A</div>"
		"<div class=\"todays_date_month\">%B</div>"
		"<div class=\"todays_date_date\">%d</div>"
		"<div class=\"todays_date_year\">%Y</div>"
		"</div>",
		&d_tm
		);
	wc_printf("%s", d_str);

	/* Right arrow */
	wc_printf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">",
		tomorrow.year, tomorrow.month, tomorrow.day);
	wc_printf("<img alt=\"%s\" src=\"static/webcit_icons/essen/16x16/forward.png\"></a>\n", _("next"));

	wc_printf("</div>\n");
	/* End todays-date-with-left-and-right-arrows */

	/* Embed a mini month calendar in this space */
	wc_printf("<br>\n");
	embeddable_mini_calendar(year, month);

	wc_printf("</td></tr>");			/* end stuff-on-the-right */
	wc_printf("</table>\n");			/* end of inner table */
}


/*
 * Display today's events.  Returns the number of items displayed.
 */
int calendar_summary_view(void) {
	long hklen;
	const char *HashKey;
	void *vCal;
	HashPos *Pos;
	disp_cal *Cal;
	icalproperty *p;
	struct icaltimetype t;
	time_t event_tt;
	struct tm event_tm;
	struct tm today_tm;
	time_t now;
	int all_day_event = 0;
	char timestring[SIZ];
	wcsession *WCC = WC;
	int num_displayed = 0;

	if (GetCount(WC->disp_cal_items) == 0) {
		return(0);
	}

	now = time(NULL);
	localtime_r(&now, &today_tm);

	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		Cal = (disp_cal*)vCal;
		p = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			if (t.is_date) {
				all_day_event = 1;
			}
			else {
				all_day_event = 0;
			}
			fmt_time(timestring, SIZ, event_tt);

			if (all_day_event) {
				gmtime_r(&event_tt, &event_tm);
			}
			else {
				localtime_r(&event_tt, &event_tm);
			}

			if ( (event_tm.tm_year == today_tm.tm_year)
				&& (event_tm.tm_mon == today_tm.tm_mon)
				&& (event_tm.tm_mday == today_tm.tm_mday)
			) {

				p = icalcomponent_get_first_property(Cal->cal, ICAL_SUMMARY_PROPERTY);
				if (p == NULL) {
					p = icalproperty_new_summary(_("Untitled Task"));
					icalcomponent_add_property(Cal->cal, p);
				}
				if (p != NULL)
				{
					if (WCC->CurRoom.view == VIEW_TASKS) {
						wc_printf("<a href=\"display_edit_task"
							"?msgnum=%ld"
							"?return_to_summary=1"
							"?go=",
							Cal->cal_msgnum
						);
						escputs(ChrPtr(WCC->CurRoom.name));
						wc_printf("\">");
					}
					else {
						wc_printf("<a href=\"display_edit_event"
							"?msgnum=%ld"
							"?calview=summary"
							"?year=%d"
							"?month=%d"
							"?day=%d"
							"?go=",
							Cal->cal_msgnum,
							today_tm.tm_year + 1900,
							today_tm.tm_mon + 1,
							today_tm.tm_mday
						);
						escputs(ChrPtr(WCC->CurRoom.name));
						wc_printf("\">");
					}
					escputs((char *) icalproperty_get_comment(p));
					if (!all_day_event) {
						wc_printf(" (%s)", timestring);
					}
					wc_printf("</a><br>\n");
					++num_displayed;
				}
			}
		}
	}
	DeleteHashPos(&Pos);
	DeleteHash(&WC->disp_cal_items);
	return(num_displayed);
}

/*
 * Parse the URL variables in order to determine the scope and display of a calendar view
 */
int calendar_GetParamsGetServerCall(SharedMessageStatus *Stat,
				    void **ViewSpecific,
				    long oper,
				    char *cmd,
				    long len,
				    char *filter,
				    long flen)
{
	wcsession *WCC = WC;
	calview *c;
	time_t now;
	struct tm tm;
	char cv[32];

	int span = 3888000;

	c = (calview*) malloc(sizeof(calview));
	memset(c, 0, sizeof(calview));
	*ViewSpecific = (void*)c;

	Stat->load_seen = 1;
	strcpy(cmd, "MSGS ALL");
	Stat->maxmsgs = 32767;

	/* In case no date was specified, go with today */
	now = time(NULL);
	localtime_r(&now, &tm);
	c->year = tm.tm_year + 1900;
	c->month = tm.tm_mon + 1;
	c->day = tm.tm_mday;

	/* Now see if a date was specified */
	if (havebstr("year")) c->year = ibstr("year");
	if (havebstr("month")) c->month = ibstr("month");
	if (havebstr("day")) c->day = ibstr("day");

	/* How would you like that cooked? */
	if (havebstr("calview")) {
		strcpy(cv, bstr("calview"));
	}
	else {
		strcpy(cv, "month");
	}

	/* Display the selected view */
	if (!strcasecmp(cv, "day")) {
		c->view = calview_day;
	}
	else if (!strcasecmp(cv, "week")) {
		c->view = calview_week;
	}
	else if (!strcasecmp(cv, "summary")) {	/* shouldn't ever happen, but just in case */
		c->view = calview_day;
	}
	else {
		if (WCC->CurRoom.view == VIEW_CALBRIEF) {
			c->view = calview_brief;
		}
		else {
			c->view = calview_month;
		}
	}

	/* Now try and set the lower and upper bounds so that we don't
	 * burn too many cpu cycles parsing data way in the past or future
	 */

	tm.tm_year = c->year - 1900;
	tm.tm_mon = c->month - 1;
	tm.tm_mday = c->day;
	now = mktime(&tm);

	if (c->view == calview_month)	span = 3888000;
	if (c->view == calview_brief)	span = 3888000;
	if (c->view == calview_week)	span = 604800;
	if (c->view == calview_day)	span = 86400;
	if (c->view == calview_summary)	span = 86400;

	c->lower_bound = now - span;
	c->upper_bound = now + span;
	return 200;
}



/*
 * Render a calendar view from data previously loaded into memory
 */
int calendar_RenderView_or_Tail(SharedMessageStatus *Stat,
				void **ViewSpecific,
				long oper)
{
	wcsession *WCC = WC;
	calview *c = (calview*) *ViewSpecific;

	if (c->view == calview_day) {
		calendar_day_view(c->year, c->month, c->day);
	}
	else if (c->view == calview_week) {
		calendar_week_view(c->year, c->month, c->day);
	}
	else {
		if (WCC->CurRoom.view == VIEW_CALBRIEF) {
			calendar_brief_month_view(c->year, c->month, c->day);
		}
		else {
			calendar_month_view(c->year, c->month, c->day);
		}
	}

	/* Free the in-memory list of calendar items */
	DeleteHash(&WC->disp_cal_items);
	return 0;
}

void
InitModule_CALENDAR_VIEW
(void)
{
	WebcitAddUrlHandler(HKEY("mini_calendar"), "", 0, ajax_mini_calendar, AJAX);
}
