/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#ifdef HAVE_ICONV

/*
 * Wrapper around iconv_open()
 * Our version adds aliases for non-standard Microsoft charsets
 * such as 'MS950', aliasing them to names like 'CP950'
 *
 * tocode	Target encoding
 * fromcode	Source encoding
 * /
iconv_t ctdl_iconv_open(const char *tocode, const char *fromcode)
{
	iconv_t ic = (iconv_t)(-1) ;
	ic = iconv_open(tocode, fromcode);
	if (ic == (iconv_t)(-1) ) {
		char alias_fromcode[64];
		if ( (strlen(fromcode) == 5) && (!strncasecmp(fromcode, "MS", 2)) ) {
			safestrncpy(alias_fromcode, fromcode, sizeof alias_fromcode);
			alias_fromcode[0] = 'C';
			alias_fromcode[1] = 'P';
			ic = iconv_open(tocode, alias_fromcode);
		}
	}
	return(ic);
}
*/


static inline char *FindNextEnd (char *bptr)
{
	char * end;
	/* Find the next ?Q? */
	end = strchr(bptr + 2, '?');
	if (end == NULL) return NULL;
	if (((*(end + 1) == 'B') || (*(end + 1) == 'Q')) && 
	    (*(end + 2) == '?')) {
		/* skip on to the end of the cluster, the next ?= */
		end = strstr(end + 3, "?=");
	}
	else
		/* sort of half valid encoding, try to find an end. */
		end = strstr(bptr, "?=");
	return end;
}

/*
 * Handle subjects with RFC2047 encoding such as:
 * =?koi8-r?B?78bP0s3Mxc7JxSDXz9rE1dvO2c3JINvB0sHNySDP?=
 */
void utf8ify_rfc822_string(char **buf) {
	char *start, *end, *next, *nextend, *ptr;
	char newbuf[1024];
	char charset[128];
	char encoding[16];
	char istr[1024];
	iconv_t ic = (iconv_t)(-1) ;
	char *ibuf;			/**< Buffer of characters to be converted */
	char *obuf;			/**< Buffer for converted characters */
	size_t ibuflen;			/**< Length of input buffer */
	size_t obuflen;			/**< Length of output buffer */
	char *isav;			/**< Saved pointer to input buffer */
	char *osav;			/**< Saved pointer to output buffer */
	int passes = 0;
	int i, len, delta;
	int illegal_non_rfc2047_encoding = 0;

	/* Sometimes, badly formed messages contain strings which were simply
	 *  written out directly in some foreign character set instead of
	 *  using RFC2047 encoding.  This is illegal but we will attempt to
	 *  handle it anyway by converting from a user-specified default
	 *  charset to UTF-8 if we see any nonprintable characters.
	 */
	len = strlen(*buf);
	for (i=0; i<len; ++i) {
		if (((*buf)[i] < 32) || ((*buf)[i] > 126)) {
			illegal_non_rfc2047_encoding = 1;
			i = len; /*< take a shortcut, it won't be more than one. */
		}
	}
	if (illegal_non_rfc2047_encoding) {
		StrBuf *default_header_charset;
		get_preference("default_header_charset", &default_header_charset);
		if ( (strcasecmp(ChrPtr(default_header_charset), "UTF-8")) && 
		     (strcasecmp(ChrPtr(default_header_charset), "us-ascii")) ) {
			ctdl_iconv_open("UTF-8", ChrPtr(default_header_charset), &ic);
			if (ic != (iconv_t)(-1) ) {
				ibuf = malloc(1024);
				isav = ibuf;
				safestrncpy(ibuf, *buf, 1023);
				ibuflen = strlen(ibuf);
				obuflen = 1024;
				obuf = (char *) malloc(obuflen);
				osav = obuf;
				iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
				osav[1023-obuflen] = 0;
				free(*buf);
				*buf = osav;
				iconv_close(ic);
				free(isav);
			}
		}
	}

	/* pre evaluate the first pair */
	nextend = end = NULL;
	len = strlen(*buf);
	start = strstr(*buf, "=?");
	if (start != NULL) 
		end = FindNextEnd (start);

	while ((start != NULL) && (end != NULL))
	{
		next = strstr(end, "=?");
		if (next != NULL)
			nextend = FindNextEnd(next);
		if (nextend == NULL)
			next = NULL;

		/* did we find two partitions */
		if ((next != NULL) && 
		    ((next - end) > 2))
		{
			ptr = end + 2;
			while ((ptr < next) && 
			       (isspace(*ptr) ||
				(*ptr == '\r') ||
				(*ptr == '\n') || 
				(*ptr == '\t')))
				ptr ++;
			/* did we find a gab just filled with blanks? */
			if (ptr == next)
			{
				memmove (end + 2,
					 next,
					 len - (next - start));

				/* now terminate the gab at the end */
				delta = (next - end) - 2;
				len -= delta;
				(*buf)[len] = '\0';

				/* move next to its new location. */
				next -= delta;
				nextend -= delta;
			}
		}
		/* our next-pair is our new first pair now. */
		start = next;
		end = nextend;
	}

	/* Now we handle foreign character sets properly encoded
	 * in RFC2047 format.
	 */
	while (start=strstr((*buf), "=?"), end=FindNextEnd((start != NULL)? start : (*buf)),
		((start != NULL) && (end != NULL) && (end > start)) )
	{
		extract_token(charset, start, 1, '?', sizeof charset);
		extract_token(encoding, start, 2, '?', sizeof encoding);
		extract_token(istr, start, 3, '?', sizeof istr);

		ibuf = malloc(1024);
		isav = ibuf;
		if (!strcasecmp(encoding, "B")) {	/**< base64 */
			ibuflen = CtdlDecodeBase64(ibuf, istr, strlen(istr));
		}
		else if (!strcasecmp(encoding, "Q")) {	/**< quoted-printable */
			size_t len;
			long pos;
			
			len = strlen(istr);
			pos = 0;
			while (pos < len)
			{
				if (istr[pos] == '_') istr[pos] = ' ';
				pos++;
			}

			ibuflen = CtdlDecodeQuotedPrintable(ibuf, istr, len);
		}
		else {
			strcpy(ibuf, istr);		/**< unknown encoding */
			ibuflen = strlen(istr);
		}

		ctdl_iconv_open("UTF-8", charset, &ic);
		if (ic != (iconv_t)(-1) ) {
			obuflen = 1024;
			obuf = (char *) malloc(obuflen);
			osav = obuf;
			iconv(ic, &ibuf, &ibuflen, &obuf, &obuflen);
			osav[1024-obuflen] = 0;

			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s%s%s", *buf, osav, end);
			strcpy(*buf, newbuf);
			
			free(osav);
			iconv_close(ic);
		}
		else {
			end = start;
			end++;
			strcpy(start, "");
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			remove_token(end, 0, '?');
			strcpy(end, &end[1]);

			snprintf(newbuf, sizeof newbuf, "%s(unreadable)%s", *buf, end);
			strcpy(*buf, newbuf);
		}

		free(isav);

		/*
		 * Since spammers will go to all sorts of absurd lengths to get their
		 * messages through, there are LOTS of corrupt headers out there.
		 * So, prevent a really badly formed RFC2047 header from throwing
		 * this function into an infinite loop.
		 */
		++passes;
		if (passes > 20) return;
	}

}
#else
inline void utf8ify_rfc822_string(char **a){};

