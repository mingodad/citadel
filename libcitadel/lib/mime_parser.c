/*
 * $Id$
 *
 * This is the MIME parser for Citadel.
 *
 * Copyright (c) 1998-2007 by the citadel.org development team.
 * This code is distributed under the GNU General Public License v3.
 *
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

#include "xdgmime/xdgmime.h"
#include "libcitadel.h"

void extract_key(char *target, char *source, char *key)
{
	char *ptr;
	char looking_for[256];
	int double_quotes = 0;

	snprintf(looking_for, sizeof looking_for, "%s=", key);

	ptr = bmstrcasestr(source, looking_for);
	if (ptr == NULL) {
		strcpy(target, "");
		return;
	}
	strcpy(target, (ptr + strlen(looking_for)));

	for (ptr=target; (*ptr != 0); ++ptr) {

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
		if (!strncmp(&encoded[pos], "=\r\n", 3))
		{
			pos += 3;
		}
		else if (!strncmp(&encoded[pos], "=\n", 2))
		{
			pos += 2;
		}
		else if (encoded[pos] == '=')
		{
			ch = 0;
			sscanf(&encoded[pos+1], "%02x", &ch);
			pos += 3;
			decoded[decoded_length++] = ch;
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
		 char *name, char *filename,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		 void (*PreMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		 void (*PostMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),
		  void *userdata,
		  int dont_decode
)
{

	char *decoded;
	size_t bytes_decoded = 0;

	/* Some encodings aren't really encodings */
	if (!strcasecmp(encoding, "7bit"))
		strcpy(encoding, "");
	if (!strcasecmp(encoding, "8bit"))
		strcpy(encoding, "");
	if (!strcasecmp(encoding, "binary"))
		strcpy(encoding, "");

	/* If this part is not encoded, send as-is */
	if ( (strlen(encoding) == 0) || (dont_decode)) {
		if (CallBack != NULL) {
			CallBack(name, filename, fixed_partnum(partnum),
				disposition, part_start,
				content_type, charset, length, encoding, userdata);
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
		CallBack(name, filename, fixed_partnum(partnum),
			disposition, decoded,
			content_type, charset, bytes_decoded, "binary", userdata);
	}

	free(decoded);
}

