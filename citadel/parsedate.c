
# line 2 "parsedate.y"
/* $Revision$
**
**  Originally written by Steven M. Bellovin <smb@research.att.com> while
**  at the University of North Carolina at Chapel Hill.  Later tweaked by
**  a couple of people on Usenet.  Completely overhauled by Rich $alz
**  <rsalz@osf.org> and Jim Berets <jberets@bbn.com> in August, 1990.
**  Further revised (removed obsolete constructs and cleaned up timezone
**  names) in August, 1991, by Rich.  Paul Eggert <eggert@twinsun.com>
**  helped in September, 1992.  Art Cancro <ajc@uncnsrd.mt-kisco.ny.us> cleaned
**  it up for ANSI C in December, 1999.
**
**  This grammar has six shift/reduce conflicts.
**
**  This code is in the public domain and has no copyright.
*/
/* SUPPRESS 530 *//* Empty body for statement */
/* SUPPRESS 593 on yyerrlab *//* Label was not used */
/* SUPPRESS 593 on yynewstate *//* Label was not used */
/* SUPPRESS 595 on yypvt *//* Automatic variable may be used before set */
#include <stdio.h>
#include <sys/types.h>
#include <ctype.h>
#include <time.h>
#include "parsedate.h"

int date_lex(void);

#define yyparse		date_parse
#define yylex		date_lex
#define yyerror		date_error


    /* See the LeapYears table in Convert. */
#define EPOCH		1970
#define END_OF_TIME	2038
    /* Constants for general time calculations. */
#define DST_OFFSET	1
#define SECSPERDAY	(24L * 60L * 60L)
    /* Readability for TABLE stuff. */
#define HOUR(x)		(x * 60)

#define LPAREN		'('
#define RPAREN		')'
#define IS7BIT(x)	((unsigned int)(x) < 0200)

#define SIZEOF(array)	((int)(sizeof array / sizeof array[0]))
#define ENDOF(array)	(&array[SIZEOF(array)])


/*
**  An entry in the lexical lookup table.
*/
typedef struct _TABLE {
    char	*name;
    int		type;
    time_t	value;
} TABLE;

/*
**  Daylight-savings mode:  on, off, or not yet known.
*/
typedef enum _DSTMODE {
    DSTon, DSToff, DSTmaybe
} DSTMODE;

/*
**  Meridian:  am, pm, or 24-hour style.
*/
typedef enum _MERIDIAN {
    MERam, MERpm, MER24
} MERIDIAN;


/*
**  Global variables.  We could get rid of most of them by using a yacc
**  union, but this is more efficient.  (This routine predates the
**  yacc %union construct.)
*/
static char	*yyInput;
static DSTMODE	yyDSTmode;
static int	yyHaveDate;
static int	yyHaveRel;
static int	yyHaveTime;
static time_t	yyTimezone;
static time_t	yyDay;
static time_t	yyHour;
static time_t	yyMinutes;
static time_t	yyMonth;
static time_t	yySeconds;
static time_t	yyYear;
static MERIDIAN	yyMeridian;
static time_t	yyRelMonth;
static time_t	yyRelSeconds;


static void		date_error(char *);

# line 100 "parsedate.y"
typedef union
#ifdef __cplusplus
	YYSTYPE
#endif
 {
    time_t		Number;
    enum _MERIDIAN	Meridian;
} YYSTYPE;
# define tDAY 257
# define tDAYZONE 258
# define tMERIDIAN 259
# define tMONTH 260
# define tMONTH_UNIT 261
# define tSEC_UNIT 262
# define tSNUMBER 263
# define tUNUMBER 264
# define tZONE 265

#include <inttypes.h>

#ifdef __STDC__
#include <stdlib.h>
#include <string.h>
#else
#include <malloc.h>
#include <memory.h>
#endif

#include <values.h>

#if defined(__cplusplus) || defined(__STDC__)

#if defined(__cplusplus) && defined(__EXTERN_C__)
extern "C" {
#endif
#ifndef yyerror
#if defined(__cplusplus)
	void yyerror(const char *);
#endif
#endif
#ifndef yylex
	int yylex(void);
#endif
	int yyparse(void);
#if defined(__cplusplus) && defined(__EXTERN_C__)
}
#endif

#endif

#define yyclearin yychar = -1
#define yyerrok yyerrflag = 0
extern int yychar;
extern int yyerrflag;
YYSTYPE yylval;
YYSTYPE yyval;
typedef int yytabelem;
#ifndef YYMAXDEPTH
#define YYMAXDEPTH 150
#endif
#if YYMAXDEPTH > 0
int yy_yys[YYMAXDEPTH], *yys = yy_yys;
YYSTYPE yy_yyv[YYMAXDEPTH], *yyv = yy_yyv;
#else	/* user does initial allocation */
int *yys;
YYSTYPE *yyv;
#endif
static int yymaxdepth = YYMAXDEPTH;
# define YYERRCODE 256

# line 286 "parsedate.y"


/* Month and day table. */
static TABLE	MonthDayTable[] = {
    { "january",	tMONTH,  1 },
    { "february",	tMONTH,  2 },
    { "march",		tMONTH,  3 },
    { "april",		tMONTH,  4 },
    { "may",		tMONTH,  5 },
    { "june",		tMONTH,  6 },
    { "july",		tMONTH,  7 },
    { "august",		tMONTH,  8 },
    { "september",	tMONTH,  9 },
    { "october",	tMONTH, 10 },
    { "november",	tMONTH, 11 },
    { "december",	tMONTH, 12 },
	/* The value of the day isn't used... */
    { "sunday",		tDAY, 0 },
    { "monday",		tDAY, 0 },
    { "tuesday",	tDAY, 0 },
    { "wednesday",	tDAY, 0 },
    { "thursday",	tDAY, 0 },
    { "friday",		tDAY, 0 },
    { "saturday",	tDAY, 0 },
};

/* Time units table. */
static TABLE	UnitsTable[] = {
    { "year",		tMONTH_UNIT,	12 },
    { "month",		tMONTH_UNIT,	1 },
    { "week",		tSEC_UNIT,	7L * 24 * 60 * 60 },
    { "day",		tSEC_UNIT,	1L * 24 * 60 * 60 },
    { "hour",		tSEC_UNIT,	60 * 60 },
    { "minute",		tSEC_UNIT,	60 },
    { "min",		tSEC_UNIT,	60 },
    { "second",		tSEC_UNIT,	1 },
    { "sec",		tSEC_UNIT,	1 },
};

