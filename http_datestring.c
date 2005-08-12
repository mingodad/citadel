/*
 * $Id$
 *
 * Function to generate HTTP-compliant textual time/date stamp
 * (This module was lifted directly from the Citadel server source)
 *
 */

#include "webcit.h"

static char *httpdate_months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *httpdate_weekdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


/*
 * Supplied with a unix timestamp, generate a textual time/date stamp
 */
void http_datestring(char *buf, size_t n, time_t xtime) {
	struct tm t;

	long offset;
	char offsign;

	localtime_r(&xtime, &t);

	/* Convert "seconds west of GMT" to "hours/minutes offset" */
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
	offset = t.tm_gmtoff;
#else
	offset = timezone;
#endif
	if (offset > 0) {
		offsign = '+';
	}
	else {
		offset = 0L - offset;
		offsign = '-';
	}
	offset = ( (offset / 3600) * 100 ) + ( offset % 60 );

	snprintf(buf, n, "%s, %02d %s %04d %02d:%02d:%02d %c%04ld",
		httpdate_weekdays[t.tm_wday],
		t.tm_mday,
		httpdate_months[t.tm_mon],
		t.tm_year + 1900,
		t.tm_hour,
		t.tm_min,
		t.tm_sec,
		offsign, offset
	);
}

