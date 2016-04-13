/*
 * This is the MIME parser for Citadel.
 *
 * Copyright (c) 1998-2010 by the citadel.org development team.
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
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>

#include "xdgmime/xdgmime.h"
#include "libcitadel.h"
#include "libcitadellocal.h"

const unsigned char FromHexTable [256] = {
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //  0
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 10
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 20
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 30
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x01, // 40
	0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0xFF, 0xFF, // 50
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, // 60
	0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 70
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, // 80
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x0A, 0x0B, 0x0C, // 90
	0x0D, 0x0E, 0x0F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //100
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //110
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //120
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //130
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //140
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //150
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //160
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //170
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //180
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //190
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //200
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //210
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //220
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //230
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, //240
	0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF                          //250
};


long extract_key(char *target, char *source, long sourcelen, char *key, long keylen, char KeyEnd)
{
	char *sptr, *ptr = NULL;
	int double_quotes = 0;
	long RealKeyLen = keylen;

	sptr = source;

	while (sptr != NULL)
	{
		ptr = bmstrcasestr_len(sptr, sourcelen - (sptr - source), 
				       key, keylen);
		if(ptr != NULL)
		{
			while (isspace(*(ptr + RealKeyLen)))
				RealKeyLen ++;
			if (*(ptr + RealKeyLen) == KeyEnd)
			{
				sptr = NULL;
				RealKeyLen ++;				
			}
			else
			{
				sptr = ptr + RealKeyLen + 1;
			}
		}
		else 
			sptr = ptr;
	}
	if (ptr == NULL) {
		*target = '\0';
		return 0;
	}
	strcpy(target, (ptr + RealKeyLen));

	for (ptr=target; (*ptr != 0); ptr++) {

		/* A semicolon means we've hit the end of the key, unless we're inside double quotes */
		if ( (double_quotes != 1) && (*ptr == ';')) {
			*ptr = 0;
		}

		/* if we find double quotes, we've got a great set of string boundaries */
		if (*ptr == '\"') {
			++double_quotes;
			if (double_quotes == 1) {
				strcpy(ptr, ptr+1);
			}
			else {
				*ptr = 0;
			}
		}
	}
	*ptr = '\0';
	return ptr - target;
}


/*
 * For non-multipart messages, we need to generate a quickie partnum of "1"
 * to return to callback functions.  Some callbacks demand it.
 */
char *fixed_partnum(char *supplied_partnum) {
	if (supplied_partnum == NULL) return "1";
	if (strlen(supplied_partnum)==0) return "1";
	return supplied_partnum;
}


static inline unsigned int _decode_hex(const char *Source)
{
	unsigned int ret = '?';
	unsigned char LO_NIBBLE;
	unsigned char HI_NIBBLE;

	HI_NIBBLE = FromHexTable[(unsigned char) *Source];
	LO_NIBBLE = FromHexTable[(unsigned char) *(Source+1)];
	
	if ((LO_NIBBLE == 0xFF) || (LO_NIBBLE == 0xFF))
		return ret;
	ret = HI_NIBBLE;
	ret = ret << 4;
	ret = ret | LO_NIBBLE;
	return ret;
}

unsigned int decode_hex(char *Source) {return _decode_hex(Source);}

/*
 * Convert "quoted-printable" to binary.  Returns number of bytes decoded.
 * according to RFC2045 section 6.7
 */
int CtdlDecodeQuotedPrintable(char *decoded, char *encoded, int sourcelen) {
	unsigned int ch;
	int decoded_length = 0;
	int pos = 0;

	while (pos < sourcelen)
	{
		if (*(encoded + pos) == '=')
		{
			pos ++;
			if (*(encoded + pos) == '\n')
			{
				pos ++;
			}
			else if (*(encoded + pos) == '\r')
			{
				pos ++;
				if (*(encoded + pos) == '\n')
					pos++;
			}
			else
			{
				ch = _decode_hex(&encoded[pos]);
				pos += 2;
				decoded[decoded_length++] = ch;
			}
		}
		else
		{
			decoded[decoded_length++] = encoded[pos];
			pos += 1;
		}
	}
	decoded[decoded_length] = 0;
	return(decoded_length);
}


