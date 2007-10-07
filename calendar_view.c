/*
 * $Id$
 */
/**
 * \defgroup CalHtmlHandles Handles the HTML display of calendar items.
 * \ingroup Calendaring
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"

#ifdef WEBCIT_WITH_CALENDAR_SERVICE

/****************************************************************************/

/**
 * \brief Display one day of a whole month view of a calendar
 * \param thetime the month we want to see 
 */
void calendar_month_view_display_events(time_t thetime) {
	int i;
	time_t event_tt;
	time_t event_tt_stripped;
	time_t event_tte;
	struct tm event_tm;
	struct tm today_tm;
	icalproperty *p = NULL;
	icalproperty *pe = NULL;
	icalproperty *q = NULL;
	struct icaltimetype t;
	int month, day, year;
	int all_day_event = 0;
	int multi_day_event = 0;
	int fill_day_event = 0;
	int show_event = 0;
	time_t tt;
	char buf[256];

	if (WC->num_cal == 0) {
		wprintf("<br /><br /><br />\n");
		return;
	}

	localtime_r(&thetime, &today_tm);
	month = today_tm.tm_mon + 1;
	day = today_tm.tm_mday;
	year = today_tm.tm_year + 1900;

	for (i=0; i<(WC->num_cal); ++i) {
		fill_day_event = 0;
		multi_day_event = 0;
		p = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						ICAL_DTSTART_PROPERTY);
		pe = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						      ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			if (t.is_date) all_day_event = 1;
			else all_day_event = 0;

			if (all_day_event) {
				gmtime_r(&event_tt, &event_tm); 
				event_tm.tm_sec = 0;
				event_tm.tm_min = 0;
				event_tm.tm_hour = 0; 
				// are we today?
				event_tt_stripped = mktime (&event_tm);
				show_event = thetime == event_tt_stripped;
			}
			else {
				localtime_r(&event_tt, &event_tm);
				// we're not interested in the hours. 
				// we just want the date for easy comparison.
				event_tm.tm_min = 0;
				event_tm.tm_hour = 0; 
				event_tt_stripped = mktime (&event_tm);
				if (pe != NULL)  // do we have a span?
				{
					struct tm event_ttm;
					time_t event_end_stripped;

					t = icalproperty_get_dtend(pe);
					event_tte = icaltime_as_timet(t);					
					gmtime_r(&event_tte, &event_ttm); 
					event_ttm.tm_sec = 0;
					event_ttm.tm_min = 0;
					event_ttm.tm_hour = 0;
					event_end_stripped = mktime(&event_ttm);

					// do we span ore than one day?
					multi_day_event = event_tt_stripped != event_end_stripped;

					// are we in the range of the event?
					show_event = ((event_tt_stripped <= thetime) && 
						      (event_end_stripped >= thetime));

					// are we not start or end day?
					fill_day_event = ((event_tt_stripped < thetime) && 
						      (event_end_stripped > thetime));
				}
				else {
					// are we today?
					show_event = event_tt_stripped == thetime;
				}
			}

			if (show_event) {
				p = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {

					if (all_day_event) {
						wprintf("<table border=0 cellpadding=2><TR>"
							"<td bgcolor=\"#CCCCDD\">"
						);
					}

					wprintf("<font size=-1>"
						"<a href=\"display_edit_event?"
						"msgnum=%ld&calview=%s&year=%s&month=%s&day=%s\""
						" btt_tooltext=\"",
						WC->disp_cal[i].cal_msgnum,
						bstr("calview"),
						bstr("year"),
						bstr("month"),
						bstr("day")
					);

					wprintf("<i>%s</i> ", _("Summary:"));
					escputs((char *)icalproperty_get_comment(p));
					wprintf("<br />");

					q = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_LOCATION_PROPERTY);
					if (q) {
						wprintf("<i>%s</i> ", _("Location:"));
						escputs((char *)icalproperty_get_comment(q));
						wprintf("<br />");
					}

					/**
					 * Only show start/end times if we're actually looking at the VEVENT
					 * component.  Otherwise it shows bogus dates for e.g. timezones
					 */
					if (icalcomponent_isa(WC->disp_cal[i].cal) == ICAL_VEVENT_COMPONENT) {
				
      						q = icalcomponent_get_first_property(WC->disp_cal[i].cal,
										ICAL_DTSTART_PROPERTY);
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
								fmt_date(buf, tt, 1);
								wprintf("<i>%s</i> %s<br>",
									_("Starting date/time:"), buf);

								/* Embed the 'show end date/time' loop inside here so it
								 * only executes if this is NOT an all day event.
								 */
      								q = icalcomponent_get_first_property(WC->disp_cal[i].cal,
													ICAL_DTEND_PROPERTY);
								if (q != NULL) {
									t = icalproperty_get_dtend(q);
									tt = icaltime_as_timet(t);
									fmt_date(buf, tt, 1);
									wprintf("<i>%s</i> %s<br>",
										_("Ending date/time:"), buf);
								}

							}
						}
					
					}

					q = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_DESCRIPTION_PROPERTY);
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
	}
}


