/*
 * $Id$
 *
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <time.h>
#include "webcit.h"
#include "webserver.h"

#ifndef WEBCIT_WITH_CALENDAR_SERVICE

void do_calendar_view(void) {	/* stub for non-libical builds */
	wprintf("<CENTER><I>Calendar view not available</I></CENTER><BR>\n");
}

void do_tasks_view(void) {	/* stub for non-libical builds */
	wprintf("<CENTER><I>Tasks view not available</I></CENTER><BR>\n");
}

#else	/* WEBCIT_WITH_CALENDAR_SERVICE */

/****************************************************************************/


void calendar_month_view_display_events(time_t thetime) {
	int i;
	time_t event_tt;
	struct tm event_tm;
	struct tm today_tm;
	icalproperty *p;
	struct icaltimetype t;
	int month, day, year;
	int all_day_event = 0;

	if (WC->num_cal == 0) {
		wprintf("<BR><BR><BR>\n");
		return;
	}

	memcpy(&today_tm, localtime(&thetime), sizeof(struct tm));
	month = today_tm.tm_mon + 1;
	day = today_tm.tm_mday;
	year = today_tm.tm_year + 1900;

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);

			if (t.is_date) all_day_event = 1;
			else all_day_event = 0;

			if (all_day_event) {
				memcpy(&event_tm, gmtime(&event_tt), sizeof(struct tm));
			}
			else {
				memcpy(&event_tm, localtime(&event_tt), sizeof(struct tm));
			}

lprintf(9, "Event: %04d/%02d/%02d, Now: %04d/%02d/%02d\n",
	event_tm.tm_year,
	event_tm.tm_mon,
	event_tm.tm_mday,
	today_tm.tm_year,
	today_tm.tm_mon,
	today_tm.tm_mday);


			if ((event_tm.tm_year == today_tm.tm_year)
			   && (event_tm.tm_mon == today_tm.tm_mon)
			   && (event_tm.tm_mday == today_tm.tm_mday)) {

				p = icalcomponent_get_first_property(
							WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {

					if (all_day_event) {
						wprintf("<TABLE border=0 cellpadding=2><TR>"
							"<TD BGCOLOR=\"#CCCCDD\">"
						);
					}

					wprintf("<FONT SIZE=-1>"
						"<A HREF=\"/display_edit_event?msgnum=%ld&calview=%s&year=%s&month=%s&day=%s\">",
						WC->cal_msgnum[i],
						bstr("calview"),
						bstr("year"),
						bstr("month"),
						bstr("day")
					);
					escputs((char *)
						icalproperty_get_comment(p));
					wprintf("</A></FONT><BR>\n");

					if (all_day_event) {
						wprintf("</TD></TR></TABLE>");
					}

				}

			}


		}
	}
}



