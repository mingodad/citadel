/*
 * mime_parser.c
 *
 * This is a really bad attempt at writing a parser to handle multipart
 * messages -- in the case of WebCit, a form containing uploaded files.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

/*
 * The very back end for the component handler
 * (This function expects to be fed CONTENT ONLY, no headers)
 */
void do_something_with_it(char *content, int length, char *content_type) {
	char filename[256];
	int a;
	static char partno = 0;
	FILE *fp;

	/* Nested multipart gets recursively fed back into the parser */
	if (!strncasecmp(content_type, "multipart", 9)) {
		mime_parser(content, length, content_type);
		}	

	/* If all else fails, save the component to disk (FIX) */
	else {
		sprintf(filename, "content.%04x.%04x.%s",
			getpid(), ++partno, content_type);
		for (a=0; a<strlen(filename); ++a)
			if (filename[a]=='/') filename[a]='.';
		fp = fopen(filename, "wb");
		fwrite(content, length, 1, fp);
		fclose(fp);
		}
	}


/*
 * Take a part, figure out its length, and do something with it
 * (This function expects to be fed HEADERS+CONTENT)
 */
void handle_part(char *content, int part_length, char *supplied_content_type) {
	char content_type[256];
	char *start;
	char buf[512];
	int crlf = 0;	/* set to 1 for crlf-style newlines */
	int actual_length;

	strcpy(content_type, supplied_content_type);

	/* Strip off any leading blank lines. */
	start = content;
	while ((!strncmp(start, "\r", 1)) || (!strncmp(start, "\n", 1))) {
		++start;
		--part_length;
		}

	/* At this point all we have left is the headers and the content. */
	do {
		strcpy(buf, "");
		do {
			buf[strlen(buf)+1] = 0;
			if (strlen(buf)<((sizeof buf)-1)) {
				strncpy(&buf[strlen(buf)], start, 1);
				}
			++start;
			--part_length;
			} while((buf[strlen(buf)-1] != 10) && (part_length>0));
		if (part_length <= 0) return;
		buf[strlen(buf)-1] = 0;
		if (buf[strlen(buf)-1]==13) {
			buf[strlen(buf)-1] = 0;
			crlf = 1;
			}
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			strcpy(content_type, &buf[14]);
			}
		} while (strlen(buf)>0);
	
	if (crlf) actual_length = part_length - 2;
	else actual_length = part_length - 1;

	/* Now that we've got this component isolated, what to do with it? */
	do_something_with_it(start, actual_length, content_type);

	}

	
/*
 * Break out the components of a multipart message
 * (This function expects to be fed CONTENT ONLY, no headers)
 */
void mime_parser(char *content, int ContentLength, char *ContentType) {
	char boundary[256];
	char endary[256];
	int have_boundary = 0;
	int a;
	char *ptr;
	char *beginning;
	int bytes_processed = 0;
	int part_length;

	fprintf(stderr, "MIME: ContentLength: %d, ContentType: %s\n",
		ContentLength, ContentType);

	/* If it's not multipart, don't process it as multipart */
	if (strncasecmp(ContentType, "multipart", 9)) {
		do_something_with_it(content, ContentLength, ContentType);
		return;
		}

	/* Figure out what the boundary is */
	strcpy(boundary, ContentType);
	for (a=0; a<strlen(boundary); ++a) {
		if (!strncasecmp(&boundary[a], "boundary=", 9)) {
			boundary[0]='-';
			boundary[1]='-';
			strcpy(&boundary[2], &boundary[a+9]);
			have_boundary = 1;
			a = 0;
			}
		if ((boundary[a]==13) || (boundary[a]==10)) {
			boundary[a] = 0;
			}
		}

	/* We can't process multipart messages without a boundary. */
	if (have_boundary == 0) return;
	strcpy(endary, boundary);
	strcat(endary, "--");

	ptr = content;

	/* Seek to the beginning of the next boundary */
	while (bytes_processed < ContentLength) {
	      /* && (strncasecmp(ptr, boundary, strlen(boundary))) ) { */

		if (strncasecmp(ptr, boundary, strlen(boundary))) {
			++ptr;
			++bytes_processed;
			}

		/* See if we're at the end */
		if (!strncasecmp(ptr, endary, strlen(endary))) {
			fprintf(stderr, "MIME: the end.\n");
			return;
			}

		/* Seek to the end of the boundary string */
		if (!strncasecmp(ptr, boundary, strlen(boundary))) {
			while ( (bytes_processed < ContentLength)
	      		      && (strncasecmp(ptr, "\n", 1)) ) {
				++ptr;
				++bytes_processed;
				}
			beginning = ptr;
			part_length = 0;
			while ( (bytes_processed < ContentLength)
	      		  && (strncasecmp(ptr, boundary, strlen(boundary))) ) {
				++ptr;
				++bytes_processed;
				++part_length;
				}
			handle_part(beginning, part_length, "");
			/* Back off so we can see the next boundary */
			--ptr;
			--bytes_processed;
			}
		}
	}
