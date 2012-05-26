/*
 * A basic toolset containing miscellaneous functions for string manipluation,
 * encoding/decoding, and a bunch of other stuff.
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include "libcitadel.h"


#define TRUE  1
#define FALSE 0

typedef unsigned char byte;	      /* Byte type */

/* Base64 encoding table */
const byte etable[256] = {
	65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81,
	82, 83, 84, 85, 86, 87, 88, 89, 90, 97, 98, 99, 100, 101, 102, 103,
	104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117,
	118, 119, 120, 121, 122, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 43,
	47, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Base64 decoding table */
const byte dtable[256] = {
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 62, 128, 128, 128, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
	128, 128, 128, 0, 128, 128, 128, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
	12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 128, 128, 128,
	128, 128, 128, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	128, 128, 0
};

/*
 * copy a string into a buffer of a known size. abort if we exceed the limits
 *
 * dest	the targetbuffer
 * src	the source string
 * n	the size od dest
 *
 * returns the number of characters copied if dest is big enough, -n if not.
 */
int safestrncpy(char *dest, const char *src, size_t n)
{
	int i = 0;

	if (dest == NULL || src == NULL) {
		fprintf(stderr, "safestrncpy: NULL argument\n");
		abort();
	}

	do {
		dest[i] = src[i];
		if (dest[i] == 0) return i;
		++i;
	} while (i<n);
	dest[n - 1] = 0;
	return -i;
}



/*
 * num_tokens()  -  discover number of parameters/tokens in a string
 */
int num_tokens(const char *source, char tok)
{
	int count = 1;
	const char *ptr = source;

	if (source == NULL) {
		return (0);
	}

	while (*ptr != '\0') {
		if (*ptr++ == tok) {
			++count;
		}
	}
	
	return (count);
}

//extern void cit_backtrace(void);


/*
 * extract_token() - a string tokenizer
 * returns -1 if not found, or length of token.
 */
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen)
{
	const char *s;			//* source * /
	int len = 0;			//* running total length of extracted string * /
	int current_token = 0;		//* token currently being processed * /

	s = source;

	if (dest == NULL) {
		return(-1);
	}

	//cit_backtrace();
	//lprintf (CTDL_DEBUG, "test >: n: %d sep: %c source: %s \n willi \n", parmnum, separator, source);
	dest[0] = 0;

	if (s == NULL) {
		return(-1);
	}
	
	maxlen--;

	while (*s) {
		if (*s == separator) {
			++current_token;
		}
		if ( (current_token == parmnum) && 
		     (*s != separator) && 
		     (len < maxlen) ) {
			dest[len] = *s;
			++len;
		}
		else if ((current_token > parmnum) || (len >= maxlen)) {
			break;
		}
		++s;
	}

	dest[len] = '\0';
	if (current_token < parmnum) {
		//lprintf (CTDL_DEBUG,"test <!: %s\n", dest);
		return(-1);
	}
	//lprintf (CTDL_DEBUG,"test <: %d; %s\n", len, dest);
	return(len);
}
//*/


/*
 * extract_token() - a string tokenizer
 * /
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen)
{
	char *d;		// dest
	const char *s;		// source
	int count = 0;
	int len = 0;

	
	//cit_backtrace();
	//lprintf (CTDL_DEBUG, "test >: n: %d sep: %c source: %s \n willi \n", parmnum, separator, source);
	strcpy(dest, "");

	//  Locate desired parameter 
	s = source;
	while (count < parmnum) {
		//  End of string, bail!
		if (!*s) {
			s = NULL;
			break;
		}
		if (*s == separator) {
			count++;
		}
		s++;
	}
	if (!s) {
		//lprintf (CTDL_DEBUG,"test <!: %s\n", dest);
		return -1;		// Parameter not found
	}
	
	for (d = dest; *s && *s != separator && ++len<maxlen; s++, d++) {
		*d = *s;
	}
	*d = 0;
	//lprintf (CTDL_DEBUG,"test <: %d; %s\n", len, dest);
	return 0;
}
*/


/*
 * remove_token() - a tokenizer that kills, maims, and destroys
 */