/**
 * \brief Display one day of a whole month view of a calendar
 * \param thetime the month we want to see 
 */
void calendar_month_view_brief_events(time_t thetime, const char *daycolor) {
	int i;
	time_t event_tt;
	time_t event_tts;
	time_t event_tte;
	struct tm event_tms;
	struct tm event_tme;
	struct tm today_tm;
	icalproperty *p;
	icalproperty *e;
	struct icaltimetype t;
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

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						ICAL_DTSTART_PROPERTY);
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
			/** \todo epoch &! daymask */
			if ((event_tms.tm_year == today_tm.tm_year)
			   && (event_tms.tm_mon == today_tm.tm_mon)
			   && (event_tms.tm_mday == today_tm.tm_mday)) {
				
				
				char sbuf[255];
				char ebuf[255];

				p = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_SUMMARY_PROPERTY);
				e = icalcomponent_get_first_property(
							WC->disp_cal[i].cal, 
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
							"<font size=-1>"
							"<a href=\"display_edit_event?msgnum=%ld&calview=%s&year=%s&month=%s&day=%s\">",
							daycolor,
							hours, minutes,
							daycolor,
							WC->disp_cal[i].cal_msgnum,
							bstr("calview"),
							bstr("year"),
							bstr("month"),
							bstr("day")
							);

					escputs((char *)
							icalproperty_get_comment(p));
					/** \todo: allso ammitime format */
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
}


