/*
 * $Id$
 */
/**
 * \defgroup FormatDates Miscellaneous routines formating dates
 * \ingroup Calendaring
 */
/*@{*/
#include "webcit.h"
#include "webserver.h"

typedef unsigned char byte; /**< a byte. */

#define FALSE 0 /**< no. */
#define TRUE 1 /**< yes. */

/**
 * \brief	Wrapper around strftime() or strftime_l()
 *		depending upon how our build is configured.
 *
 * \param	s	String target buffer
 * \param	max	Maximum size of string target buffer
 * \param	format	strftime() format
 * \param	tm	Input date/time
 */
size_t wc_strftime(char *s, size_t max, const char *format, const struct tm *tm)
{
#ifdef ENABLE_NLS
	return strftime_l(s, max, format, tm, wc_locales[WC->selected_language]);
#else
	return strftime(s, max, format, tm);
#endif
}


/**
 * \brief Format a date/time stamp for output 
 * \param buf the output buffer
 * \param thetime time to convert to string 
 * \param brief do we want compact view?????
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

		/** If date == today, show only the time */
		if ((tm.tm_year == today_tm.tm_year)
		  &&(tm.tm_mon == today_tm.tm_mon)
		  &&(tm.tm_mday == today_tm.tm_mday)) {
			wc_strftime(buf, 32, "%l:%M%p", &tm);
		}
		/** Otherwise, for messages up to 6 months old, show the
		 * month and day, and the time */
		else if (today_timet - thetime < 15552000) {
			wc_strftime(buf, 32, "%b %d %l:%M%p", &tm);
		}
		/** older than 6 months, show only the date */
		else {
			wc_strftime(buf, 32, "%b %d %Y", &tm);
		}
	}
	else {
		wc_strftime(buf, 32, "%c", &tm);
	}
}

/**
 * \brief	Convenience function to return a month name
 *
 * \param	m		Numeric month
 */
char *monthname(int m)
{
	static char months[12][32];
	static int initialized = 0;

	time_t tt;
	struct tm tm;
	int i;

	if (!initialized) {
		for (i=0; i<12; ++i) {
			tt = 1137997451 + (i * 2592000);
			localtime_r(&tt, &tm);
			wc_strftime(months[i], 32, "%B", &tm);
			lprintf(9, "%s\n", months[i]);
		}
	}
	initialized = 1;

	return months[m];
		
}



/**
 * \brief Format TIME ONLY for output 
 * \param buf the output buffer
 * \param thetime time to format into buf
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




/**
 * \brief Break down the timestamp used in HTTP headers
 * Should read rfc1123 and rfc850 dates OK
 * \todo FIXME won't read asctime
 * Doesn't understand timezone, but we only should be using GMT/UTC anyway
 * \param buf time to parse
 * \return the time found in buf
 */
time_t httpdate_to_timestamp(char *buf)
{
	time_t t = 0;
	struct tm tt;
	char *c;
	char tz[256];

	/** Skip day of week, to number */
	for (c = buf; *c != ' '; c++)
		;
	c++;

	/* Get day of month */
	tt.tm_mday = atoi(c);
	for (; *c != ' ' && *c != '-'; c++);
	c++;

	/** Get month */
	switch (*c) {
	case 'A':	/** April, August */
		tt.tm_mon = (c[1] == 'p') ? 3 : 7;
		break;
	case 'D':	/** December */
		tt.tm_mon = 11;
		break;
	case 'F':	/** February */
		tt.tm_mon = 1;
		break;
	case 'M':	/** March, May */
		tt.tm_mon = (c[2] == 'r') ? 2 : 4;
		break;
	case 'J':	/** January, June, July */
		tt.tm_mon = (c[2] == 'n') ? ((c[1] == 'a') ? 0 : 5) : 6;
		break;
	case 'N':	/** November */
		tt.tm_mon = 10;
		break;
	case 'O':	/** October */
		tt.tm_mon = 9;
		break;
	case 'S':	/** September */
		tt.tm_mon = 8;
		break;
	default:
		return 42;
		break;	/** NOTREACHED */
	}
	c += 4;

	tt.tm_year = 0;
	/** Get year */
	tt.tm_year = atoi(c);
	for (; *c != ' '; c++);
	c++;
	if (tt.tm_year >= 1900)
		tt.tm_year -= 1900;

	/** Get hour */
	tt.tm_hour = atoi(c);
	for (; *c != ':'; c++);
	c++;

	/** Get minute */
	tt.tm_min = atoi(c);
	for (; *c != ':'; c++);
	c++;

	/** Get second */
	tt.tm_sec = atoi(c);
	for (; *c && *c != ' '; c++);

	/** Got everything; let's go */
	/** First, change to UTC */
	if (getenv("TZ"))
		sprintf(tz, "TZ=%s", getenv("TZ"));
	else
		strcpy(tz, "TZ=");
	putenv("TZ=UTC");
	tzset();
	t = mktime(&tt);
	putenv(tz);
	tzset();
	return t;
}




/*@}*/
