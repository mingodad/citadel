/*
 * mime_parser.c
 *
 * This is a really bad attempt at writing a parser to handle MIME-encoded
 * messages.
 *
 * Copyright (c) 1998-1999 by Art Cancro
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
#include "mime_parser.h"



void extract_key(char *target, char *source, char *key) {
	int a, b;

	strcpy(target, source);
	for (a=0; a<strlen(target); ++a) {
		if ((!strncasecmp(&target[a], key, strlen(key)))
		   && (target[a+strlen(key)]=='=')) {
			strcpy(target, &target[a+strlen(key)+1]);
			if (target[0]==34) strcpy(target, &target[1]);
			for (b=0; b<strlen(target); ++b)
				if (target[b]==34) target[b]=0;
			return;
			}
		}
	strcpy(target, "");
	}



	/**** OTHERWISE, HERE'S WHERE WE HANDLE THE STUFF!! *****

	CallBack(name, filename, "", content, content_type, length);

	**** END OF STUFF-HANDLER ****/


/* 
 * Utility function to "readline" from memory
 * (returns new pointer)
 */
char *memreadline(char *start, char *buf, int maxlen) {
	char ch;
	char *ptr;

	ptr = start;
	bzero(buf, maxlen);

	while(1) {
		ch = *ptr++;
		if ((ch==10)||(ch==0)) {
			if (strlen(buf)>0)
				if (buf[strlen(buf)-1]==13)
					buf[strlen(buf)-1] = 0;
			return ptr;
			}
		if (strlen(buf) < (maxlen-1)) {
			buf[strlen(buf)+1] = 0;
			buf[strlen(buf)] = ch;
			}
		}
	}

/*
 * Given a message or message-part body and a length, handle any necessary
 * decoding and pass the request up the stack.
 */
void mime_decode(char *partnum,
		char *part_start, size_t length,
		char *content_type, char *encoding,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		) {

	cprintf("part=%s, type=%s, length=%d, encoding=%s\n",
		partnum, content_type, length, encoding);

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
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		) {

	char *ptr;
	char *part_start, *part_end;
	char buf[256];
	char header[256];
	char boundary[256];
	char startary[256];
	char endary[256];
	char content_type[256];
	char encoding[256];
	int is_multipart;
	int part_seq = 0;
	int i;
	size_t length;
	char nested_partnum[256];

	ptr = content_start;
	bzero(boundary, sizeof boundary);
	bzero(content_type, sizeof content_type);
	bzero(encoding, sizeof encoding);

	/* Learn interesting things from the headers */
	strcpy(header, "");
	do {
		ptr = memreadline(ptr, buf, sizeof buf);
		if (*ptr == 0) return; /* premature end of message */
		if (content_end != NULL)
			if (ptr >= content_end) return;

		for (i=0; i<strlen(buf); ++i)
			if (isspace(buf[i])) buf[i]=' ';
		if (!isspace(buf[0])) {
			if (!strncasecmp(header, "Content-type: ", 14))
				strcpy(content_type, &header[14]);
			if (!strncasecmp(header,
				"Content-transfer-encoding: ", 27))
					strcpy(encoding, &header[27]);
			if (strlen(boundary)==0)
				extract_key(boundary, header, "boundary");
			strcpy(header, "");
			}
		if ((strlen(header)+strlen(buf)+2)<sizeof(header))
			strcat(header, buf);
		} while ((strlen(buf) > 0) && (*ptr != 0));

	for (i=0; i<strlen(content_type); ++i) 
		if (content_type[i]==';') content_type[i] = 0;

	if (strlen(boundary) > 0) {
		is_multipart = 1;
		}
	else {
		is_multipart = 0;
		}

	/* If this is a multipart message, then recursively process it */
	part_start = NULL;
	if (is_multipart) {
		sprintf(startary, "--%s", boundary);
		sprintf(endary, "--%s--", boundary);
		do {
			part_end = ptr;
			ptr = memreadline(ptr, buf, sizeof buf);
			if (*ptr == 0) return; /* premature end of message */
			if (content_end != NULL)
				if (ptr >= content_end) return;
			if ((!strcasecmp(buf, startary))
			    ||(!strcasecmp(buf, endary))) {
				if (part_start != NULL) {
					sprintf(nested_partnum, "%s.%d",
						partnum, ++part_seq);
					the_mime_parser(nested_partnum,
							part_start, part_end,
							CallBack);
					}
				part_start = ptr;
				}
			} while (strcasecmp(buf, endary));
		}

	/* If it's not a multipart message, then do something with it */
	if (!is_multipart) {
		part_start = ptr;
		length = 0;
		while ((*ptr != 0)&&((content_end==NULL)||(ptr<content_end))) {
			++length;
			part_end = ptr++;
			}
		mime_decode(partnum,
				part_start, length,
				content_type, encoding, CallBack);
		}
	
	}

/*
 * Entry point for the MIME parser.
 * (This function expects to be fed HEADERS + CONTENT)
 * Note: NULL can be supplied as content_end; in this case, the message is
 * considered to have ended when the parser encounters a 0x00 byte.
 */
void mime_parser(char *content_start, char *content_end,
		void (*CallBack)
			(char *cbname,
			char *cbfilename,
			char *cbencoding,
			void *cbcontent,
			char *cbtype,
			size_t cblength)
		) {

	the_mime_parser("1", content_start, content_end, CallBack);
	}
