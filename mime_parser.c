/*
 * $Id$
 *
 * This is the MIME parser for Citadel.  Sometimes it actually works.
 *
 * Copyright (c) 1998-2003 by Art Cancro
 * This code is distributed under the terms of the GNU General Public License.
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

#include "webcit.h"

#include "mime_parser.h"


void extract_key(char *target, char *source, char *key)
{
	int a, b;

	strcpy(target, source);
	for (a = 0; a < strlen(target); ++a) {
		if ((!strncasecmp(&target[a], key, strlen(key)))
		    && (target[a + strlen(key)] == '=')) {
			strcpy(target, &target[a + strlen(key) + 1]);
			if (target[0] == 34)
				strcpy(target, &target[1]);
			for (b = 0; b < strlen(target); ++b)
				if (target[b] == 34)
					target[b] = 0;
			return;
		}
	}
	strcpy(target, "");
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
 */
int CtdlDecodeQuotedPrintable(char *decoded, char *encoded, int sourcelen) {
	char buf[SIZ];
	int buf_length = 0;
	int soft_line_break = 0;
	int ch;
	int decoded_length = 0;
	int i;

	decoded[0] = 0;
	decoded_length = 0;
	buf[0] = 0;
	buf_length = 0;

	for (i = 0; i < sourcelen; ++i) {

		buf[buf_length++] = encoded[i];

		if ( (encoded[i] == '\n')
		   || (encoded[i] == 0)
		   || (i == (sourcelen-1)) ) {
			buf[buf_length++] = 0;

			/*** begin -- process one line ***/

			if (buf[strlen(buf)-1] == '\n') {
				buf[strlen(buf)-1] = 0;
			}
			if (buf[strlen(buf)-1] == '\r') {
				buf[strlen(buf)-1] = 0;
			}
			while (isspace(buf[strlen(buf)-1])) {
				buf[strlen(buf)-1] = 0;
			}
			soft_line_break = 0;

			while (strlen(buf) > 0) {
				if (!strcmp(buf, "=")) {
					soft_line_break = 1;
					strcpy(buf, "");
				} else if ((strlen(buf)>=3) && (buf[0]=='=')) {
					sscanf(&buf[1], "%02x", &ch);
					decoded[decoded_length++] = ch;
					strcpy(buf, &buf[3]);
				} else {
					decoded[decoded_length++] = buf[0];
					strcpy(buf, &buf[1]);
				}
			}
			if (soft_line_break == 0) {
				decoded[decoded_length++] = '\r';
				decoded[decoded_length++] = '\n';
			}
			buf_length = 0;
			/*** end -- process one line ***/
		}
	}

	decoded[decoded_length++] = 0;
	return(decoded_length);
}

/*
 * Given a message or message-part body and a length, handle any necessary
 * decoding and pass the request up the stack.
 */
