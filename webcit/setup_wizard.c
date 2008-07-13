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
	wprintf("<img src=\"static/citadel-logo.gif\" WIDTH=64 HEIGHT=64");
	wprintf("<h1>&nbsp;First time setup</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<form method=\"post\" action=\"setup_wizard\">\n"
	);
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wprintf("<div align=center>"
		"This is where the setup wizard will be placed.<br>\n"
		"For now, just click Finish.<br><br>\n"
	);

	wprintf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Next\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Finish\">\n");

	wprintf("</form></div></div>\n");
	wDumpContent(1);
}

void 
InitModule_SETUP_WIZARD
(void)
{
	WebcitAddUrlHandler(HKEY("setup_wizard"), do_setup_wizard, 0);
}
