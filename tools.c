/*
 * $Id$
 *
 * Miscellaneous routines 
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
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
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

static byte dtable[256];	/* base64 encode / decode table */

char *safestrncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
		abort();
	}
	strncpy(dest, src, n);
	dest[n - 1] = 0;
	return dest;
}



/*
 * num_tokens()  -  discover number of parameters/tokens in a string
 */
int num_tokens(char *source, char tok)
{
	int a;
	int count = 1;

	if (source == NULL)
		return (0);
	for (a = 0; a < strlen(source); ++a) {
		if (source[a] == tok)
			++count;
	}
	return (count);
}

/*
 * extract_token() - a string tokenizer
 */
void extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen)
{
	char *d;		/* dest */
	const char *s;		/* source */
	int count = 0;
	int len = 0;

	dest[0] = 0;

	/* Locate desired parameter */
	s = source;
	while (count < parmnum) {
		/* End of string, bail! */
		if (!*s) {
			s = NULL;
			break;
		}
		if (*s == separator) {
			count++;
		}
		s++;
	}
	if (!s) return;		/* Parameter not found */

	for (d = dest; *s && *s != separator && ++len<maxlen; s++, d++) {
		*d = *s;
	}
	*d = 0;
}



/*
 * remove_token()  -  a tokenizer that kills, maims, and destroys
 */
void remove_token(char *source, int parmnum, char separator)
{
	int i;
	int len;
	int curr_parm;
	int start, end;

	len = 0;
	curr_parm = 0;
	start = (-1);
	end = (-1);

	if (strlen(source) == 0) {
		return;
	}

	for (i = 0; i < strlen(source); ++i) {
		if ((start < 0) && (curr_parm == parmnum)) {
			start = i;
		}

		if ((end < 0) && (curr_parm == (parmnum + 1))) {
			end = i;
		}

		if (source[i] == separator) {
			++curr_parm;
		}
	}

	if (end < 0)
		end = strlen(source);

	strcpy(&source[start], &source[end]);
}




/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(const char *source, int parmnum)
{
	char buf[32];
	
	extract_token(buf, source, parmnum, '|', sizeof buf);
	return(atoi(buf));
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(const char *source, int parmnum)
{
	char buf[32];
	
	extract_token(buf, source, parmnum, '|', sizeof buf);
	return(atol(buf));
}






/*
 * check for the presence of a character within a string (returns count)
 */
int haschar(st, ch)
char st[];
char ch;
{
	int a, b;
	b = 0;
	for (a = 0; a < strlen(st); ++a)
		if (st[a] == ch)
			++b;
	return (b);
}


/*
 * Format a date/time stamp for output 
 */
void fmt_date(char *buf, time_t thetime, int brief)
{
	struct tm tm;
	struct tm today_tm;
	time_t today_timet;
	int hour;

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
			sprintf(buf, "%2d:%02d%s",
				hour, tm.tm_min,
				((tm.tm_hour >= 12) ? "pm" : "am")
			);
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
		sprintf(buf, "%s %d %d %2d:%02d%s",
			ascmonths[tm.tm_mon],
			tm.tm_mday,
			tm.tm_year + 1900,
			hour, tm.tm_min, ((tm.tm_hour >= 12) ? "pm" : "am")
		);
	}
}



/*
 * Format TIME ONLY for output 
 */
