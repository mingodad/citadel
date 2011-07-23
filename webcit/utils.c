/*
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
				b = decode_hex(hex);
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

/* 
 * static wrapper for ecsputs1
 */
void escputs(const char *strbuf)
{
	StrEscAppend(WC->WBuf, NULL, strbuf, 0, 0);
}

/*
 * urlescape buffer and print it to the client
 */
void urlescputs(const char *strbuf)
{
	StrBufUrlescAppend(WC->WBuf, NULL, strbuf);
}


/**
 * urlescape buffer and print it as header 
 */
void hurlescputs(const char *strbuf) 
{
	StrBufUrlescAppend(WC->HBuf, NULL, strbuf);
}


/*
 * Output a string to the client as a CDATA block
 */
void cdataout(char *rawdata)
{
	char *ptr = rawdata;
	wc_printf("<![CDATA[");

	while ((ptr != NULL) && (ptr[0] != 0))
	{
		if (!strncmp(ptr, "]]>", 3)) {
			wc_printf("]]]]><![CDATA[>");
			++ptr; ++ptr; ++ptr;
		}
		else {
			wc_printf("%c", ptr[0]);
			++ptr;
		}
	}

	wc_printf("]]>");
}