void mime_decode(char *partnum,
		 char *part_start, size_t length,
		 char *content_type, char *encoding,
		 char *disposition,
		 char *name, char *filename,
		 void (*CallBack)
		  (char *cbname,
		   char *cbfilename,
		   char *cbpartnum,
		   char *cbdisp,
		   void *cbcontent,
		   char *cbtype,
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
				content_type, length, encoding, userdata);
			}
		return;
	}
	
	if ((strcasecmp(encoding, "base64"))
	    && (strcasecmp(encoding, "quoted-printable"))) {
		return;
	}
	/*
	 * Allocate a buffer for the decoded data.  The output buffer is the
	 * same size as the input buffer; this assumes that the decoded data
	 * will never be larger than the encoded data.  This is a safe
	 * assumption with base64, uuencode, and quoted-printable.
	 */
	decoded = mallok(length+2048);
	if (decoded == NULL) {
		return;
	}

	if (!strcasecmp(encoding, "base64")) {
		bytes_decoded = CtdlDecodeBase64(decoded, part_start, length);
	}
	else if (!strcasecmp(encoding, "quoted-printable")) {
		bytes_decoded = CtdlDecodeQuotedPrintable(decoded,
							part_start, length);
	}

	if (bytes_decoded > 0) if (CallBack != NULL) {
		CallBack(name, filename, fixed_partnum(partnum),
			disposition, decoded,
			content_type, bytes_decoded, "binary", userdata);
	}

	phree(decoded);
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
		       size_t cblength,
		       char *cbencoding,
		       void *cbuserdata),
		      void *userdata,
		      int dont_decode
)
{

	char *ptr;
	char *part_start, *part_end = NULL;
	char buf[SIZ];
	char *header;
	char *boundary;
	char *startary;
	char *endary;
	char *content_type;
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
	char nested_partnum[SIZ];

	ptr = content_start;
	content_length = 0;

	boundary = mallok(SIZ);
	memset(boundary, 0, SIZ);

	startary = mallok(SIZ);
	memset(startary, 0, SIZ);

	endary = mallok(SIZ);
	memset(endary, 0, SIZ);

	header = mallok(SIZ);
	memset(header, 0, SIZ);

	content_type = mallok(SIZ);
	memset(content_type, 0, SIZ);

	encoding = mallok(SIZ);
	memset(encoding, 0, SIZ);

	content_type_name = mallok(SIZ);
	memset(content_type_name, 0, SIZ);

	content_disposition_name = mallok(SIZ);
	memset(content_disposition_name, 0, SIZ);

	filename = mallok(SIZ);
	memset(filename, 0, SIZ);

	disposition = mallok(SIZ);
	memset(disposition, 0, SIZ);

	/* If the caller didn't supply an endpointer, generate one by measure */
	if (content_end == NULL) {
		content_end = &content_start[strlen(content_start)];
	}

	/* Learn interesting things from the headers */
	strcpy(header, "");
	do {
		ptr = memreadline(ptr, buf, SIZ);
		if (ptr >= content_end) {
			goto end_parser;
		}

		for (i = 0; i < strlen(buf); ++i)
			if (isspace(buf[i]))
				buf[i] = ' ';
		if (!isspace(buf[0])) {
			if (!strncasecmp(header, "Content-type: ", 14)) {
				strcpy(content_type, &header[14]);
				extract_key(content_type_name, content_type, "name");
				/* Deal with weird headers */
				if (strchr(content_type, ' '))
					*(strchr(content_type, ' ')) = '\0';
				if (strchr(content_type, ';'))
					*(strchr(content_type, ';')) = '\0';
			}
			if (!strncasecmp(header, "Content-Disposition: ", 21)) {
				strcpy(disposition, &header[21]);
				extract_key(content_disposition_name, disposition, "name");
				extract_key(filename, disposition, "filename");
			}
			if (!strncasecmp(header, "Content-length: ", 16)) {
				content_length = (size_t) atol(&header[16]);
			}
			if (!strncasecmp(header,
				      "Content-transfer-encoding: ", 27))
				strcpy(encoding, &header[27]);
			if (strlen(boundary) == 0)
				extract_key(boundary, header, "boundary");
			strcpy(header, "");
		}
		if ((strlen(header) + strlen(buf) + 2) < SIZ)
			strcat(header, buf);
	} while ((strlen(buf) > 0) && (*ptr != 0));

	if (strchr(disposition, ';'))
		*(strchr(disposition, ';')) = '\0';
	striplt(disposition);
	if (strchr(content_type, ';'))
		*(strchr(content_type, ';')) = '\0';
	striplt(content_type);

	if (strlen(boundary) > 0) {
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
				NULL, content_type,
				0, encoding, userdata);
		}

		/* Figure out where the boundaries are */
		snprintf(startary, SIZ, "--%s", boundary);
		snprintf(endary, SIZ, "--%s--", boundary);
		do {
			if ( (!strncasecmp(ptr, startary, strlen(startary)))
			   || (!strncasecmp(ptr, endary, strlen(endary))) ) {
				if (part_start != NULL) {
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
							dont_decode);
				}
				ptr = memreadline(ptr, buf, SIZ);
				part_start = ptr;
			}
			else {
				part_end = ptr;
				++ptr;
			}
			/* If we pass out of scope in the MIME multipart (by
			 * hitting the end boundary), force the pointer out
			 * of scope so this loop ends.
			 */
			if (ptr < content_end) {
				if (!strcasecmp(ptr, endary)) {
					ptr = content_end++;
				}
			}
		} while (ptr <= content_end);
		if (PostMultiPartCallBack != NULL) {
			PostMultiPartCallBack("", "", partnum, "", NULL,
				content_type, 0, encoding, userdata);
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
		/* fix an off-by-one error */
		--part_end;
		--length;
		
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
		
		mime_decode(partnum,
			    part_start, length,
			    content_type, encoding, disposition,
			    name, filename,
			    CallBack, NULL, NULL,
			    userdata, dont_decode);
	}

end_parser:	/* free the buffers!  end the oppression!! */
	phree(boundary);
	phree(startary);
	phree(endary);	
	phree(header);
	phree(content_type);
	phree(encoding);
	phree(content_type_name);
	phree(content_disposition_name);
	phree(filename);
	phree(disposition);
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