/* Timezone table. */
static TABLE	TimezoneTable[] = {
    { "gmt",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
    { "ut",	tZONE,     HOUR( 0) },	/* Universal */
    { "utc",	tZONE,     HOUR( 0) },	/* Universal Coordinated */
    { "cut",	tZONE,     HOUR( 0) },	/* Coordinated Universal */
    { "z",	tZONE,     HOUR( 0) },	/* Greenwich Mean */
    { "wet",	tZONE,     HOUR( 0) },	/* Western European */
    { "bst",	tDAYZONE,  HOUR( 0) },	/* British Summer */
    { "nst",	tZONE,     HOUR(3)+30 }, /* Newfoundland Standard */
    { "ndt",	tDAYZONE,  HOUR(3)+30 }, /* Newfoundland Daylight */
    { "ast",	tZONE,     HOUR( 4) },	/* Atlantic Standard */
    { "adt",	tDAYZONE,  HOUR( 4) },	/* Atlantic Daylight */
    { "est",	tZONE,     HOUR( 5) },	/* Eastern Standard */
    { "edt",	tDAYZONE,  HOUR( 5) },	/* Eastern Daylight */
    { "cst",	tZONE,     HOUR( 6) },	/* Central Standard */
    { "cdt",	tDAYZONE,  HOUR( 6) },	/* Central Daylight */
    { "mst",	tZONE,     HOUR( 7) },	/* Mountain Standard */
    { "mdt",	tDAYZONE,  HOUR( 7) },	/* Mountain Daylight */
    { "pst",	tZONE,     HOUR( 8) },	/* Pacific Standard */
    { "pdt",	tDAYZONE,  HOUR( 8) },	/* Pacific Daylight */
    { "yst",	tZONE,     HOUR( 9) },	/* Yukon Standard */
    { "ydt",	tDAYZONE,  HOUR( 9) },	/* Yukon Daylight */
    { "akst",	tZONE,     HOUR( 9) },	/* Alaska Standard */
    { "akdt",	tDAYZONE,  HOUR( 9) },	/* Alaska Daylight */
    { "hst",	tZONE,     HOUR(10) },	/* Hawaii Standard */
    { "hast",	tZONE,     HOUR(10) },	/* Hawaii-Aleutian Standard */
    { "hadt",	tDAYZONE,  HOUR(10) },	/* Hawaii-Aleutian Daylight */
    { "ces",	tDAYZONE,  -HOUR(1) },	/* Central European Summer */
    { "cest",	tDAYZONE,  -HOUR(1) },	/* Central European Summer */
    { "mez",	tZONE,     -HOUR(1) },	/* Middle European */
    { "mezt",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
    { "cet",	tZONE,     -HOUR(1) },	/* Central European */
    { "met",	tZONE,     -HOUR(1) },	/* Middle European */
    { "eet",	tZONE,     -HOUR(2) },	/* Eastern Europe */
    { "msk",	tZONE,     -HOUR(3) },	/* Moscow Winter */
    { "msd",	tDAYZONE,  -HOUR(3) },	/* Moscow Summer */
    { "wast",	tZONE,     -HOUR(8) },	/* West Australian Standard */
    { "wadt",	tDAYZONE,  -HOUR(8) },	/* West Australian Daylight */
    { "hkt",	tZONE,     -HOUR(8) },	/* Hong Kong */
    { "cct",	tZONE,     -HOUR(8) },	/* China Coast */
    { "jst",	tZONE,     -HOUR(9) },	/* Japan Standard */
    { "kst",	tZONE,     -HOUR(9) },	/* Korean Standard */
    { "kdt",	tZONE,     -HOUR(9) },	/* Korean Daylight */
    { "cast",	tZONE,     -(HOUR(9)+30) }, /* Central Australian Standard */
    { "cadt",	tDAYZONE,  -(HOUR(9)+30) }, /* Central Australian Daylight */
    { "east",	tZONE,     -HOUR(10) },	/* Eastern Australian Standard */
    { "eadt",	tDAYZONE,  -HOUR(10) },	/* Eastern Australian Daylight */
    { "nzst",	tZONE,     -HOUR(12) },	/* New Zealand Standard */
    { "nzdt",	tDAYZONE,  -HOUR(12) },	/* New Zealand Daylight */

    /* For completeness we include the following entries. */
#if 0

    /* Duplicate names.  Either they conflict with a zone listed above
     * (which is either more likely to be seen or just been in circulation
     * longer), or they conflict with another zone in this section and
     * we could not reasonably choose one over the other. */
    { "fst",	tZONE,     HOUR( 2) },	/* Fernando De Noronha Standard */
    { "fdt",	tDAYZONE,  HOUR( 2) },	/* Fernando De Noronha Daylight */
    { "bst",	tZONE,     HOUR( 3) },	/* Brazil Standard */
    { "est",	tZONE,     HOUR( 3) },	/* Eastern Standard (Brazil) */
    { "edt",	tDAYZONE,  HOUR( 3) },	/* Eastern Daylight (Brazil) */
    { "wst",	tZONE,     HOUR( 4) },	/* Western Standard (Brazil) */
    { "wdt",	tDAYZONE,  HOUR( 4) },	/* Western Daylight (Brazil) */
    { "cst",	tZONE,     HOUR( 5) },	/* Chile Standard */
    { "cdt",	tDAYZONE,  HOUR( 5) },	/* Chile Daylight */
    { "ast",	tZONE,     HOUR( 5) },	/* Acre Standard */
    { "adt",	tDAYZONE,  HOUR( 5) },	/* Acre Daylight */
    { "cst",	tZONE,     HOUR( 5) },	/* Cuba Standard */
    { "cdt",	tDAYZONE,  HOUR( 5) },	/* Cuba Daylight */
    { "est",	tZONE,     HOUR( 6) },	/* Easter Island Standard */
    { "edt",	tDAYZONE,  HOUR( 6) },	/* Easter Island Daylight */
    { "sst",	tZONE,     HOUR(11) },	/* Samoa Standard */
    { "ist",	tZONE,     -HOUR(2) },	/* Israel Standard */
    { "idt",	tDAYZONE,  -HOUR(2) },	/* Israel Daylight */
    { "idt",	tDAYZONE,  -(HOUR(3)+30) }, /* Iran Daylight */
    { "ist",	tZONE,     -(HOUR(3)+30) }, /* Iran Standard */
    { "cst",	 tZONE,     -HOUR(8) },	/* China Standard */
    { "cdt",	 tDAYZONE,  -HOUR(8) },	/* China Daylight */
    { "sst",	 tZONE,     -HOUR(8) },	/* Singapore Standard */

    /* Dubious (e.g., not in Olson's TIMEZONE package) or obsolete. */
    { "gst",	tZONE,     HOUR( 3) },	/* Greenland Standard */
    { "wat",	tZONE,     -HOUR(1) },	/* West Africa */
    { "at",	tZONE,     HOUR( 2) },	/* Azores */
    { "gst",	tZONE,     -HOUR(10) },	/* Guam Standard */
    { "nft",	tZONE,     HOUR(3)+30 }, /* Newfoundland */
    { "idlw",	tZONE,     HOUR(12) },	/* International Date Line West */
    { "mewt",	tZONE,     -HOUR(1) },	/* Middle European Winter */
    { "mest",	tDAYZONE,  -HOUR(1) },	/* Middle European Summer */
    { "swt",	tZONE,     -HOUR(1) },	/* Swedish Winter */
    { "sst",	tDAYZONE,  -HOUR(1) },	/* Swedish Summer */
    { "fwt",	tZONE,     -HOUR(1) },	/* French Winter */
    { "fst",	tDAYZONE,  -HOUR(1) },	/* French Summer */
    { "bt",	tZONE,     -HOUR(3) },	/* Baghdad */
    { "it",	tZONE,     -(HOUR(3)+30) }, /* Iran */
    { "zp4",	tZONE,     -HOUR(4) },	/* USSR Zone 3 */
    { "zp5",	tZONE,     -HOUR(5) },	/* USSR Zone 4 */
    { "ist",	tZONE,     -(HOUR(5)+30) }, /* Indian Standard */
    { "zp6",	tZONE,     -HOUR(6) },	/* USSR Zone 5 */
    { "nst",	tZONE,     -HOUR(7) },	/* North Sumatra */
    { "sst",	tZONE,     -HOUR(7) },	/* South Sumatra */
    { "jt",	tZONE,     -(HOUR(7)+30) }, /* Java (3pm in Cronusland!) */
    { "nzt",	tZONE,     -HOUR(12) },	/* New Zealand */
    { "idle",	tZONE,     -HOUR(12) },	/* International Date Line East */
    { "cat",	tZONE,     HOUR(10) },	/* -- expired 1967 */
    { "nt",	tZONE,     HOUR(11) },	/* -- expired 1967 */
    { "ahst",	tZONE,     HOUR(10) },	/* -- expired 1983 */
    { "hdt",	tDAYZONE,  HOUR(10) },	/* -- expired 1986 */
#endif /* 0 */
};


/* ARGSUSED */
static void
date_error(char *s)
{
    /* NOTREACHED */
}


static time_t
ToSeconds(time_t Hours, time_t Minutes, time_t Seconds, MERIDIAN Meridian)
{
    if (Minutes < 0 || Minutes > 59 || Seconds < 0 || Seconds > 61)
	return -1;
    if (Meridian == MER24) {
	if (Hours < 0 || Hours > 23)
	    return -1;
    }
    else {
	if (Hours < 1 || Hours > 12)
	    return -1;
	if (Hours == 12)
	    Hours = 0;
	if (Meridian == MERpm)
	    Hours += 12;
    }
    return (Hours * 60L + Minutes) * 60L + Seconds;
}


static time_t
Convert(time_t Month, time_t Day, time_t Year,
	time_t Hours, time_t Minutes, time_t Seconds,
	MERIDIAN Meridian, DSTMODE dst)
{
    static int	DaysNormal[13] = {
	0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    static int	DaysLeap[13] = {
	0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
    };
    static int	LeapYears[] = {
	1972, 1976, 1980, 1984, 1988, 1992, 1996,
	2000, 2004, 2008, 2012, 2016, 2020, 2024, 2028, 2032, 2036
    };
    register int	*yp;
    register int	*mp;
    register time_t	Julian;
    register int	i;
    time_t		tod;

    if (Year < 0)
	Year = -Year;
    if (Year < 100)
	Year += 1900;
    if (Year < EPOCH)
	Year += 100;
    for (mp = DaysNormal, yp = LeapYears; yp < ENDOF(LeapYears); yp++)
	if (Year == *yp) {
	    mp = DaysLeap;
	    break;
	}
    if (Year < EPOCH || Year > END_OF_TIME
     || Month < 1 || Month > 12
     /* NOSTRICT *//* conversion from long may lose accuracy */
     || Day < 1 || Day > mp[(int)Month])
	return -1;

    Julian = Day - 1 + (Year - EPOCH) * 365;
    for (yp = LeapYears; yp < ENDOF(LeapYears); yp++, Julian++)
	if (Year <= *yp)
	    break;
    for (i = 1; i < Month; i++)
	Julian += *++mp;
    Julian *= SECSPERDAY;
    Julian += yyTimezone * 60L;
    if ((tod = ToSeconds(Hours, Minutes, Seconds, Meridian)) < 0)
	return -1;
    Julian += tod;
    tod = Julian;
    if (dst == DSTon || (dst == DSTmaybe && localtime(&tod)->tm_isdst))
	Julian -= DST_OFFSET * 60L * 60L;
    return Julian;
}


static time_t
DSTcorrect(time_t Start, time_t Future)
{
    time_t	StartDay;
    time_t	FutureDay;

    StartDay = (localtime(&Start)->tm_hour + 1) % 24;
    FutureDay = (localtime(&Future)->tm_hour + 1) % 24;
    return (Future - Start) + (StartDay - FutureDay) * DST_OFFSET * 60L * 60L;
}


static time_t
RelativeMonth(time_t Start, time_t RelMonth)
{
    struct tm	*tm;
    time_t	Month;
    time_t	Year;

    tm = localtime(&Start);
    Month = 12 * tm->tm_year + tm->tm_mon + RelMonth;
    Year = Month / 12;
    Month = Month % 12 + 1;
    return DSTcorrect(Start,
	    Convert(Month, (time_t)tm->tm_mday, Year,
		(time_t)tm->tm_hour, (time_t)tm->tm_min, (time_t)tm->tm_sec,
		MER24, DSTmaybe));
}


static int
LookupWord(char *buff, register int length)
{
    register char	*p;
    register char	*q;
    register TABLE	*tp;
    register int	c;

    p = buff;
    c = p[0];

    /* See if we have an abbreviation for a month. */
    if (length == 3 || (length == 4 && p[3] == '.'))
	for (tp = MonthDayTable; tp < ENDOF(MonthDayTable); tp++) {
	    q = tp->name;
	    if (c == q[0] && p[1] == q[1] && p[2] == q[2]) {
		yylval.Number = tp->value;
		return tp->type;
	    }
	}
    else
	for (tp = MonthDayTable; tp < ENDOF(MonthDayTable); tp++)
	    if (c == tp->name[0] && strcmp(p, tp->name) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }

    /* Try for a timezone. */
    for (tp = TimezoneTable; tp < ENDOF(TimezoneTable); tp++)
	if (c == tp->name[0] && p[1] == tp->name[1]
	 && strcmp(p, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}

    /* Try the units table. */
    for (tp = UnitsTable; tp < ENDOF(UnitsTable); tp++)
	if (c == tp->name[0] && strcmp(p, tp->name) == 0) {
	    yylval.Number = tp->value;
	    return tp->type;
	}

    /* Strip off any plural and try the units table again. */
    if (--length > 0 && p[length] == 's') {
	p[length] = '\0';
	for (tp = UnitsTable; tp < ENDOF(UnitsTable); tp++)
	    if (c == tp->name[0] && strcmp(p, tp->name) == 0) {
		p[length] = 's';
		yylval.Number = tp->value;
		return tp->type;
	    }
	p[length] = 's';
    }
    length++;

    /* Drop out any periods. */
    for (p = buff, q = (char*)buff; *q; q++)
	if (*q != '.')
	    *p++ = *q;
    *p = '\0';

    /* Try the meridians. */
    if (buff[1] == 'm' && buff[2] == '\0') {
	if (buff[0] == 'a') {
	    yylval.Meridian = MERam;
	    return tMERIDIAN;
	}
	if (buff[0] == 'p') {
	    yylval.Meridian = MERpm;
	    return tMERIDIAN;
	}
    }

    /* If we saw any periods, try the timezones again. */
    if (p - buff != length) {
	c = buff[0];
	for (p = buff, tp = TimezoneTable; tp < ENDOF(TimezoneTable); tp++)
	    if (c == tp->name[0] && p[1] == tp->name[1]
	    && strcmp(p, tp->name) == 0) {
		yylval.Number = tp->value;
		return tp->type;
	    }
    }

    /* Unknown word -- assume GMT timezone. */
    yylval.Number = 0;
    return tZONE;
}


int
date_lex(void)
{
    register char	c;
    register char	*p;
    char		buff[20];
    register int	sign;
    register int	i;
    register int	nesting;

    for ( ; ; ) {
	/* Get first character after the whitespace. */
	for ( ; ; ) {
	    while (isspace(*yyInput))
		yyInput++;
	    c = *yyInput;

	    /* Ignore RFC 822 comments, typically time zone names. */
	    if (c != LPAREN)
		break;
	    for (nesting = 1; (c = *++yyInput) != RPAREN || --nesting; )
		if (c == LPAREN)
		    nesting++;
		else if (!IS7BIT(c) || c == '\0' || c == '\r'
		     || (c == '\\' && ((c = *++yyInput) == '\0' || !IS7BIT(c))))
		    /* Lexical error: bad comment. */
		    return '?';
	    yyInput++;
	}

	/* A number? */
	if (isdigit(c) || c == '-' || c == '+') {
	    if (c == '-' || c == '+') {
		sign = c == '-' ? -1 : 1;
		yyInput++;
		if (!isdigit(*yyInput))
		    /* Skip the plus or minus sign. */
		    continue;
	    }
	    else
		sign = 0;
	    for (i = 0; (c = *yyInput++) != '\0' && isdigit(c); )
		i = 10 * i + c - '0';
	    yyInput--;
	    yylval.Number = sign < 0 ? -i : i;
	    return sign ? tSNUMBER : tUNUMBER;
	}

	/* A word? */
	if (isalpha(c)) {
	    for (p = buff; (c = *yyInput++) == '.' || isalpha(c); )
		if (p < &buff[sizeof buff - 1])
		    *p++ = isupper(c) ? tolower(c) : c;
	    *p = '\0';
	    yyInput--;
	    return LookupWord(buff, p - buff);
	}

	return *yyInput++;
    }
}


time_t
parsedate(char *p)
{
    extern int		date_parse(void);
    time_t		Start;

    yyInput = p;

    yyYear = 0;
    yyMonth = 0;
    yyDay = 0;
    yyTimezone = 0;
    yyDSTmode = DSTmaybe;
    yyHour = 0;
    yyMinutes = 0;
    yySeconds = 0;
    yyMeridian = MER24;
    yyRelSeconds = 0;
    yyRelMonth = 0;
    yyHaveDate = 0;
    yyHaveRel = 0;
    yyHaveTime = 0;

    if (date_parse() || yyHaveTime > 1 || yyHaveDate > 1)
	return -1;

    if (yyHaveDate || yyHaveTime) {
	Start = Convert(yyMonth, yyDay, yyYear, yyHour, yyMinutes, yySeconds,
		    yyMeridian, yyDSTmode);
	if (Start < 0)
	    return -1;
    }
    else
	return -1;

    Start += yyRelSeconds;
    if (yyRelMonth)
	Start += RelativeMonth(Start, yyRelMonth);

    /* Have to do *something* with a legitimate -1 so it's distinguishable
     * from the error return value.  (Alternately could set errno on error.) */
    return Start == -1 ? 0 : Start;
}


#ifdef TEST

#if YYDEBUG
extern int	yydebug;
#endif /* YYDEBUG */

/* ARGSUSED */
int
main(int ac, char *av[])
{
    char	buff[128];
    time_t	d;

#if YYDEBUG
    yydebug = 1;
#endif /* YYDEBUG */

    (void)printf("Enter date, or blank line to exit.\n\t> ");
    for ( ; ; ) {
	(void)printf("\t> ");
	(void)fflush(stdout);
	if (gets(buff) == NULL || buff[0] == '\n')
	    break;
#if YYDEBUG
	if (strcmp(buff, "yydebug") == 0) {
	    yydebug = !yydebug;
	    printf("yydebug = %s\n", yydebug ? "on" : "off");
	    continue;
	}
#endif /* YYDEBUG */
	d = parsedate(buff, (TIMEINFO *)NULL);
	if (d == -1)
	    (void)printf("Bad format - couldn't convert.\n");
	else
	    (void)printf("%s", ctime(&d));
    }

    exit(0);
    /* NOTREACHED */
}
#endif /* TEST */
static const yytabelem yyexca[] ={
-1, 1,
	0, -1,
	-2, 0,
	};
# define YYNPROD 30
# define YYLAST 216
static const yytabelem yyact[]={

    17,    12,    34,    40,    39,    21,    14,     8,    11,    14,
     7,    16,    38,     9,     6,    36,    31,    29,    28,    27,
    22,    14,    25,    24,    37,    35,    30,    23,    13,    15,
     5,     4,     3,     2,     1,    10,     0,     0,     0,     0,
    26,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,    33,    32,     0,     0,
     0,     0,     0,     0,     0,     0,     0,    42,    41,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
     0,     0,     0,    21,     0,     0,     0,    14,     0,     0,
     0,     0,    21,    18,    20,    19 };
static const yytabelem yypact[]={

-10000000,  -250,-10000000,  -257,-10000000,-10000000,   -47,  -244,   -17,  -239,
-10000000,  -242,-10000000,-10000000,-10000000,-10000000,  -245,  -246,  -247,-10000000,
-10000000,-10000000,   -18,  -248,-10000000,-10000000,-10000000,   -56,   -22,-10000000,
  -249,  -236,-10000000,-10000000,  -252,  -260,-10000000,  -261,  -254,-10000000,
-10000000,-10000000,-10000000 };
static const yytabelem yypgo[]={

     0,    28,    35,    29,    34,    33,    32,    31,    30 };
static const yytabelem yyr1[]={

     0,     4,     4,     5,     5,     5,     5,     6,     6,     6,
     6,     6,     2,     2,     2,     2,     1,     7,     7,     7,
     7,     7,     7,     7,     8,     8,     8,     8,     3,     3 };
static const yytabelem yyr2[]={

     0,     0,     4,     3,     5,     3,     3,     5,     9,     9,
    13,    13,     3,     3,     5,     3,     3,     7,    11,     5,
     9,     5,     7,    11,     5,     5,     5,     5,     1,     3 };
static const yytabelem yychk[]={

-10000000,    -4,    -5,    -6,    -7,    -8,   264,   260,   257,   263,
    -2,   265,   258,    -1,   263,    -3,    58,    47,   260,   262,
   261,   259,   264,    44,   262,   261,    -1,   264,   264,   264,
    44,   264,    -3,    -1,    58,    47,   264,   260,   264,   264,
   264,    -3,    -1 };
static const yytabelem yydef[]={

     1,    -2,     2,     3,     5,     6,    28,     0,     0,     0,
     4,    12,    13,    15,    16,     7,     0,     0,    21,    25,
    27,    29,    19,     0,    24,    26,    14,    28,    17,    22,
     0,     0,     8,     9,     0,     0,    20,     0,    28,    18,
    23,    10,    11 };
typedef struct
#ifdef __cplusplus
	yytoktype
#endif
{ char *t_name; int t_val; } yytoktype;
#ifndef YYDEBUG
#	define YYDEBUG	0	/* don't allow debugging */
#endif

#if YYDEBUG

yytoktype yytoks[] =
{
	"tDAY",	257,
	"tDAYZONE",	258,
	"tMERIDIAN",	259,
	"tMONTH",	260,
	"tMONTH_UNIT",	261,
	"tSEC_UNIT",	262,
	"tSNUMBER",	263,
	"tUNUMBER",	264,
	"tZONE",	265,
	"-unknown-",	-1	/* ends search */
};

char * yyreds[] =
{
	"-no such reduction-",
	"spec : /* empty */",
	"spec : spec item",
	"item : time",
	"item : time zone",
	"item : date",
	"item : rel",
	"time : tUNUMBER o_merid",
	"time : tUNUMBER ':' tUNUMBER o_merid",
	"time : tUNUMBER ':' tUNUMBER numzone",
	"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER o_merid",
	"time : tUNUMBER ':' tUNUMBER ':' tUNUMBER numzone",
	"zone : tZONE",
	"zone : tDAYZONE",
	"zone : tZONE numzone",
	"zone : numzone",
	"numzone : tSNUMBER",
	"date : tUNUMBER '/' tUNUMBER",
	"date : tUNUMBER '/' tUNUMBER '/' tUNUMBER",
	"date : tMONTH tUNUMBER",
	"date : tMONTH tUNUMBER ',' tUNUMBER",
	"date : tUNUMBER tMONTH",
	"date : tUNUMBER tMONTH tUNUMBER",
	"date : tDAY ',' tUNUMBER tMONTH tUNUMBER",
	"rel : tSNUMBER tSEC_UNIT",
	"rel : tUNUMBER tSEC_UNIT",
	"rel : tSNUMBER tMONTH_UNIT",
	"rel : tUNUMBER tMONTH_UNIT",
	"o_merid : /* empty */",
	"o_merid : tMERIDIAN",
};
#endif /* YYDEBUG */
# line	1 "/usr/ccs/bin/yaccpar"
/*
 * Copyright (c) 1993 by Sun Microsystems, Inc.
 */

#pragma ident	"@(#)yaccpar	6.15	97/12/08 SMI"

/*
** Skeleton parser driver for yacc output
*/

/*
** yacc user known macros and defines
*/
#define YYERROR		goto yyerrlab
#define YYACCEPT	return(0)
#define YYABORT		return(1)
#define YYBACKUP( newtoken, newvalue )\
{\
	if ( yychar >= 0 || ( yyr2[ yytmp ] >> 1 ) != 1 )\
	{\
		yyerror( "syntax error - cannot backup" );\
		goto yyerrlab;\
	}\
	yychar = newtoken;\
	yystate = *yyps;\
	yylval = newvalue;\
	goto yynewstate;\
}
#define YYRECOVERING()	(!!yyerrflag)
#define YYNEW(type)	malloc(sizeof(type) * yynewmax)
#define YYCOPY(to, from, type) \
	(type *) memcpy(to, (char *) from, yymaxdepth * sizeof (type))
#define YYENLARGE( from, type) \
	(type *) realloc((char *) from, yynewmax * sizeof(type))
#ifndef YYDEBUG
#	define YYDEBUG	1	/* make debugging available */
#endif

/*
** user known globals
*/
int yydebug;			/* set to 1 to get debugging */

/*
** driver internal defines
*/
#define YYFLAG		(-10000000)

/*
** global variables used by the parser
*/
YYSTYPE *yypv;			/* top of value stack */
int *yyps;			/* top of state stack */

int yystate;			/* current state */
int yytmp;			/* extra var (lasts between blocks) */

int yynerrs;			/* number of errors */
int yyerrflag;			/* error recovery flag */
int yychar;			/* current input token number */



#ifdef YYNMBCHARS
#define YYLEX()		yycvtok(yylex())
/*
** yycvtok - return a token if i is a wchar_t value that exceeds 255.
**	If i<255, i itself is the token.  If i>255 but the neither 
**	of the 30th or 31st bit is on, i is already a token.
*/
#if defined(__STDC__) || defined(__cplusplus)
int yycvtok(int i)
#else
int yycvtok(i) int i;
#endif
{
	int first = 0;
	int last = YYNMBCHARS - 1;
	int mid;
	wchar_t j;

	if(i&0x60000000){/*Must convert to a token. */
		if( yymbchars[last].character < i ){
			return i;/*Giving up*/
		}
		while ((last>=first)&&(first>=0)) {/*Binary search loop*/
			mid = (first+last)/2;
			j = yymbchars[mid].character;
			if( j==i ){/*Found*/ 
				return yymbchars[mid].tvalue;
			}else if( j<i ){
				first = mid + 1;
			}else{
				last = mid -1;
			}
		}
		/*No entry in the table.*/
		return i;/* Giving up.*/
	}else{/* i is already a token. */
		return i;
	}
}
#else/*!YYNMBCHARS*/
#define YYLEX()		yylex()
#endif/*!YYNMBCHARS*/

/*
** yyparse - return 0 if worked, 1 if syntax error not recovered from
*/
#if defined(__STDC__) || defined(__cplusplus)
int yyparse(void)
#else
int yyparse()
#endif
{
	register YYSTYPE *yypvt = 0;	/* top of value stack for $vars */

#if defined(__cplusplus) || defined(lint)
/*
	hacks to please C++ and lint - goto's inside
	switch should never be executed
*/
	static int __yaccpar_lint_hack__ = 0;
	switch (__yaccpar_lint_hack__)
	{
		case 1: goto yyerrlab;
		case 2: goto yynewstate;
	}
#endif

	/*
	** Initialize externals - yyparse may be called more than once
	*/
	yypv = &yyv[-1];
	yyps = &yys[-1];
	yystate = 0;
	yytmp = 0;
	yynerrs = 0;
	yyerrflag = 0;
	yychar = -1;

#if YYMAXDEPTH <= 0
	if (yymaxdepth <= 0)
	{
		if ((yymaxdepth = YYEXPAND(0)) <= 0)
		{
			yyerror("yacc initialization error");
			YYABORT;
		}
	}
#endif

	{
		register YYSTYPE *yy_pv;	/* top of value stack */
		register int *yy_ps;		/* top of state stack */
		register int yy_state;		/* current state */
		register int  yy_n;		/* internal state number info */
	goto yystack;	/* moved from 6 lines above to here to please C++ */

		/*
		** get globals into registers.
		** branch to here only if YYBACKUP was called.
		*/
	yynewstate:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;
		goto yy_newstate;

		/*
		** get globals into registers.
		** either we just started, or we just finished a reduction
		*/
	yystack:
		yy_pv = yypv;
		yy_ps = yyps;
		yy_state = yystate;

		/*
		** top of for (;;) loop while no reductions done
		*/
	yy_stack:
		/*
		** put a state and value onto the stacks
		*/
#if YYDEBUG
		/*
		** if debugging, look up token value in list of value vs.
		** name pairs.  0 and negative (-1) are special values.
		** Note: linear search is used since time is not a real
		** consideration while debugging.
		*/
		if ( yydebug )
		{
			register int yy_i;

			printf( "State %d, token ", yy_state );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ++yy_ps >= &yys[ yymaxdepth ] )	/* room on stack? */
		{
			/*
			** reallocate and recover.  Note that pointers
			** have to be reset, or bad things will happen
			*/
			long yyps_index = (yy_ps - yys);
			long yypv_index = (yy_pv - yyv);
			long yypvt_index = (yypvt - yyv);
			int yynewmax;
#ifdef YYEXPAND
			yynewmax = YYEXPAND(yymaxdepth);
#else
			yynewmax = 2 * yymaxdepth;	/* double table size */
			if (yymaxdepth == YYMAXDEPTH)	/* first time growth */
			{
				char *newyys = (char *)YYNEW(int);
				char *newyyv = (char *)YYNEW(YYSTYPE);
				if (newyys != 0 && newyyv != 0)
				{
					yys = YYCOPY(newyys, yys, int);
					yyv = YYCOPY(newyyv, yyv, YYSTYPE);
				}
				else
					yynewmax = 0;	/* failed */
			}
			else				/* not first time */
			{
				yys = YYENLARGE(yys, int);
				yyv = YYENLARGE(yyv, YYSTYPE);
				if (yys == 0 || yyv == 0)
					yynewmax = 0;	/* failed */
			}
#endif
			if (yynewmax <= yymaxdepth)	/* tables not expanded */
			{
				yyerror( "yacc stack overflow" );
				YYABORT;
			}
			yymaxdepth = yynewmax;

			yy_ps = yys + yyps_index;
			yy_pv = yyv + yypv_index;
			yypvt = yyv + yypvt_index;
		}
		*yy_ps = yy_state;
		*++yy_pv = yyval;

		/*
		** we have a new state - find out what to do
		*/
	yy_newstate:
		if ( ( yy_n = yypact[ yy_state ] ) <= YYFLAG )
			goto yydefault;		/* simple state */
#if YYDEBUG
		/*
		** if debugging, need to mark whether new token grabbed
		*/
		yytmp = yychar < 0;
#endif
		if ( ( yychar < 0 ) && ( ( yychar = YYLEX() ) < 0 ) )
			yychar = 0;		/* reached EOF */
#if YYDEBUG
		if ( yydebug && yytmp )
		{
			register int yy_i;

			printf( "Received token " );
			if ( yychar == 0 )
				printf( "end-of-file\n" );
			else if ( yychar < 0 )
				printf( "-none-\n" );
			else
			{
				for ( yy_i = 0; yytoks[yy_i].t_val >= 0;
					yy_i++ )
				{
					if ( yytoks[yy_i].t_val == yychar )
						break;
				}
				printf( "%s\n", yytoks[yy_i].t_name );
			}
		}
#endif /* YYDEBUG */
		if ( ( ( yy_n += yychar ) < 0 ) || ( yy_n >= YYLAST ) )
			goto yydefault;
		if ( yychk[ yy_n = yyact[ yy_n ] ] == yychar )	/*valid shift*/
		{
			yychar = -1;
			yyval = yylval;
			yy_state = yy_n;
			if ( yyerrflag > 0 )
				yyerrflag--;
			goto yy_stack;
		}

	yydefault:
		if ( ( yy_n = yydef[ yy_state ] ) == -2 )
		{
#if YYDEBUG
			yytmp = yychar < 0;
#endif
			if ( ( yychar < 0 ) && ( ( yychar = YYLEX() ) < 0 ) )
				yychar = 0;		/* reached EOF */
#if YYDEBUG
			if ( yydebug && yytmp )
			{
				register int yy_i;

				printf( "Received token " );
				if ( yychar == 0 )
					printf( "end-of-file\n" );
				else if ( yychar < 0 )
					printf( "-none-\n" );
				else
				{
					for ( yy_i = 0;
						yytoks[yy_i].t_val >= 0;
						yy_i++ )
					{
						if ( yytoks[yy_i].t_val
							== yychar )
						{
							break;
						}
					}
					printf( "%s\n", yytoks[yy_i].t_name );
				}
			}
#endif /* YYDEBUG */
			/*
			** look through exception table
			*/
			{
				register const int *yyxi = yyexca;

				while ( ( *yyxi != -1 ) ||
					( yyxi[1] != yy_state ) )
				{
					yyxi += 2;
				}
				while ( ( *(yyxi += 2) >= 0 ) &&
					( *yyxi != yychar ) )
					;
				if ( ( yy_n = yyxi[1] ) < 0 )
					YYACCEPT;
			}
		}

		/*
		** check for syntax error
		*/
		if ( yy_n == 0 )	/* have an error */
		{
			/* no worry about speed here! */
			switch ( yyerrflag )
			{
			case 0:		/* new error */
				yyerror( "syntax error" );
				goto skip_init;
			yyerrlab:
				/*
				** get globals into registers.
				** we have a user generated syntax type error
				*/
				yy_pv = yypv;
				yy_ps = yyps;
				yy_state = yystate;
			skip_init:
				yynerrs++;
				/* FALLTHRU */
			case 1:
			case 2:		/* incompletely recovered error */
					/* try again... */
				yyerrflag = 3;
				/*
				** find state where "error" is a legal
				** shift action
				*/
				while ( yy_ps >= yys )
				{
					yy_n = yypact[ *yy_ps ] + YYERRCODE;
					if ( yy_n >= 0 && yy_n < YYLAST &&
						yychk[yyact[yy_n]] == YYERRCODE)					{
						/*
						** simulate shift of "error"
						*/
						yy_state = yyact[ yy_n ];
						goto yy_stack;
					}
					/*
					** current state has no shift on
					** "error", pop stack
					*/
#if YYDEBUG
#	define _POP_ "Error recovery pops state %d, uncovers state %d\n"
					if ( yydebug )
						printf( _POP_, *yy_ps,
							yy_ps[-1] );
#	undef _POP_
#endif
					yy_ps--;
					yy_pv--;
				}
				/*
				** there is no state on stack with "error" as
				** a valid shift.  give up.
				*/
				YYABORT;
			case 3:		/* no shift yet; eat a token */
#if YYDEBUG
				/*
				** if debugging, look up token in list of
				** pairs.  0 and negative shouldn't occur,
				** but since timing doesn't matter when
				** debugging, it doesn't hurt to leave the
				** tests here.
				*/
				if ( yydebug )
				{
					register int yy_i;

					printf( "Error recovery discards " );
					if ( yychar == 0 )
						printf( "token end-of-file\n" );
					else if ( yychar < 0 )
						printf( "token -none-\n" );
					else
					{
						for ( yy_i = 0;
							yytoks[yy_i].t_val >= 0;
							yy_i++ )
						{
							if ( yytoks[yy_i].t_val
								== yychar )
							{
								break;
							}
						}
						printf( "token %s\n",
							yytoks[yy_i].t_name );
					}
				}
#endif /* YYDEBUG */
				if ( yychar == 0 )	/* reached EOF. quit */
					YYABORT;
				yychar = -1;
				goto yy_newstate;
			}
		}/* end if ( yy_n == 0 ) */
		/*
		** reduction by production yy_n
		** put stack tops, etc. so things right after switch
		*/
#if YYDEBUG
		/*
		** if debugging, print the string that is the user's
		** specification of the reduction which is just about
		** to be done.
		*/
		if ( yydebug )
			printf( "Reduce by (%d) \"%s\"\n",
				yy_n, yyreds[ yy_n ] );
#endif
		yytmp = yy_n;			/* value to switch over */
		yypvt = yy_pv;			/* $vars top of value stack */
		/*
		** Look in goto table for next state
		** Sorry about using yy_state here as temporary
		** register variable, but why not, if it works...
		** If yyr2[ yy_n ] doesn't have the low order bit
		** set, then there is no action to be done for
		** this reduction.  So, no saving & unsaving of
		** registers done.  The only difference between the
		** code just after the if and the body of the if is
		** the goto yy_stack in the body.  This way the test
		** can be made before the choice of what to do is needed.
		*/
		{
			/* length of production doubled with extra bit */
			register int yy_len = yyr2[ yy_n ];

			if ( !( yy_len & 01 ) )
			{
				yy_len >>= 1;
				yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
				yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
					*( yy_ps -= yy_len ) + 1;
				if ( yy_state >= YYLAST ||
					yychk[ yy_state =
					yyact[ yy_state ] ] != -yy_n )
				{
					yy_state = yyact[ yypgo[ yy_n ] ];
				}
				goto yy_stack;
			}
			yy_len >>= 1;
			yyval = ( yy_pv -= yy_len )[1];	/* $$ = $1 */
			yy_state = yypgo[ yy_n = yyr1[ yy_n ] ] +
				*( yy_ps -= yy_len ) + 1;
			if ( yy_state >= YYLAST ||
				yychk[ yy_state = yyact[ yy_state ] ] != -yy_n )
			{
				yy_state = yyact[ yypgo[ yy_n ] ];
			}
		}
					/* save until reenter driver code */
		yystate = yy_state;
		yyps = yy_ps;
		yypv = yy_pv;
	}
	/*
	** code supplied by user is placed in this switch
	*/
	switch( yytmp )
	{
		
case 3:
# line 118 "parsedate.y"
{
	    yyHaveTime++;
#ifdef lint
	    /* I am compulsive about lint natterings... */
	    if (yyHaveTime == -1) {
		YYERROR;
	    }
#endif /* lint */
	} break;
case 4:
# line 127 "parsedate.y"
{
	    yyHaveTime++;
	    yyTimezone = yypvt[-0].Number;
	} break;
case 5:
# line 131 "parsedate.y"
{
	    yyHaveDate++;
	} break;
case 6:
# line 134 "parsedate.y"
{
	    yyHaveRel = 1;
	} break;
case 7:
# line 139 "parsedate.y"
{
	    if (yypvt[-1].Number < 100) {
		yyHour = yypvt[-1].Number;
		yyMinutes = 0;
	    }
	    else {
		yyHour = yypvt[-1].Number / 100;
		yyMinutes = yypvt[-1].Number % 100;
	    }
	    yySeconds = 0;
	    yyMeridian = yypvt[-0].Meridian;
	} break;
case 8:
# line 151 "parsedate.y"
{
	    yyHour = yypvt[-3].Number;
	    yyMinutes = yypvt[-1].Number;
	    yySeconds = 0;
	    yyMeridian = yypvt[-0].Meridian;
	} break;
case 9:
# line 157 "parsedate.y"
{
	    yyHour = yypvt[-3].Number;
	    yyMinutes = yypvt[-1].Number;
	    yyTimezone = yypvt[-0].Number;
	    yyMeridian = MER24;
	    yyDSTmode = DSToff;
	} break;
case 10:
# line 164 "parsedate.y"
{
	    yyHour = yypvt[-5].Number;
	    yyMinutes = yypvt[-3].Number;
	    yySeconds = yypvt[-1].Number;
	    yyMeridian = yypvt[-0].Meridian;
	} break;
case 11:
# line 170 "parsedate.y"
{
	    yyHour = yypvt[-5].Number;
	    yyMinutes = yypvt[-3].Number;
	    yySeconds = yypvt[-1].Number;
	    yyTimezone = yypvt[-0].Number;
	    yyMeridian = MER24;
	    yyDSTmode = DSToff;
	} break;
case 12:
# line 180 "parsedate.y"
{
	    yyval.Number = yypvt[-0].Number;
	    yyDSTmode = DSToff;
	} break;
case 13:
# line 184 "parsedate.y"
{
	    yyval.Number = yypvt[-0].Number;
	    yyDSTmode = DSTon;
	} break;
case 14:
# line 188 "parsedate.y"
{
	    /* Only allow "GMT+300" and "GMT-0800" */
	    if (yypvt[-1].Number != 0) {
		YYABORT;
	    }
	    yyval.Number = yypvt[-0].Number;
	    yyDSTmode = DSToff;
	} break;
case 15:
# line 196 "parsedate.y"
{
	    yyval.Number = yypvt[-0].Number;
	    yyDSTmode = DSToff;
	} break;
case 16:
# line 202 "parsedate.y"
{
	    int		i;

	    /* Unix and GMT and numeric timezones -- a little confusing. */
	    if (yypvt[-0].Number < 0) {
		/* Don't work with negative modulus. */
		yypvt[-0].Number = -yypvt[-0].Number;
		if (yypvt[-0].Number > 9999 || (i = yypvt[-0].Number % 100) >= 60) {
		    YYABORT;
		}
		yyval.Number = (yypvt[-0].Number / 100) * 60 + i;
	    }
	    else {
		if (yypvt[-0].Number > 9999 || (i = yypvt[-0].Number % 100) >= 60) {
		    YYABORT;
		}
		yyval.Number = -((yypvt[-0].Number / 100) * 60 + i);
	    }
	} break;
case 17:
# line 223 "parsedate.y"
{
	    yyMonth = yypvt[-2].Number;
	    yyDay = yypvt[-0].Number;
	} break;
case 18:
# line 227 "parsedate.y"
{
	    if (yypvt[-4].Number > 100) {
		yyYear = yypvt[-4].Number;
		yyMonth = yypvt[-2].Number;
		yyDay = yypvt[-0].Number;
	    }
	    else {
		yyMonth = yypvt[-4].Number;
		yyDay = yypvt[-2].Number;
		yyYear = yypvt[-0].Number;
	    }
	} break;
case 19:
# line 239 "parsedate.y"
{
	    yyMonth = yypvt[-1].Number;
	    yyDay = yypvt[-0].Number;
	} break;
case 20:
# line 243 "parsedate.y"
{
	    yyMonth = yypvt[-3].Number;
	    yyDay = yypvt[-2].Number;
	    yyYear = yypvt[-0].Number;
	} break;
case 21:
# line 248 "parsedate.y"
{
	    yyDay = yypvt[-1].Number;
	    yyMonth = yypvt[-0].Number;
	} break;
case 22:
# line 252 "parsedate.y"
{
	    yyDay = yypvt[-2].Number;
	    yyMonth = yypvt[-1].Number;
	    yyYear = yypvt[-0].Number;
	} break;
case 23:
# line 257 "parsedate.y"
{
	    yyDay = yypvt[-2].Number;
	    yyMonth = yypvt[-1].Number;
	    yyYear = yypvt[-0].Number;
	} break;
case 24:
# line 264 "parsedate.y"
{
	    yyRelSeconds += yypvt[-1].Number * yypvt[-0].Number;
	} break;
case 25:
# line 267 "parsedate.y"
{
	    yyRelSeconds += yypvt[-1].Number * yypvt[-0].Number;
	} break;
case 26:
# line 270 "parsedate.y"
{
	    yyRelMonth += yypvt[-1].Number * yypvt[-0].Number;
	} break;
case 27:
# line 273 "parsedate.y"
{
	    yyRelMonth += yypvt[-1].Number * yypvt[-0].Number;
	} break;
case 28:
# line 278 "parsedate.y"
{
	    yyval.Meridian = MER24;
	} break;
case 29:
# line 281 "parsedate.y"
{
	    yyval.Meridian = yypvt[-0].Meridian;
	} break;
# line	531 "/usr/ccs/bin/yaccpar"
	}
	goto yystack;		/* reset registers in driver code */
}