/*
 * Given a message or message-part body and a length, handle any necessary
 * decoding and pass the request up the stack.
 */
void mime_decode(char *partnum,
		 char *part_start, size_t length,
		 char *content_type, char *charset, char *encoding,
		 char *disposition,
		 char *id,
		 char *name, char *filename,
		 MimeParserCallBackType CallBack,
		 MimeParserCallBackType PreMultiPartCallBack,
		 MimeParserCallBackType PostMultiPartCallBack,
		 void *userdata,
		 int dont_decode)
{

	char *decoded;
	size_t bytes_decoded = 0;

	/* Some encodings aren't really encodings */
	if (!strcasecmp(encoding, "7bit"))
		*encoding = '\0';
	if (!strcasecmp(encoding, "8bit"))
		*encoding = '\0';
	if (!strcasecmp(encoding, "binary"))
		*encoding = '\0';
	if (!strcasecmp(encoding, "ISO-8859-1"))
		*encoding = '\0';

	/* If this part is not encoded, send as-is */
	if ( (strlen(encoding) == 0) || (dont_decode)) {
		if (CallBack != NULL) {
			CallBack(name, 
				 filename, 
				 fixed_partnum(partnum),
				 disposition, 
				 part_start,
				 content_type, 
				 charset, 
				 length, 
				 encoding, 
				 id,
				 userdata);
			}
		return;
	}
	
	/* Fail silently if we hit an unknown encoding. */
	if ((strcasecmp(encoding, "base64"))
	    && (strcasecmp(encoding, "quoted-printable"))) {
		return;
	}

	/*
	 * Allocate a buffer for the decoded data.  The output buffer is slightly
	 * larger than the input buffer; this assumes that the decoded data
	 * will never be significantly larger than the encoded data.  This is a
	 * safe assumption with base64, uuencode, and quoted-printable.
	 */
	decoded = malloc(length + 32768);
	if (decoded == NULL) {
		return;
	}

	if (!strcasecmp(encoding, "base64")) {
		bytes_decoded = CtdlDecodeBase64(decoded, part_start, length);
	}
	else if (!strcasecmp(encoding, "quoted-printable")) {
		bytes_decoded = CtdlDecodeQuotedPrintable(decoded, part_start, length);
	}

	if (bytes_decoded > 0) if (CallBack != NULL) {
			char encoding_buf[SIZ];

			strcpy(encoding_buf, "binary");
			CallBack(name, 
				 filename, 
				 fixed_partnum(partnum),
				 disposition, 
				 decoded,
				 content_type, 
				 charset, 
				 bytes_decoded, 
				 encoding_buf, 
				 id, 
				 userdata);
	}

	free(decoded);
}

/*
 * this is the extract of mime_decode which can be called if 'dont_decode' was set; 
 * to save the cpu intense process of decoding to the time when it realy wants the content. 
 * returns: 
 *   - > 0 we decoded something, its on *decoded, you need to free it.
 *   - = 0 no need to decode stuff. *decoded will be NULL.
 *   - < 0 an error occured, either an unknown encoding, or alloc failed. no need to free.
 */
int mime_decode_now (char *part_start, 
		     size_t length,
		     char *encoding,
		     char **decoded,
		     size_t *bytes_decoded)
{
	*bytes_decoded = 0;
	*decoded = NULL;
	/* Some encodings aren't really encodings */
	if (!strcasecmp(encoding, "7bit"))
		*encoding = '\0';
	if (!strcasecmp(encoding, "8bit"))
		*encoding = '\0';
	if (!strcasecmp(encoding, "binary"))
		*encoding = '\0';

	/* If this part is not encoded, send as-is */
	if (strlen(encoding) == 0) {
		return 0;
	}
	

	/* Fail if we hit an unknown encoding. */
	if ((strcasecmp(encoding, "base64"))
	    && (strcasecmp(encoding, "quoted-printable"))) {
		return -1;
	}

	/*
	 * Allocate a buffer for the decoded data.  The output buffer is slightly
	 * larger than the input buffer; this assumes that the decoded data
	 * will never be significantly larger than the encoded data.  This is a
	 * safe assumption with base64, uuencode, and quoted-printable.
	 */
	*decoded = malloc(length + 32768);
	if (decoded == NULL) {
		return -1;
	}

	if (!strcasecmp(encoding, "base64")) {
		*bytes_decoded = CtdlDecodeBase64(*decoded, part_start, length);
		return 1;
	}
	else if (!strcasecmp(encoding, "quoted-printable")) {
		*bytes_decoded = CtdlDecodeQuotedPrintable(*decoded, part_start, length);
		return 1;
	}
	return -1;
}

