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

void mime_parser(char *content, int ContentLength, char *ContentType) {
	char boundary[256];
	int have_boundary = 0;
	int a;
	char *ptr;
	int bytes_processed = 0;

	fprintf(stderr, "MIME: ContentLength: %d, ContentType: %s\n",
		ContentLength, ContentType);

	/* Figure out what the boundary is */
	strcpy(boundary, ContentType);
	for (a=0; a<strlen(boundary); ++a) {
		if (!strncasecmp(&boundary[a], "boundary=", 9)) {
			strcpy(boundary, &boundary[a+10]);
			have_boundary = 1;
			}
		if ((boundary[a]==13) || (boundary[a]==10)) {
			boundary[a] = 0;
			}
		}

	/* We can't process multipart messages without a boundary. */
	if (have_boundary == 0) return;

	ptr = content;

	/* Seek to the beginning of the next boundary */
	while ( (bytes_processed < ContentLength)
	      && (strncasecmp(ptr, boundary, strlen(boundary))) ) {
		++ptr;
		++bytes_processed;

		/* Seek to the end of the boundary string */
		if (!strncasecmp(ptr, boundary, strlen(boundary))) {
			fprintf(stderr, "MIME: founda bounda\n");
			while ( (bytes_processed < ContentLength)
	      		      && (strncasecmp(ptr, "\n", 1)) ) {
				++ptr;
				++bytes_processed;
				}
			}
		}
	}
