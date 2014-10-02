/*
 * Function to generate RFC822-compliant textual time/date stamp
 */

#include "sysdep.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "genstamp.h"


static char *months[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

static char *weekdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


/*
 * Supplied with a unix timestamp, generate an RFC822-compliant textual
 * time and date stamp.
 */
long datestring(char *buf, size_t n, time_t xtime, int which_format) {
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

	switch(which_format) {

		case DATESTRING_RFC822:
			return snprintf(
				buf, n,
				"%s, %02d %s %04d %02d:%02d:%02d %c%04ld",
				weekdays[t.tm_wday],
				t.tm_mday,
				months[t.tm_mon],
				t.tm_year + 1900,
				t.tm_hour,
				t.tm_min,
				t.tm_sec,
				offsign, offset
				);
		break;

		case DATESTRING_IMAP:
			return snprintf(
				buf, n,
				"%02d-%s-%04d %02d:%02d:%02d %c%04ld",
				t.tm_mday,
				months[t.tm_mon],
				t.tm_year + 1900,
				t.tm_hour,
				t.tm_min,
				t.tm_sec,
				offsign, offset
				);
		break;

	}
	return 0;
}