typedef enum _eIntMimeHdrs {
	boundary,
	startary,
	endary,
	content_type,
	charset,
	encoding,
	content_type_name,
	content_disposition_name,
	filename,
	disposition,
	id,
	eMax /* don't move ! */
} eIntMimeHdrs;

typedef struct _CBufStr {
	char Key[SIZ];
	long len;
}CBufStr;

typedef struct _interesting_mime_headers {
	CBufStr b[eMax];
	long content_length;
	long is_multipart;
} interesting_mime_headers;


static void FlushInterestingMimes(interesting_mime_headers *m)
{
	int i;
	
	for (i = 0; i < eMax; i++) {
	     m->b[i].Key[0] = '\0';
	     m->b[i].len = 0;
	}
	m->content_length = -1;
}
static interesting_mime_headers *InitInterestingMimes(void)
{
	interesting_mime_headers *m;
	m = (interesting_mime_headers*) malloc( sizeof(interesting_mime_headers));

	FlushInterestingMimes(m);

	return m;
}


static long parse_MimeHeaders(interesting_mime_headers *m, 
			      char** pcontent_start, 
			      char *content_end)
{
	char buf[SIZ];
	char header[SIZ];
	long headerlen;
	char *ptr, *pch;
	int buflen = 0;
	int i;

	/* Learn interesting things from the headers */
	ptr = *pcontent_start;
	*header = '\0';
	headerlen = 0;
	do {
		ptr = memreadlinelen(ptr, buf, SIZ, &buflen);

		for (i = 0; i < buflen; ++i) {
			if (isspace(buf[i])) {
				buf[i] = ' ';
			}
		}

		if (!isspace(buf[0]) && (headerlen > 0)) {
			if (!strncasecmp(header, "Content-type:", 13)) {
				memcpy (m->b[content_type].Key, &header[13], headerlen - 12);
				m->b[content_type].Key[headerlen - 12] = '\0';
				m->b[content_type].len = striplt (m->b[content_type].Key);

				m->b[content_type_name].len = extract_key(m->b[content_type_name].Key, CKEY(m->b[content_type]), HKEY("name"), '=');
				m->b[charset].len           = extract_key(m->b[charset].Key,           CKEY(m->b[content_type]), HKEY("charset"), '=');
				m->b[boundary].len          = extract_key(m->b[boundary].Key,          header,       headerlen,  HKEY("boundary"), '=');

				/* Deal with weird headers */
				pch = strchr(m->b[content_type].Key, ' ');
				if (pch != NULL) {
					*pch = '\0';
					m->b[content_type].len = m->b[content_type].Key - pch;
				}
				pch = strchr(m->b[content_type].Key, ';');
				if (pch != NULL) {
					*pch = '\0';
					m->b[content_type].len = m->b[content_type].Key - pch;
				}
			}
			else if (!strncasecmp(header, "Content-Disposition:", 20)) {
				memcpy (m->b[disposition].Key, &header[20], headerlen - 19);
				m->b[disposition].Key[headerlen - 19] = '\0';
				m->b[disposition].len = striplt(m->b[disposition].Key);

				m->b[content_disposition_name].len = extract_key(m->b[content_disposition_name].Key, CKEY(m->b[disposition]), HKEY("name"), '=');
				m->b[filename].len                 = extract_key(m->b[filename].Key,                 CKEY(m->b[disposition]), HKEY("filename"), '=');
				pch = strchr(m->b[disposition].Key, ';');
				if (pch != NULL) *pch = '\0';
				m->b[disposition].len = striplt(m->b[disposition].Key);
			}
			else if (!strncasecmp(header, "Content-ID:", 11)) {
				memcpy(m->b[id].Key, &header[11], headerlen - 11);
				m->b[id].Key[headerlen - 11] = '\0';
				striplt(m->b[id].Key);
				m->b[id].len = stripallbut(m->b[id].Key, '<', '>');
			}
			else if (!strncasecmp(header, "Content-length: ", 15)) {
				char *clbuf;
				clbuf = &header[15];
				while (isspace(*clbuf))
					clbuf ++;
				m->content_length = (size_t) atol(clbuf);
			}
			else if (!strncasecmp(header, "Content-transfer-encoding: ", 26)) {
				memcpy(m->b[encoding].Key, &header[26], headerlen - 26);
				m->b[encoding].Key[headerlen - 26] = '\0';
				m->b[encoding].len = striplt(m->b[encoding].Key);
			}
			*header = '\0';
			headerlen = 0;
		}
		if ((headerlen + buflen + 2) < SIZ) {
			memcpy(&header[headerlen], buf, buflen);
			headerlen += buflen;
			header[headerlen] = '\0';
		}
		if (ptr >= content_end) {
			return -1;
		}
	} while ((!IsEmptyStr(buf)) && (*ptr != 0));

	m->is_multipart = m->b[boundary].len != 0;
	*pcontent_start = ptr;

	return 0;
}


