/*
 * html.c -- Functions which handle translation between HTML and plain text
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>


/*
 * Convert HTML to plain text.
 */
void html_to_ascii(int screenwidth) {
	char inbuf[256];
	char outbuf[256];
	char tag[1024];
	int done_reading = 0;
	char *ptr;
	int i, ch;
	int nest = 0;		/* Bracket nesting level */

	strcpy(inbuf, "");
	strcpy(outbuf, "");

	do {
		/* Fill the input buffer */
		if ( (done_reading == 0) && (strlen(inbuf) < 128) ) {
			/* FIX ... genericize this */
			ptr = fgets(&inbuf[strlen(inbuf)], 127, stdin);
			if (ptr == NULL) done_reading = 1;
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

				if (!strcasecmp(tag, "HR")) {
					strcat(outbuf, "\n ----- \n");
				}

				if (!strcasecmp(tag, "BR")) {
					strcat(outbuf, "\n");
				}

				if (!strcasecmp(tag, "TR")) {
					strcat(outbuf, "\n");
				}

				if (!strcasecmp(tag, "/TABLE")) {
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

		/* Output our finely-crafted plain ASCII */
		printf("%s", outbuf);	/* FIX ... genericize this */
		strcpy(outbuf, "");

	} while (done_reading == 0);

}


/*
 * Temporary main loop for testing
 */
int main() {
	html_to_ascii(80);
	return 0;
}
