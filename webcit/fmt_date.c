/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

#include "webcit.h"
#include "webserver.h"

#ifdef HAVE_USELOCALE
extern locale_t *wc_locales;
#endif

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
	else {
		return strftime_l(s, max, format, tm, wc_locales[WC->selected_language]);
	}
#else
	return strftime(s, max, format, tm);
#endif
#else
	return strftime(s, max, format, tm);
#endif
}



/*
 * Format a date/time stamp for output 
 */
void webcit_fmt_date(char *buf, size_t siz, time_t thetime, int Format)
{
	struct tm tm;
	struct tm today_tm;
	time_t today_timet;
	int time_format;

	time_format = get_time_format_cached ();
	today_timet = time(NULL);
	localtime_r(&today_timet, &today_tm);

	localtime_r(&thetime, &tm);

	/*
	 * DATEFMT_FULL:      full display 
	 * DATEFMT_BRIEF:     if date == today, show only the time
	 *		      otherwise, for messages up to 6 months old, 
	 *                 show the month and day, and the time
	 *		      older than 6 months, show only the date
	 * DATEFMT_RAWDATE:   show full date, regardless of age 
	 * DATEFMT_LOCALEDATE:   show full date as prefered for the locale
	 */

	switch (Format) {
		case DATEFMT_BRIEF:
			if ((tm.tm_year == today_tm.tm_year)
			  &&(tm.tm_mon == today_tm.tm_mon)
			  &&(tm.tm_mday == today_tm.tm_mday)) {
				if (time_format == WC_TIMEFORMAT_24) 
					wc_strftime(buf, siz, "%k:%M", &tm);
				else
					wc_strftime(buf, siz, "%l:%M%p", &tm);
			}
			else if (today_timet - thetime < 15552000) {
				if (time_format == WC_TIMEFORMAT_24) 
					wc_strftime(buf, siz, "%b %d %k:%M", &tm);
				else
					wc_strftime(buf, siz, "%b %d %l:%M%p", &tm);
			}
			else {
				wc_strftime(buf, siz, "%b %d %Y", &tm);
			}
			break;
		case DATEFMT_FULL:
			if (time_format == WC_TIMEFORMAT_24)
				wc_strftime(buf, siz, "%a %b %d %Y %T %Z", &tm);
			else
				wc_strftime(buf, siz, "%a %b %d %Y %r %Z", &tm);
			break;
		case DATEFMT_RAWDATE:
			wc_strftime(buf, siz, "%a %b %d %Y", &tm);
			break;
		case DATEFMT_LOCALEDATE:
			wc_strftime(buf, siz, "%x", &tm);
			break;
	}
}


/* 
 * Try to guess whether the user will prefer 12 hour or 24 hour time based on the locale.
 */
long guess_calhourformat(void) {
	char buf[64];
	struct tm tm;
	memset(&tm, 0, sizeof tm);
	wc_strftime(buf, 64, "%X", &tm);
	if (buf[strlen(buf)-1] == 'M') {
		return 12;
	}
	return 24;
}


/*
 * learn the users timeformat preference.
 */
int get_time_format_cached (void)
{
	long calhourformat;
	int *time_format_cache;
	time_format_cache = &(WC->time_format_cache);
	if (*time_format_cache == WC_TIMEFORMAT_NONE)
	{
		get_pref_long("calhourformat", &calhourformat, 99);

		/* If we don't know the user's time format preference yet,
		 * make a guess based on the locale.
		 */
		if (calhourformat == 99) {
			calhourformat = guess_calhourformat();
		}

		/* Now set the preference */
		if (calhourformat == 24) 
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
void fmt_time(char *buf, size_t siz, time_t thetime)
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
		snprintf(buf, siz, "%d:%02d",
			tm->tm_hour, tm->tm_min
		);
	}
	else {
		snprintf(buf, siz, "%d:%02d%s",
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
time_t httpdate_to_timestamp(StrBuf *buf)
{
	time_t t = 0;
	struct tm tt;
	const char *c;

	/** Skip day of week, to number */
	for (c = ChrPtr(buf); *c != ' '; c++)
		;
	c++;
	
	memset(&tt, 0, sizeof(tt));

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


void LoadTimeformatSettingsCache(StrBuf *Preference, long lvalue)
{
	int *time_format_cache;
	
	 time_format_cache = &(WC->time_format_cache);
	 if (lvalue == 24) 
		 *time_format_cache = WC_TIMEFORMAT_24;
	 else
		 *time_format_cache = WC_TIMEFORMAT_AMPM;
}



void 
InitModule_DATETIME
(void)
{
	RegisterPreference("calhourformat", _("Time format"), PRF_INT, LoadTimeformatSettingsCache);


}