static int IsAsciiEncoding(interesting_mime_headers *m)
{

	if ((m->b[encoding].len != 0) &&
	    (strcasecmp(m->b[encoding].Key, "base64") == 0))
		return 1;
	if ((m->b[encoding].len != 0) &&
	    (strcmp(m->b[encoding].Key, "quoted-printable") == 0))
		return 1;

	return 0;
}

static char *FindNextContent(char *ptr,
			     char *content_end,
			     interesting_mime_headers *SubMimeHeaders,
			     interesting_mime_headers *m)
{
	char *next_boundary;
	char  tmp;

	if (IsAsciiEncoding(SubMimeHeaders)) {
		tmp = *content_end;
		*content_end = '\0';

		/** 
		 * ok, if we have a content length of the mime part, 
		 * try skipping the content on the search for the next
		 * boundary. since we don't trust the content_length
		 * to be all accurate, and suspect it to lose one digit 
		 * per line with a line length of 80 chars, we need 
		 * to start searching a little before..
		 */
				   
		if ((SubMimeHeaders->content_length != -1) &&
		    (SubMimeHeaders->content_length > 10))
		{
			char *pptr;
			long lines;
					
			lines = SubMimeHeaders->content_length / 80;
			pptr = ptr + SubMimeHeaders->content_length - lines - 10;
			if (pptr < content_end)
				ptr = pptr;
		}
			
		next_boundary = strstr(ptr, m->b[startary].Key);
		*content_end = tmp;
	}
	else {
		char *srch;
		/** 
		 * ok, if we have a content length of the mime part, 
		 * try skipping the content on the search for the next
		 * boundary. since we don't trust the content_length
		 * to be all accurate, start searching a little before..
		 */
				   
		if ((SubMimeHeaders->content_length != -1) &&
		    (SubMimeHeaders->content_length > 10))
		{
			char *pptr;
			pptr = ptr + SubMimeHeaders->content_length - 10;
			if (pptr < content_end)
				ptr = pptr;
		}
		

		srch = next_boundary = NULL;
		for (srch = memchr(ptr, '-',  content_end - ptr);
		     (srch != NULL) && (srch < content_end); 
		     srch = memchr(srch, '-',  content_end - srch)) 
		{
			if (!memcmp(srch, 
				    m->b[startary].Key, 
				    m->b[startary].len)) 
			{
				next_boundary = srch;
				srch = content_end;
			}
			else srch ++;

		}

	}
	return next_boundary;
}