/*
 * Break out the components of a multipart message
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
void the_mime_parser(char *partnum,
		     char *content_start, char *content_end,
		     void (*CallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		     void (*PreMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		     void (*PostMultiPartCallBack)
		      (char *cbname,
		       char *cbfilename,
		       char *cbpartnum,
		       char *cbdisp,
		       void *cbcontent,
		       char *cbtype,
		       char *cbcharset,
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		      void *userdata,
		      int dont_decode
)
{

	char *ptr;
	char *srch = NULL;
	char *part_start, *part_end = NULL;
	char buf[SIZ];
	char *header;
	char *boundary;
	char *startary;
	size_t startary_len = 0;
	char *endary;
	char *next_boundary;
	char *content_type;
	char *charset;
	size_t content_length;
	char *encoding;
	char *disposition;
	char *name = NULL;
	char *content_type_name;
	char *content_disposition_name;
	char *filename;
	int is_multipart;
	int part_seq = 0;
	int i;
	size_t length;
	char nested_partnum[256];
	int crlf_in_use = 0;
	char *evaluate_crlf_ptr = NULL;
	int buflen = 0;
	int headerlen = 0;

	ptr = content_start;
	content_length = 0;

	boundary = malloc(SIZ);
	memset(boundary, 0, SIZ);

	startary = malloc(SIZ);
	memset(startary, 0, SIZ);

	endary = malloc(SIZ);
	memset(endary, 0, SIZ);

	header = malloc(SIZ);
	memset(header, 0, SIZ);

	content_type = malloc(SIZ);
	memset(content_type, 0, SIZ);

	charset = malloc(SIZ);
	memset(charset, 0, SIZ);

	encoding = malloc(SIZ);
	memset(encoding, 0, SIZ);

	content_type_name = malloc(SIZ);
	memset(content_type_name, 0, SIZ);

	content_disposition_name = malloc(SIZ);
	memset(content_disposition_name, 0, SIZ);

	filename = malloc(SIZ);
	memset(filename, 0, SIZ);

	disposition = malloc(SIZ);
	memset(disposition, 0, SIZ);

	/* If the caller didn't supply an endpointer, generate one by measure */
	if (content_end == NULL) {
		content_end = &content_start[strlen(content_start)];
	}

	/* Learn interesting things from the headers */
	strcpy(header, "");
	headerlen = 0;
	do {
		ptr = memreadlinelen(ptr, buf, SIZ, &buflen);
		if (ptr >= content_end) {
			goto end_parser;
		}

		for (i = 0; i < buflen; ++i) {
			if (isspace(buf[i])) {
				buf[i] = ' ';
			}
		}

		if (!isspace(buf[0])) {
			if (!strncasecmp(header, "Content-type:", 13)) {
				strcpy(content_type, &header[13]);
				striplt(content_type);
				extract_key(content_type_name, content_type, "name");
				extract_key(charset, content_type, "charset");
				extract_key(boundary, header, "boundary");
				/* Deal with weird headers */
				if (strchr(content_type, ' '))
					*(strchr(content_type, ' ')) = '\0';
				if (strchr(content_type, ';'))
					*(strchr(content_type, ';')) = '\0';
			}
			if (!strncasecmp(header, "Content-Disposition:", 20)) {
				strcpy(disposition, &header[20]);
				striplt(disposition);
				extract_key(content_disposition_name, disposition, "name");
				extract_key(filename, disposition, "filename");
			}
			if (!strncasecmp(header, "Content-length: ", 15)) {
				char clbuf[10];
				safestrncpy(clbuf, &header[15], sizeof clbuf);
				striplt(clbuf);
				content_length = (size_t) atol(clbuf);
			}
			if (!strncasecmp(header, "Content-transfer-encoding: ", 26)) {
				strcpy(encoding, &header[26]);
				striplt(encoding);
			}
			strcpy(header, "");
			headerlen = 0;
		}
		if ((headerlen + buflen + 2) < SIZ) {
			memcpy(&header[headerlen], buf, buflen);
			headerlen += buflen;
			header[headerlen] = '\0';
		}
	} while ((!IsEmptyStr(buf)) && (*ptr != 0));

	if (strchr(disposition, ';'))
		*(strchr(disposition, ';')) = '\0';
	striplt(disposition);
	if (strchr(content_type, ';'))
		*(strchr(content_type, ';')) = '\0';
	striplt(content_type);

	if (!IsEmptyStr(boundary)) {
		is_multipart = 1;
	} else {
		is_multipart = 0;
	}

	/* If this is a multipart message, then recursively process it */
	part_start = NULL;
	if (is_multipart) {

		/* Tell the client about this message's multipartedness */
		if (PreMultiPartCallBack != NULL) {
			PreMultiPartCallBack("", "", partnum, "",
				NULL, content_type, charset,
				0, encoding, userdata);
		}

		/* Figure out where the boundaries are */
		snprintf(startary, SIZ, "--%s", boundary);
		snprintf(endary, SIZ, "--%s--", boundary);
		startary_len = strlen(startary);

		part_start = NULL;
		do {
			next_boundary = NULL;
			for (srch=ptr; srch<content_end; ++srch) {
				if (!memcmp(srch, startary, startary_len)) {
					next_boundary = srch;
					srch = content_end;
				}
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
				the_mime_parser(nested_partnum,
					    part_start, part_end,
						CallBack,
						PreMultiPartCallBack,
						PostMultiPartCallBack,
						userdata,
						dont_decode);
			}

			if (next_boundary != NULL) {
				/* If we pass out of scope, don't attempt to
				 * read past the end boundary. */
				if (!strcmp(next_boundary, endary)) {
					ptr = content_end;
				}
				else {
					/* Set up for the next part. */
					part_start = strstr(next_boundary, "\n");
					
					/* Determine whether newlines are LF or CRLF */
					evaluate_crlf_ptr = part_start;
					--evaluate_crlf_ptr;
					if (!memcmp(evaluate_crlf_ptr, "\r\n", 2)) {
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
		} while ( (ptr < content_end) && (next_boundary != NULL) );

		if (PostMultiPartCallBack != NULL) {
			PostMultiPartCallBack("", "", partnum, "", NULL,
				content_type, charset, 0, encoding, userdata);
		}
		goto end_parser;
	}

	/* If it's not a multipart message, then do something with it */
	if (!is_multipart) {
		part_start = ptr;
		length = 0;
		while (ptr < content_end) {
			++ptr;
			++length;
		}
		part_end = content_end;

		/******
		 * I thought there was an off-by-one error here, but there isn't.
		 * This probably means that there's an off-by-one error somewhere
		 * else ... or maybe only in certain messages?
		--part_end;
		--length;
		******/
		
		/* Truncate if the header told us to */
		if ( (content_length > 0) && (length > content_length) ) {
			length = content_length;
		}

		/* Sometimes the "name" field is tacked on to Content-type,
		 * and sometimes it's tacked on to Content-disposition.  Use
		 * whichever one we have.
		 */
		if (strlen(content_disposition_name) > strlen(content_type_name)) {
			name = content_disposition_name;
		}
		else {
			name = content_type_name;
		}
	
		/* Ok, we've got a non-multipart part here, so do something with it.
		 */
		mime_decode(partnum,
			part_start, length,
			content_type, charset, encoding, disposition,
			name, filename,
			CallBack, NULL, NULL,
			userdata, dont_decode
		);

		/*
		 * Now if it's an encapsulated message/rfc822 then we have to recurse into it
		 */
		if (!strcasecmp(content_type, "message/rfc822")) {

			if (PreMultiPartCallBack != NULL) {
				PreMultiPartCallBack("", "", partnum, "",
					NULL, content_type, charset,
					0, encoding, userdata);
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
					part_start, part_end,
					CallBack,
					PreMultiPartCallBack,
					PostMultiPartCallBack,
					userdata,
					dont_decode
				);
			}
			if (PostMultiPartCallBack != NULL) {
				PostMultiPartCallBack("", "", partnum, "", NULL,
					content_type, charset, 0, encoding, userdata);
			}


		}

	}

end_parser:	/* free the buffers!  end the oppression!! */
	free(boundary);
	free(startary);
	free(endary);	
	free(header);
	free(content_type);
	free(charset);
	free(encoding);
	free(content_type_name);
	free(content_disposition_name);
	free(filename);
	free(disposition);
}



/*
 * Entry point for the MIME parser.
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
void mime_parser(char *content_start,
		char *content_end,

		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),

		 void (*PreMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),

		 void (*PostMultiPartCallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
		   char *cbcharset,
		   size_t cblength,
		   char *cbencoding,
		   void *cbuserdata),

		  void *userdata,
		  int dont_decode
)
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


const char *GuessMimeType(char *data, size_t dlen)
{
	return (xdg_mime_get_mime_type_for_data(data, dlen));
/*
	int MimeIndex = 0;

	while (MyMimes[MimeIndex].PatternLen != 0)
	{
		if ((MyMimes[MimeIndex].PatternLen + 
		     MyMimes[MimeIndex].PatternOffset < dlen) &&
		    strncmp(MyMimes[MimeIndex].Pattern, 
			    &data[MyMimes[MimeIndex].PatternOffset], 
			    MyMimes[MimeIndex].PatternLen) == 0)
		{
			break;
		}
		MimeIndex ++;
	}
	return MyMimes[MimeIndex].MimeString;
*/
}


