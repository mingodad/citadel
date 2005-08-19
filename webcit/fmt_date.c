/*
 * $Id$
 *
 * Miscellaneous routines 
 */

#include "webcit.h"
#include "webserver.h"

typedef unsigned char byte;

#define FALSE 0
#define TRUE 1

char *ascmonths[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *ascdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};

/*
 * Format a date/time stamp for output 
 */
void fmt_date(char *buf, time_t thetime, int brief)
{
	struct tm tm;
	struct tm today_tm;
	time_t today_timet;
	int hour;
	char calhourformat[16];

	get_preference("calhourformat", calhourformat, sizeof calhourformat);

	today_timet = time(NULL);
	localtime_r(&today_timet, &today_tm);

	localtime_r(&thetime, &tm);
	hour = tm.tm_hour;
	if (hour == 0)
		hour = 12;
	else if (hour > 12)
		hour = hour - 12;

	buf[0] = 0;

	if (brief) {

		if ((tm.tm_year == today_tm.tm_year)
		  &&(tm.tm_mon == today_tm.tm_mon)
		  &&(tm.tm_mday == today_tm.tm_mday)) {
			if (!strcasecmp(calhourformat, "24")) {
				sprintf(buf, "%2d:%02d",
					tm.tm_hour, tm.tm_min
				);
			}
			else {
				sprintf(buf, "%2d:%02d%s",
					hour, tm.tm_min,
					((tm.tm_hour >= 12) ? "pm" : "am")
				);
			}
		}
		else {
			sprintf(buf, "%s %d %d",
				ascmonths[tm.tm_mon],
				tm.tm_mday,
				tm.tm_year + 1900
			);
		}
	}
	else {
		if (!strcasecmp(calhourformat, "24")) {
			sprintf(buf, "%s %d %d %2d:%02d",
				ascmonths[tm.tm_mon],
				tm.tm_mday,
				tm.tm_year + 1900,
				tm.tm_hour, tm.tm_min
			);
		}
		else {
			sprintf(buf, "%s %d %d %2d:%02d%s",
				ascmonths[tm.tm_mon],
				tm.tm_mday,
				tm.tm_year + 1900,
				hour, tm.tm_min, ((tm.tm_hour >= 12) ? "pm" : "am")
			);
		}
	}
}



/*
 * Format TIME ONLY for output 
 */
void fmt_time(char *buf, time_t thetime)
{
	struct tm *tm;
	int hour;
	char calhourformat[16];

	get_preference("calhourformat", calhourformat, sizeof calhourformat);

	buf[0] = 0;
	tm = localtime(&thetime);
	hour = tm->tm_hour;
	if (hour == 0)
		hour = 12;
	else if (hour > 12)
		hour = hour - 12;

	if (!strcasecmp(calhourformat, "24")) {
		sprintf(buf, "%2d:%02d",
			tm->tm_hour, tm->tm_min
		);
	}
	else {
		sprintf(buf, "%d:%02d%s",
			hour, tm->tm_min, ((tm->tm_hour > 12) ? "pm" : "am")
		);
	}
}




/*
 * Format a date/time stamp to the format used in HTTP headers
 */
void httpdate(char *buf, time_t thetime)
{
	struct tm *tm;

	buf[0] = 0;
	tm = localtime(&thetime);

	sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d",
		ascdays[tm->tm_wday],
		tm->tm_mday,
		ascmonths[tm->tm_mon],
		tm->tm_year + 1900, tm->tm_hour, tm->tm_min, tm->tm_sec);
}


/*
 * Break down the timestamp used in HTTP headers
 * Should read rfc1123 and rfc850 dates OK
 * FIXME won't read asctime
 * Doesn't understand timezone, but we only should be using GMT/UTC anyway
 */
time_t httpdate_to_timestamp(const char *buf)
{
	time_t t = 0;
	struct tm tt;
	char *c;

lprintf(3, "datestamp %s\n", buf);
	/* Skip day of week, to number */
	for (c = buf; *c < '0' && *c > '9'; c++)
		;

	/* Get day of month */
	tt.tm_mday = 0;
	for (; *c != ' ' && *c != '-'; c++)
		tt.tm_mday += 10 * (*c - '0');
	c++;

	/* Get month */
	switch (*c) {
	case 'A':	/* April, August */
		tt.tm_mon = (c[1] == 'p') ? 3 : 7;
		break;
	case 'D':	/* December */
		tt.tm_mon = 11;
		break;
	case 'F':	/* February */
		tt.tm_mon = 1;
		break;
	case 'M':	/* March, May */
		tt.tm_mon = (c[2] == 'r') ? 2 : 4;
		break;
	case 'J':	/* January, June, July */
		tt.tm_mon = (c[2] == 'n') ? ((c[1] == 'a') ? 0 : 5) : 6;
		break;
	case 'N':	/* November */
		tt.tm_mon = 10;
		break;
	case 'O':	/* October */
		tt.tm_mon = 9;
		break;
	case 'S':	/* September */
		tt.tm_mon = 8;
		break;
	default:
		return 0;
		break;	/* NOTREACHED */
	}
	c += 4;

	/* Get year */
	for (; *c != ' '; c++)
		tt.tm_year += 10 * (*c - '0');
	c++;
	/* Y2K business for rfc850 dates */
	if (tt.tm_year > 69)
		tt.tm_year += 1900;
	else
		tt.tm_year += 2000;

	/* Get hour */
	for (; *c != ':'; c++)
		tt.tm_hour += 10 * (*c - '0');
	c++;

	/* Get minute */
	for (; *c != ':'; c++)
		tt.tm_min += 10 * (*c - '0');
	c++;

	/* Get second */
	for (; *c && *c != ' '; c++)
		tt.tm_sec += 10 * (*c - '0');

	/* Got everything; let's go */
	t = mktime(&tt);
	return t;
}
