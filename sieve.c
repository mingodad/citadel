/* 
 * $Id: $
 */
/**
 * \defgroup Sieve view/edit sieve config
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"

#define MAX_SCRIPTS	100

/**
 * \brief view/edit sieve config
 */
void display_sieve(void)
{
	char script_names[MAX_SCRIPTS][64];
	int num_scripts = 0;
	int active_script = (-1);
	char buf[256];
	int i;
	

	memset(script_names, 0, sizeof script_names);

	serv_puts("MSIV listscripts");
	serv_getln(buf, sizeof(buf));
	if (buf[0] == '1') while (serv_getln(buf, sizeof(buf)), strcmp(buf, "000")) {
		if (num_scripts < MAX_SCRIPTS) {
			extract_token(script_names[num_scripts], buf, 0, '|', 64);
			if (extract_int(buf, 1) > 0) {
				active_script = num_scripts;
			}
			++num_scripts;
		}
	}

	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<script type=\"text/javascript\">					\n"
		"									\n"
		"var previously_active_script;						\n"
		"									\n"
		"function ToggleSievePanels() {						\n"
		" d = ($('sieveform').bigaction.options[$('sieveform').bigaction.selectedIndex].value);	\n"
		" for (i=0; i<3; ++i) {							\n"
		"  if (i == d) {							\n"
		"   $('sievediv' + i).style.display = 'block';				\n"
		"  }									\n"
		"  else {								\n"
		"   $('sievediv' + i).style.display = 'none';				\n"
		"  }									\n"
		" }									\n"
		"}									\n"
		"									\n"
		"function ToggleScriptPanels() {					\n"
		" d = ($('sieveform').active_script.options[$('sieveform').active_script.selectedIndex].value);	\n"
		" if ($('script_' + previously_active_script)) {			\n"
		"  $('script_' + previously_active_script).style.display = 'none';	\n"
		" }									\n"
		" $('script_' + d).style.display = 'block';				\n"
		" previously_active_script = d;						\n"
		"}									\n"
		"									\n"
		"</script>								\n"
	);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("View/edit server-side mail filters"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#FFFFFF\">"
		"<tr><td valign=top>\n");


	wprintf("<form id=\"sieveform\" method=\"post\" action=\"save_sieve\">\n");

	wprintf(_("When new mail arrives: "));
        wprintf("<select name=\"bigaction\" size=1 onChange=\"ToggleSievePanels();\">\n");

	wprintf("<option %s value=\"0\">", ((active_script < 0) ? "selected" : ""));
	wprintf(_("Leave it in my inbox without filtering"));
	wprintf("</option>\n");

	wprintf("<option value=\"1\">");
	wprintf(_("Filter it according to rules selected below"));
	wprintf("</option>\n");

	wprintf("<option %s value=\"2\">", ((active_script >= 0) ? "selected" : ""));
	wprintf(_("Filter it through a manually edited script (advanced users only)"));
	wprintf("</option>\n");

	wprintf("</select>");



	/* The "no filtering" div */

	wprintf("<div id=\"sievediv0\" style=\"display:none\">\n");
	wprintf("<div align=\"center\"><br /><br />");
	wprintf(_("Your incoming mail will not be filtered through any scripts."));
	wprintf("<br /><br /></div>\n");
	wprintf("</div>\n");

	/* The "webcit managed scripts" div */

	wprintf("<div id=\"sievediv1\" style=\"display:none\">\n");
	wprintf("<div align=\"center\"><br /><br />");
	wprintf("FIXME div 1 isn't finished yet");
	wprintf("<br /><br /></div>\n");
	wprintf("</div>\n");


	/* The "I'm smart and can write my own Sieve scripts" div */

	wprintf("<div id=\"sievediv2\" style=\"display:none\">\n");

	if (num_scripts > 0) {
		wprintf(_("The currently active script is: "));
        	wprintf("<select name=\"active_script\" size=1 onChange=\"ToggleScriptPanels();\">\n");
		for (i=0; i<num_scripts; ++i) {
			wprintf("<option %s value=\"%s\">%s</option>\n",
				((active_script == i) ? "selected" : ""),
				script_names[i],
				script_names[i]
			);
		}
		wprintf("</select>\n");
	}

	wprintf("&nbsp;&nbsp;&nbsp;");
	wprintf("<a href=\"display_add_remove_scripts\">%s</a>\n", _("Add/remove scripts"));

	wprintf("<br />\n");

	if (num_scripts > 0) {
		for (i=0; i<num_scripts; ++i) {
			wprintf("<div id=\"script_%s\" style=\"display:none\">\n", script_names[i]);
			wprintf("<textarea name=\"text_%s\" wrap=soft rows=20 cols=80 width=80>\n", script_names[i]);
			serv_printf("MSIV getscript|%s", script_names[i]);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '1') while(serv_getln(buf, sizeof (buf)), strcmp(buf, "000")) {
				wprintf("%s\n", buf);
			}
			wprintf("</textarea>\n");
			wprintf("</div>\n");
		}
	}

	wprintf("<script type=\"text/javascript\">	\n"
		"ToggleScriptPanels();			\n"
		"</script>				\n"
	);

	wprintf("</div>\n");


	/* The rest of this is common for all panels... */

	wprintf("<div align=\"center\"><br>");
	wprintf("<input type=\"submit\" name=\"save_button\" value=\"%s\">", _("Save changes"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wprintf("</div></form>\n");

	wprintf("</td></tr></table></div>\n");

	wprintf("<script type=\"text/javascript\">	\n"
		"ToggleSievePanels();			\n"
		"</script>				\n"
	);

	wDumpContent(1);

}


/**
 * \brief save sieve config
 */
void save_sieve(void) {
	int bigaction;
	char script_names[MAX_SCRIPTS][64];
	int num_scripts = 0;
	int active_script = (-1);
	int i;
	char this_name[64];
	char buf[256];

	if (strlen(bstr("save_button")) == 0) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  Changes were not saved."));
		display_main_menu();
		return;
	}

	serv_puts("MSIV listscripts");
	serv_getln(buf, sizeof(buf));
	if (buf[0] == '1') while (serv_getln(buf, sizeof(buf)), strcmp(buf, "000")) {
		if (num_scripts < MAX_SCRIPTS) {
			extract_token(script_names[num_scripts], buf, 0, '|', 64);
			if (extract_int(buf, 1) > 0) {
				active_script = num_scripts;
			}
			++num_scripts;
		}
	}

	bigaction = atoi(bstr("bigaction"));

	if (bigaction == 0) {
		serv_puts("MSIV setactive||");
		serv_getln(buf, sizeof buf);
	}

	else if (bigaction == 2) {
		serv_printf("MSIV setactive|%s|", bstr("active_script"));
		serv_getln(buf, sizeof buf);
	}

	if (num_scripts > 0) {
		for (i=0; i<num_scripts; ++i) {
			serv_printf("MSIV putscript|%s|", script_names[i]);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '4') {
				snprintf(this_name, sizeof this_name, "text_%s", script_names[i]);
				striplt(bstr(this_name));
				serv_printf("%s", bstr(this_name));
				serv_puts("000");
			}
		}
	}

	strcpy(WC->ImportantMessage, _("Your changes have been saved."));
	display_main_menu();
	return;
}


void display_add_remove_scripts(void) {
	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Add/remove Sieve scripts"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#FFFFFF\">"
		"<tr><td valign=top>\n");


	/* blah blah go here FIXME */

	wprintf("</td></tr></table></div>\n");

	wprintf("<script type=\"text/javascript\">	\n"
		"ToggleSievePanels();			\n"
		"</script>				\n"
	);

	wDumpContent(1);

}

/*@}*/
