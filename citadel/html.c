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
	char *inptr;
	int i, j, ch, did_out, rb;
	int nest = 0;		/* Bracket nesting level */

	inptr = inputmsg;
	strcpy(inbuf, "");
	strcpy(outbuf, "");

	do {
		/* Fill the input buffer */
		if ( (done_reading == 0) && (strlen(inbuf) < 128) ) {

			/* FIX ... genericize this */
			ch = *inputmsg++;
			if (ch > 0) {
				inbuf[strlen(inbuf)+1] = 0;
				inbuf[strlen(inbuf)] = ch;
			} 
			else {
				done_reading = 1;
			}

		}

		/* Do some parsing */
		if (strlen(inbuf)>0) {

		    /* Fold in all the spacing */
		    for (i=0; i<strlen(inbuf); ++i) {
			if (inbuf[i]==10) inbuf[i]=32;
			if (inbuf[i]==13) inbuf[i]=32;
			if (inbuf[i]==9) inbuf[i]=32;
			if ((inbuf[i]<32) || (inbuf[i]>126))
				strcpy(&inbuf[i], &inbuf[i+1]);
			while ((inbuf[i]==32)&&(inbuf[i+1]==32))
				strcpy(&inbuf[i], &inbuf[i+1]);
		    }

		    for (i=0; i<strlen(inbuf); ++i) {

			ch = inbuf[i];

			if (ch == '<') {
				++nest;
				strcpy(tag, "");
			}

			else if (ch == '>') {
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
					for (j=0; j<screenwidth-2; ++j)
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

		/* Convert &; tags to the forbidden characters */
		if (strlen(outbuf)>0) for (i=0; i<strlen(outbuf); ++i) {

			if (!strncasecmp(&outbuf[i], "&nbsp;", 6)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

			else if (!strncasecmp(&outbuf[i], "&lt;", 4)) {
				outbuf[i] = '<';
				strcpy(&outbuf[i+1], &outbuf[i+4]);
			}

			else if (!strncasecmp(&outbuf[i], "&gt;", 4)) {
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

			else if (!strncasecmp(&outbuf[i], "&copy;", 6)) {
				outbuf[i] = '(';
				outbuf[i+1] = 'c';
				outbuf[i+2] = ')';
				strcpy(&outbuf[i+3], &outbuf[i+6]);
			}

		}

		/* Output any lines terminated with hard line breaks */
		do {
			did_out = 0;
			if (strlen(outbuf)>0)
			    for (i = 0; i<strlen(outbuf); ++i) {
				if ( (i<(screenwidth-2)) && (outbuf[i]=='\n')) {
					fwrite(outbuf, i+1, 1, stdout);
					strcpy(outbuf, &outbuf[i+1]);
					i = 0;
					did_out = 1;
				}
			}
		} while (did_out);

		/* Add soft line breaks */
		if (strlen(outbuf) > (screenwidth - 2)) {
			rb = (-1);
			for (i=0; i<(screenwidth-2); ++i) {
				if (outbuf[i]==32) rb = i;
			}
			if (rb>=0) {
				fwrite(outbuf, rb, 1, stdout);
				fwrite("\n", 1, 1, stdout);
				strcpy(outbuf, &outbuf[rb+1]);
			} else {
				fwrite(outbuf, screenwidth-2, 1, stdout);
				fwrite("\n", 1, 1, stdout);
				strcpy(outbuf, &outbuf[screenwidth-2]);
			}
		}

	} while (done_reading == 0);
	fwrite(outbuf, strlen(outbuf), 1, stdout);
	fwrite("\n", 1, 1, stdout);

	inptr = mallok(100);
	strcpy(inptr, "This is eekish.\n");
	return inptr;

}

