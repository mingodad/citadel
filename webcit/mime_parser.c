/*
 * $Id$
 *
 * This is the MIME parser brought over from the Citadel server source code.
 * We use it to handle HTTP uploads, which are sent in MIME format.  In the
 * future we'll use it to output MIME messages as well.
 *
 * Copyright (c) 1998-2001 by Art Cancro
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
 * Utility function to "readline" from memory
 * (returns new pointer)
 */
char *memreadline(char *start, char *buf, int maxlen)
{
	char ch;
	char *ptr;
	int len = 0;	/* tally our own length to avoid strlen() delays */

	ptr = start;
	memset(buf, 0, maxlen);

	while (1) {
		ch = *ptr++;
		if ( (len < (maxlen - 1)) && (ch != 13) && (ch != 10) ) {
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
 * For non-multipart messages, we need to generate a quickie partnum of "1"
 * to return to callback functions.  Some callbacks demand it.
 */
char *fixed_partnum(char *supplied_partnum) {
	if (supplied_partnum == NULL) return "1";
	if (strlen(supplied_partnum)==0) return "1";
	return supplied_partnum;
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
	struct stat statbuf;
	int sendpipe[2];
	int recvpipe[2];
	int childpid;
	size_t bytes_sent = 0;
	size_t bytes_recv = 0;
	size_t blocksize;
	int write_error = 0;

	fprintf(stderr, "mime_decode() called\n");

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
		fprintf(stderr, "ERROR: unknown MIME encoding '%s'\n", encoding);
		return;
	}
	/*
	 * Allocate a buffer for the decoded data.  The output buffer is the
	 * same size as the input buffer; this assumes that the decoded data
	 * will never be larger than the encoded data.  This is a safe
	 * assumption with base64, uuencode, and quoted-printable.  Just to
	 * be safe, we still pad the buffer a bit.
	 */
	decoded = malloc(length + 1024);
	if (decoded == NULL) {
		fprintf(stderr, "ERROR: cannot allocate memory.\n");
		return;
	}
	if (pipe(sendpipe) != 0)
		return;
	if (pipe(recvpipe) != 0)
		return;

	childpid = fork();
	if (childpid < 0) {
		free(decoded);
		return;
	}
	if (childpid == 0) {
		close(2);
		/* send stdio to the pipes */
		if (dup2(sendpipe[0], 0) < 0)
			fprintf(stderr, "ERROR dup2()\n");
		if (dup2(recvpipe[1], 1) < 0)
			fprintf(stderr, "ERROR dup2()\n");
		close(sendpipe[1]);	/* Close the ends we're not using */
		close(recvpipe[0]);
		if (!strcasecmp(encoding, "base64"))
			execlp("./base64", "base64", "-d", NULL);
		else if (!strcasecmp(encoding, "quoted-printable"))
			execlp("./qpdecode", "qpdecode", NULL);
		fprintf(stderr, "ERROR: cannot exec decoder for %s\n", encoding);
		exit(1);
	}
	close(sendpipe[0]);	/* Close the ends we're not using  */
	close(recvpipe[1]);

	while ((bytes_sent < length) && (write_error == 0)) {
		/* Empty the input pipe FIRST */
		while (fstat(recvpipe[0], &statbuf), (statbuf.st_size > 0)) {
			blocksize = read(recvpipe[0], &decoded[bytes_recv],
					 statbuf.st_size);
			if (blocksize < 0)
				fprintf(stderr, "ERROR: cannot read from pipe\n");
			else
				bytes_recv = bytes_recv + blocksize;
		}
		/* Then put some data into the output pipe */
		blocksize = length - bytes_sent;
		if (blocksize > 2048)
			blocksize = 2048;
		if (write(sendpipe[1], &part_start[bytes_sent], blocksize) < 0) {
			fprintf(stderr, "ERROR: cannot write to pipe: %s\n",
				strerror(errno));
			write_error = 1;
		}
		bytes_sent = bytes_sent + blocksize;
	}
	close(sendpipe[1]);
	/* Empty the input pipe */
	while ((blocksize = read(recvpipe[0], &decoded[bytes_recv], 1)),
	       (blocksize > 0)) {
		bytes_recv = bytes_recv + blocksize;
	}

	if (bytes_recv > 0) if (CallBack != NULL) {
		CallBack(name, filename, fixed_partnum(partnum),
			disposition, decoded,
			content_type, bytes_recv, "binary", userdata);
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
	char *part_start, *part_end;
	char buf[SIZ];
	char header[SIZ];
	char boundary[SIZ];
	char startary[SIZ];
	char endary[SIZ];
	char content_type[SIZ];
	char encoding[SIZ];
	char disposition[SIZ];
	char name[SIZ];
	char filename[SIZ];
	int is_multipart;
	int part_seq = 0;
	int i;
	size_t length;
	char nested_partnum[SIZ];

	fprintf(stderr, "the_mime_parser() called\n");
	ptr = content_start;
	memset(boundary, 0, sizeof boundary);
	memset(content_type, 0, sizeof content_type);
	memset(encoding, 0, sizeof encoding);
	memset(name, 0, sizeof name);
	memset(filename, 0, sizeof filename);
	memset(disposition, 0, sizeof disposition);

	/* Learn interesting things from the headers */
	strcpy(header, "");
	do {
		ptr = memreadline(ptr, buf, sizeof buf);
		if (*ptr == 0)
			return;	/* premature end of message */
		if (content_end != NULL)
			if (ptr >= content_end)
				return;

		for (i = 0; i < strlen(buf); ++i)
			if (isspace(buf[i]))
				buf[i] = ' ';
		if (!isspace(buf[0])) {
			if (!strncasecmp(header, "Content-type: ", 14)) {
				strcpy(content_type, &header[14]);
				extract_key(name, content_type, "name");
			}
			if (!strncasecmp(header, "Content-Disposition: ", 21)) {
				strcpy(disposition, &header[21]);
				extract_key(filename, disposition, "filename");
			}
			if (!strncasecmp(header,
				      "Content-transfer-encoding: ", 27))
				strcpy(encoding, &header[27]);
			if (strlen(boundary) == 0)
				extract_key(boundary, header, "boundary");
			strcpy(header, "");
		}
		if ((strlen(header) + strlen(buf) + 2) < sizeof(header))
			strcat(header, buf);
	} while ((strlen(buf) > 0) && (*ptr != 0));

	for (i = 0; i < strlen(disposition); ++i)
		if (disposition[i] == ';')
			disposition[i] = 0;
	while (isspace(disposition[0]))
		strcpy(disposition, &disposition[1]);
	for (i = 0; i < strlen(content_type); ++i)
		if (content_type[i] == ';')
			content_type[i] = 0;
	while (isspace(content_type[0]))
		strcpy(content_type, &content_type[1]);

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
		/*
		if (CallBack != NULL) {
			CallBack("", "", fixed_partnum(partnum),
				"", NULL, content_type,
				0, encoding, userdata);
		}
		 */

		/* Figure out where the boundaries are */
		sprintf(startary, "--%s", boundary);
		sprintf(endary, "--%s--", boundary);
		do {
			part_end = ptr;
			ptr = memreadline(ptr, buf, sizeof buf);
			if (content_end != NULL)
				if (ptr >= content_end) goto END_MULTI;

			if ( (!strcasecmp(buf, startary))
			   || (!strcasecmp(buf, endary)) ) {
				if (part_start != NULL) {
					if (strlen(partnum) > 0) {
						sprintf(nested_partnum, "%s.%d",
							partnum, ++part_seq);
					}
					else {
						sprintf(nested_partnum, "%d",
							++part_seq);
					}
					the_mime_parser(nested_partnum,
						    part_start, part_end,
							CallBack,
							PreMultiPartCallBack,
							PostMultiPartCallBack,
							userdata,
							dont_decode);
				}
				part_start = ptr;
			}
		} while ( (strcasecmp(buf, endary)) && (ptr != 0) );
END_MULTI:	if (PostMultiPartCallBack != NULL) {
			PostMultiPartCallBack("", "", partnum, "", NULL,
				content_type, 0, encoding, userdata);
		}
		return;
	}

	/* If it's not a multipart message, then do something with it */
	if (!is_multipart) {
		part_start = ptr;
		length = 0;
		while ((*ptr != 0)
		      && ((content_end == NULL) || (ptr < content_end))) {
			++length;
			part_end = ptr++;
		}
		mime_decode(partnum,
			    part_start, length,
			    content_type, encoding, disposition,
			    name, filename,
			    CallBack, NULL, NULL,
			    userdata, dont_decode);
	}


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

	fprintf(stderr, "mime_parser() called\n");
	the_mime_parser("", content_start, content_end,
			CallBack,
			PreMultiPartCallBack,
			PostMultiPartCallBack,
			userdata, dont_decode);
}
