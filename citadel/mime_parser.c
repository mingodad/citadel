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
 * Break out the components of a multipart message
 * (This function expects to be fed HEADERS + CONTENT)
 */
void mime_parser(char *content,
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
	char content_type[256];
	char encoding[256];
	int content_length;
	int i;

	ptr = content;
	bzero(boundary, sizeof boundary);
	bzero(content_type, sizeof content_type);
	bzero(encoding, sizeof encoding);
	content_length = 0;

	/* Learn interesting things from the headers */
	strcpy(header, "");
	do {
		ptr = memreadline(ptr, buf, sizeof buf);
		for (i=0; i<strlen(buf); ++i)
			if (isspace(buf[i])) buf[i]=' ';
		if (!isspace(buf[0])) {
			if (!strncasecmp(header, "Content-type: ", 14))
				strcpy(content_type, &header[14]);
			if (!strncasecmp(header, "Content-length: ", 16))
				content_length = atoi(&header[16]);
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

	cprintf("Content type is <%s>\n", content_type);
	cprintf("Encoding is <%s>\n", encoding);
	cprintf("Content length is %d\n", content_length);
	cprintf("Boundary is <%s>\n", boundary);

	if (*ptr == 0) return; /* premature end of message */

	/* If this is a multipart message, then recursively process it */
	if (strlen(boundary)>0) {
		}
	

	}
