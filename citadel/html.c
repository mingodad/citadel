/*
 * html.c -- Functions which handle translation between HTML and plain text
 * $Id$
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include <syslog.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "tools.h"
#include "room_ops.h"
#include "html.h"

/*
 * Convert HTML to plain text.
 */
char *html_to_ascii(char *inputmsg, int screenwidth) {
	char inbuf[256];
	char outbuf[256];
	char tag[1024];
	int done_reading = 0;
	char *readptr, *outputbuf;
	size_t input_length, this_read, output_length;
	int i, ch, did_out, rb;
	int nest = 0;		/* Bracket nesting level */

	input_length = strlen(inputmsg);
	readptr = inputmsg;
	output_length = strlen(inputmsg);
	outputbuf = mallok(output_length);
	if (outputbuf==NULL) return NULL;
	strcpy(inbuf, "");
	strcpy(outbuf, "");

	lprintf(9, "Decoding %d bytes of HTML\n", input_length);

	do {
		/* Fill the input buffer */
		if ( (done_reading == 0) && (strlen(inbuf) < 128) ) {

			/* copy from the input buffer */
			lprintf(9, "input loop\n");
			this_read = strlen(readptr);
			if (this_read > 127) this_read = 127;
			lprintf(9, "%d bytes\n", this_read);
			for (i=0; i<this_read; ++i) {
				inbuf[strlen(inbuf)+1] = 0;
				inbuf[strlen(inbuf)] = readptr[0];
				++readptr;
			}

			if (strlen(readptr)==0) done_reading = 1;
		}
		else {
			lprintf(9, "skipped input loop\n");
		}

		/* Do some parsing */
		lprintf(9, "parse loop\n");
		if (strlen(inbuf)>0) {

		    /* Fold in all the spacing */
			lprintf(9, "spacing loop\n");
		    for (i=0; i<strlen(inbuf); ++i) {
			if (inbuf[i]==10) inbuf[i]=32;
			if (inbuf[i]==13) inbuf[i]=32;
			if (inbuf[i]==9) inbuf[i]=32;
			if ((inbuf[i]<32) || (inbuf[i]>126))
				strcpy(&inbuf[i], &inbuf[i+1]);
			while ((inbuf[i]==32)&&(inbuf[i+1]==32))
				strcpy(&inbuf[i], &inbuf[i+1]);
		    }

			lprintf(9, "foo loop\n");
		    for (i=0; i<strlen(inbuf); ++i) {

			ch = inbuf[i];

			if (ch == '<') {
			lprintf(9, "bar loop\n");
				++nest;
				strcpy(tag, "");
			}

			else if (ch == '>') {
			lprintf(9, "baz loop\n");
				if (nest > 0) --nest;
				
				if (!strcasecmp(tag, "P")) {
					strcat(outbuf, "\n\n");
				}

				else if (!strcasecmp(tag, "H1")) {
					strcat(outbuf, "\n\n");
				}

				else if (!strcasecmp(tag, "H2")) {
					strcat(outbuf, "\n\n");
				}

				else if (!strcasecmp(tag, "H3")) {
					strcat(outbuf, "\n\n");
				}

				else if (!strcasecmp(tag, "H4")) {
					strcat(outbuf, "\n\n");
				}

				else if (!strcasecmp(tag, "/H1")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "/H2")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "/H3")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "/H4")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "HR")) {
					strcat(outbuf, "\n ");
					for (i=0; i<screenwidth-2; ++i)
						strcat(outbuf, "-");
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "BR")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "TR")) {
					strcat(outbuf, "\n");
				}

				else if (!strcasecmp(tag, "/TABLE")) {
					strcat(outbuf, "\n");
				}

			}

			else if ((nest > 0) && (strlen(tag)<(sizeof(tag)-1))) {
				tag[strlen(tag)+1] = 0;
				tag[strlen(tag)] = ch;
			}
				
			else if (!nest) {
				outbuf[strlen(outbuf)+1] = 0;
				outbuf[strlen(outbuf)] = ch;
			}
		    }
		    strcpy(inbuf, &inbuf[i]);
		}

		lprintf(9, "checkquepoynte\n");

		/* Convert &; tags to the forbidden characters */
		if (strlen(outbuf)>0) for (i=0; i<strlen(outbuf); ++i) {
			lprintf(9, "eek loop\n");

			if (!strncasecmp(&outbuf[i], "&nbsp;", 6)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

			else if (!strncasecmp(&outbuf[i], "&lb;", 4)) {
				outbuf[i] = '<';
				strcpy(&outbuf[i+1], &outbuf[i+4]);
			}

			else if (!strncasecmp(&outbuf[i], "&rb;", 4)) {
				outbuf[i] = '>';
				strcpy(&outbuf[i+1], &outbuf[i+4]);
			}

			else if (!strncasecmp(&outbuf[i], "&amp;", 5)) {
				strcpy(&outbuf[i+1], &outbuf[i+5]);
			}

			else if (!strncasecmp(&outbuf[i], "&quot;", 6)) {
				outbuf[i] = '\"';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

		}

		/* Make sure the output buffer is big enough */
		if ((strlen(outputbuf) + strlen(outbuf) + 2) > output_length) {
			lprintf(9, "realloc loop\n");
			output_length = output_length + strlen(outbuf) + 2;
			outputbuf = reallok(outputbuf, output_length);
		}

		/* Output any lines terminated with hard line breaks */
		lprintf(9, "output loop 1\n");
		do {
			did_out = 0;
			if (strlen(outbuf)>0)
			    for (i = 0; i<strlen(outbuf); ++i) {
				if ( (i<(screenwidth-2)) && (outbuf[i]=='\n')) {
					strncat(outputbuf, outbuf, i+1);
					strcpy(outbuf, &outbuf[i+1]);
					i = 0;
					did_out = 1;
				}
			}
		} while (did_out);

		/* Add soft line breaks */
		lprintf(9, "output loop 2\n");
		if (strlen(outbuf) > (screenwidth - 2)) {
			rb = (-1);
			for (i=0; i<(screenwidth-2); ++i) {
				if (outbuf[i]==32) rb = i;
			}
			if (rb>=0) {
				strncat(outputbuf, outbuf, rb);
				strcat(outputbuf, "\n");
				strcpy(outbuf, &outbuf[rb+1]);
			} else {
				strncat(outputbuf, outbuf, screenwidth-2);
				strcat(outputbuf, "\n");
				strcpy(outbuf, &outbuf[screenwidth-2]);
			}
		}

	} while (done_reading == 0);
	lprintf(9, "output loop 3\n");
	strncat(outputbuf, outbuf, strlen(outbuf));
	strcat(outputbuf, "\n");

	return outputbuf;
}
