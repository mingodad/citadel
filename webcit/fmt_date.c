/*
 * $Id$
 */

#include "webcit.h"
#include "webserver.h"

typedef unsigned char byte;

#define FALSE 0 /**< no. */
#define TRUE 1 /**< yes. */

/*
 * Wrapper around strftime() or strftime_l()
 * depending upon how our build is configured.
 *
 * s		String target buffer
 * max		Maximum size of string target buffer
 * format	strftime() format
 * tm		Input date/time
 */
size_t wc_strftime(char *s, size_t max, const char *format, const struct tm *tm)
{

#ifdef ENABLE_NLS
#ifdef HAVE_USELOCALE
	if (wc_locales[WC->selected_language] == NULL) {
		return strftime(s, max, format, tm);
	}
	else { // TODO: this gives empty strings on debian.
		return strftime_l(s, max, format, tm, wc_locales[WC->selected_language]);
	}
#endif
#else
	return strftime(s, max, format, tm);
#endif
}


/*
 * Format a date/time stamp for output 
 */
void webcit_fmt_date(char *buf, time_t thetime, int brief)
{
	struct tm tm;
	struct tm today_tm;
	time_t today_timet;
	int time_format;

	time_format = get_time_format_cached ();
	today_timet = time(NULL);
	localtime_r(&today_timet, &today_tm);

	localtime_r(&thetime, &tm);

	if (brief) {

		/* If date == today, show only the time */
		if ((tm.tm_year == today_tm.tm_year)
		  &&(tm.tm_mon == today_tm.tm_mon)
		  &&(tm.tm_mday == today_tm.tm_mday)) {
			if (time_format == WC_TIMEFORMAT_24) 
				wc_strftime(buf, 32, "%k:%M", &tm);
			else
				wc_strftime(buf, 32, "%l:%M%p", &tm);
		}
		/* Otherwise, for messages up to 6 months old, show the month and day, and the time */
		else if (today_timet - thetime < 15552000) {
			if (time_format == WC_TIMEFORMAT_24) 
				wc_strftime(buf, 32, "%b %d %k:%M", &tm);
			else
				wc_strftime(buf, 32, "%b %d %l:%M%p", &tm);
		}
		/* older than 6 months, show only the date */
		else {
			wc_strftime(buf, 32, "%b %d %Y", &tm);
		}
	}
	else {
		if (time_format == WC_TIMEFORMAT_24)
			wc_strftime(buf, 32, "%a %b %d %Y %T %Z", &tm);
		else
			wc_strftime(buf, 32, "%a %b %d %Y %r %Z", &tm);
	}
}


/*
 * learn the users timeformat preference.
 */
int get_time_format_cached (void)
{
	char calhourformat[16];
	int *time_format_cache;
	time_format_cache = &(WC->time_format_cache);
	if (*time_format_cache == WC_TIMEFORMAT_NONE)
	{
		get_preference("calhourformat", calhourformat, sizeof calhourformat);
		if (!strcasecmp(calhourformat, "24")) 
			*time_format_cache = WC_TIMEFORMAT_24;
		else
			*time_format_cache = WC_TIMEFORMAT_AMPM;
	}
	return *time_format_cache;
}

/*
 * Format TIME ONLY for output 
 * buf		the output buffer
 * thetime	time to format into buf
 */
void fmt_time(char *buf, time_t thetime)
{
	struct tm *tm;
	int hour;
	int time_format;
	
	time_format = get_time_format_cached ();
	buf[0] = 0;
	tm = localtime(&thetime);
	hour = tm->tm_hour;
	if (hour == 0)
		hour = 12;
	else if (hour > 12)
		hour = hour - 12;

	if (time_format == WC_TIMEFORMAT_24) {
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
 * Break down the timestamp used in HTTP headers
 * Should read rfc1123 and rfc850 dates OK
 * FIXME won't read asctime
 * Doesn't understand timezone, but we only should be using GMT/UTC anyway
 */
time_t httpdate_to_timestamp(char *buf)
{
	time_t t = 0;
	struct tm tt;
	char *c;

	/** Skip day of week, to number */
	for (c = buf; *c != ' '; c++)
		;
	c++;

	/* Get day of month */
	tt.tm_mday = atoi(c);
	for (; *c != ' ' && *c != '-'; c++);
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
		return 42;
		break;	/* NOTREACHED */
	}
	c += 4;

	tt.tm_year = 0;
	/* Get year */
	tt.tm_year = atoi(c);
	for (; *c != ' '; c++);
	c++;
	if (tt.tm_year >= 1900)
		tt.tm_year -= 1900;

	/* Get hour */
	tt.tm_hour = atoi(c);
	for (; *c != ':'; c++);
	c++;

	/* Get minute */
	tt.tm_min = atoi(c);
	for (; *c != ':'; c++);
	c++;

	/* Get second */
	tt.tm_sec = atoi(c);
	for (; *c && *c != ' '; c++);

	/* Got everything; let's go.  The global 'timezone' variable contains the
	 * local timezone's offset from UTC, in seconds, so we apply that to tm_sec.
	 * This produces an illegal value for tm_sec, but mktime() will normalize
	 * it for us.  This eliminates the need to temporarily switch the environment
	 * variable TZ to UTC, which is good because it fails to switch back on
	 * some systems.
	 */
	tzset();
	tt.tm_sec = tt.tm_sec - (int)timezone;
	t = mktime(&tt);
	return t;
}
