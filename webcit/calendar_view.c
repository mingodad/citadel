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
	starting_tm.tm_mon = month;
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

	/* Now back up until we're on a Sunday */
	tm = localtime(&thetime);
	while (tm->tm_wday != 0) {
		thetime = thetime - (time_t)86400;	/* go back 24 hours */
		tm = localtime(&thetime);
	}

	wprintf("<CENTER><H2>");

	tm = localtime(&previous_month);
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm->tm_year)+1900, tm->tm_mon);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/back.gif\" BORDER=0></A>\n");

	wprintf("&nbsp;&nbsp;%s %d&nbsp;&nbsp;", months[month], year);

	tm = localtime(&next_month);
	wprintf("<A HREF=\"readfwd?calview=month&year=%d&month=%d&day=1\">",
		(int)(tm->tm_year)+1900, tm->tm_mon + 1);
	wprintf("<IMG ALIGN=MIDDLE SRC=\"/static/forward.gif\" BORDER=0></A>\n");

	wprintf("</H2>");

	wprintf("<TABLE border=1 width=100%%>");
	for (i=0; i<7; ++i) {
		wprintf("<TH>%s</TH>", days[i]);
	}

	/* Now do 35 days */
	for (i = 0; i < 35; ++i) {
		tm = localtime(&thetime);
		if (tm->tm_wday == 0) {
			wprintf("<TR>");
		}

		wprintf("<TD>");
		if ((i==0) || (tm->tm_mday == 1)) {
			wprintf("%s ", months[tm->tm_mon]);
		}
		wprintf("%d", tm->tm_mday);

		/* FIXME ... put the data here, stupid */
		wprintf("<BR><BR><BR>");

		wprintf("</TD>");

		if (tm->tm_wday == 6) {
			wprintf("</TR>\n");
		}

		thetime += (time_t)86400;		/* ahead 24 hours */
	}

	wprintf("</TABLE></CENTER>\n");
}


void calendar_week_view(int year, int month, int day) {
	wprintf("<CENTER><I>week view FIXME</I></CENTER><BR>\n");
}


void calendar_day_view(int year, int month, int day) {
	wprintf("<CENTER><I>day view FIXME</I></CENTER><BR>\n");
}



void do_calendar_view(void) {
	time_t now;
	struct tm *tm;
	int year, month, day;
	char calview[SIZ];

	/* In case no date was specified, go with today */
	now = time(NULL);
	tm = localtime(&now);
	year = tm->tm_year + 1900;
	month = tm->tm_mon;
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
}


#endif	/* HAVE_ICAL_H */
