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

char *months[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December"
};

char *days[] = {
	"Sunday", "Monday", "Tuesday", "Wednesday",
	"Thursday", "Friday", "Saturday"
};

#ifdef HAVE_ICAL_H


void display_icaltimetype_as_webform(struct icaltimetype *t, char *prefix) {
	int i;
	time_t now;
	struct tm *tm;
	int this_year;
	const int span = 10;

	now = time(NULL);
	tm = localtime(&now);
	this_year = tm->tm_year + 1900;

	if (t == NULL) return;

	wprintf("Month: ");
	wprintf("<SELECT NAME=\"%s_month\" SIZE=\"1\">\n", prefix);
	for (i=1; i<=12; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
			((t->month == i) ? "SELECTED" : ""),
			i,
			months[i-1]
		);
	}
	wprintf("</SELECT>\n");

	wprintf("Day: ");
	wprintf("<SELECT NAME=\"%s_day\" SIZE=\"1\">\n", prefix);
	for (i=1; i<=31; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
			((t->day == i) ? "SELECTED" : ""),
			i, i
		);
	}
	wprintf("</SELECT>\n");

	wprintf("Year: ");
	wprintf("<SELECT NAME=\"%s_year\" SIZE=\"1\">\n", prefix);
	if ((this_year - t->year) > span) {
		wprintf("<OPTION SELECTED VALUE=\"%d\">%d</OPTION>\n",
			t->year, t->year);
	}
	for (i=(this_year-span); i<=(this_year+span); ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
			((t->year == i) ? "SELECTED" : ""),
			i, i
		);
	}
	if ((t->year - this_year) > span) {
		wprintf("<OPTION SELECTED VALUE=\"%d\">%d</OPTION>\n",
			t->year, t->year);
	}
	wprintf("</SELECT>\n");

	wprintf("Hour: ");
	wprintf("<SELECT NAME=\"%s_hour\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=23; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
			((t->hour == i) ? "SELECTED" : ""),
			i, i
		);
	}
	wprintf("</SELECT>\n");

	wprintf("Minute: ");
	wprintf("<SELECT NAME=\"%s_minute\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=59; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
			((t->minute == i) ? "SELECTED" : ""),
			i, i
		);
	}
	wprintf("</SELECT>\n");

	wprintf("<INPUT TYPE=\"checkbox\" NAME=\"%s_alldayevent\" "
		"VALUE=\"yes\" %s> All day event",
		prefix,
		((t->is_date) ? "CHECKED" : ""));
}


struct icaltimetype icaltime_from_webform(char *prefix) {
	struct icaltimetype t;
	time_t now;
	char vname[SIZ];

	now = time(NULL);
	t = icaltime_from_timet(now, 0);

	sprintf(vname, "%s_month", prefix);	t.month = atoi(bstr(vname));
	sprintf(vname, "%s_day", prefix);	t.day = atoi(bstr(vname));
	sprintf(vname, "%s_year", prefix);	t.year = atoi(bstr(vname));
	sprintf(vname, "%s_hour", prefix);	t.hour = atoi(bstr(vname));
	sprintf(vname, "%s_minute", prefix);	t.minute = atoi(bstr(vname));

	sprintf(vname, "%s_alldayevent", prefix);
	if (!strcasecmp(bstr(vname), "yes")) {
		t.hour = 0;
		t.minute = 0;
		t.is_date = 1;
	}

	t = icaltime_normalize(t);
	return(t);
}


/*
 * Generae a new, globally unique UID parameter for a calendar object.
 */
void generate_new_uid(char *buf) {
	static int seq = 0;

	sprintf(buf, "%ld-%d@%s",
		(long)time(NULL),
		(seq++),
		serv_info.serv_fqdn);
}


#endif