/**
 * \brief view one month. pretty view
 * \param year the year
 * \param month the month
 * \param day the actual day we want to see
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
	int chg_month = 0;

	/** Determine what day to start.
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

	/** Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/** Now back up until we're on a Sunday */
	localtime_r(&thetime, &tm);
	while (tm.tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/** Outer table (to get the background color) */
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"calendar\"> \n <tr><td>"); 

	wprintf("<table width=100%% border=0 cellpadding=0 cellspacing=0><tr>\n");

	wprintf("<td align=center>");

	localtime_r(&previous_month, &tm);
	wprintf("<a href=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>\n");

	wc_strftime(colheader_label, sizeof colheader_label, "%B", &starting_tm);
	wprintf("&nbsp;&nbsp;"
		"<font size=+1 color=\"#FFFFFF\">"
		"%s %d"
		"</font>"
		"&nbsp;&nbsp;", colheader_label, year);

	localtime_r(&next_month, &tm);
	wprintf("<a href=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\" border=0></A>\n");

	wprintf("</td></tr></table>\n");

	/** Inner table (the real one) */
	wprintf("<table width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#204B78 id=\"inner_month\"><tr>");
	colheader_time = thetime;
	for (i=0; i<7; ++i) {
		colheader_time = thetime + (i * 86400) ;
		localtime_r(&colheader_time, &colheader_tm);
		wc_strftime(colheader_label, sizeof colheader_label, "%A", &colheader_tm);
		wprintf("<td align=center width=14%%>"
			"<font color=\"#FFFFFF\">%s</font></th>", colheader_label);

	}
	wprintf("</tr>\n");


        /** Now do 35 or 42 days */
        for (i = 0; i < 42; ++i) {
                localtime_r(&thetime, &tm);

                if ((i < 35) || (chg_month == 0)) {

                        if ((i > 27) && ((tm.tm_mday == 1) || (tm.tm_mday == 31))) {
                                chg_month = 1;
                        }
                        if (i > 35) {
                                chg_month = 0;
                        }

			/** Before displaying Sunday, start a new row */
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
			wprintf("<a href=\"readfwd?calview=day&year=%d&month=%d&day=%d\">"
				"%d</a></div>",
				tm.tm_year + 1900,
				tm.tm_mon + 1,
				tm.tm_mday,
				tm.tm_mday);

			/** put the data here, stupid */
			calendar_month_view_display_events(thetime);

			wprintf("</td>");

			/** After displaying Saturday, end the row */
			if ((i % 7) == 6) {
				wprintf("</tr>\n");
			}

		}

		thetime += (time_t)86400;		/** ahead 24 hours */
	}

	wprintf("</table>"			/** end of inner table */
		"</td></tr></table>"		/** end of outer table */
		"</div>\n");

	/**
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
	wprintf("<script type=\"text/javascript\">"
		" setTimeout(\"btt_enableTooltips('inner_month')\", 1); "
		"</script>\n"
	);
}

/**
 * \brief view one month. brief view
 * \param year the year
 * \param month the month
 * \param day the actual day we want to see
 */
