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

char *hourname[] = {
	"12am", "1am", "2am", "3am", "4am", "5am", "6am",
	"7am", "8am", "9am", "10am", "11am", "12pm",
	"1pm", "2pm", "3pm", "4pm", "5pm", "6pm",
	"7pm", "8pm", "9pm", "10pm", "11pm"
};

#ifdef WEBCIT_WITH_CALENDAR_SERVICE

/*
 * The display_icaltimetype_as_webform() and icaltime_from_webform() functions
 * handle the display and editing of date/time properties in web pages.  The
 * first one converts an icaltimetype into valid HTML markup -- a series of form
 * fields for editing the date and time.  When the user submits the form, the
 * results can be fed back into the second function, which turns it back into
 * an icaltimetype.  The "prefix" string required by both functions is prepended
 * to all field names.  This allows a form to contain more than one date/time
 * property (for example, a start and end time) by ensuring the field names are
 * unique within the form.
 *
 * NOTE: These functions assume that the icaltimetype being edited is in UTC, and
 * will convert to/from local time for editing.  "local" in this case is assumed
 * to be the time zone in which the WebCit server is running.  A future improvement
 * might be to allow the user to specify his/her timezone.
 */


void display_icaltimetype_as_webform(struct icaltimetype *t, char *prefix) {
	int i;

	time_t now;
	struct tm tm_now;
	int this_year;

	time_t tt;
	struct tm tm;

	const int span = 10;

	now = time(NULL);
	memcpy(&tm_now, localtime(&now), sizeof(struct tm));
	this_year = tm_now.tm_year + 1900;

	if (t == NULL) return;
	tt = icaltime_as_timet(*t);
	memcpy(&tm, localtime(&tt), sizeof(struct tm));

	wprintf("Month: ");
	wprintf("<SELECT NAME=\"%s_month\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=11; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
			((tm.tm_mon == i) ? "SELECTED" : ""),
			i+1,
			months[i]
		);
	}
	wprintf("</SELECT>\n");

	wprintf("Day: ");
	wprintf("<SELECT NAME=\"%s_day\" SIZE=\"1\">\n", prefix);
	for (i=1; i<=31; ++i) {
		wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
			((tm.tm_mday == i) ? "SELECTED" : ""),
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
		wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
			((tm.tm_hour == i) ? "SELECTED" : ""),
			i, hourname[i]
		);
	}
	wprintf("</SELECT>\n");

	wprintf("Minute: ");
	wprintf("<SELECT NAME=\"%s_minute\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=59; ++i) {
		if ( (i % 5 == 0) || (tm.tm_min == i) ) {
			wprintf("<OPTION %s VALUE=\"%d\">:%02d</OPTION>\n",
				((tm.tm_min == i) ? "SELECTED" : ""),
				i, i
			);
		}
	}
	wprintf("</SELECT>\n");
}


struct icaltimetype icaltime_from_webform(char *prefix) {
	struct icaltimetype t;
	time_t tt;
	struct tm tm;
	char vname[SIZ];

	tt = time(NULL);
	memcpy(&tm, localtime(&tt), sizeof(struct tm));

	sprintf(vname, "%s_month", prefix);	tm.tm_mon = atoi(bstr(vname)) - 1;
	sprintf(vname, "%s_day", prefix);	tm.tm_mday = atoi(bstr(vname));
	sprintf(vname, "%s_year", prefix);	tm.tm_year = atoi(bstr(vname)) - 1900;
	sprintf(vname, "%s_hour", prefix);	tm.tm_hour = atoi(bstr(vname));
	sprintf(vname, "%s_minute", prefix);	tm.tm_min = atoi(bstr(vname));

	tt = mktime(&tm);
	t = icaltime_from_timet(tt, 0);
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

/*
 * Render a PARTSTAT parameter as a string (and put it in parentheses)
 */
void partstat_as_string(char *buf, icalproperty *attendee) {
	icalparameter *partstat_param;
	icalparameter_partstat partstat;

	strcpy(buf, "(status unknown)");

	partstat_param = icalproperty_get_first_parameter(
				attendee,
				ICAL_PARTSTAT_PARAMETER
	);
	if (partstat_param == NULL) {
		return;
	}

	partstat = icalparameter_get_partstat(partstat_param);
	switch(partstat) {
		case ICAL_PARTSTAT_X:
			strcpy(buf, "(x)");
			break;
		case ICAL_PARTSTAT_NEEDSACTION:
			strcpy(buf, "(needs action)");
			break;
		case ICAL_PARTSTAT_ACCEPTED:
			strcpy(buf, "(accepted)");
			break;
		case ICAL_PARTSTAT_DECLINED:
			strcpy(buf, "(declined)");
			break;
		case ICAL_PARTSTAT_TENTATIVE:
			strcpy(buf, "(tenative)");
			break;
		case ICAL_PARTSTAT_DELEGATED:
			strcpy(buf, "(delegated)");
			break;
		case ICAL_PARTSTAT_COMPLETED:
			strcpy(buf, "(completed)");
			break;
		case ICAL_PARTSTAT_INPROCESS:
			strcpy(buf, "(in process)");
			break;
		case ICAL_PARTSTAT_NONE:
			strcpy(buf, "(none)");
			break;
	}
}


/*
 * Utility function to encapsulate a subcomponent into a full VCALENDAR
 */
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp) {
	icalcomponent *encaps;

	/* If we're already looking at a full VCALENDAR component,
	 * don't bother ... just return itself.
	 */
	if (icalcomponent_isa(subcomp) == ICAL_VCALENDAR_COMPONENT) {
		return subcomp;
	}

	/* Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
	if (encaps == NULL) {
		lprintf(3, "Error at %s:%d - could not allocate component!\n",
			__FILE__, __LINE__);
		return NULL;
	}

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/* Encapsulate the subcomponent inside */
	icalcomponent_add_component(encaps, subcomp);

	/* Convert all timestamps to UTC so we don't have to deal with
	 * stupid VTIMEZONE crap.
	 */
	ical_dezonify(encaps);

	/* Return the object we just created. */
	return(encaps);
}




#endif
