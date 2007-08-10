/*
 * $Id$
 *
 * Utility functions that are used by both the client and server.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>

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

#include "tools.h"
#include "citadel.h"
#include "sysdep_decls.h"

#define TRUE  1
#define FALSE 0

typedef unsigned char byte;	      /* Byte type */
static byte dtable[256];	      /* base64 encode / decode table */

/* Month strings for date conversions */
char *ascmonths[12] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *safestrncpy(char *dest, const char *src, size_t n)
{
	int i = 0;

	if (dest == NULL || src == NULL) {
		fprintf(stderr, "safestrncpy: NULL argument\n");
		abort();
	}

	do {
		dest[i] = src[i];
		if (dest[i] == 0) return(dest);
		++i;
	} while (i<n);
	dest[n - 1] = 0;
	return dest;
}



#ifndef HAVE_STRNCASECMP
int strncasecmp(char *lstr, char *rstr, int len)
{
	int pos = 0;
	char lc,rc;
	while (pos<len) {
		lc=tolower(lstr[pos]);
		rc=tolower(rstr[pos]);
		if ((lc==0)&&(rc==0)) return(0);
		if (lc<rc) return(-1);
		if (lc>rc) return(1);
		pos=pos+1;
	}
	return(0);
}
#endif



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


/*
 * extract_token() - a string tokenizer
 * returns -1 if not found, or length of token.
 */
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen)
{
	const char *s;			/* source */
	int len = 0;			/* running total length of extracted string */
	int current_token = 0;		/* token currently being processed */

	s = source;

	if (dest == NULL) {
		return(-1);
	}

	dest[0] = 0;

	if (s == NULL) {
		return(-1);
	}

	while (*s) {
		if (*s == separator) {
			++current_token;
		}
		if ( (current_token == parmnum) && (len < maxlen) ) {
			dest[len] = *s;
			++len;
		}
		else if ((current_token > parmnum) || (len >= maxlen)) {
			break;
		}
		++s;
	}

	dest[len] = 0;
	if (current_token < parmnum) return(-1);
	return(len);
}


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
 * extract_unsigned_long() - extract an unsigned long parm
 */
unsigned long extract_unsigned_long(const char *source, int parmnum)
{
	char buf[32];

	extract_token(buf, source, parmnum, '|', sizeof buf);
	return strtoul(buf, NULL, 10);
}


/*
 * CtdlDecodeBase64() and CtdlEncodeBase64() are adaptations of code by
 * John Walker, found in full in the file "base64.c" included with this
 * distribution.  We are moving in the direction of eventually discarding
 * the separate executables, and using the ones in our code exclusively.
 */

void CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen)
{
    int i, hiteof = FALSE;
    int spos = 0;
    int dpos = 0;

    /*	Fill dtable with character encodings.  */

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
	    ogroup[1] = dtable[((igroup[0] & 3) << 4) | (igroup[1] >> 4)];
	    ogroup[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
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
	}
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

    /*CONSTANTCONDITION*/
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

	result = (char*) malloc(strlen(UTF8_HEADER) + 4 + length * 2);
	strncpy (result, UTF8_HEADER, strlen (UTF8_HEADER));
	CtdlEncodeBase64(result + strlen(UTF8_HEADER), line, length);
	end = strlen (result);
        result[end]='?';
	result[end+1]='=';
	result[end+2]='\0';
	return result;
}


/*
 * Strip leading and trailing spaces from a string
 */
void striplt(char *buf)
{
	if (IsEmptyStr(buf)) return;
        while ((!IsEmptyStr(buf)) && (isspace(buf[0])))
                strcpy(buf, &buf[1]);
	if (IsEmptyStr(buf)) return;
        while ((!IsEmptyStr(buf)) && (isspace(buf[strlen(buf) - 1])))
                buf[strlen(buf) - 1] = 0;
}