void calendar_brief_month_view(int year, int month, int day) {
	struct tm starting_tm;
	struct tm tm;
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;
	char month_label[32];

	/** Determine what day to start.
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

	/** Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/** Now back up until we're on a Sunday */
	localtime_r(&thetime, &tm);
	while (tm.tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		localtime_r(&thetime, &tm);
	}

	/** Outer table (to get the background color) */
	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#204B78><TR><TD>\n");

	wprintf("<table width=100%% border=0 cellpadding=0 cellspacing=0><tr>\n");

	wprintf("<td align=center>");

	localtime_r(&previous_month, &tm);
	wprintf("<a href=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>\n");

	wc_strftime(month_label, sizeof month_label, "%B", &tm);
	wprintf("&nbsp;&nbsp;"
		"<font size=+1 color=\"#FFFFFF\">"
		"%s %d"
		"</font>"
		"&nbsp;&nbsp;", month_label, year);

	localtime_r(&next_month, &tm);
	wprintf("<a href=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\" border=0></A>\n");

	wprintf("</td></tr></table>\n");

	/** Inner table (the real one) */
	wprintf("<table width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#EEEECC><TR>");
	wprintf("</tr>\n");
	wprintf("<tr><td colspan=\"100%\">\n");

	/** Now do 35 days */
	for (i = 0; i < 35; ++i) {
		char weeknumber[255];
		char weekday_name[32];
		char *daycolor;
		localtime_r(&thetime, &tm);


		/** Before displaying Sunday, start a new CELL */
		if ((i % 7) == 0) {
			wc_strftime(&weeknumber[0], sizeof(weeknumber), "%U", &tm);
			wprintf("<table border='0' bgcolor=\"#EEEECC\" width='100%'> <tr><th colspan='4'>%s %s</th></tr>"
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
		
		/** Day Header */
		wc_strftime(weekday_name, sizeof weekday_name, "%A", &tm);
		wprintf("<tr><td bgcolor='%s' colspan='1' align='left'> %s,%i."
				"</td><td bgcolor='%s' colspan='3'><hr></td></tr>\n",
				daycolor,
				weekday_name,tm.tm_mday,
				daycolor);

		/** put the data of one day  here, stupid */
		calendar_month_view_brief_events(thetime, daycolor);


		/** After displaying Saturday, end the row */
		if ((i % 7) == 6) {
			wprintf("</td></tr></table>\n");
		}

		thetime += (time_t)86400;		/** ahead 24 hours */
	}

	wprintf("</table>"			/** end of inner table */
		"</td></tr></table>"		/** end of outer table */
		"</div>\n");
}

/** 
 * \brief view one week
 * this should view just one week, but it's not here yet.
 * \todo ny implemented
 * \param year the year
 * \param month the month
 * \param day the day which we want to see the week around
 */
void calendar_week_view(int year, int month, int day) {
	wprintf("<center><i>week view FIXME</i></center><br />\n");
}


/**
 * \brief display one day
 * Display events for a particular hour of a particular day.
 * (Specify hour < 0 to show "all day" events)
 * \param year the year
 * \param month the month
 * \param day the day
 * \param hour the hour we want to start displaying
 * \param inner a flag to display between daystart and dayend
 * (Specify inner to 1 to show inner events)
 * (Specify inner to 0 to show "all day events and events after dayend)
 * \param dstart daystart 
 */
void calendar_day_view_display_events(int year, int month,
					int day, int hour,
					int inner, int dstart) {
	int i;
	icalproperty *p;
	icalproperty *pe = NULL;
	struct icaltimetype t;
	struct icaltimetype te;
	time_t event_tt;
	time_t event_tte;
	struct tm event_te;
	struct tm event_tm;
	int all_day_event = 0;

	if (WC->num_cal == 0) {
		// \todo FIXME wprintf("<br /><br /><br />\n");
		return;
	}

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						ICAL_DTSTART_PROPERTY);
		pe = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						      ICAL_DTEND_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			if (t.is_date) {
				all_day_event = 1;
			}
			else {
				all_day_event = 0;
			}

			if (all_day_event) {
				gmtime_r(&event_tt, &event_tm);
				gmtime_r(&event_tt, &event_te);
			}
			else {
				localtime_r(&event_tt, &event_tm);
				if (pe != NULL)
				{
					te = icalproperty_get_dtend(pe);
					event_tte = icaltime_as_timet(te);
					localtime_r(&event_tte, &event_te);
				}
				else 
					localtime_r(&event_tt, &event_te);
			}

			if (((event_tm.tm_year <= (year-1900))
			     && (event_tm.tm_mon <= (month-1))
			     && (event_tm.tm_mday <= day)
			     && (event_te.tm_year >= (year-1900))
			     && (event_te.tm_mon >= (month-1))
			     && (event_te.tm_mday >= day))
			    && (
                                 // are we in the start hour?
				    ((event_tm.tm_mday == day)
				     && (event_tm.tm_hour == hour) 
				     && (!t.is_date)) 
                                 // are we an all day event?
				    || ((hour<0)&&(t.is_date))
                                 // does it span multible days and we're not at the start day?
				    || ((hour<0)
					&& (event_tm.tm_mday < day) 
					&& (event_te.tm_mday >= day))
				    ))
			{
				p = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {

					if (all_day_event) {
                                        wprintf("<dd><a href=\"display_edit_event?msgnum=%ld&calview=day&year=%d&month=%d&day=%d&hour=%d\">",
                                                WC->disp_cal[i].cal_msgnum,
                                                year, month, day, hour
                                        );
                                        escputs((char *)
                                                icalproperty_get_comment(p));
                                        wprintf("</a></dd>\n");
					}
					else {
						if (inner) {
                                               		wprintf("<dd  class=\"event\" "
                                                       		"style=\"position: absolute; "
                                                       		"top:%dpx; left:100px; "
                                                       		"height:%dpx; \" >",
                                               			(1 + (event_te.tm_hour - hour) + (hour * 30) - (dstart * 30)),
                                               			((event_te.tm_hour - hour) * 30)
							);
						}
						else {
                                        		wprintf("<dd>");
						}
                                        	wprintf("<a href=\"display_edit_event?msgnum=%ld&calview=day&year=%d&month=%d&day=%d&hour=%d\">",
                                               		WC->disp_cal[i].cal_msgnum,
                                               		year, month, day, hour
                                        	);
                                        	escputs((char *)
                                               		icalproperty_get_comment(p));
                                        	wprintf("</a></dd>\n");
                                       	  
					}
				}
			}

		}
	}
}



