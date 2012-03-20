/*
 * First-time setup wizard
 */

#include "webcit.h"

void do_setup_wizard(void)
{
	char *step;
	FILE *fp;

	step = bstr("step");

	if (!strcasecmp(step, "Finish")) {
		fp = fopen(wizard_filename, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", WC->serv_info->serv_rev_level);
			fclose(fp);
		}
		do_welcome();
		return;
	}

	output_headers(1, 1, 1, 0, 0, 0);

	wc_printf("<div id=\"room_banner_override\">\n");
	wc_printf("<img src=\"static/citadel-logo.gif\" WIDTH=64 HEIGHT=64");
	wc_printf("<h1>&nbsp;First time setup</h1>");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<form method=\"post\" action=\"setup_wizard\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wc_printf("<div align=center>"
		"This is where the setup wizard will be placed.<br>\n"
		"For now, just click Finish.<br><br>\n"
	);

	wc_printf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Next\">\n");
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"step\" VALUE=\"Finish\">\n");

	wc_printf("</form></div>\n");
	wDumpContent(1);
}

void 
InitModule_SETUP_WIZARD
(void)
{
	WebcitAddUrlHandler(HKEY("setup_wizard"), "", 0, do_setup_wizard, 0);
}