/**
 * \brief check for the presence of a character within a string (returns count)
 * \param st the string to examine
 * \param ch the char to search
 * \return the position inside of st
 */
int haschar(const char *st,int ch)
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

	strcpy(buf, "");
	localtime_r(&thetime, &tm);

	hour = tm.tm_hour;
	if (hour == 0)	hour = 12;
	else if (hour > 12) hour = hour - 12;

	if (seconds) {
		snprintf(buf, n, "%s %d %4d %d:%02d:%02d%s",
			ascmonths[tm.tm_mon],
			tm.tm_mday,
			tm.tm_year + 1900,
			hour,
			tm.tm_min,
			tm.tm_sec,
			( (tm.tm_hour >= 12) ? "pm" : "am" )
		);
	} else {
		snprintf(buf, n, "%s %d %4d %d:%02d%s",
			ascmonths[tm.tm_mon],
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
int is_msg_in_sequence_set(char *mset, long msgnum) {
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




/*
 * Strip a boundarized substring out of a string (for example, remove
 * parentheses and anything inside them).
 */
void stripout(char *str, char leftboundary, char rightboundary) {
	int a;
        int lb = (-1);
        int rb = (-1);

        for (a = 0; a < strlen(str); ++a) {
                if (str[a] == leftboundary) lb = a;
                if (str[a] == rightboundary) rb = a;
        }

        if ( (lb > 0) && (rb > lb) ) {
                strcpy(&str[lb - 1], &str[rb + 1]);
        }

        else if ( (lb == 0) && (rb > lb) ) {
                strcpy(str, &str[rb + 1]);
        }

}


/*
 * Reduce a string down to a boundarized substring (for example, remove
 * parentheses and anything outside them).
 */
void stripallbut(char *str, char leftboundary, char rightboundary) {
	int a;

	for (a = 0; a < strlen(str); ++ a) {
		if (str[a] == leftboundary) strcpy(str, &str[a+1]);
	}

	for (a = 0; a < strlen(str); ++ a) {
		if (str[a] == rightboundary) str[a] = 0;
	}

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

/*
 * Escape a string for feeding out as a URL.
 * Output buffer must be big enough to handle escape expansion!
 */
void urlesc(char *outbuf, char *strbuf)
{
	int a, b, c;
	char *ec = " #&;`'|*?-~<>^()[]{}$\\";

	strcpy(outbuf, "");

	for (a = 0; a < (int)strlen(strbuf); ++a) {
		c = 0;
		for (b = 0; b < strlen(ec); ++b) {
			if (strbuf[a] == ec[b])
				c = 1;
		}
		b = strlen(outbuf);
		if (c == 1)
			sprintf(&outbuf[b], "%%%02x", strbuf[a]);
		else
			sprintf(&outbuf[b], "%c", strbuf[a]);
	}
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
	static int seq = 0;

	sprintf(buf, "%lx-"F_XPID_T"-%x",
		time(NULL),
		getpid(),
		(seq++)
	);
}

/*
 * bmstrcasestr() -- case-insensitive substring search
 *
 * This uses the Boyer-Moore search algorithm and is therefore quite fast.
 * The code is roughly based on the strstr() replacement from 'tin' written
 * by Urs Jannsen.
 */
char *bmstrcasestr(char *text, char *pattern) {

	register unsigned char *p, *t;
	register int i, j, *delta;
	register size_t p1;
	int deltaspace[256];
	size_t textlen;
	size_t patlen;

	textlen = strlen (text);
	patlen = strlen (pattern);

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
 * Local replacement for controversial C library function that generates
 * names for temporary files.  Included to shut up compiler warnings.
 */
void CtdlMakeTempFileName(char *name, int len) {
	int i = 0;

	while (i++, i < 100) {
		snprintf(name, len, "/tmp/ctdl."F_XPID_T".%04x",
			getpid(),
			rand()
		);
		if (!access(name, F_OK)) {
			return;
		}
	}
}
