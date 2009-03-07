/*
 * $Id$
 *
 * Handles the HTML display of calendar items.
 */

#include "webcit.h"
#include "webserver.h"

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

	wprintf("<div class=\"mini_calendar\" id=\"%s\">\n", div_id);

	/* Previous month link */
	localtime_r(&previous_month, &tm);
	wprintf("<a href=\"javascript:minical_change_month(%d,%d);\">&laquo;</a>", 
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);

	wc_strftime(colheader_label, sizeof colheader_label, "%B", &starting_tm);
	wprintf("&nbsp;&nbsp;"
		"<span class=\"mini_calendar_month_label\">"
		"%s %d"
		"</span>"
		"&nbsp;&nbsp;", colheader_label, year);

	/* Next month link */
	localtime_r(&next_month, &tm);
	wprintf("<a href=\"javascript:minical_change_month(%d,%d);\">&raquo;</a>",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);

	wprintf("<table border=0 cellpadding=1 cellspacing=1 class=\"mini_calendar_days\">"
		"<tr>");
	colheader_time = thetime;
	for (i=0; i<7; ++i) {
		colheader_time = thetime + (i * 86400) ;
		localtime_r(&colheader_time, &colheader_tm);
		wc_strftime(colheader_label, sizeof colheader_label, "%A", &colheader_tm);
		wprintf("<th>%c</th>", colheader_label[0]);

	}
	wprintf("</tr>\n");


        /* Now do 35 or 42 days */
        for (i = 0; i < 42; ++i) {
                localtime_r(&thetime, &tm);

                if (i < 35) {

			/* Before displaying Sunday, start a new row */
			if ((i % 7) == 0) {
				wprintf("<tr>");
			}

			if (tm.tm_mon == month-1) {
				snprintf(url, sizeof url, "readfwd?calview=day&year=%d&month=%d&day=%d", 
					tm.tm_year+1900, tm.tm_mon+1, tm.tm_mday);
				wprintf("<td><a href=\"%s\">%d</a></td>", url, tm.tm_mday);
			}
			else {
				wprintf("<td> </td>");
			}

			/* After displaying one week, end the row */
			if ((i % 7) == 6) {
				wprintf("</tr>\n");
			}

		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wprintf("</table>"			/* end of inner table */
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
	struct tm starting_tm;
	struct tm ending_tm;
	int all_day_event = 0;
	int show_event = 0;
	char buf[256];
	wcsession *WCC = WC;
	time_t tt;

	if (GetCount(WCC->disp_cal_items) == 0) {
		wprintf("<br /><br /><br />\n");
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
			show_event = ((t.year == year) && (t.month == month) && (t.day == day));
		}
		else
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_start_t, today_end_t);
		}

		/*
		 * If we determined that this event occurs today, then display it.
	 	 */
		if (show_event) {
			p = icalcomponent_get_first_property(Cal->cal, ICAL_SUMMARY_PROPERTY);
			if (p != NULL) {

				if (all_day_event) {
					wprintf("<table border=0 cellpadding=2><TR>"
						"<td bgcolor=\"#CCCCDD\">"
						);
				}

				wprintf("<font size=\"-1\">"
					"<a class=\"event%s\" href=\"display_edit_event?"
					"msgnum=%ld?calview=month?year=%d?month=%d?day=%d\""
					" btt_tooltext=\"",
					(Cal->unread)?"_unread":"_read",
					Cal->cal_msgnum,
					year, month, day
					);

				wprintf("<i>%s: %s</i><br />", _("From"), Cal->from);
				wprintf("<i>%s</i> ",          _("Summary:"));
				escputs((char *)icalproperty_get_comment(p));
				wprintf("<br />");
				
				q = icalcomponent_get_first_property(
					Cal->cal,
					ICAL_LOCATION_PROPERTY);
				if (q) {
					wprintf("<i>%s</i> ", _("Location:"));
					escputs((char *)icalproperty_get_comment(q));
					wprintf("<br />");
				}
				
				/*
				 * Only show start/end times if we're actually looking at the VEVENT
				 * component.  Otherwise it shows bogus dates for e.g. timezones
				 */
				if (icalcomponent_isa(Cal->cal) == ICAL_VEVENT_COMPONENT) {
					
					q = icalcomponent_get_first_property(Cal->cal, ICAL_DTSTART_PROPERTY);
					if (q != NULL) {
						t = icalproperty_get_dtstart(q);
						
						if (t.is_date) {
							struct tm d_tm;
							char d_str[32];
							memset(&d_tm, 0, sizeof d_tm);
							d_tm.tm_year = t.year - 1900;
							d_tm.tm_mon = t.month - 1;
							d_tm.tm_mday = t.day;
							wc_strftime(d_str, sizeof d_str, "%x", &d_tm);
							wprintf("<i>%s</i> %s<br>",
								_("Date:"), d_str);
						}
						else {
							tt = icaltime_as_timet(t);
							webcit_fmt_date(buf, tt, DATEFMT_BRIEF);
							wprintf("<i>%s</i> %s<br>",
								_("Starting date/time:"), buf);
							
							/*
							 * Embed the 'show end date/time' loop inside here so it
							 * only executes if this is NOT an all day event.
							 */
							q = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
							if (q != NULL) {
								t = icalproperty_get_dtend(q);
								tt = icaltime_as_timet(t);
								webcit_fmt_date(buf, tt, DATEFMT_BRIEF);
								wprintf("<i>%s</i> %s<br>", _("Ending date/time:"), buf);
							}
							
						}
					}
					
				}
				
				q = icalcomponent_get_first_property(Cal->cal, ICAL_DESCRIPTION_PROPERTY);
				if (q) {
					wprintf("<i>%s</i> ", _("Notes:"));
					escputs((char *)icalproperty_get_comment(q));
					wprintf("<br />");
				}
				
				wprintf("\">");
				escputs((char *)
					icalproperty_get_comment(p));
				wprintf("</a></font><br />\n");
				
				if (all_day_event) {
					wprintf("</td></tr></table>");
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
	int month, day, year;
	int all_day_event = 0;
	char *timeformat;
	int time_format;
	
	time_format = get_time_format_cached ();

	if (time_format == WC_TIMEFORMAT_24) timeformat="%k:%M";
	else timeformat="%I:%M %p";

	localtime_r(&thetime, &today_tm);
	month = today_tm.tm_mon + 1;
	day = today_tm.tm_mday;
	year = today_tm.tm_year + 1900;

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
				wprintf("<tr><td bgcolor='%s'>%i:%2i</td><td bgcolor='%s'>"
					"<font size=\"-1\">"
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
				
				wprintf("</a></font></td>"
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
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;
	time_t colheader_time;
	struct tm colheader_tm;
	char colheader_label[32];
	long weekstart = 0;

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

	/* Outer table (to get the background color) */
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"calendar\"> \n <tr><td>"); 

	wprintf("<table width=100%% border=0 cellpadding=0 cellspacing=0><tr>\n");

	wprintf("<td align=center>");

	localtime_r(&previous_month, &tm);
	wprintf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>\n");

	wc_strftime(colheader_label, sizeof colheader_label, "%B", &starting_tm);
	wprintf("&nbsp;&nbsp;"
		"<font size=+1 color=\"#FFFFFF\">"
		"%s %d"
		"</font>"
		"&nbsp;&nbsp;", colheader_label, year);

	localtime_r(&next_month, &tm);
	wprintf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\" border=0></A>\n");

	wprintf("</td></tr></table>\n");

	/* Inner table (the real one) */
	wprintf("<table width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#204B78 id=\"inner_month\"><tr>");
	colheader_time = thetime;
	for (i=0; i<7; ++i) {
		colheader_time = thetime + (i * 86400) ;
		localtime_r(&colheader_time, &colheader_tm);
		wc_strftime(colheader_label, sizeof colheader_label, "%A", &colheader_tm);
		wprintf("<th align=center width=14%%>"
			"<font color=\"#FFFFFF\">%s</font></th>", colheader_label);

	}
	wprintf("</tr>\n");


        /* Now do 35 or 42 days */
	localtime_r(&thetime, &tm);
        for (i = 0; i<42; ++i) {

		/* Before displaying the first day of the week, start a new row */
		if ((i % 7) == 0) {
			wprintf("<tr>");
		}

		wprintf("<td class=\"cal%s\"><div class=\"day\">",
			((tm.tm_mon != month-1) ? "out" :
				((tm.tm_wday==0 || tm.tm_wday==6) ? "weekend" :
					"day"))
			);
		if ((i==0) || (tm.tm_mday == 1)) {
			wc_strftime(colheader_label, sizeof colheader_label, "%B", &tm);
			wprintf("%s ", colheader_label);
		}
		wprintf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">"
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

		wprintf("</td>");

		/* After displaying the last day of the week, end the row */
		if ((i % 7) == 6) {
			wprintf("</tr>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
		localtime_r(&thetime, &tm);

		if ( ((i % 7) == 6) && (tm.tm_mon != month-1) && (tm.tm_mday < 15) ) {
			i = 100;	/* break out of the loop */
		}
	}

	wprintf("</table>"			/* end of inner table */
		"</td></tr></table>"		/* end of outer table */
		"</div>\n");

	/*
	 * Initialize the bubble tooltips.
	 *
	 * Yes, this is as stupid as it looks.  Instead of just making the call
	 * to btt_enableTooltips() straight away, we have to create a timer event
	 * and let it initialize as an event after 1 millisecond.  This is to
	 * work around a bug in Internet Explorer that causes it to crash if we
	 * manipulate the innerHTML of various DOM nodes while the page is still
	 * being rendered.  See http://www.shaftek.org/blog/archives/000212.html
	 * for more information.
	 */ 
	StrBufAppendPrintf(WC->trailing_javascript,
		" setTimeout(\"btt_enableTooltips('inner_month')\", 1);	\n"
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

	/* Outer table (to get the background color) */
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#204B78><TR><TD>\n");

	wprintf("<table width=100%% border=0 cellpadding=0 cellspacing=0><tr>\n");

	wprintf("<td align=center>");

	localtime_r(&previous_month, &tm);
	wprintf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>\n");

	wc_strftime(month_label, sizeof month_label, "%B", &tm);
	wprintf("&nbsp;&nbsp;"
		"<font size=+1 color=\"#FFFFFF\">"
		"%s %d"
		"</font>"
		"&nbsp;&nbsp;", month_label, year);

	localtime_r(&next_month, &tm);
	wprintf("<a href=\"readfwd?calview=month?year=%d?month=%d?day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\" border=0></A>\n");

	wprintf("</td></tr></table>\n");

	/* Inner table (the real one) */
	wprintf("<table width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#EEEECC><TR>");
	wprintf("</tr>\n");
	wprintf("<tr><td colspan=\"100%%\">\n");

	/* Now do 35 days */
	for (i = 0; i < 35; ++i) {
		char weeknumber[255];
		char weekday_name[32];
		char *daycolor;
		localtime_r(&thetime, &tm);


		/* Before displaying Sunday, start a new CELL */
		if ((i % 7) == 0) {
			wc_strftime(&weeknumber[0], sizeof(weeknumber), "%U", &tm);
			wprintf("<table border='0' bgcolor=\"#EEEECC\" width='100%%'> <tr><th colspan='4'>%s %s</th></tr>"
				"   <tr><td>%s</td><td width=70%%>%s</td><td>%s</td><td>%s</td></tr>\n",
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
		wprintf("<tr><td bgcolor='%s' colspan='1' align='left'> %s,%i."
			"</td><td bgcolor='%s' colspan='3'><hr></td></tr>\n",
			daycolor,
			weekday_name,tm.tm_mday,
			daycolor);

		/* put the data of one day  here, stupid */
		calendar_month_view_brief_events(thetime, daycolor);


		/* After displaying Saturday, end the row */
		if ((i % 7) == 6) {
			wprintf("</td></tr></table>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wprintf("</table>"			/* end of inner table */
		"</td></tr></table>"		/* end of outer table */
		"</div>\n");
}

/*
 * Calendar week view -- not implemented yet, this is a stub function
 */
void calendar_week_view(int year, int month, int day) {
	wprintf("<center><i>week view FIXME</i></center><br />\n");
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
	struct tm starting_tm;
	struct tm ending_tm;
	int top = 0;
	int bottom = 0;
	int gap = 1;
	int startmin = 0;
	int diffmin = 0;
	int endmin = 0;

        char buf[256];
        struct tm d_tm;
        char d_str[32];

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
		q = icalcomponent_get_first_property(Cal->cal, ICAL_DTEND_PROPERTY);
		if (q != NULL) {
			end_t = icalproperty_get_dtend(q);
			event_tte = icaltime_as_timet(end_t);
			localtime_r(&event_tte, &event_tm);
		}
		else {
			memset(&end_t, 0, sizeof end_t);
		}
		if (t.is_date) all_day_event = 1;

		if (all_day_event)
		{
			show_event = ((t.year == year) && (t.month == month) && (t.day == day) && (notime_events));
		}
		else
		{
			show_event = ical_ctdl_is_overlap(t, end_t, today_start_t, today_end_t);
		}

		/* If we determined that this event occurs today, then display it.
	 	 */
		p = icalcomponent_get_first_property(Cal->cal,ICAL_SUMMARY_PROPERTY);

		if ((show_event) && (p != NULL)) {

			if ((event_te.tm_mday != day) || (event_tm.tm_mday != day)) ongoing_event = 1; 

			if (all_day_event && notime_events)
			{
				wprintf("<li class=\"event_framed%s\"> "
					"<a href=\"display_edit_event?"
					"msgnum=%ld?calview=day?year=%d?month=%d?day=%d\" "
					" class=\"event_title\" "
					" btt_tooltext=\"",
					(Cal->unread)?"_unread":"_read",
                                        Cal->cal_msgnum, year, month, day);
                                wprintf("<i>%s</i><br />",      _("All day event"));
				wprintf("<i>%s: %s</i><br />",  _("From"), Cal->from);
                                wprintf("<i>%s</i> ",           _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wprintf("<br />");
				q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
								}
                                memset(&d_tm, 0, sizeof d_tm);
                                d_tm.tm_year = t.year - 1900;
                                d_tm.tm_mon = t.month - 1;
                                d_tm.tm_mday = t.day;
                                wc_strftime(d_str, sizeof d_str, "%x", &d_tm);
                                wprintf("<i>%s</i> %s<br>",_("Date:"), d_str);
				q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
                                }
                                wprintf("\">");
                                escputs((char *) icalproperty_get_comment(p));
                                wprintf("</a> <span>(");
                                wprintf(_("All day event"));
                                wprintf(")</span></li>\n");
			}
			else if (ongoing_event && notime_events) 
			{
				wprintf("<li class=\"event_framed%s\"> "
					"<a href=\"display_edit_event?"
					"msgnum=%ld&calview=day?year=%d?month=%d?day=%d\" "
					" class=\"event_title\" " 
					"btt_tooltext=\"",
					(Cal->unread)?"_unread":"_read",
					Cal->cal_msgnum, year, month, day);
                                wprintf("<i>%s</i><br />",     _("Ongoing event"));
				wprintf("<i>%s: %s</i><br />", _("From"), Cal->from);
                                wprintf("<i>%s</i> ",          _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wprintf("<br />");
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
								}
                                webcit_fmt_date(buf, event_tt, DATEFMT_BRIEF);
                                wprintf("<i>%s</i> %s<br>", _("Starting date/time:"), buf);
                                webcit_fmt_date(buf, event_tte, DATEFMT_BRIEF);
                                wprintf("<i>%s</i> %s<br>", _("Ending date/time:"), buf);
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
                                }
                                wprintf("\">");
				escputs((char *) icalproperty_get_comment(p));
				wprintf("</a> <span>(");
				wprintf(_("Ongoing event"));
				wprintf(")</span></li>\n");
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

				wprintf("<dd  class=\"event_framed%s\" "
					"style=\"position: absolute; "
					"top:%dpx; left:%dpx; "
					"height:%dpx; \" >",
					(Cal->unread)?"_unread":"_read",
					top, (gap * 40), (bottom-top)
					);
				wprintf("<a href=\"display_edit_event?"
					"msgnum=%ld?calview=day?year=%d?month=%d?day=%d?hour=%d\" "
					"class=\"event_title\" "
                               		"btt_tooltext=\"",
					Cal->cal_msgnum, year, month, day, t.hour);
				wprintf("<i>%s: %s</i><br />", _("From"), Cal->from);
                                wprintf("<i>%s</i> ",          _("Summary:"));
                                escputs((char *) icalproperty_get_comment(p));
                                wprintf("<br />");
                                q = icalcomponent_get_first_property(Cal->cal,ICAL_LOCATION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Location:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
								}
                                webcit_fmt_date(buf, event_tt, DATEFMT_BRIEF);
                                wprintf("<i>%s</i> %s<br>", _("Starting date/time:"), buf);
                                webcit_fmt_date(buf, event_tte, DATEFMT_BRIEF);
                                wprintf("<i>%s</i> %s<br>", _("Ending date/time:"), buf);
				q = icalcomponent_get_first_property(Cal->cal,ICAL_DESCRIPTION_PROPERTY);
                                if (q) {
                                        wprintf("<i>%s</i> ", _("Notes:"));
                                        escputs((char *)icalproperty_get_comment(q));
                                        wprintf("<br />");
                                }
                                wprintf("\">");

				escputs((char *) icalproperty_get_comment(p));
				wprintf("</a></dd>\n");
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
	char d_str[128];
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

	wprintf("<div class=\"fix_scrollbar_bug\">");

	/* Inner table (the real one) */
	wprintf("<table class=\"calendar\" id=\"inner_day\"><tr> \n");

	/* Innermost cell (contains hours etc.) */
	wprintf("<td class=\"events_of_the_day\" >");
       	wprintf("<dl class=\"events\" >");

	/* Now the middle of the day... */

	extrahourlabel = extratimeline - 2;
	hourlabel = extrahourlabel * 150 / 100;
	if (hourlabel > (timeline - 2)) hourlabel = timeline - 2;

	for (hour = 0; hour < daystart; ++hour) {	/* could do HEIGHT=xx */
		wprintf("<dt class=\"extrahour\" 	"
			"style=\"		"
			"position: absolute; 	"
			"top: %dpx; left: 0px; 	"
			"height: %dpx;		"
			"font-size: %dpx;	"
			"\" >			"
			"<a href=\"display_edit_event?msgnum=0"
			"?calview=day?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
			(hour * extratimeline ),
			extratimeline,
			extrahourlabel,
			year, month, day, hour
			);

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("%2d:00</a> ", hour);
		}
		else {
			wprintf("%d:00%s</a> ",
				((hour == 0) ? 12 : (hour <= 12 ? hour : hour-12)),
				(hour < 12 ? "am" : "pm")
				);
		}

		wprintf("</dt>");
	}

	gap = daystart * extratimeline;

        for (hour = daystart; hour <= dayend; ++hour) {       /* could do HEIGHT=xx */
                wprintf("<dt class=\"hour\"     "
                        "style=\"               "
                        "position: absolute;    "
                        "top: %ldpx; left: 0px; "
                        "height: %dpx;          "
			"font-size: %dpx;	"
                        "\" >                   "
                        "<a href=\"display_edit_event?msgnum=0?calview=day"
                        "?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
                        gap + ((hour - daystart) * timeline ),
			timeline,
			hourlabel,
                        year, month, day, hour
			);

                if (time_format == WC_TIMEFORMAT_24) {
                        wprintf("%2d:00</a> ", hour);
                }
                else {
                        wprintf("%d:00%s</a> ",
                                (hour <= 12 ? hour : hour-12),
                                (hour < 12 ? "am" : "pm")
						);
                }

                wprintf("</dt>");
        }

	gap = gap + ((dayend - daystart + 1) * timeline);

        for (hour = (dayend + 1); hour < 24; ++hour) {       /* could do HEIGHT=xx */
                wprintf("<dt class=\"extrahour\"     "
                        "style=\"               "
                        "position: absolute;    "
                        "top: %ldpx; left: 0px; "
                        "height: %dpx;          "
			"font-size: %dpx;	"
                        "\" >                   "
                        "<a href=\"display_edit_event?msgnum=0?calview=day"
                        "?year=%d?month=%d?day=%d?hour=%d?minute=0\">",
                        gap + ((hour - dayend - 1) * extratimeline ),
			extratimeline,
			extrahourlabel,
                        year, month, day, hour
                );

                if (time_format == WC_TIMEFORMAT_24) {
                        wprintf("%2d:00</a> ", hour);
                }
                else {
                        wprintf("%d:00%s</a> ",
                                (hour <= 12 ? hour : hour-12),
                                (hour < 12 ? "am" : "pm")
                        );
                }

                wprintf("</dt>");
        }

	/* Display events with start and end times on this day */
	calendar_day_view_display_events(today_t, year, month, day, 0, daystart, dayend);

       	wprintf("</dl>");
	wprintf("</td>");			/* end of innermost table */

	/* Display extra events (start/end times not present or not today) in the middle column */
        wprintf("<td class=\"extra_events\">");

        wprintf("<ul>");

        /* Display all-day events */
	calendar_day_view_display_events(today_t, year, month, day, 1, daystart, dayend);

        wprintf("</ul>");

	wprintf("</td>");	/* end extra on the middle */

	wprintf("<td width=20%% align=center valign=top>");	/* begin stuff-on-the-right */

	/* Begin todays-date-with-left-and-right-arrows */
	wprintf("<table border=0 width=100%% "
		"cellspacing=0 cellpadding=0 bgcolor=\"#FFFFFF\">\n");
	wprintf("<tr>");

	/* Left arrow */	
	wprintf("<td align=center>");
	wprintf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">",
		yesterday.year, yesterday.month, yesterday.day);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>");
	wprintf("</td>");

	wc_strftime(d_str, sizeof d_str,
		"<td align=center>"
		"<font size=+2>%B</font><br />"
		"<font size=+3>%d</font><br />"
		"<font size=+2>%Y</font><br />"
		"</td>",
		&d_tm
		);
	wprintf("%s", d_str);

	/* Right arrow */
	wprintf("<td align=center>");
	wprintf("<a href=\"readfwd?calview=day?year=%d?month=%d?day=%d\">",
		tomorrow.year, tomorrow.month, tomorrow.day);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\""
		" border=0></a>\n");
	wprintf("</td>");

	wprintf("</tr></table>\n");
	/* End todays-date-with-left-and-right-arrows */

	/* Embed a mini month calendar in this space */
	wprintf("<br />\n");
	embeddable_mini_calendar(year, month);

	wprintf("</font></center>\n");

	wprintf("</td></tr>");			/* end stuff-on-the-right */

	wprintf("</table>"			/* end of inner table */
		"</div>");

	StrBufAppendPrintf(WC->trailing_javascript,
                " setTimeout(\"btt_enableTooltips('inner_day')\", 1);	\n"
        );
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
			fmt_time(timestring, event_tt);

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
				if (p != NULL) {


					if (WCC->wc_view == VIEW_TASKS) {
						wprintf("<a href=\"display_edit_task"
							"?msgnum=%ld"
							"?return_to_summary=1"
							"?gotofirst=",
							Cal->cal_msgnum
						);
						escputs(ChrPtr(WCC->wc_roomname));
						wprintf("\">");
					}
					else {
						wprintf("<a href=\"display_edit_event"
							"?msgnum=%ld"
							"?calview=summary"
							"?year=%d"
							"?month=%d"
							"?day=%d"
							"?gotofirst=",
							Cal->cal_msgnum,
							today_tm.tm_year + 1900,
							today_tm.tm_mon + 1,
							today_tm.tm_mday
						);
						escputs(ChrPtr(WCC->wc_roomname));
						wprintf("\">");
					}
					escputs((char *) icalproperty_get_comment(p));
					if (!all_day_event) {
						wprintf(" (%s)", timestring);
					}
					wprintf("</a><br />\n");
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
void parse_calendar_view_request(struct calview *c) {
	time_t now;
	struct tm tm;
	char calview[32];
	int span = 3888000;

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
		strcpy(calview, bstr("calview"));
	}
	else {
		strcpy(calview, "month");
	}

	/* Display the selected view */
	if (!strcasecmp(calview, "day")) {
		c->view = calview_day;
	}
	else if (!strcasecmp(calview, "week")) {
		c->view = calview_week;
	}
	else if (!strcasecmp(calview, "summary")) {	/* shouldn't ever happen, but just in case */
		c->view = calview_day;
	}
	else {
		if (WC->wc_view == VIEW_CALBRIEF) {
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
}



/*
 * Render a calendar view from data previously loaded into memory
 */
void render_calendar_view(struct calview *c)
{
	if (c->view == calview_day) {
		calendar_day_view(c->year, c->month, c->day);
	}
	else if (c->view == calview_week) {
		calendar_week_view(c->year, c->month, c->day);
	}
	else {
		if (WC->wc_view == VIEW_CALBRIEF) {
			calendar_brief_month_view(c->year, c->month, c->day);
		}
		else {
			calendar_month_view(c->year, c->month, c->day);
		}
	}

	/* Free the in-memory list of calendar items */
	DeleteHash(&WC->disp_cal_items);
}


/*
 * Helper function for do_tasks_view().  Returns the due date/time of a vtodo.
 */
time_t get_task_due_date(icalcomponent *vtodo, int *is_date) {
	icalproperty *p;

	if (vtodo == NULL) {
		return(0L);
	}

	/*
	 * If we're looking at a fully encapsulated VCALENDAR
	 * rather than a VTODO component, recurse into the data
	 * structure until we get a VTODO.
	 */
	if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
		return get_task_due_date(
			icalcomponent_get_first_component(
				vtodo, ICAL_VTODO_COMPONENT
				), is_date
			);
	}

	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	if (p != NULL) {
		struct icaltimetype t = icalproperty_get_due(p);

		if (is_date)
			*is_date = t.is_date;
		return(icaltime_as_timet(t));
	}
	else {
		return(0L);
	}
}


/*
 * Compare the due dates of two tasks (this is for sorting)
 */
int task_due_cmp(const void *vtask1, const void *vtask2) {
	disp_cal * Task1 = (disp_cal *)GetSearchPayload(vtask1);
	disp_cal * Task2 = (disp_cal *)GetSearchPayload(vtask2);

	time_t t1;
	time_t t2;

	t1 =  get_task_due_date(Task1->cal, NULL);
	t2 =  get_task_due_date(Task2->cal, NULL);
	if (t1 < t2) return(-1);
	if (t1 > t2) return(1);
	return(0);
}

/*
 * qsort filter to move completed tasks to bottom of task list
 */
int task_completed_cmp(const void *vtask1, const void *vtask2) {
	disp_cal * Task1 = (disp_cal *)GetSearchPayload(vtask1);
/*	disp_cal * Task2 = (disp_cal *)GetSearchPayload(vtask2); */

	icalproperty_status t1 = icalcomponent_get_status((Task1)->cal);
	/* icalproperty_status t2 = icalcomponent_get_status(((struct disp_cal *)task2)->cal); */
	
	if (t1 == ICAL_STATUS_COMPLETED) 
		return 1;
	return 0;
}



/*
 * do the whole task view stuff
 */
void do_tasks_view(void) {
	long hklen;
	const char *HashKey;
	void *vCal;
	disp_cal *Cal;
	HashPos *Pos;
	int nItems;
	time_t due;
	char buf[SIZ];
	icalproperty *p;
	wcsession *WCC = WC;

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"calendar_view_background\"><tbody id=\"taskview\">\n<tr>\n"
		"<th>");
	wprintf(_("Completed?"));
	wprintf("</th><th>");
	wprintf(_("Name of task"));
	wprintf("</th><th>");
	wprintf(_("Date due"));
	wprintf("</th><th>");
	wprintf(_("Category"));
	wprintf(" (<select id=\"selectcategory\"><option value=\"showall\">%s</option></select>)</th></tr>\n",
		_("Show All"));

	nItems = GetCount(WC->disp_cal_items);

	/* Sort them if necessary
	if (nItems > 1) {
		SortByPayload(WC->disp_cal_items, task_due_cmp);
	}
	* this shouldn't be neccessary, since we sort by the start time.
	*/

	/* And then again, by completed */
	if (nItems > 1) {
		SortByPayload(WC->disp_cal_items, 
			      task_completed_cmp);
	}

	Pos = GetNewHashPos(WCC->disp_cal_items, 0);
	while (GetNextHashPos(WCC->disp_cal_items, Pos, &hklen, &HashKey, &vCal)) {
		icalproperty_status todoStatus;
		int is_date;

		Cal = (disp_cal*)vCal;
		wprintf("<tr><td>");
		todoStatus = icalcomponent_get_status(Cal->cal);
		wprintf("<input type=\"checkbox\" name=\"completed\" value=\"completed\" ");
		if (todoStatus == ICAL_STATUS_COMPLETED) {
			wprintf("checked=\"checked\" ");
		}
		wprintf("disabled=\"disabled\">\n</td><td>");
		p = icalcomponent_get_first_property(Cal->cal,
			ICAL_SUMMARY_PROPERTY);
		wprintf("<a href=\"display_edit_task?msgnum=%ld?taskrm=", Cal->cal_msgnum);
		urlescputs(ChrPtr(WC->wc_roomname));
		wprintf("\">");
		/* wprintf("<img align=middle "
		"src=\"static/taskmanag_16x.gif\" border=0>&nbsp;"); */
		if (p != NULL) {
			escputs((char *)icalproperty_get_comment(p));
		}
		wprintf("</a>\n");
		wprintf("</td>\n");

		due = get_task_due_date(Cal->cal, &is_date);
		wprintf("<td><span");
		if (due > 0) {
			webcit_fmt_date(buf, due, is_date ? DATEFMT_RAWDATE : DATEFMT_FULL);
			wprintf(">%s",buf);
		}
		else {
			wprintf(">");
		}
		wprintf("</span></td>");
		wprintf("<td>");
		p = icalcomponent_get_first_property(Cal->cal,
			ICAL_CATEGORIES_PROPERTY);
		if (p != NULL) {
			escputs((char *)icalproperty_get_categories(p));
		}
		wprintf("</td>");
		wprintf("</tr>");
	}

	wprintf("</tbody></table></div>\n");

	/* Free the list */
	DeleteHash(&WC->disp_cal_items);
	DeleteHashPos(&Pos);
}

