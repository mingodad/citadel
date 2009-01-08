/*
 * $Id: utils.c 6808 2008-12-11 00:00:36Z dothebart $
 *
 * de/encoding stuff. hopefully mostly to be depricated in favour of subst.c + strbuf
 */

#define SHOW_ME_VAPPEND_PRINTF
#include <stdio.h>
#include <stdarg.h>
#include "webcit.h"


/*   
 * remove escaped strings from i.e. the url string (like %20 for blanks)
 */
long unescape_input(char *buf)
{
	unsigned int a, b;
	char hex[3];
	long buflen;
	long len;

	buflen = strlen(buf);

	while ((buflen > 0) && (isspace(buf[buflen - 1]))){
		buf[buflen - 1] = 0;
		buflen --;
	}

	a = 0; 
	while (a < buflen) {
		if (buf[a] == '+')
			buf[a] = ' ';
		if (buf[a] == '%') {
			/* don't let % chars through, rather truncate the input. */
			if (a + 2 > buflen) {
				buf[a] = '\0';
				buflen = a;
			}
			else {			
				hex[0] = buf[a + 1];
				hex[1] = buf[a + 2];
				hex[2] = 0;
				b = 0;
				sscanf(hex, "%02x", &b);
				buf[a] = (char) b;
				len = buflen - a - 2;
				if (len > 0)
					memmove(&buf[a + 1], &buf[a + 3], len);
			
				buflen -=2;
			}
		}
		a++;
	}
	return a;
}

/*
 * Copy a string, escaping characters which have meaning in HTML.  
 *
 * target              target buffer
 * strbuf              source buffer
 * nbsp                        If nonzero, spaces are converted to non-breaking spaces.
 * nolinebreaks                if set, linebreaks are removed from the string.
 */
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks)
{
        char *aptr, *bptr, *eptr;
 
        *target = '\0';
        aptr = strbuf;
        bptr = target;
        eptr = target + tSize - 6; /* our biggest unit to put in...  */
 
 
        while ((bptr < eptr) && !IsEmptyStr(aptr) ){
                if (*aptr == '<') {
                        memcpy(bptr, "&lt;", 4);
                        bptr += 4;
                }
                else if (*aptr == '>') {
                        memcpy(bptr, "&gt;", 4);
                        bptr += 4;
                }
                else if (*aptr == '&') {
                        memcpy(bptr, "&amp;", 5);
                        bptr += 5;
                }
                else if (*aptr == '\"') {
                        memcpy(bptr, "&quot;", 6);
                        bptr += 6;
                }
                else if (*aptr == '\'') {
                        memcpy(bptr, "&#39;", 5);
                        bptr += 5;
                }
                else if (*aptr == LB) {
                        *bptr = '<';
                        bptr ++;
                }
                else if (*aptr == RB) {
                        *bptr = '>';
                        bptr ++;
                }
                else if (*aptr == QU) {
                        *bptr ='"';
                        bptr ++;
                }
                else if ((*aptr == 32) && (nbsp == 1)) {
                        memcpy(bptr, "&nbsp;", 6);
                        bptr += 6;
                }
                else if ((*aptr == '\n') && (nolinebreaks)) {
                        *bptr='\0';     /* nothing */
                }
                else if ((*aptr == '\r') && (nolinebreaks)) {
                        *bptr='\0';     /* nothing */
                }
                else{
                        *bptr = *aptr;
                        bptr++;
                }
                aptr ++;
        }
        *bptr = '\0';
        if ((bptr = eptr - 1 ) && !IsEmptyStr(aptr) )
                return -1;
        return (bptr - target);
}


void escputs1(const char *strbuf, int nbsp, int nolinebreaks)
{
	StrEscAppend(WC->WBuf, NULL, strbuf, nbsp, nolinebreaks);
}

void StrEscputs1(const StrBuf *strbuf, int nbsp, int nolinebreaks)
{
	StrEscAppend(WC->WBuf, strbuf, NULL, nbsp, nolinebreaks);
}

/* 
 * static wrapper for ecsputs1
 */
void escputs(const char *strbuf)
{
	escputs1(strbuf, 0, 0);
}


/* 
 * static wrapper for ecsputs1
 */
void StrEscPuts(const StrBuf *strbuf)
{
	StrEscputs1(strbuf, 0, 0);
}


/*
 * urlescape buffer and print it to the client
 */
void urlescputs(const char *strbuf)
{
	StrBufUrlescAppend(WC->WBuf, NULL, strbuf);
}

/*
 * urlescape buffer and print it to the client
 */
void UrlescPutStrBuf(const StrBuf *strbuf)
{
	StrBufUrlescAppend(WC->WBuf, strbuf, NULL);
}

/**
 * urlescape buffer and print it as header 
 */
void hurlescputs(const char *strbuf) 
{
	StrBufUrlescAppend(WC->HBuf, NULL, strbuf);
}


/*
 * Copy a string, escaping characters for JavaScript strings.
 */
void jsesc(char *target, size_t tlen, char *strbuf)
{
	int len;
	char *tend;
	char *send;
	char *tptr;
	char *sptr;

	target[0]='\0';
	len = strlen (strbuf);
	send = strbuf + len;
	tend = target + tlen;
	sptr = strbuf;
	tptr = target;
	
	while (!IsEmptyStr(sptr) && 
	       (sptr < send) &&
	       (tptr < tend)) {
	       
		if (*sptr == '<')
			*tptr = '[';
		else if (*sptr == '>')
			*tptr = ']';
		else if (*sptr == '\'') {
			if (tend - tptr < 3)
				return;
			*(tptr++) = '\\';
			*tptr = '\'';
		}
		else if (*sptr == '"') {
			if (tend - tptr < 8)
				return;
			*(tptr++) = '&';
			*(tptr++) = 'q';
			*(tptr++) = 'u';
			*(tptr++) = 'o';
			*(tptr++) = 't';
			*tptr = ';';
		}
		else if (*sptr == '&') {
			if (tend - tptr < 7)
				return;
			*(tptr++) = '&';
			*(tptr++) = 'a';
			*(tptr++) = 'm';
			*(tptr++) = 'p';
			*tptr = ';';
		} else {
			*tptr = *sptr;
		}
		tptr++; sptr++;
	}
	*tptr = '\0';
}

/*
 * escape and print javascript
 */
void jsescputs(char *strbuf)
{
	char outbuf[SIZ];
	
	jsesc(outbuf, SIZ, strbuf);
	wprintf("%s", outbuf);
}

/*
 * print a string to the client after cleaning it with msgesc() and stresc()
 */
void msgescputs1( char *strbuf)
{
	StrBuf *OutBuf;

	if ((strbuf == NULL) || IsEmptyStr(strbuf))
		return;
	OutBuf = NewStrBuf();
	StrMsgEscAppend(OutBuf, NULL, strbuf);
	StrEscAppend(WC->WBuf, OutBuf, NULL, 0, 0);
	FreeStrBuf(&OutBuf);
}

/*
 * print a string to the client after cleaning it with msgesc()
 */
void msgescputs(char *strbuf) {
	if ((strbuf != NULL) && !IsEmptyStr(strbuf))
		StrMsgEscAppend(WC->WBuf, NULL, strbuf);
}