/*
 * Break out the components of a multipart message
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
static void recurseable_mime_parser(char *partnum,
				    char *content_start, char *content_end,
				    MimeParserCallBackType CallBack,
				    MimeParserCallBackType PreMultiPartCallBack,
				    MimeParserCallBackType PostMultiPartCallBack,
				    void *userdata,
				    int dont_decode, 
				    interesting_mime_headers *m)
{
	interesting_mime_headers *SubMimeHeaders;
	char     *ptr;
	char     *part_start;
	char     *part_end = NULL;
	char     *evaluate_crlf_ptr = NULL;
	char     *next_boundary;
	char      nested_partnum[256];
	int       crlf_in_use = 0;
	int       part_seq = 0;
	CBufStr  *chosen_name;


	/* If this is a multipart message, then recursively process it */
	ptr = content_start;
	part_start = NULL;
	if (m->is_multipart) {

		/* Tell the client about this message's multipartedness */
		if (PreMultiPartCallBack != NULL) {
			PreMultiPartCallBack("", 
					     "", 
					     partnum, 
					     "",
					     NULL, 
					     m->b[content_type].Key, 
					     m->b[charset].Key,
					     0, 
					     m->b[encoding].Key, 
					     m->b[id].Key, 
					     userdata);
		}

		/* Figure out where the boundaries are */
		m->b[startary].len = snprintf(m->b[startary].Key, SIZ, "--%s", m->b[boundary].Key);
		SubMimeHeaders = InitInterestingMimes ();

		while ((*ptr == '\r') || (*ptr == '\n')) ptr ++;

		if (strncmp(ptr, m->b[startary].Key, m->b[startary].len) == 0)
			ptr += m->b[startary].len;

		while ((*ptr == '\r') || (*ptr == '\n')) ptr ++;

		part_start = NULL;
		do {
			char *optr;

			optr = ptr;
			if (parse_MimeHeaders(SubMimeHeaders, &ptr, content_end) != 0)
				break;
			if ((ptr - optr > 2) && 
			    (*(ptr - 2) == '\r'))
				crlf_in_use = 1;
			
			part_start = ptr;
			
			next_boundary = FindNextContent(ptr,
							content_end,
							SubMimeHeaders,
							m);
			if ((next_boundary != NULL) && 
			    (next_boundary - part_start < 3)) {
				FlushInterestingMimes(SubMimeHeaders);

				continue;
			}

			if ( (part_start != NULL) && (next_boundary != NULL) ) {
				part_end = next_boundary;
				--part_end;		/* omit the trailing LF */
				if (crlf_in_use) {
					--part_end;	/* omit the trailing CR */
				}

				if (!IsEmptyStr(partnum)) {
					snprintf(nested_partnum,
						 sizeof nested_partnum,
						 "%s.%d", partnum,
						 ++part_seq);
				}
				else {
					snprintf(nested_partnum,
						 sizeof nested_partnum,
						 "%d", ++part_seq);
				}
				recurseable_mime_parser(nested_partnum,
							part_start, 
							part_end,
							CallBack,
							PreMultiPartCallBack,
							PostMultiPartCallBack,
							userdata,
							dont_decode, 
							SubMimeHeaders);
			}

			if (next_boundary != NULL) {
				/* If we pass out of scope, don't attempt to
				 * read past the end boundary. */
				if ((*(next_boundary + m->b[startary].len) == '-') && 
				    (*(next_boundary + m->b[startary].len + 1) == '-') ){
					ptr = content_end;
				}
				else {
					/* Set up for the next part. */
					part_start = strstr(next_boundary, "\n");
					
					/* Determine whether newlines are LF or CRLF */
					evaluate_crlf_ptr = part_start;
					--evaluate_crlf_ptr;
					if ((*evaluate_crlf_ptr == '\r') && 
					    (*(evaluate_crlf_ptr + 1) == '\n'))
					{
						crlf_in_use = 1;
					}
					else {
						crlf_in_use = 0;
					}

					/* Advance past the LF ... now we're in the next part */
					++part_start;
					ptr = part_start;
				}
			}
			else {
				/* Invalid end of multipart.  Bail out! */
				ptr = content_end;
			}
			FlushInterestingMimes(SubMimeHeaders);
		} while ( (ptr < content_end) && (next_boundary != NULL) );

		free(SubMimeHeaders);

		if (PostMultiPartCallBack != NULL) {
			PostMultiPartCallBack("", 
					      "", 
					      partnum, 
					      "", 
					      NULL,
					      m->b[content_type].Key, 
					      m->b[charset].Key,
					      0, 
					      m->b[encoding].Key, 
					      m->b[id].Key, 
					      userdata);
		}
	} /* If it's not a multipart message, then do something with it */
	else {
		size_t length;
		part_start = ptr;
		length = content_end - part_start;
		ptr = part_end = content_end;


		/* The following code will truncate the MIME part to the size
		 * specified by the Content-length: header.   We have commented it
		 * out because these headers have a tendency to be wrong.
		 *
		 *	if ( (content_length > 0) && (length > content_length) ) {
		 *		length = content_length;
		 *	}
                 */

		/* Sometimes the "name" field is tacked on to Content-type,
		 * and sometimes it's tacked on to Content-disposition.  Use
		 * whichever one we have.
		 */
		if (m->b[content_disposition_name].len > m->b[content_type_name].len) {
			chosen_name = &m->b[content_disposition_name];
		}
		else {
			chosen_name = &m->b[content_type_name];
		}
	
		/* Ok, we've got a non-multipart part here, so do something with it.
		 */
		mime_decode(partnum,
			    part_start, 
			    length,
			    m->b[content_type].Key, 
			    m->b[charset].Key,
			    m->b[encoding].Key, 
			    m->b[disposition].Key, 
			    m->b[id].Key, 
			    chosen_name->Key, 
			    m->b[filename].Key,
			    CallBack, 
			    NULL, NULL,
			    userdata, 
			    dont_decode
			);

		/*
		 * Now if it's an encapsulated message/rfc822 then we have to recurse into it
		 */
		if (!strcasecmp(&m->b[content_type].Key[0], "message/rfc822")) {

			if (PreMultiPartCallBack != NULL) {
				PreMultiPartCallBack("", 
						     "", 
						     partnum, 
						     "",
						     NULL, 
						     m->b[content_type].Key, 
						     m->b[charset].Key,
						     0, 
						     m->b[encoding].Key, 
						     m->b[id].Key, 
						     userdata);
			}
			if (CallBack != NULL) {
				if (strlen(partnum) > 0) {
					snprintf(nested_partnum,
						 sizeof nested_partnum,
						 "%s.%d", partnum,
						 ++part_seq);
				}
				else {
					snprintf(nested_partnum,
						 sizeof nested_partnum,
						 "%d", ++part_seq);
				}
				the_mime_parser(nested_partnum,
						part_start, 
						part_end,
						CallBack,
						PreMultiPartCallBack,
						PostMultiPartCallBack,
						userdata,
						dont_decode
					);
			}
			if (PostMultiPartCallBack != NULL) {
				PostMultiPartCallBack("", 
						      "", 
						      partnum, 
						      "", 
						      NULL,
						      m->b[content_type].Key, 
						      m->b[charset].Key,
						      0, 
						      m->b[encoding].Key, 
						      m->b[id].Key, 
						      userdata);
			}


		}

	}

}

