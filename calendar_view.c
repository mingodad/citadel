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

#ifndef HAVE_ICAL_H

void do_calendar_view(void) {	/* stub for non-libical builds */
	wprintf("<CENTER><I>Calendar view not available</I></CENTER><BR>\n");
}

#else	/* HAVE_ICAL_H */

/****************************************************************************/

void calendar_month_view_display_events(time_t thetime) {
	int i;
	struct tm *tm;
	icalproperty *p;
	struct icaltimetype t;
	int month, day, year;

	if (WC->num_cal == 0) {
		wprintf("<BR><BR><BR>\n");
		return;
	}

	tm = localtime(&thetime);
	month = tm->tm_mon + 1;
	day = tm->tm_mday;
	year = tm->tm_year + 1900;

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			if ((t.year == year)
			   && (t.month == month)
			   && (t.day == day)) {

				p = icalcomponent_get_first_property(
							WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {
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
				}

			}


		}
	}
}



void calendar_month_view(int year, int month, int day) {
	struct tm starting_tm;
	struct tm *tm;
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

	tm = &starting_tm;
	while (tm->tm_mday != 1) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		tm = localtime(&thetime);
	}

	/* Determine previous and next months ... for links */
	previous_month = thetime - (time_t)864000L;	/* back 10 days */
	next_month = thetime + (time_t)(31L * 86400L);	/* ahead 31 days */
	lprintf(9, "previous month is %s", asctime(localtime(&previous_month)));
	lprintf(9, "next month is %s", asctime(localtime(&next_month)));

	/* Now back up until we're on a Sunday */
	tm = localtime(&thetime);
	while (tm->tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		tm = localtime(&thetime);
	}

	/* Outer table (to get the background color) */
	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#4444FF><TR><TD>\n");

	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0>"
		"<TR><TD align=left><font color=#FFFFFF>"
		"&nbsp;<A HREF=\"/display_edit_event?msgnum=0\">"
		"Add new calendar event</A>"
		"</font></TD>\n");

	wprintf("<TD><CENTER><H3>");

	tm = localtime(&previous_month);
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm->tm_year)+1900, tm->tm_mon + 1);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/back.gif\" BORDER=0></A>\n");

	wprintf("&nbsp;&nbsp;"
		"<FONT COLOR=#FFFFFF>"
		"%s %d"
		"</FONT>"
		"&nbsp;&nbsp;", months[month-1], year);

	tm = localtime(&next_month);
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm->tm_year)+1900, tm->tm_mon + 1);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/forward.gif\" BORDER=0></A>\n");

	wprintf("</H3></TD><TD align=right><font color=#FFFFFF size=-2>"
		"Click on any date for day view&nbsp;"
		"</FONT></TD></TR></TABLE>\n");

	/* Inner table (the real one) */
	wprintf("<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#4444FF>");
	for (i=0; i<7; ++i) {
		wprintf("<TH><FONT COLOR=#FFFFFF>%s</FONT></TH>", days[i]);
	}

	/* Now do 35 days */
	for (i = 0; i < 35; ++i) {
		tm = localtime(&thetime);
		if (tm->tm_wday == 0) {
			wprintf("<TR>");
		}

		wprintf("<TD BGCOLOR=FFFFFF WIDTH=14%% HEIGHT=60 VALIGN=TOP>"
			"<B>");
		if ((i==0) || (tm->tm_mday == 1)) {
			wprintf("%s ", months[tm->tm_mon]);
		}
		wprintf("<A HREF=\"readfwd?calview=day&year=%d&month=%d&day=%d\">"
			"%d</A></B><BR>",
			tm->tm_year + 1900,
			tm->tm_mon + 1,
			tm->tm_mday,
			tm->tm_mday);

		/* put the data here, stupid */
		calendar_month_view_display_events(thetime);

		wprintf("</TD>");

		if (tm->tm_wday == 6) {
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

	if (WC->num_cal == 0) {
		wprintf("<BR><BR><BR>\n");
		return;
	}

	for (i=0; i<(WC->num_cal); ++i) {
		p = icalcomponent_get_first_property(WC->disp_cal[i],
						ICAL_DTSTART_PROPERTY);
		if (p != NULL) {
			t = icalproperty_get_dtstart(p);
			if ((t.year == year)
			   && (t.month == month)
			   && (t.day == day)
			   && ( (t.hour == hour) || ((hour<0)&&(t.is_date)) )
			   ) {

				p = icalcomponent_get_first_property(
							WC->disp_cal[i],
							ICAL_SUMMARY_PROPERTY);
				if (p != NULL) {
					wprintf("<FONT SIZE=-1>"
						"<A HREF=\"/display_edit_event?msgnum=%ld&calview=day&year=%d&month=%d&day=%d\">",
						WC->cal_msgnum[i],
						year, month, day
					);
					escputs((char *)
						icalproperty_get_comment(p));
					wprintf("</A></FONT><BR>\n");
				}

			}


		}
	}
}



void calendar_day_view(int year, int month, int day) {
	int hour;

	/* Outer table (to get the background color) */
	wprintf("<TABLE width=100%% border=0 cellpadding=0 cellspacing=0 "
		"bgcolor=#4444FF><TR><TD>\n");

	/* Inner table (the real one) */
	wprintf("<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#4444FF><TR>\n");

	wprintf("<TD WIDTH=50%% VALIGN=top>");	/* begin stuff-on-the-left */

	wprintf("<CENTER><H3><FONT COLOR=#FFFFFF>"
		"%s %d, %d"
		"</FONT></H3></CENTER>\n",
		months[month-1], day, year);

	wprintf("<CENTER><font color=#FFFFFF>"
		"&nbsp;<A HREF=\"/display_edit_event?msgnum=0\">"
		"Add new calendar event</A>"
		"<BR><BR>\n");

	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">"
		"Back to month view</A>\n", year, month);

	wprintf("</FONT></CENTER>\n");

	wprintf("</TD>");			/* end stuff-on-the-left */

	/* Innermost table (contains hours etc.) */
	wprintf("<TD WIDTH=50%%>"
		"<TABLE width=100%% border=0 cellpadding=1 cellspacing=1 "
		"bgcolor=#4444FF>\n");

	/* Display events before 8:00 (hour=-1 is all-day events) */
	wprintf("<TR><TD BGCOLOR=FFFFFF VALIGN=TOP>");
	for (hour = (-1); hour <= 7; ++hour) {
		calendar_day_view_display_events(year, month, day, hour);
	}
	wprintf("</TD></TR>\n");

	/* Now the middle of the day... */	
	for (hour = 8; hour <= 17; ++hour) {	/* could do HEIGHT=xx */
		wprintf("<TR><TD BGCOLOR=FFFFFF VALIGN=TOP>");
		wprintf("%d:00 ", hour);

		/* put the data here, stupid */
		calendar_day_view_display_events(year, month, day, hour);

		wprintf("</TD></TR>\n");
	}

	/* Display events after 5:00... */
	wprintf("<TR><TD BGCOLOR=FFFFFF VALIGN=TOP>");
	for (hour = 18; hour <= 23; ++hour) {
		calendar_day_view_display_events(year, month, day, hour);
	}
	wprintf("</TD></TR>\n");


	wprintf("</TABLE>"			/* end of innermost table */
		"</TD></TR></TABLE>"		/* end of inner table */
		"</TD></TR></TABLE>"		/* end of outer table */
	);



}




void do_calendar_view(void) {
	int i;
	time_t now;
	struct tm *tm;
	int year, month, day;
	char calview[SIZ];

	/* In case no date was specified, go with today */
	now = time(NULL);
	tm = localtime(&now);
	year = tm->tm_year + 1900;
	month = tm->tm_mon + 1;
	day = tm->tm_mday;

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
	if (WC->num_cal) for (i=0; i<(WC->num_cal); ++i) {
		icalcomponent_free(WC->disp_cal[i]);
	}
	WC->num_cal = 0;
	free(WC->disp_cal);
	WC->disp_cal = NULL;
	free(WC->cal_msgnum);
	WC->cal_msgnum = NULL;
}


#endif	/* HAVE_ICAL_H */