/**
 * \brief view one day
 * \param year the year
 * \param month the month 
 * \param day the day we want to display
 */
void calendar_day_view(int year, int month, int day) {
	int hour;
	struct icaltimetype today, yesterday, tomorrow;
	int daystart = 8;
	int dayend = 17;
	char daystart_str[16], dayend_str[16];
	struct tm d_tm;
	char d_str[128];
	int time_format;
	
	time_format = get_time_format_cached ();
	get_preference("daystart", daystart_str, sizeof daystart_str);
	if (!IsEmptyStr(daystart_str)) daystart = atoi(daystart_str);
	get_preference("dayend", dayend_str, sizeof dayend_str);
	if (!IsEmptyStr(dayend_str)) dayend = atoi(dayend_str);
	

	/** Figure out the dates for "yesterday" and "tomorrow" links */

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

	/** Inner table (the real one) */
	wprintf("<table class=\"calendar\"><tr> \n");

	/** Innermost cell (contains hours etc.) */
	wprintf("<td class=\"middle_of_the_day\">");
       	wprintf("<dl style=\" "
		"       padding:0 ;"
		"       margin: 0;"
        	"	position: absolute ;"
        	"	top: 0;"
		"        left: 0;"
		"        width: 500px;"
		"        clip: rect(0px 500px %dpx 0px);"
		"        clip: rect(0px, 500px, %dpx, 0px);"
		"\">",
	(dayend - daystart) * 30,
	(dayend - daystart) * 30
	); 
	/** Now the middle of the day... */	
	for (hour = 0; hour <= dayend; ++hour) {	/* could do HEIGHT=xx */
		if (hour >= daystart) {
			wprintf("<dt><a href=\"display_edit_event?msgnum=0"
				"&year=%d&month=%d&day=%d&hour=%d&minute=0\">",
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

		/* put the data here, stupid */
		calendar_day_view_display_events(year, month, day, hour, 1 , daystart);

	}

       	wprintf("</dl>");
	wprintf("</td>");			/* end of innermost table */

	/** Extra events on the middle */
        wprintf("<td class=\"extra_events\">");

        wprintf("<dl>");

        /** Display all-day events) */
	wprintf("<dt>All day events</dt>");
                calendar_day_view_display_events(year, month, day, -1, 0 , daystart);

        /** Display events before daystart */
	wprintf("<dt>Before day start</dt>");
        for (hour = 0; hour <= (daystart-1); ++hour) {
                calendar_day_view_display_events(year, month, day, hour, 0, daystart );
        }

        /** Display events after dayend... */
	wprintf("<dt>After</dt>");
        for (hour = (dayend+1); hour <= 23; ++hour) {
                calendar_day_view_display_events(year, month, day, hour, 0, daystart );
        }

        wprintf("</dl>");

	wprintf("</td>");	/** end extra on the middle */

	wprintf("<td width=20%% valign=top>");	/** begin stuff-on-the-right */

	/** Begin todays-date-with-left-and-right-arrows */
	wprintf("<table border=0 width=100%% "
		"cellspacing=0 cellpadding=0 bgcolor=\"#FFFFFF\">\n");
	wprintf("<tr>");

	/** Left arrow */	
	wprintf("<td align=center>");
	wprintf("<a href=\"readfwd?calview=day&year=%d&month=%d&day=%d\">",
		yesterday.year, yesterday.month, yesterday.day);
	wprintf("<img align=middle src=\"static/prevdate_32x.gif\" border=0></A>");
	wprintf("</td>");

	/** Today's date */
	memset(&d_tm, 0, sizeof d_tm);
	d_tm.tm_year = year - 1900;
	d_tm.tm_mon = month - 1;
	d_tm.tm_mday = day;
	wc_strftime(d_str, sizeof d_str,
		"<td align=center>"
		"<font size=+2>%B</font><br />"
		"<font size=+3>%d</font><br />"
		"<font size=+2>%Y</font><br />"
		"</td>",
		&d_tm
	);
	wprintf("%s", d_str);

	/** Right arrow */
	wprintf("<td align=center>");
	wprintf("<a href=\"readfwd?calview=day&year=%d&month=%d&day=%d\">",
		tomorrow.year, tomorrow.month, tomorrow.day);
	wprintf("<img align=middle src=\"static/nextdate_32x.gif\""
		" border=0></a>\n");
	wprintf("</td>");

	wprintf("</tr></table>\n");
	/** End todays-date-with-left-and-right-arrows */

	/** \todo In the future we might want to put a month-o-matic here */

	wprintf("</font></center>\n");

	wprintf("</td></tr>");			/** end stuff-on-the-right */

	wprintf("</table>"			/** end of inner table */
		"</div>");

}
/**
 * \brief Display today's events.
 */
void calendar_summary_view(void) {
	int i;
	icalproperty *p;
	struct icaltimetype t;
	time_t event_tt;
	struct tm event_tm;
	struct tm today_tm;
	time_t now;
	int all_day_event = 0;
	char timestring[SIZ];

	if (WC->num_cal == 0) {
		return;
	}

	now = time(NULL);
	localtime_r(&now, &today_tm);

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i].cal,
						ICAL_DTSTART_PROPERTY);
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


				p = icalcomponent_get_first_property(
							WC->disp_cal[i].cal,
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {
					escputs((char *)
						icalproperty_get_comment(p));
					wprintf(" (%s)<br />\n", timestring);
				}
			}
		}
	}
	free_calendar_buffer();
}


