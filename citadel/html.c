/*
 * $Id$
 *
 * Functions which handle translation between HTML and plain text
 * Copyright (c) 2000-2005 by Art Cancro and others.   This program is
 * released under the terms of the GNU General Public License.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "control.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "msgbase.h"
#include "room_ops.h"
#include "html.h"
 

/*
 * Convert HTML to plain text.
 *
 * inputmsg      = pointer to raw HTML message
 * screenwidth   = desired output screenwidth
 * do_citaformat = set to 1 to indent newlines with spaces
 */
char *html_to_ascii(char *inputmsg, int msglen, int screenwidth, int do_citaformat) {
	char inbuf[SIZ];
	int inbuf_len = 0;
	char outbuf[SIZ];
	char tag[1024];
	int done_reading = 0;
	char *inptr;
	char *outptr;
	size_t outptr_buffer_size;
	size_t output_len = 0;
	int i, j, ch, did_out, rb, scanch;
	int nest = 0;		/* Bracket nesting level */
	int blockquote = 0;	/* BLOCKQUOTE nesting level */
	int styletag = 0;	/* STYLE tag nesting level */
	int styletag_start = 0;
	int bytes_processed = 0;
	char nl[128];

	strcpy(nl, "\n");
	inptr = inputmsg;
	strcpy(inbuf, "");
	strcpy(outbuf, "");
	if (msglen == 0) msglen = strlen(inputmsg);

	outptr_buffer_size = strlen(inptr) + SIZ;
	outptr = malloc(outptr_buffer_size);
	if (outptr == NULL) return NULL;
	strcpy(outptr, "");
	output_len = 0;

	do {
		/* Fill the input buffer */
		inbuf_len = strlen(inbuf);
		if ( (done_reading == 0) && (inbuf_len < (SIZ-128)) ) {

			ch = *inptr++;
			if (ch != 0) {
				inbuf[inbuf_len++] = ch;
				inbuf[inbuf_len] = 0;
			} 
			else {
				done_reading = 1;
			}

			++bytes_processed;
			if (bytes_processed > msglen) {
				done_reading = 1;
			}

		}

		/* Do some parsing */
		if (!IsEmptyStr(inbuf)) {


		    /* Fold in all the spacing */
		    for (i=0; !IsEmptyStr(&inbuf[i]); ++i) {
			if (inbuf[i]==10) inbuf[i]=32;
			if (inbuf[i]==13) inbuf[i]=32;
			if (inbuf[i]==9) inbuf[i]=32;
			/*** we like foreign characters now.
			if ((inbuf[i]<32) || (inbuf[i]>126)) {
				inbuf[i] = '?';
			} */
		    }
		    for (i=0; !IsEmptyStr(&inbuf[i]); ++i) {
			while ((inbuf[i]==32)&&(inbuf[i+1]==32))
				strcpy(&inbuf[i], &inbuf[i+1]);
		    }

		    for (i=0; !IsEmptyStr(&inbuf[i]); ++i) {

			ch = inbuf[i];

			if (ch == '<') {
				++nest;
				strcpy(tag, "");
			}

			else if (ch == '>') {	/* We have a tag. */
				if (nest > 0) --nest;

				/* Unqualify the tag (truncate at first space) */
				if (strchr(tag, ' ') != NULL) {
					strcpy(strchr(tag, ' '), "");
				}
				
				if (!strcasecmp(tag, "P")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				if (!strcasecmp(tag, "/DIV")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				if (!strcasecmp(tag, "LI")) {
					strcat(outbuf, nl);
					strcat(outbuf, " * ");
				}

				else if (!strcasecmp(tag, "/UL")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "H1")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "H2")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "H3")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "H4")) {
					strcat(outbuf, nl);
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/H1")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/H2")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/H3")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/H4")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "HR")) {
					strcat(outbuf, nl);
					strcat(outbuf, " ");
					for (j=0; j<screenwidth-2; ++j)
						strcat(outbuf, "-");
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "BR")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "TR")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/TABLE")) {
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "BLOCKQUOTE")) {
					++blockquote;
					strcpy(nl, "\n");
					for (j=0; j<blockquote; ++j) strcat(nl, ">");
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "/BLOCKQUOTE")) {
					strcat(outbuf, "\n");
					--blockquote;
					strcpy(nl, "\n");
					for (j=0; j<blockquote; ++j) strcat(nl, ">");
					strcat(outbuf, nl);
				}

				else if (!strcasecmp(tag, "STYLE")) {
					++styletag;
					if (styletag == 1) {
						styletag_start = strlen(outbuf);
					}
				}

				else if (!strcasecmp(tag, "/STYLE")) {
					--styletag;
					if (styletag == 0) {
						outbuf[styletag_start] = 0;
					}
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
		if (!IsEmptyStr(outbuf)) for (i=0; !IsEmptyStr(&outbuf[i]); ++i) {

			/* Character entity references */
			if (!strncasecmp(&outbuf[i], "&nbsp;", 6)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

			if (!strncasecmp(&outbuf[i], "&ensp;", 6)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

			if (!strncasecmp(&outbuf[i], "&emsp;", 6)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

			if (!strncasecmp(&outbuf[i], "&thinsp;", 8)) {
				outbuf[i] = ' ';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
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

			else if (!strncasecmp(&outbuf[i], "&lsquo;", 7)) {
				outbuf[i] = '`';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&rsquo;", 7)) {
				outbuf[i] = '\'';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&copy;", 6)) {
				outbuf[i] = '(';
				outbuf[i+1] = 'c';
				outbuf[i+2] = ')';
				strcpy(&outbuf[i+3], &outbuf[i+6]);
			}

			else if (!strncasecmp(&outbuf[i], "&bull;", 6)) {
				outbuf[i] = ' ';
				outbuf[i+1] = '*';
				outbuf[i+2] = ' ';
				strcpy(&outbuf[i+3], &outbuf[i+6]);
			}

			else if (!strncasecmp(&outbuf[i], "&hellip;", 8)) {
				outbuf[i] = '.';
				outbuf[i+1] = '.';
				outbuf[i+2] = '.';
				strcpy(&outbuf[i+3], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&trade;", 7)) {
				outbuf[i] = '(';
				outbuf[i+1] = 't';
				outbuf[i+2] = 'm';
				outbuf[i+3] = ')';
				strcpy(&outbuf[i+4], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&reg;", 5)) {
				outbuf[i] = '(';
				outbuf[i+1] = 'r';
				outbuf[i+2] = ')';
				strcpy(&outbuf[i+3], &outbuf[i+5]);
			}

			else if (!strncasecmp(&outbuf[i], "&frac14;", 8)) {
				outbuf[i] = '1';
				outbuf[i+1] = '/';
				outbuf[i+2] = '4';
				strcpy(&outbuf[i+3], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&frac12;", 8)) {
				outbuf[i] = '1';
				outbuf[i+1] = '/';
				outbuf[i+2] = '2';
				strcpy(&outbuf[i+3], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&frac34;", 8)) {
				outbuf[i] = '3';
				outbuf[i+1] = '/';
				outbuf[i+2] = '4';
				strcpy(&outbuf[i+3], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&ndash;", 7)) {
				outbuf[i] = '-';
				outbuf[i+1] = '-';
				strcpy(&outbuf[i+2], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&mdash;", 7)) {
				outbuf[i] = '-';
				outbuf[i+1] = '-';
				outbuf[i+2] = '-';
				strcpy(&outbuf[i+3], &outbuf[i+7]);
			}

			else if (!strncmp(&outbuf[i], "&Ccedil;", 8)) {
				outbuf[i] = 'C';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&ccedil;", 8)) {
				outbuf[i] = 'c';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncmp(&outbuf[i], "&Egrave;", 8)) {
				outbuf[i] = 'E';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&egrave;", 8)) {
				outbuf[i] = 'e';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncmp(&outbuf[i], "&Ecirc;", 7)) {
				outbuf[i] = 'E';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&ecirc;", 7)) {
				outbuf[i] = 'e';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			else if (!strncmp(&outbuf[i], "&Eacute;", 8)) {
				outbuf[i] = 'E';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&eacute;", 8)) {
				outbuf[i] = 'e';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncmp(&outbuf[i], "&Agrave;", 8)) {
				outbuf[i] = 'A';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&agrave;", 8)) {
				outbuf[i] = 'a';
				strcpy(&outbuf[i+1], &outbuf[i+8]);
			}

			else if (!strncasecmp(&outbuf[i], "&ldquo;", 7)) {
				outbuf[i] = '\"';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			else if (!strncasecmp(&outbuf[i], "&rdquo;", 7)) {
				outbuf[i] = '\"';
				strcpy(&outbuf[i+1], &outbuf[i+7]);
			}

			/* two-digit decimal equivalents */
			else if ((!strncmp(&outbuf[i], "&#", 2))
			      && (outbuf[i+4] == ';') ) {
				scanch = 0;
				sscanf(&outbuf[i+2], "%02d", &scanch);
				outbuf[i] = scanch;
				strcpy(&outbuf[i+1], &outbuf[i+5]);
			}

			/* three-digit decimal equivalents */
			else if ((!strncmp(&outbuf[i], "&#", 2))
			      && (outbuf[i+5] == ';') ) {
				scanch = 0;
				sscanf(&outbuf[i+2], "%03d", &scanch);
				outbuf[i] = scanch;
				strcpy(&outbuf[i+1], &outbuf[i+6]);
			}

		}

		/* Make sure the output buffer is big enough */
		if ((output_len + strlen(outbuf) + SIZ) > outptr_buffer_size) {
			outptr_buffer_size += SIZ;
			outptr = realloc(outptr, outptr_buffer_size);
			if (outptr == NULL) {
				abort();
			}
		}

		/* Output any lines terminated with hard line breaks */
		do {
			did_out = 0;
			if (strlen(outbuf) > 0) {
			    for (i = 0; i<strlen(outbuf); ++i) {
				if ( (i<(screenwidth-2)) && (outbuf[i]=='\n')) {

					strncpy(&outptr[output_len], outbuf, i+1);
					output_len += (i+1);

					if (do_citaformat) {
						strcpy(&outptr[output_len], " ");
						++output_len;
					}

					strcpy(outbuf, &outbuf[i+1]);
					i = 0;
					did_out = 1;
				}
			}
		    }
		} while (did_out);

		/* Add soft line breaks */
		if (strlen(outbuf) > (screenwidth - 2 )) {
			rb = (-1);
			for (i=0; i<(screenwidth-2); ++i) {
				if (outbuf[i]==32) rb = i;
			}
			if (rb>=0) {
				strncpy(&outptr[output_len], outbuf, rb);
				output_len += rb;
				strcpy(&outptr[output_len], nl);
				output_len += strlen(nl);
				if (do_citaformat) {
					strcpy(&outptr[output_len], " ");
					++output_len;
				}
				strcpy(outbuf, &outbuf[rb+1]);
			} else {
				strncpy(&outptr[output_len], outbuf,
					screenwidth-2);
				output_len += (screenwidth-2);
				strcpy(&outptr[output_len], nl);
				output_len += strlen(nl);
				if (do_citaformat) {
					strcpy(&outptr[output_len], " ");
					++output_len;
				}
				strcpy(outbuf, &outbuf[screenwidth-2]);
			}
		}

	} while (done_reading == 0);

	strcpy(&outptr[output_len], outbuf);
	output_len += strlen(outbuf);

	/* Strip leading/trailing whitespace.  We can't do this with
	 * striplt() because it uses too many strlen()'s
	 */
	while ((output_len > 0) && (isspace(outptr[0]))) {
		strcpy(outptr, &outptr[1]);
		--output_len;
	}
	while ((output_len > 0) && (isspace(outptr[output_len-1]))) {
		outptr[output_len-1] = 0;
		--output_len;
	}

	if (outptr[output_len-1] != '\n') {
		strcat(outptr, "\n");
		++output_len;
	}

	return outptr;

}