void calendar_month_view(int year, int month, int day) {
	struct tm starting_tm;
	struct tm tm;
	time_t thetime;
	int i;
	time_t previous_month;
	time_t next_month;

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
		memcpy(&tm, localtime(&thetime), sizeof(struct tm));
	}

	/* Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */

	/* Now back up until we're on a Sunday */
	memcpy(&tm, localtime(&thetime), sizeof(struct tm));
	while (tm.tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		memcpy(&tm, localtime(&thetime), sizeof(struct tm));
	}

	/* Outer table (to get the background color) */
	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#204B78><TR><TD>\n");

	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0>"
		"<TR><TD align=left><font color=#FFFFFF>"
		"&nbsp;<A HREF=\"/display_edit_event?msgnum=0"
		"&year=%d&month=%d&day=%d\">"
		"Add new calendar event</A>"
		"</font></TD>\n",
		year, month, day
	);

	wprintf("<TD ALIGN=CENTER>");

	memcpy(&tm, localtime(&previous_month), sizeof(struct tm));
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/back.gif\" BORDER=0></A>\n");

	wprintf("&nbsp;&nbsp;"
		"<FONT SIZE=+1 COLOR=\"#FFFFFF\">"
		"%s %d"
		"</FONT>"
		"&nbsp;&nbsp;", months[month-1], year);

	memcpy(&tm, localtime(&next_month), sizeof(struct tm));
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm.tm_year)+1900, tm.tm_mon + 1);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/forward.gif\" BORDER=0></A>\n");

	wprintf("</TD><TD align=right><font color=#FFFFFF size=-2>"
		"Click on any date for day view&nbsp;"
		"</FONT></TD></TR></TABLE>\n");

	/* Inner table (the real one) */
	wprintf("<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#204B78><TR>");
	for (i=0; i<7; ++i) {
		wprintf("<TD ALIGN=CENTER WIDTH=14%%>"
			"<FONT COLOR=\"#FFFFFF\">%s</FONT></TH>", days[i]);
	}
	wprintf("</TR>\n");

	/* Now do 35 days */
	for (i = 0; i < 35; ++i) {
		memcpy(&tm, localtime(&thetime), sizeof(struct tm));

		/* Before displaying Sunday, start a new row */
		if ((i % 7) == 0) {
			wprintf("<TR>");
		}

		wprintf("<TD BGCOLOR=\"#%s\" WIDTH=14%% HEIGHT=60 VALIGN=TOP><B>",
			((tm.tm_mon != month-1) ? "DDDDDD" :
			((tm.tm_wday==0 || tm.tm_wday==6) ? "EEEECC" :
			"FFFFFF"))
		);
		if ((i==0) || (tm.tm_mday == 1)) {
			wprintf("%s ", months[tm.tm_mon]);
		}
		wprintf("<A HREF=\"readfwd?calview=day&year=%d&month=%d&day=%d\">"
			"%d</A></B><BR>",
			tm.tm_year + 1900,
			tm.tm_mon + 1,
			tm.tm_mday,
			tm.tm_mday);

		/* put the data here, stupid */
		calendar_month_view_display_events(thetime);

		wprintf("</TD>");

		/* After displaying Saturday, end the row */
		if ((i % 7) == 6) {
			wprintf("</TR>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wprintf("</TABLE>"			/* end of inner table */
		"</TD></TR></TABLE>"		/* end of outer table */
		"</CENTER>\n");
}


void calendar_week_view(int year, int month, int day) {
	wprintf("<CENTER><I>week view FIXME</I></CENTER><BR>\n");
}


/*
 * Display events for a particular hour of a particular day.
 * (Specify hour < 0 to show "all day" events)
 */
void calendar_day_view_display_events(int year, int month,
					int day, int hour) {
	int i;
	icalproperty *p;
	struct icaltimetype t;
	time_t event_tt;
	struct tm *event_tm;
	int all_day_event = 0;

	if (WC->num_cal == 0) {
		wprintf("<BR><BR><BR>\n");
		return;
	}

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			if (t.is_date) all_day_event = 1;

			if (all_day_event) {
				event_tm = gmtime(&event_tt);
			}
			else {
				event_tm = localtime(&event_tt);
			}

			if ((event_tm->tm_year == (year-1900))
			   && (event_tm->tm_mon == (month-1))
			   && (event_tm->tm_mday == day)
			   && ( ((event_tm->tm_hour == hour)&&(!t.is_date)) || ((hour<0)&&(t.is_date)) )
			   ) {


				p = icalcomponent_get_first_property(
							WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {

					if (all_day_event) {
						wprintf("<TABLE border=1 cellpadding=2><TR>"
							"<TD BGCOLOR=\"#CCCCCC\">"
						);
					}

					wprintf("<FONT SIZE=-1>"
						"<A HREF=\"/display_edit_event?msgnum=%ld&calview=day&year=%d&month=%d&day=%d\">",
						WC->cal_msgnum[i],
						year, month, day
					);
					escputs((char *)
						icalproperty_get_comment(p));
					wprintf("</A></FONT><BR>\n");

					if (all_day_event) {
						wprintf("</TD></TR></TABLE>");
					}
				}

			}


		}
	}
}