/**
 * \brief clean up ical memory
 * \todo this could get troubel with future ical versions
 */
void free_calendar_buffer(void) {
	int i;
	if (WC->num_cal) for (i=0; i<(WC->num_cal); ++i) {
		icalcomponent_free(WC->disp_cal[i].cal);
	}
	WC->num_cal = 0;
	free(WC->disp_cal);
	WC->disp_cal = NULL;
}



/**
 * \brief do the whole calendar page
 * view any part of the calender. decide which way, etc.
 */
void do_calendar_view(void) {
	time_t now;
	struct tm tm;
	int year, month, day;
	char calview[SIZ];

	/** In case no date was specified, go with today */
	now = time(NULL);
	localtime_r(&now, &tm);
	year = tm.tm_year + 1900;
	month = tm.tm_mon + 1;
	day = tm.tm_mday;

	/** Now see if a date was specified */
	if (!IsEmptyStr(bstr("year"))) year = atoi(bstr("year"));
	if (!IsEmptyStr(bstr("month"))) month = atoi(bstr("month"));
	if (!IsEmptyStr(bstr("day"))) day = atoi(bstr("day"));

	/** How would you like that cooked? */
	if (!IsEmptyStr(bstr("calview"))) {
		strcpy(calview, bstr("calview"));
	}
	else {
		strcpy(calview, "month");
	}

	/** Display the selected view */
	if (!strcasecmp(calview, "day")) {
		calendar_day_view(year, month, day);
	}
	else if (!strcasecmp(calview, "week")) {
		calendar_week_view(year, month, day);
	}
	else {
		if (WC->wc_view == VIEW_CALBRIEF) {
			calendar_brief_month_view(year, month, day);
		}
		else {
			calendar_month_view(year, month, day);
		}
	}

	/** Free the calendar stuff */
	free_calendar_buffer();

}