/*
 * Break out the components of a multipart message
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
void the_mime_parser(char *partnum,
		     char *content_start, char *content_end,
		     MimeParserCallBackType CallBack,
		     MimeParserCallBackType PreMultiPartCallBack,
		     MimeParserCallBackType PostMultiPartCallBack,
		     void *userdata,
		     int dont_decode)
{
	interesting_mime_headers *m;

	/* If the caller didn't supply an endpointer, generate one by measure */
	if (content_end == NULL) {
		content_end = &content_start[strlen(content_start)];
	}

	m = InitInterestingMimes();

	if (!parse_MimeHeaders(m, &content_start, content_end))
	{

		recurseable_mime_parser(partnum,
					content_start, content_end,
					CallBack,
					PreMultiPartCallBack,
					PostMultiPartCallBack,
					userdata,
					dont_decode,
					m);
	}
	free(m);
}

/*
 * Entry point for the MIME parser.
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
void mime_parser(char *content_start,
		 char *content_end,
		 MimeParserCallBackType CallBack,
		 MimeParserCallBackType PreMultiPartCallBack,
		 MimeParserCallBackType PostMultiPartCallBack,
		 void *userdata,
		 int dont_decode)
{

	the_mime_parser("", content_start, content_end,
			CallBack,
			PreMultiPartCallBack,
			PostMultiPartCallBack,
			userdata, dont_decode);
}






typedef struct _MimeGuess {
	const char *Pattern;
	size_t PatternLen;
	long PatternOffset;
	const char *MimeString;
} MimeGuess;

MimeGuess MyMimes [] = {
	{
		"GIF",
		3,
		0,
		"image/gif"
	},
	{
		"\xff\xd8",
		2,
		0,
		"image/jpeg"
	},
	{
		"\x89PNG",
		4,
		0,
		"image/png"
	},
	{ // last...
		"",
		0,
		0,
		""
	}
};


const char *GuessMimeType(const char *data, size_t dlen)
{
	int MimeIndex = 0;

	while (MyMimes[MimeIndex].PatternLen != 0)
	{
		if ((MyMimes[MimeIndex].PatternLen + 
		     MyMimes[MimeIndex].PatternOffset < dlen) &&
		    strncmp(MyMimes[MimeIndex].Pattern, 
			    &data[MyMimes[MimeIndex].PatternOffset], 
			    MyMimes[MimeIndex].PatternLen) == 0)
		{
			return MyMimes[MimeIndex].MimeString;
		}
		MimeIndex ++;
	}
	/* 
	 * ok, our simple minded algorythm didn't find anything, 
	 * let the big chegger try it, he wil default to application/octet-stream
	 */
	return (xdg_mime_get_mime_type_for_data(data, dlen));
}