#endif




/**
 * \brief	RFC2047-encode a header field if necessary.
 *		If no non-ASCII characters are found, the string
 *		will be copied verbatim without encoding.
 *
 * \param	target		Target buffer.
 * \param	maxlen		Maximum size of target buffer.
 * \param	source		Source string to be encoded.
 * \param       SourceLen       Length of the source string
 * \returns     encoded length; -1 if non success.
 */
int webcit_rfc2047encode(char *target, int maxlen, char *source, long SourceLen)
{
	const char headerStr[] = "=?UTF-8?Q?";
	int need_to_encode = 0;
	int i = 0;
	int len;
	unsigned char ch;

	if ((source == NULL) || 
	    (target == NULL) ||
	    (SourceLen > maxlen)) return -1;

	while ((!IsEmptyStr (&source[i])) && 
	       (need_to_encode == 0) &&
	       (i < SourceLen) ) {
		if (((unsigned char) source[i] < 32) || 
		    ((unsigned char) source[i] > 126)) {
			need_to_encode = 1;
		}
		i++;
	}

	if (!need_to_encode) {
		memcpy (target, source, SourceLen);
		target[SourceLen] = '\0';
		return SourceLen;
	}
	
	if (sizeof (headerStr + SourceLen + 2) > maxlen)
		return -1;
	memcpy (target, headerStr, sizeof (headerStr));
	len = sizeof (headerStr) - 1;
	for (i=0; (i < SourceLen) && (len + 3< maxlen) ; ++i) {
		ch = (unsigned char) source[i];
		if ((ch < 32) || (ch > 126) || (ch == 61)) {
			sprintf(&target[len], "=%02X", ch);
			len += 3;
		}
		else {
			sprintf(&target[len], "%c", ch);
			len ++;
		}
	}
	
	if (len + 2 < maxlen) {
		strcat(&target[len], "?=");
		len +=2;
		return len;
	}
	else
		return -1;
}