/**
 * \brief get task due date
 * Helper function for do_tasks_view().  
 * \param vtodo a task to get the due date
 * \return the date/time due.
 */
time_t get_task_due_date(icalcomponent *vtodo) {
	icalproperty *p;

	if (vtodo == NULL) {
		return(0L);
	}

	/**
	 * If we're looking at a fully encapsulated VCALENDAR
	 * rather than a VTODO component, recurse into the data
	 * structure until we get a VTODO.
	 */
	if (icalcomponent_isa(vtodo) == ICAL_VCALENDAR_COMPONENT) {
		return get_task_due_date(
			icalcomponent_get_first_component(
				vtodo, ICAL_VTODO_COMPONENT
			)
		);
	}

	p = icalcomponent_get_first_property(vtodo, ICAL_DUE_PROPERTY);
	if (p != NULL) {
		return(icaltime_as_timet(icalproperty_get_due(p)));
	}
	else {
		return(0L);
	}
}


/**
 * \brief Compare the due dates of two tasks (this is for sorting)
 * \param task1 first task to compare
 * \param task2 second task to compare
 */
int task_due_cmp(const void *task1, const void *task2) {
	time_t t1;
	time_t t2;

	t1 =  get_task_due_date(((struct disp_cal *)task1)->cal);
	t2 =  get_task_due_date(((struct disp_cal *)task2)->cal);

	if (t1 < t2) return(-1);
	if (t1 > t2) return(1);
	return(0);
}




/**
 * \brief do the whole task view stuff
 */
void do_tasks_view(void) {
	int i;
	time_t due;
	int bg = 0;
	char buf[SIZ];
	icalproperty *p;

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"calendar_view_background\">\n<tr>\n"
		"<th>");
	wprintf(_("Name of task"));
	wprintf("</th><th>");
	wprintf(_("Date due"));
	wprintf("</th></tr>\n"
	);

	/** Sort them if necessary */
	if (WC->num_cal > 1) {
		qsort(WC->disp_cal,
			WC->num_cal,
			sizeof(struct disp_cal),
			task_due_cmp
		);
	}

	if (WC->num_cal) for (i=0; i<(WC->num_cal); ++i) {

		bg = 1 - bg;
		wprintf("<tr bgcolor=\"#%s\"><td>",
			(bg ? "DDDDDD" : "FFFFFF")
		);

		p = icalcomponent_get_first_property(WC->disp_cal[i].cal,
							ICAL_SUMMARY_PROPERTY);
		wprintf("<a href=\"display_edit_task?msgnum=%ld&taskrm=",
			WC->disp_cal[i].cal_msgnum );
		urlescputs(WC->wc_roomname);
		wprintf("\">");
		wprintf("<img align=middle "
			"src=\"static/taskmanag_16x.gif\" border=0>&nbsp;");
		if (p != NULL) {
			escputs((char *)icalproperty_get_comment(p));
		}
		wprintf("</a>\n");
		wprintf("</td>\n");

		due = get_task_due_date(WC->disp_cal[i].cal);
		fmt_date(buf, due, 0);
		wprintf("<td><font");
		if (due < time(NULL)) {
			wprintf(" color=\"#FF0000\"");
		}
		wprintf(">%s</font></td></tr>\n", buf);
	}

	wprintf("</table></div>\n");

	/** Free the list */
	free_calendar_buffer();

}

#else	/* WEBCIT_WITH_CALENDAR_SERVICE */

/**\brief stub for non-libical builds */
void do_calendar_view(void) {
	wprintf("<center><i>");
	wprintf(_("The calendar view is not available."));
	wprintf("</i></center><br />\n");
}

/**\brief stub for non-libical builds */
void do_tasks_view(void) {	
	wprintf("<center><I>");
	wprintf(_("The tasks view is not available."));
	wprintf("</i></center><br />\n");
}


#endif	/* WEBCIT_WITH_CALENDAR_SERVICE */

/** @} */
