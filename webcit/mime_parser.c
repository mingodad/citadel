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
 * Take a part, figure out its length, and do something with it
 */
void process_part(char *content, int part_length) {
	FILE *fp;
	char filename[256];
	static char partno = 0;

	fprintf(stderr, "MIME: process_part() called with a length o' %d\n",
		part_length);

	sprintf(filename, "content.%04x.%04x", getpid(), ++partno);
	fp = fopen(filename, "wb");
	fwrite(content, part_length, 1, fp);
	fclose(fp);
	}

	
/*
 * Main function of parser
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
	fprintf(stderr, "MIME: boundary is %s\n", boundary);
	fprintf(stderr, "MIME:   endary is %s\n", endary);

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
			fprintf(stderr, "MIME: founda bounda\n");
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
			process_part(beginning, part_length);
			/* Back off so we can see the next boundary */
			--ptr;
			--bytes_processed;
			}
		}
	}
