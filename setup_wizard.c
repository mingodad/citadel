/*
 * $Id$
 *
 * First-time setup wizard
 */

#include "webcit.h"


/*
 */
void do_setup_wizard(void)
{
	char *step;
	FILE *fp;

	step = bstr("step");

	if (!strcasecmp(step, "Finish")) {
		fp = fopen(wizard_filename, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", serv_info.serv_rev_level);
			fclose(fp);
		}
		do_welcome();
		return;
	}

	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE class=\"setup_banner\"><TR><TD>");
	wprintf("<img src=\"static/citadel-logo.gif\" WIDTH=64 HEIGHT=64 ALT=\" \" ALIGN=MIDDLE>");
	wprintf("<SPAN CLASS=\"titlebar\">&nbsp;First time setup");
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<form method=\"post\" action=\"setup_wizard\">\n"
	);

	wprintf("<div align=center>"
		"This is where the setup wizard will be placed.<br>\n"
		"For now, just click Finish.<br><br>\n"
	);

	wprintf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Next\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Finish\">\n");

	wprintf("</form></div></div>\n");
	wDumpContent(1);
}


