/*
 * $Id$
 *
 * Function to generate RFC822-compliant textual time/date stamp
 *
 */

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
void generate_rfc822_datestamp(char *buf, time_t xtime) {
	struct tm *t;

	t = localtime(&xtime);

	sprintf(buf, "%s, %02d %s %04d %02d:%02d:%02d %s",
		weekdays[t->tm_wday],
		t->tm_mday,
		months[t->tm_mon],
		t->tm_year + 1900,
		t->tm_hour,
		t->tm_min,
		t->tm_sec,
		tzname[0]
		);
}