const char* GuessMimeByFilename(const char *what, size_t len)
{
	return xdg_mime_get_mime_type_from_file_name(what);
/*
	if (!strncasecmp(&what[len - 4], ".gif", 4))
		return "image/gif";
	else if (!strncasecmp(&what[len - 3], ".js", 3))
		return  "text/javascript";
	else if (!strncasecmp(&what[len - 4], ".txt", 4))
		return "text/plain";
	else if (!strncasecmp(&what[len - 4], ".css", 4))
		return "text/css";
	else if (!strncasecmp(&what[len - 4], ".jpg", 4))
		return "image/jpeg";
	else if (!strncasecmp(&what[len - 4], ".png", 4))
		return "image/png";
	else if (!strncasecmp(&what[len - 4], ".ico", 4))
		return "image/x-icon";
	else if (!strncasecmp(&what[len - 5], ".html", 5))
		return "text/html";
	else if (!strncasecmp(&what[len - 4], ".htm", 4))
		return "text/html";
	else if (!strncasecmp(&what[len - 4], ".wml", 4))
		return "text/vnd.wap.wml";
	else if (!strncasecmp(&what[len - 5], ".wmls", 5))
		return "text/vnd.wap.wmlscript";
	else if (!strncasecmp(&what[len - 5], ".wmlc", 5))
		return "application/vnd.wap.wmlc";
	else if (!strncasecmp(&what[len - 6], ".wmlsc", 6))
		return "application/vnd.wap.wmlscriptc";
	else if (!strncasecmp(&what[len - 5], ".wbmp", 5))
		return "image/vnd.wap.wbmp";
	else
		return "application/octet-stream";
*/
}