void calendar_day_view(int year, int month, int day) {
	int hour;
	struct icaltimetype today, yesterday, tomorrow;


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


	/* Outer table (to get the background color) */
	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#204B78><TR><TD>\n");

	/* Inner table (the real one) */
	wprintf("<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#204B78><TR>\n");

	/* Innermost table (contains hours etc.) */
	wprintf("<TD WIDTH=80%%>"
		"<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#204B78>\n");

	/* Display events before 8:00 (hour=-1 is all-day events) */
	wprintf("<TR>"
		"<TD BGCOLOR=\"#CCCCDD\" VALIGN=MIDDLE WIDTH=10%%></TD>"
		"<TD BGCOLOR=\"#FFFFFF\" VALIGN=TOP>");
	for (hour = (-1); hour <= 7; ++hour) {
		calendar_day_view_display_events(year, month, day, hour);
	}
	wprintf("</TD></TR>\n");

	/* Now the middle of the day... */	
	for (hour = 8; hour <= 17; ++hour) {	/* could do HEIGHT=xx */
		wprintf("<TR HEIGHT=30><TD BGCOLOR=\"#CCCCDD\" ALIGN=MIDDLE "
			"VALIGN=MIDDLE WIDTH=10%%>");
		wprintf("<A HREF=\"/display_edit_event?msgnum=0"
			"&year=%d&month=%d&day=%d&hour=%d&minute=0\">",
			year, month, day, hour
		);
		wprintf("%d:00%s</A> ",
			(hour <= 12 ? hour : hour-12),
			(hour < 12 ? "am" : "pm")
		);
		wprintf("</TD><TD BGCOLOR=\"#FFFFFF\" VALIGN=TOP>");

		/* put the data here, stupid */
		calendar_day_view_display_events(year, month, day, hour);

		wprintf("</TD></TR>\n");
	}

	/* Display events after 5:00... */
	wprintf("<TR>"
		"<TD BGCOLOR=\"#CCCCDD\" VALIGN=MIDDLE WIDTH=10%%></TD>"
		"<TD BGCOLOR=\"#FFFFFF\" VALIGN=TOP>");
	for (hour = 18; hour <= 23; ++hour) {
		calendar_day_view_display_events(year, month, day, hour);
	}
	wprintf("</TD></TR>\n");


	wprintf("</TABLE>"			/* end of innermost table */
		"</TD>"
	);

	wprintf("<TD WIDTH=20%% VALIGN=top>");	/* begin stuff-on-the-right */


	/* Begin todays-date-with-left-and-right-arrows */
	wprintf("<TABLE BORDER=0 WIDTH=100%% "
		"CELLSPACING=0 CELLPADDING=0 BGCOLOR=\"#FFFFFF\">\n");
	wprintf("<TR>");

	/* Left arrow */	
	wprintf("<TD ALIGN=CENTER>");
	wprintf("<A HREF=\"readfwd?calview=day&year=%d&month=%d&day=%d\">",
		yesterday.year, yesterday.month, yesterday.day);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/back.gif\" BORDER=0></A>");
	wprintf("</TD>");

	/* Today's date */
	wprintf("<TD ALIGN=CENTER>");
	wprintf("<FONT SIZE=+2>%s</FONT><BR>"
		"<FONT SIZE=+3>%d</FONT><BR>"
		"<FONT SIZE=+2>%d</FONT><BR>",
		months[month-1], day, year);
	wprintf("</TD>");

	/* Right arrow */
	wprintf("<TD ALIGN=CENTER>");
	wprintf("<A HREF=\"readfwd?calview=day&year=%d&month=%d&day=%d\">",
		tomorrow.year, tomorrow.month, tomorrow.day);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/forward.gif\""
		" BORDER=0></A>\n");
	wprintf("</TD>");

	wprintf("</TR></TABLE>\n");
	/* End todays-date-with-left-and-right-arrows */

	wprintf("<BR><BR><CENTER><font color=#FFFFFF>"
		"&nbsp;<A HREF=\"/display_edit_event?msgnum=0"
		"&year=%d&month=%d&day=%d\">"
		"Add new calendar event</A>"
		"<BR><BR>\n",
		year, month, day
	);

	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">"
		"Back to month view</A>\n", year, month);

	wprintf("</FONT></CENTER>\n");

	wprintf("</TD>");			/* end stuff-on-the-right */



	wprintf("</TR></TABLE>"			/* end of inner table */
		"</TD></TR></TABLE>"		/* end of outer table */
	);



}