const char* GuessMimeByFilename(const char *what, size_t len)
{
	/* we know some hardcoded on our own, try them... */
	if ((len > 3) && !strncasecmp(&what[len - 4], ".gif", 4))
		return "image/gif";
	else if ((len > 2) && !strncasecmp(&what[len - 3], ".js", 3))
		return  "text/javascript";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".txt", 4))
		return "text/plain";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".css", 4))
		return "text/css";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".htc", 4))
		return "text/x-component";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".jpg", 4))
		return "image/jpeg";
	else if ((len > 4) && !strncasecmp(&what[len - 5], ".jpeg", 5))
		return "image/jpeg";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".png", 4))
		return "image/png";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".ico", 4))
		return "image/x-icon";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".vcf", 4))
		return "text/x-vcard";
	else if ((len > 4) && !strncasecmp(&what[len - 5], ".html", 5))
		return "text/html";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".htm", 4))
		return "text/html";
	else if ((len > 3) && !strncasecmp(&what[len - 4], ".wml", 4))
		return "text/vnd.wap.wml";
	else if ((len > 4) && !strncasecmp(&what[len - 5], ".wmls", 5))
		return "text/vnd.wap.wmlscript";
	else if ((len > 4) && !strncasecmp(&what[len - 5], ".wmlc", 5))
		return "application/vnd.wap.wmlc";
	else if ((len > 5) && !strncasecmp(&what[len - 6], ".wmlsc", 6))
		return "application/vnd.wap.wmlscriptc";
	else if ((len > 4) && !strncasecmp(&what[len - 5], ".wbmp", 5))
		return "image/vnd.wap.wbmp";
	else
		/* and let xdgmime do the fallback. */
		return xdg_mime_get_mime_type_from_file_name(what);
}

static HashList *IconHash = NULL;

typedef struct IconName IconName;

struct IconName {
	char *FlatName;
	char *FileName;
};

static void DeleteIcon(void *IconNamePtr)
{
	IconName *Icon = (IconName*) IconNamePtr;
	free(Icon->FlatName);
	free(Icon->FileName);
	free(Icon);
}

