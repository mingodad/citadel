/*
 * $Id$
 *
 * Functions which handle "sticky notes"
 *
 */

#include "webcit.h"
#include "webserver.h"

void display_note(long msgnum) {
	char buf[SIZ];
	char notetext[SIZ];
	char display_notetext[SIZ];
	int in_text = 0;
	int i;

	wprintf("<IMG ALIGN=MIDDLE src=\"static/storenotes_48x.gif\">\n");

	serv_printf("MSG0 %ld", msgnum);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		wprintf("%s<br />\n", &buf[4]);
		return;
	}

	strcpy(notetext, "");
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		/* Fill the buffer to at least 256 characters */
		if ( (in_text) && (strlen(notetext) < 256) ) {
			strcat(notetext, buf);
		}

		if ( (!in_text) && (!strcasecmp(buf, "text")) ) {
			in_text = 1;
		}
	}

	/* Now sanitize the buffer, and shorten it to just a small snippet */
	for (i=0; i<strlen(notetext); ++i) {
		if (isspace(notetext[i])) notetext[i] = ' ';
	}
	strcpy(&notetext[72], "...");

	/* Make it HTML-happy and print it. */
	stresc(display_notetext, notetext, 1, 1);
	wprintf("%s<br />\n", display_notetext);
}