/*
 * Display today's events.
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
	memcpy(&today_tm, localtime(&now), sizeof(struct tm));

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			event_tt = icaltime_as_timet(t);
			if (t.is_date) all_day_event = 1;
			fmt_time(timestring, event_tt);

			if (all_day_event) {
				memcpy(&event_tm, gmtime(&event_tt), sizeof(struct tm));
			}
			else {
				memcpy(&event_tm, localtime(&event_tt), sizeof(struct tm));
			}

			if ( (event_tm.tm_year == today_tm.tm_year)
			   && (event_tm.tm_mon == today_tm.tm_mon)
			   && (event_tm.tm_mday == today_tm.tm_mday)
			   ) {


				p = icalcomponent_get_first_property(
							WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {
					escputs((char *)
						icalproperty_get_comment(p));
					wprintf(" (%s)<BR>\n", timestring);
				}
			}
		}
	}
	free_calendar_buffer();
}



void free_calendar_buffer(void) {
	int i;
	if (WC->num_cal) for (i=0; i<(WC->num_cal); ++i) {
		icalcomponent_free(WC->disp_cal[i]);
	}
	WC->num_cal = 0;
	free(WC->disp_cal);
	WC->disp_cal = NULL;
	free(WC->cal_msgnum);
	WC->cal_msgnum = NULL;
}




void do_calendar_view(void) {
	time_t now;
	struct tm tm;
	int year, month, day;
	char calview[SIZ];

	/* In case no date was specified, go with today */
	now = time(NULL);
	memcpy(&tm, localtime(&now), sizeof(struct tm));
	year = tm.tm_year + 1900;
	month = tm.tm_mon + 1;
	day = tm.tm_mday;

	/* Now see if a date was specified */
	if (strlen(bstr("year")) > 0) year = atoi(bstr("year"));
	if (strlen(bstr("month")) > 0) month = atoi(bstr("month"));
	if (strlen(bstr("day")) > 0) day = atoi(bstr("day"));

	/* How would you like that cooked? */
	if (strlen(bstr("calview")) > 0) {
		strcpy(calview, bstr("calview"));
	}
	else {
		strcpy(calview, "month");
	}

	/* Display the selected view */
	if (!strcasecmp(calview, "day")) {
		calendar_day_view(year, month, day);
	}
	else if (!strcasecmp(calview, "week")) {
		calendar_week_view(year, month, day);
	}
	else {
		calendar_month_view(year, month, day);
	}

	/* Free the calendar stuff */
	free_calendar_buffer();

}


void do_tasks_view(void) {
	int i;
	icalproperty *p;

	if (WC->num_cal) for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
		wprintf("<A HREF=\"/display_edit_task?msgnum=%ld&taskrm=",
			WC->cal_msgnum[i] );
		urlescputs(WC->wc_roomname);
		wprintf("\">");
		if (p != NULL) {
			escputs((char *)icalproperty_get_comment(p));
		}
		wprintf("</A><BR>\n");
	}

	wprintf("<BR><BR><A HREF=\"/display_edit_task?msgnum=0\">"
		"Add new task</A>\n"
	);


	/* Free the list */
	free_calendar_buffer();

}

#endif	/* WEBCIT_WITH_CALENDAR_SERVICE */