/*
static const char *PrintFlat(void *IconNamePtr)
{
	IconName *Icon = (IconName*) IconNamePtr;
	return Icon->FlatName;
}
static const char *PrintFile(void *IconNamePtr)
{
	IconName *Icon = (IconName*) IconNamePtr;
	return Icon->FileName;
}
*/

#define GENSTR "x-generic"
#define IGNORE_PREFIX_1 "gnome-mime"
int LoadIconDir(const char *DirName)
{
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	int d_namelen;
	int d_without_ext;
	IconName *Icon;

	filedir = opendir (DirName);
	IconHash = NewHash(1, NULL);
	if (filedir == NULL) {
		return 0;
	}

	while ((filedir_entry = readdir(filedir)))
	{
		char *MinorPtr;
		char *PStart;
#ifdef _DIRENT_HAVE_D_NAMLEN
		d_namelen = filedir_entry->d_namlen;
#else
		d_namelen = strlen(filedir_entry->d_name);
#endif
		d_without_ext = d_namelen;
		while ((d_without_ext > 0) && (filedir_entry->d_name[d_without_ext] != '.'))
			d_without_ext --;
		if ((d_without_ext == 0) || (d_namelen < 3))
			continue;

		if ((sizeof(IGNORE_PREFIX_1) < d_namelen) &&
		    (strncmp(IGNORE_PREFIX_1, 
			     filedir_entry->d_name, 
			     sizeof(IGNORE_PREFIX_1) - 1) == 0)) {
			PStart = filedir_entry->d_name + sizeof(IGNORE_PREFIX_1);
			d_without_ext -= sizeof(IGNORE_PREFIX_1);
		}
		else {
			PStart = filedir_entry->d_name;
		}
		Icon = malloc(sizeof(IconName));

		Icon->FileName = malloc(d_namelen + 1);
		memcpy(Icon->FileName, filedir_entry->d_name, d_namelen + 1);

		Icon->FlatName = malloc(d_without_ext + 1);
		memcpy(Icon->FlatName, PStart, d_without_ext);
		Icon->FlatName[d_without_ext] = '\0';
		/* Try to find Minor type in image-jpeg */
		MinorPtr = strchr(Icon->FlatName, '-');
		if (MinorPtr != NULL) {
			size_t MinorLen;
			MinorLen = 1 + d_without_ext - (MinorPtr - Icon->FlatName + 1);
			if ((MinorLen == sizeof(GENSTR)) && 
			    (strncmp(MinorPtr + 1, GENSTR, sizeof(GENSTR)) == 0)) {
				/* ok, we found a generic filename. cut the generic. */
				*MinorPtr = '\0';
				d_without_ext = d_without_ext - (MinorPtr - Icon->FlatName);
			}
			else { /* Map the major / minor separator to / */
				*MinorPtr = '/';
			}
		}

//		PrintHash(IconHash, PrintFlat, PrintFile);
//		printf("%s - %s\n", Icon->FlatName, Icon->FileName);
		Put(IconHash, Icon->FlatName, d_without_ext, Icon, DeleteIcon);
//		PrintHash(IconHash, PrintFlat, PrintFile);
	}
	closedir(filedir);
	return 1;
}

const char *GetIconFilename(char *MimeType, size_t len)
{
	void *vIcon;
	IconName *Icon;
	
	if(IconHash == NULL)
		return NULL;

	GetHash(IconHash, MimeType, len, &vIcon), Icon = (IconName*) vIcon;
	/* didn't find the exact mimetype? try major only. */
	if (Icon == NULL) {
		char * pMinor;
		pMinor = strchr(MimeType, '/');
		if (pMinor != NULL) {
			*pMinor = '\0';
			GetHash(IconHash, MimeType, pMinor - MimeType, &vIcon),
				Icon = (IconName*) vIcon;
		}
	}
	if (Icon == NULL) {
		return NULL;
	}

	/*printf("Getting: [%s] == [%s] -> [%s]\n", MimeType, Icon->FlatName, Icon->FileName);*/
	return Icon->FileName;
}

void ShutDownLibCitadelMime(void)
{
	DeleteHash(&IconHash);
}
