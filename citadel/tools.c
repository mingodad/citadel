/*
 * tools.c -- Miscellaneous routines used by both the client and server.
 * $Id$
 */

#include "sysdep.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/time.h>
#include "tools.h"

#define TRUE  1
#define FALSE 0

typedef unsigned char byte;	      /* Byte type */
static byte dtable[256];	      /* base64 encode / decode table */


char *safestrncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
		fprintf(stderr, "safestrncpy: NULL argument\n");
		abort();
	}
	strncpy(dest, src, n);
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
int num_tokens(char *source, char tok) {
	int a;
	int count = 1;

	for (a=0; a<strlen(source); ++a) {
		if (source[a]==tok) ++count;
	}
	return(count);
}

/*
 * extract_token()  -  a smarter string tokenizer
 */
void extract_token(char *dest, char *source, int parmnum, char separator) 
{
	int i;
	int len;
	int curr_parm;

	strcpy(dest,"");
	len = 0;
	curr_parm = 0;

	if (strlen(source)==0) {
		return;
		}

	for (i=0; i<strlen(source); ++i) {
		if (source[i]==separator) {
			++curr_parm;
		}
		else if (curr_parm == parmnum) {
			dest[len+1] = 0;
			dest[len++] = source[i];
		}
	}
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

	if (strlen(source)==0) {
		return;
		}

	for (i=0; i<strlen(source); ++i) {
		if ( (start < 0) && (curr_parm == parmnum) ) {
			start = i;
		}

		if ( (end < 0) && (curr_parm == (parmnum+1)) ) {
			end = i;
		}

		if (source[i]==separator) {
			++curr_parm;
		}
	}

	if (end < 0) end = strlen(source);

	printf("%d .. %d\n", start, end);

	strcpy(&source[start], &source[end]);
}




/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(char *source, int parmnum)
{
	char buf[256];
	
	extract_token(buf, source, parmnum, '|');
	return(atoi(buf));
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(char *source, long int parmnum)
{
	char buf[256];
	
	extract_token(buf, source, parmnum, '|');
	return(atol(buf));
}



/*
 * decode_base64() and encode_base64() are adaptations of code by
 * John Walker, found in full in the file "base64.c" included with this
 * distribution.  The difference between those functions and these is that
 * these are intended to encode/decode small string buffers, and those are
 * intended to encode/decode entire MIME parts.
 */

void encode_base64(char *dest, char *source)
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
	    c = source[spos++];
	    if (c == 0) {
		hiteof = TRUE;
		break;
	    }
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



void decode_base64(char *dest, char *source)
{
    int i;
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
	    int c = source[spos++];

	    if (c == 0) {
		if (i > 0) {
		    return;
		}
		return;
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
	    return;
	}
    }
}



/*
 * Strip leading and trailing spaces from a string
 */
void striplt(char *buf)
{
        while ((strlen(buf) > 0) && (buf[0] == 32))
                strcpy(buf, &buf[1]);
        while (buf[strlen(buf) - 1] == 32)
                buf[strlen(buf) - 1] = 0;
}





/* 
 * Return the number of occurances of character ch in string st
 */ 
int haschar(char *st, int ch)
{
	int a, b;
	b = 0;
	for (a = 0; a < strlen(st); ++a)
		if (st[a] == ch)
			++b;
	return (b);
}




/*
 * Compare two strings, insensitive to case, punctuation, and non-alnum chars
 */
int collapsed_strcmp(char *s1, char *s2) {
	char *c1, *c2;
	int i, ret, pos;

	c1 = malloc(strlen(s1)+1);
	c2 = malloc(strlen(s2)+1);
	c1[0] = 0;
	c2[0] = 0;

	pos = 0;
	for (i=0; i<strlen(s1); ++i) {
		if (isalnum(s1[i])) {
			c1[pos] = tolower(s1[i]);
			c1[++pos] = 0;
		}
	}

	pos = 0;
	for (i=0; i<strlen(s2); ++i) {
		if (isalnum(s2[i])) {
			c2[pos] = tolower(s2[i]);
			c2[++pos] = 0;
		}
	}

	ret = strcmp(c1, c2);
	free(c1);
	free(c2);
	return(ret);
}



/*
 * Format a date/time stamp for output 
 */
void fmt_date(char *buf, time_t thetime) {
	struct tm *tm;
	int hour;

	char *ascmonths[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	strcpy(buf, "");
	tm = localtime(&thetime);

	hour = tm->tm_hour;
	if (hour == 0)	hour = 12;
	else if (hour > 12) hour = hour - 12;

	sprintf(buf, "%s %d %4d %d:%02d%s",
		ascmonths[tm->tm_mon],
		tm->tm_mday,
		tm->tm_year + 1900,
		hour,
		tm->tm_min,
		( (tm->tm_hour >= 12) ? "pm" : "am" )
	);
}