void fmt_time(char *buf, time_t thetime)
{
	struct tm *tm;
	int hour;

	buf[0] = 0;
	tm = localtime(&thetime);
	hour = tm->tm_hour;
	if (hour == 0)
		hour = 12;
	else if (hour > 12)
		hour = hour - 12;

	sprintf(buf, "%d:%02d%s",
		hour, tm->tm_min, ((tm->tm_hour > 12) ? "pm" : "am")
	    );
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
 * Utility function to "readline" from memory
 * (returns new pointer)
 */
char *memreadline(char *start, char *buf, int maxlen)
{
	char ch;
	char *ptr;
	int len = 0;		/* tally our own length to avoid strlen() delays */

	ptr = start;
	memset(buf, 0, maxlen);

	while (1) {
		ch = *ptr++;
		if ((len < (maxlen - 1)) && (ch != 13) && (ch != 10)) {
			buf[strlen(buf) + 1] = 0;
			buf[strlen(buf)] = ch;
			++len;
		}
		if ((ch == 10) || (ch == 0)) {
			return ptr;
		}
	}
}



/*
 * pattern2()  -  searches for patn within search string, returns pos
 */
int pattern2(char *search, char *patn)
{
	int a;
	for (a = 0; a < strlen(search); ++a) {
		if (!strncasecmp(&search[a], patn, strlen(patn)))
			return (a);
	}
	return (-1);
}


/*
 * Strip leading and trailing spaces from a string
 */
void striplt(char *buf)
{
	if (strlen(buf) == 0) return;
	while ((strlen(buf) > 0) && (isspace(buf[0])))
		strcpy(buf, &buf[1]);
	if (strlen(buf) == 0) return;
	while (isspace(buf[strlen(buf) - 1]))
		buf[strlen(buf) - 1] = 0;
}


/*
 * Determine whether the specified message number is contained within the
 * specified set.
 */
int is_msg_in_mset(char *mset, long msgnum) {
	int num_sets;
	int s;
	char setstr[SIZ], lostr[SIZ], histr[SIZ];	/* was 1024 */
	long lo, hi;

	/*
	 * Now set it for all specified messages.
	 */
	num_sets = num_tokens(mset, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, mset, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		if ((msgnum >= lo) && (msgnum <= hi)) return(1);
	}

	return(0);
}



/*
 * Strip a boundarized substring out of a string (for example, remove
 * parentheses and anything inside them).
 *
 * This improved version can strip out *multiple* boundarized substrings.
 */
void stripout(char *str, char leftboundary, char rightboundary)
{
	int a;
	int lb = (-1);
	int rb = (-1);

	do {
		lb = (-1);
		rb = (-1);

		for (a = 0; a < strlen(str); ++a) {
			if (str[a] == leftboundary)
				lb = a;
			if (str[a] == rightboundary)
				rb = a;
		}

		if ((lb > 0) && (rb > lb)) {
			strcpy(&str[lb - 1], &str[rb + 1]);
		}

	} while ((lb > 0) && (rb > lb));

}



/*
 * Replacement for sleep() that uses select() in order to avoid SIGALRM
 */
void sleeeeeeeeeep(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}



/*
 * CtdlDecodeBase64() and CtdlEncodeBase64() are adaptations of code by
 * John Walker, copied over from the Citadel server.
 */

void CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen)
{
	int i, hiteof = FALSE;
	int spos = 0;
	int dpos = 0;
	int thisline = 0;

	/*  Fill dtable with character encodings.  */

	for (i = 0; i < 26; i++) {
		dtable[i] = 'A' + i;
		dtable[26 + i] = 'a' + i;
	}
	for (i = 0; i < 10; i++) {
		dtable[52 + i] = '0' + i;
	}
	dtable[62] = '+';
	dtable[63] = '/';

	while (!hiteof) {
		byte igroup[3], ogroup[4];
		int c, n;

		igroup[0] = igroup[1] = igroup[2] = 0;
		for (n = 0; n < 3; n++) {
			if (spos >= sourcelen) {
				hiteof = TRUE;
				break;
			}
			c = source[spos++];
			igroup[n] = (byte) c;
		}
		if (n > 0) {
			ogroup[0] = dtable[igroup[0] >> 2];
			ogroup[1] =
			    dtable[((igroup[0] & 3) << 4) |
				   (igroup[1] >> 4)];
			ogroup[2] =
			    dtable[((igroup[1] & 0xF) << 2) |
				   (igroup[2] >> 6)];
			ogroup[3] = dtable[igroup[2] & 0x3F];

			/* Replace characters in output stream with "=" pad
			   characters if fewer than three characters were
			   read from the end of the input stream. */

			if (n < 3) {
				ogroup[3] = '=';
				if (n < 2) {
					ogroup[2] = '=';
				}
			}
			for (i = 0; i < 4; i++) {
				dest[dpos++] = ogroup[i];
				dest[dpos] = 0;
			}
			thisline += 4;
			if (thisline > 70) {
				dest[dpos++] = '\r';
				dest[dpos++] = '\n';
				dest[dpos] = 0;
				thisline = 0;
			}
		}
	}
	if (thisline > 70) {
		dest[dpos++] = '\r';
		dest[dpos++] = '\n';
		dest[dpos] = 0;
		thisline = 0;
	}
}


/* 
 * Convert base64-encoded to binary.  Returns the length of the decoded data.
 * It will stop after reading 'length' bytes.
 */
int CtdlDecodeBase64(char *dest, const char *source, size_t length)
{
	int i, c;
	int dpos = 0;
	int spos = 0;

	for (i = 0; i < 255; i++) {
		dtable[i] = 0x80;
	}
	for (i = 'A'; i <= 'Z'; i++) {
		dtable[i] = 0 + (i - 'A');
	}
	for (i = 'a'; i <= 'z'; i++) {
		dtable[i] = 26 + (i - 'a');
	}
	for (i = '0'; i <= '9'; i++) {
		dtable[i] = 52 + (i - '0');
	}
	dtable['+'] = 62;
	dtable['/'] = 63;
	dtable['='] = 0;

	 /*CONSTANTCONDITION*/ while (TRUE) {
		byte a[4], b[4], o[3];

		for (i = 0; i < 4; i++) {
			if (spos >= length) {
				return (dpos);
			}
			c = source[spos++];

			if (c == 0) {
				if (i > 0) {
					return (dpos);
				}
				return (dpos);
			}
			if (dtable[c] & 0x80) {
				/* Ignoring errors: discard invalid character */
				i--;
				continue;
			}
			a[i] = (byte) c;
			b[i] = (byte) dtable[c];
		}
		o[0] = (b[0] << 2) | (b[1] >> 4);
		o[1] = (b[1] << 4) | (b[2] >> 2);
		o[2] = (b[2] << 6) | b[3];
		i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
		if (i >= 1)
			dest[dpos++] = o[0];
		if (i >= 2)
			dest[dpos++] = o[1];
		if (i >= 3)
			dest[dpos++] = o[2];
		dest[dpos] = 0;
		if (i < 3) {
			return (dpos);
		}
	}
}


/*
 * Generate a new, globally unique UID parameter for a calendar etc. object
 */
void generate_uuid(char *buf) {
	static int seq = 0;

	sprintf(buf, "{%08x-%04x-%04x-%04x-%012x}",
		(int)time(NULL),
		(seq++),
		getpid(),
		rand(),
		rand()
	);
}