void remove_token(char *source, int parmnum, char separator)
{
	char *d, *s;		/* dest, source */
	int count = 0;

	/* Find desired parameter */
	d = source;
	while (count < parmnum) {
		/* End of string, bail! */
		if (!*d) {
			d = NULL;
			break;
		}
		if (*d == separator) {
			count++;
		}
		d++;
	}
	if (!d) return;		/* Parameter not found */

	/* Find next parameter */
	s = d;
	while (*s && *s != separator) {
		s++;
	}

	/* Hack and slash */
	if (*s)
		strcpy(d, ++s);
	else if (d == source)
		*d = 0;
	else
		*--d = 0;
	/*
	while (*s) {
		*d++ = *s++;
	}
	*d = 0;
	*/
}


/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(const char *source, int parmnum)
{
	char buf[32];
	
	if (extract_token(buf, source, parmnum, '|', sizeof buf) > 0)
		return(atoi(buf));
	else
		return 0;
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(const char *source, int parmnum)
{
	char buf[32];
	
	if (extract_token(buf, source, parmnum, '|', sizeof buf) > 0)
		return(atol(buf));
	else
		return 0;
}


/*
 * extract_unsigned_long() - extract an unsigned long parm
 */
unsigned long extract_unsigned_long(const char *source, int parmnum)
{
	char buf[32];

	if (extract_token(buf, source, parmnum, '|', sizeof buf) > 0)
		return strtoul(buf, NULL, 10);
	else 
		return 0;
}


/*
 * CtdlDecodeBase64() and CtdlEncodeBase64() are adaptations of code by John Walker.
 */

size_t CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen, int linebreaks)
{
	int i, hiteof = FALSE;
	int spos = 0;
	int dpos = 0;
	int thisline = 0;

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
			ogroup[0] = etable[igroup[0] >> 2];
			ogroup[1] =
			    etable[((igroup[0] & 3) << 4) |
				   (igroup[1] >> 4)];
			ogroup[2] =
			    etable[((igroup[1] & 0xF) << 2) |
				   (igroup[2] >> 6)];
			ogroup[3] = etable[igroup[2] & 0x3F];

			/*
			 * Replace characters in output stream with "=" pad
			 * characters if fewer than three characters were
			 * read from the end of the input stream. 
			 */

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
			if ( (linebreaks) && (thisline > 70) ) {
				dest[dpos++] = '\r';
				dest[dpos++] = '\n';
				dest[dpos] = 0;
				thisline = 0;
			}
		}
	}
	if ( (linebreaks) && (thisline > 70) ) {
		dest[dpos++] = '\r';
		dest[dpos++] = '\n';
		dest[dpos] = 0;
	}

	return(dpos);
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

    while (TRUE) {
	byte a[4], b[4], o[3];

	for (i = 0; i < 4; i++) {
	    if (spos >= length) {
		return(dpos);
	    }
	    c = source[spos++];

	    if (c == 0) {
		if (i > 0) {
		    return(dpos);
		}
		return(dpos);
	    }
	    if (dtable[c] & 0x80) {
		/* Ignoring errors: discard invalid character. */
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
	if (i>=1) dest[dpos++] = o[0];
	if (i>=2) dest[dpos++] = o[1];
	if (i>=3) dest[dpos++] = o[2];
	dest[dpos] = 0;
	if (i < 3) {
	    return(dpos);
	}
    }
}


/*
 * if we send out non ascii subjects, we encode it this way.
 */
char *rfc2047encode(char *line, long length)
{
	char *AlreadyEncoded;
	char *result;
	long end;
#define UTF8_HEADER "=?UTF-8?B?"

	/* check if we're already done */
	AlreadyEncoded = strstr(line, "=?");
	if ((AlreadyEncoded != NULL) &&
	    ((strstr(AlreadyEncoded, "?B?") != NULL)||
	     (strstr(AlreadyEncoded, "?Q?") != NULL)))
	{
		return strdup(line);
	}

	result = (char*) malloc(sizeof(UTF8_HEADER) + 4 + length * 2);
	strncpy (result, UTF8_HEADER, strlen (UTF8_HEADER));
	CtdlEncodeBase64(result + strlen(UTF8_HEADER), line, length, 0);
	end = strlen (result);
        result[end]='?';
	result[end+1]='=';
	result[end+2]='\0';
	return result;
}

/*
 * removes double slashes from pathnames
 * allows / disallows trailing slashes
 */
void StripSlashes(char *Dir, int TrailingSlash)
{
	char *a, *b;

	a = b = Dir;

	while (!IsEmptyStr(a)) {
		if (*a == '/') {
			while (*a == '/')
				a++;
			*b = '/';
			b++;
		}
		else {
			*b = *a;
			b++; a++;
		}
	}
	if ((TrailingSlash) && (*(b - 1) != '/')){
		*b = '/';
		b++;
	}
	*b = '\0';

}

/*
 * Strip leading and trailing spaces from a string
 */
size_t striplt(char *buf) {
	char *first_nonspace = NULL;
	char *last_nonspace = NULL;
	char *ptr;
	size_t new_len = 0;

	if ((buf == NULL) || (*buf == '\0')) {
		return 0;
	}

	for (ptr=buf; *ptr!=0; ++ptr) {
		if (!isspace(*ptr)) {
			if (!first_nonspace) {
				first_nonspace = ptr;
			}
			last_nonspace = ptr;
		}
	}

	if ((!first_nonspace) || (!last_nonspace)) {
		buf[0] = 0;
		return 0;
	}

	new_len = last_nonspace - first_nonspace + 1;
	memmove(buf, first_nonspace, new_len);
	buf[new_len] = 0;
	return new_len;
}


/**
 * \brief check for the presence of a character within a string (returns count)
 * \param st the string to examine
 * \param ch the char to search
 * \return the number of times ch appears in st
 */
int haschar(const char *st, int ch)
{
	const char *ptr;
	int b;
	b = 0;
	ptr = st;
	while (!IsEmptyStr(ptr))
	{
		if (*ptr == ch)
			++b;
		ptr ++;
	}
	return (b);
}





/*
 * Format a date/time stamp for output 
 * seconds is whether to print the seconds
 */
void fmt_date(char *buf, size_t n, time_t thetime, int seconds) {
	struct tm tm;
	int hour;

	/* Month strings for date conversions ... this needs to be localized eventually */
	char *fmt_date_months[12] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	strcpy(buf, "");
	localtime_r(&thetime, &tm);

	hour = tm.tm_hour;
	if (hour == 0)	hour = 12;
	else if (hour > 12) hour = hour - 12;

	if (seconds) {
		snprintf(buf, n, "%s %d %4d %d:%02d:%02d%s",
			fmt_date_months[tm.tm_mon],
			tm.tm_mday,
			tm.tm_year + 1900,
			hour,
			tm.tm_min,
			tm.tm_sec,
			( (tm.tm_hour >= 12) ? "pm" : "am" )
		);
	} else {
		snprintf(buf, n, "%s %d %4d %d:%02d%s",
			fmt_date_months[tm.tm_mon],
			tm.tm_mday,
			tm.tm_year + 1900,
			hour,
			tm.tm_min,
			( (tm.tm_hour >= 12) ? "pm" : "am" )
		);
	}
}



/*
 * Determine whether the specified message number is contained within the
 * specified sequence set.
 */
int is_msg_in_sequence_set(const char *mset, long msgnum) {
	int num_sets;
	int s;
	char setstr[128], lostr[128], histr[128];
	long lo, hi;

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

/** 
 * \brief Utility function to "readline" from memory
 * \param start Location in memory from which we are reading.
 * \param buf the buffer to place the string in.
 * \param maxlen Size of string buffer
 * \return Pointer to the source memory right after we stopped reading.
 */
char *memreadline(char *start, char *buf, int maxlen)
{
	char ch;
	char *ptr;
	int len = 0;		/**< tally our own length to avoid strlen() delays */

	ptr = start;

	while (1) {
		ch = *ptr++;
		if ((len + 1 < (maxlen)) && (ch != 13) && (ch != 10)) {
			buf[len++] = ch;
		}
		if ((ch == 10) || (ch == 0)) {
			buf[len] = 0;
			return ptr;
		}
	}
}


/** 
 * \brief Utility function to "readline" from memory
 * \param start Location in memory from which we are reading.
 * \param buf the buffer to place the string in.
 * \param maxlen Size of string buffer
 * \param retlen the length of the returned string
 * \return Pointer to the source memory right after we stopped reading.
 */
char *memreadlinelen(char *start, char *buf, int maxlen, int *retlen)
{
	char ch;
	char *ptr;
	int len = 0;		/**< tally our own length to avoid strlen() delays */

	ptr = start;

	while (1) {
		ch = *ptr++;
		if ((len + 1 < (maxlen)) && (ch != 13) && (ch != 10)) {
			buf[len++] = ch;
		}
		if ((ch == 10) || (ch == 0)) {
			buf[len] = 0;
			*retlen = len;
			return ptr;
		}
	}
}


/** 
 * \brief Utility function to "readline" from memory
 * \param start Location in memory from which we are reading.
 * \param buf the buffer to place the string in.
 * \param maxlen Size of string buffer
 * \return Pointer to the source memory right after we stopped reading.
 */
const char *cmemreadline(const char *start, char *buf, int maxlen)
{
	char ch;
	const char *ptr;
	int len = 0;		/**< tally our own length to avoid strlen() delays */

	ptr = start;

	while (1) {
		ch = *ptr++;
		if ((len + 1 < (maxlen)) && (ch != 13) && (ch != 10)) {
			buf[len++] = ch;
		}
		if ((ch == 10) || (ch == 0)) {
			buf[len] = 0;
			return ptr;
		}
	}
}


/** 
 * \brief Utility function to "readline" from memory
 * \param start Location in memory from which we are reading.
 * \param buf the buffer to place the string in.
 * \param maxlen Size of string buffer
 * \param retlen the length of the returned string
 * \return Pointer to the source memory right after we stopped reading.
 */
const char *cmemreadlinelen(const char *start, char *buf, int maxlen, int *retlen)
{
	char ch;
	const char *ptr;
	int len = 0;		/**< tally our own length to avoid strlen() delays */

	ptr = start;

	while (1) {
		ch = *ptr++;
		if ((len + 1 < (maxlen)) && (ch != 13) && (ch != 10)) {
			buf[len++] = ch;
		}
		if ((ch == 10) || (ch == 0)) {
			buf[len] = 0;
			*retlen = len;
			return ptr;
		}
	}
}




/*
 * Strip a boundarized substring out of a string (for example, remove
 * parentheses and anything inside them).
 */
int stripout(char *str, char leftboundary, char rightboundary) {
	int a;
        int lb = (-1);
        int rb = (-1);

        for (a = 0; a < strlen(str); ++a) {
                if (str[a] == leftboundary) lb = a;
                if (str[a] == rightboundary) rb = a;
        }

        if ( (lb > 0) && (rb > lb) ) {
                strcpy(&str[lb - 1], &str[rb + 1]);
		return 1;
        }

        else if ( (lb == 0) && (rb > lb) ) {
                strcpy(str, &str[rb + 1]);
		return 1;
        }
	return 0;
}


/*
 * Reduce a string down to a boundarized substring (for example, remove
 * parentheses and anything outside them).
 */
long stripallbut(char *str, char leftboundary, char rightboundary) {
	long len = 0;

	char *lb = NULL;
	char *rb = NULL;

	lb = strrchr(str, leftboundary);
	if (lb != NULL) {
		++lb;
		rb = strchr(str, rightboundary);
		if ((rb != NULL) && (rb >= lb))  {
			*rb = 0;
			fflush(stderr);
			len = (long)rb - (long)lb;
			memmove(str, lb, len);
			str[len] = 0;
			return(len);
		}
	}

	return (long)strlen(str);
}


char *myfgets(char *s, int size, FILE *stream) {
	char *ret = fgets(s, size, stream);
	char *nl;

	if (ret != NULL) {
		nl = strchr(s, '\n');

		if (nl != NULL)
			*nl = 0;
	}

	return ret;
}

/** 
 * \brief Escape a string for feeding out as a URL.
 * \param outbuf the output buffer
 * \param oblen the size of outbuf to sanitize
 * \param strbuf the input buffer
 */
void urlesc(char *outbuf, size_t oblen, char *strbuf)
{
	int a, b, c, len, eclen, olen;
	char *ec = " +#&;`'|*?-~<>^()[]{}/$\"\\";

	strcpy(outbuf, "");
	len = strlen(strbuf);
	eclen = strlen(ec);
	olen = 0;
	for (a = 0; a < len; ++a) {
		c = 0;
		for (b = 0; b < eclen; ++b) {
			if (strbuf[a] == ec[b])
				c = 1;
		}
		if (c == 1) {
			snprintf(&outbuf[olen], oblen - olen, "%%%02x", strbuf[a]);
			olen += 3;
		}
		else 
			outbuf[olen ++] = strbuf[a];
	}
	outbuf[olen] = '\0';
}



/*
 * In our world, we want strcpy() to be able to work with overlapping strings.
 */
#ifdef strcpy
#undef strcpy
#endif
char *strcpy(char *dest, const char *src) {
	memmove(dest, src, (strlen(src) + 1) );
	return(dest);
}


/*
 * Generate a new, globally unique UID parameter for a calendar etc. object
 */
void generate_uuid(char *buf) {
	static int seq = (-1);
	static int no_kernel_uuid = 0;

	/* If we are running on Linux then we have a kernelspace uuid generator available */

	if (no_kernel_uuid == 0) {
		FILE *fp;
		fp = fopen("/proc/sys/kernel/random/uuid", "rb");
		if (fp) {
			int rv;
			rv = fread(buf, 36, 1, fp);
			fclose(fp);
			if (rv == 1) return;
		}
	}

	/* If the kernel didn't provide us with a uuid, we generate a pseudo-random one */

	no_kernel_uuid = 1;

	if (seq == (-1)) {
		seq = (int)rand();
	}
	++seq;
	seq = (seq % 0x0FFF) ;

	sprintf(buf, "%08lx-%04lx-4%03x-a%03x-%012lx",
		(long)time(NULL),
		(long)getpid(),
		seq,
		seq,
		(long)rand()
	);
}

/*
 * bmstrcasestr() -- case-insensitive substring search
 *
 * This uses the Boyer-Moore search algorithm and is therefore quite fast.
 * The code is roughly based on the strstr() replacement from 'tin' written
 * by Urs Jannsen.
 */
inline static char *_bmstrcasestr_len(char *text, size_t textlen, const char *pattern, size_t patlen) {

	register unsigned char *p, *t;
	register int i, j, *delta;
	register size_t p1;
	int deltaspace[256];

	if (!text) return(NULL);
	if (!pattern) return(NULL);

	/* algorithm fails if pattern is empty */
	if ((p1 = patlen) == 0)
		return (text);

	/* code below fails (whenever i is unsigned) if pattern too long */
	if (p1 > textlen)
		return (NULL);

	/* set up deltas */
	delta = deltaspace;
	for (i = 0; i <= 255; i++)
		delta[i] = p1;
	for (p = (unsigned char *) pattern, i = p1; --i > 0;)
		delta[tolower(*p++)] = i;

	/*
	 * From now on, we want patlen - 1.
	 * In the loop below, p points to the end of the pattern,
	 * t points to the end of the text to be tested against the
	 * pattern, and i counts the amount of text remaining, not
	 * including the part to be tested.
	 */
	p1--;
	p = (unsigned char *) pattern + p1;
	t = (unsigned char *) text + p1;
	i = textlen - patlen;
	while(1) {
		if (tolower(p[0]) == tolower(t[0])) {
			if (strncasecmp ((const char *)(p - p1), (const char *)(t - p1), p1) == 0) {
				return ((char *)t - p1);
			}
		}
		j = delta[tolower(t[0])];
		if (i < j)
			break;
		i -= j;
		t += j;
	}
	return (NULL);
}

/*
 * bmstrcasestr() -- case-insensitive substring search
 *
 * This uses the Boyer-Moore search algorithm and is therefore quite fast.
 * The code is roughly based on the strstr() replacement from 'tin' written
 * by Urs Jannsen.
 */
char *bmstrcasestr(char *text, const char *pattern) {
	size_t textlen;
	size_t patlen;

	if (!text) return(NULL);
	if (!pattern) return(NULL);

	textlen = strlen (text);
	patlen = strlen (pattern);

	return _bmstrcasestr_len(text, textlen, pattern, patlen);
}

char *bmstrcasestr_len(char *text, size_t textlen, const char *pattern, size_t patlen) {
	return _bmstrcasestr_len(text, textlen, pattern, patlen);
}




/*
 * bmstrcasestr() -- case-insensitive substring search
 *
 * This uses the Boyer-Moore search algorithm and is therefore quite fast.
 * The code is roughly based on the strstr() replacement from 'tin' written
 * by Urs Jannsen.
 */
inline static const char *_cbmstrcasestr_len(const char *text, size_t textlen, const char *pattern, size_t patlen) {

	register unsigned char *p, *t;
	register int i, j, *delta;
	register size_t p1;
	int deltaspace[256];

	if (!text) return(NULL);
	if (!pattern) return(NULL);

	/* algorithm fails if pattern is empty */
	if ((p1 = patlen) == 0)
		return (text);

	/* code below fails (whenever i is unsigned) if pattern too long */
	if (p1 > textlen)
		return (NULL);

	/* set up deltas */
	delta = deltaspace;
	for (i = 0; i <= 255; i++)
		delta[i] = p1;
	for (p = (unsigned char *) pattern, i = p1; --i > 0;)
		delta[tolower(*p++)] = i;

	/*
	 * From now on, we want patlen - 1.
	 * In the loop below, p points to the end of the pattern,
	 * t points to the end of the text to be tested against the
	 * pattern, and i counts the amount of text remaining, not
	 * including the part to be tested.
	 */
	p1--;
	p = (unsigned char *) pattern + p1;
	t = (unsigned char *) text + p1;
	i = textlen - patlen;
	while(1) {
		if (tolower(p[0]) == tolower(t[0])) {
			if (strncasecmp ((const char *)(p - p1), (const char *)(t - p1), p1) == 0) {
				return ((char *)t - p1);
			}
		}
		j = delta[tolower(t[0])];
		if (i < j)
			break;
		i -= j;
		t += j;
	}
	return (NULL);
}

/*
 * bmstrcasestr() -- case-insensitive substring search
 *
 * This uses the Boyer-Moore search algorithm and is therefore quite fast.
 * The code is roughly based on the strstr() replacement from 'tin' written
 * by Urs Jannsen.
 */
const char *cbmstrcasestr(const char *text, const char *pattern) {
	size_t textlen;
	size_t patlen;

	if (!text) return(NULL);
	if (!pattern) return(NULL);

	textlen = strlen (text);
	patlen = strlen (pattern);

	return _cbmstrcasestr_len(text, textlen, pattern, patlen);
}

const char *cbmstrcasestr_len(const char *text, size_t textlen, const char *pattern, size_t patlen) {
	return _cbmstrcasestr_len(text, textlen, pattern, patlen);
}

/*
 * Local replacement for controversial C library function that generates
 * names for temporary files.  Included to shut up compiler warnings.
 */
void CtdlMakeTempFileName(char *name, int len) {
	int i = 0;

	while (i++, i < 100) {
		snprintf(name, len, "/tmp/ctdl.%04lx.%04x",
			(long)getpid(),
			rand()
		);
		if (!access(name, F_OK)) {
			return;
		}
	}
}



/*
 * Determine whether the specified message number is contained within the specified set.
 * Returns nonzero if the specified message number is in the specified message set string.
 */
int is_msg_in_mset(const char *mset, long msgnum) {
	int num_sets;
	int s;
	char setstr[SIZ], lostr[SIZ], histr[SIZ];       /* was 1024 */
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
 * searches for a pattern within a search string
 * returns position in string
 */
int pattern2(char *search, char *patn)
{
	int a;
	int len, plen;
	len = strlen (search);
	plen = strlen (patn);
	for (a = 0; a < len; ++a) {
		if (!strncasecmp(&search[a], patn, plen))
			return (a);
	}
	return (-1);
}


/*
 * Strip leading and trailing spaces from a string; with premeasured and adjusted length.
 * buf - the string to modify
 * len - length of the string. 
 */
void stripltlen(char *buf, int *len)
{
	int delta = 0;
	if (*len == 0) return;
	while ((*len > delta) && (isspace(buf[delta]))){
		delta ++;
	}
	memmove (buf, &buf[delta], *len - delta + 1);
	(*len) -=delta;

	if (*len == 0) return;
	while (isspace(buf[(*len) - 1])){
		buf[--(*len)] = '\0';
	}
}


/*
 * Convert all whitespace characters in a supplied string to underscores
 */
void convert_spaces_to_underscores(char *str)
{
	int len;
	int i;

	if (!str) return;

	len = strlen(str);
	for (i=0; i<len; ++i) {
		if (isspace(str[i])) {
			str[i] = '_';
		}
	}
}


/*
 * check whether the provided string needs to be qp encoded or not
 */
int CheckEncode(const char *pch, long len, const char *pche)
{
	if (pche == NULL)
		pche = pch + len;
	while (pch < pche) {
		if (((unsigned char) *pch < 32) || 
		    ((unsigned char) *pch > 126)) {
			return 1;
		}
		pch++;
	}
	return 0;
}

