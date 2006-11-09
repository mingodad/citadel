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
#define MAX_RULES	8

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
	wprintf("<SPAN CLASS=\"titlebar\">"
		"<img src=\"static/advanpage2_48x.gif\">");
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
	display_rules_editor_inner_div();
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
	wprintf("<a href=\"display_add_remove_scripts\">%s</a>\n", _("Add or delete scripts"));

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


/**
 * \brief show a list of available scripts to add/remove them
 */
void display_add_remove_scripts(char *message)
{
	char buf[256];
	char script_name[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<table width=100%% border=0 bgcolor=#444455><tr>"
		"<td>"
		"<span class=\"titlebar\">"
		"<img src=\"static/advanpage2_48x.gif\">");
	wprintf(_("Add or delete scripts"));
	wprintf("</span></td></tr></table>\n"
		"</div>\n<div id=\"content\">\n"
	);

	if (message != NULL) wprintf(message);

	wprintf("<table border=0 cellspacing=10><tr valign=top><td>\n");

	svprintf("BOXTITLE", WCS_STRING, _("Add a new script"));
	do_template("beginbox");

	wprintf(_("To create a new script, enter the desired "
		"script name in the box below and click 'Create'."));
	wprintf("<br /><br />");

        wprintf("<center><form method=\"POST\" action=\"create_script\">\n");
        wprintf(_("Script name: "));
        wprintf("<input type=\"text\" name=\"script_name\"><br />\n"
        	"<input type=\"submit\" name=\"create_button\" value=\"%s\">"
		"</form></center>\n", _("Create"));

	do_template("endbox");

	svprintf("BOXTITLE", WCS_STRING, _("Edit scripts"));
	do_template("beginbox");
	wprintf("<br /><div align=center><a href=\"display_sieve\">%s</a><br /><br />\n",
		_("Return to the script editing screen")
	);
	do_template("endbox");

	wprintf("</td><td>");

	svprintf("BOXTITLE", WCS_STRING, _("Delete scripts"));
	do_template("beginbox");

	wprintf(_("To delete an existing script, select the script "
		"name from the list and click 'Delete'."));
	wprintf("<br /><br />");
	
        wprintf("<center>"
		"<form method=\"POST\" action=\"delete_script\">\n");
        wprintf("<select name=\"script_name\" size=10 style=\"width:100%%\">\n");

        serv_puts("MSIV listscripts");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(script_name, buf, 0, '|', sizeof script_name);
			if (extract_int(buf, 1) == 0) {
                        	wprintf("<option>");
                        	escputs(script_name);
                        	wprintf("</option>\n");
			}
                }
        }
        wprintf("</select><br />\n");

        wprintf("<input type=\"submit\" name=\"delete_button\" value=\"%s\" "
		"onClick=\"return confirm('%s');\">", _("Delete script"), _("Delete this script?"));
        wprintf("</form></center>\n");
	do_template("endbox");

	wprintf("</td></tr></table>\n");

	wDumpContent(1);
}



/**
 * \brief delete a script
 */
void delete_script(void) {
	char buf[256];

	serv_printf("MSIV deletescript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
	display_add_remove_scripts(&buf[4]);
}
		


/**
 * \brief create a new script
 * take the web environment script name and create it on the citadel server
 */
void create_script(void) {
	char buf[256];

	serv_printf("MSIV getscript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof(buf)), strcmp(buf, "000")) {
			/* flush */
		}
		display_add_remove_scripts(_("A script by that name already exists."));
		return;
	}
	
	serv_printf("MSIV putscript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		serv_puts("keep;");
		serv_puts("000");
		display_add_remove_scripts(_("A new script has been created.  Return to the script editing screen to edit and activate it."));
		return;
	}

	display_add_remove_scripts(&buf[4]);
}




void display_rules_editor_inner_div(void) {
	int i;

/*
 * This script should get called by every onChange event...
 *
 */
	wprintf("<script type=\"text/javascript\">					\n"
		"									\n"
		"function UpdateRules() {						\n"
		"  for (i=0; i<%d; ++i) {						\n", MAX_RULES);
	wprintf("  d = ($('action'+i).options[$('action'+i).selectedIndex].value);	\n"
		"  if (d == 'fileinto') {						\n"
		"    $('div_fileinto'+i).style.display = 'block';			\n"
		"    $('div_redirect'+i).style.display = 'none';			\n"
		"  } else if (d == 'redirect') {					\n"
		"    $('div_fileinto'+i).style.display = 'none';			\n"
		"    $('div_redirect'+i).style.display = 'block';			\n"
		"  } else  {								\n"
		"    $('div_fileinto'+i).style.display = 'none';			\n"
		"    $('div_redirect'+i).style.display = 'none';			\n"
		"  }									\n"
		" }									\n"
		"}									\n"
		"</script>								\n"
	);


/*
 * Show/hide alternating rows.  This is obviously bogus, it's just here to test the show/hide logic.
	wprintf("  for (i=0; i<%d; ++i) {						\n", MAX_RULES);
	wprintf("   if ( (i % 2) == 0 )  {						\n"
		"     $('rule' + i).style.display = 'block';				\n"
		"   }									\n"
		"   else {								\n"
		"     $('rule' + i).style.display = 'none';				\n"
		"   }									\n"
		"  }									\n"
		"}									\n"
		"									\n"
*/


	wprintf("<br />");
	wprintf("<table class=\"mailbox_summary\" rules=rows cellpadding=2 "
		"style=\"width:100%%;-moz-user-select:none;\">"
	);

	for (i=0; i<MAX_RULES; ++i) {
		
		wprintf("<tr id=\"rule%d\">", i);

		wprintf("<td>%d. %s</td>", i+1, _("If") );

		wprintf("<td>");
		wprintf("<select name=\"hfield%d\" size=1 onChange=\"UpdateRules();\">", i);
		wprintf("<option value=\"sender\">%s</option>", _("Sender"));
		wprintf("<option value=\"recipient\">%s</option>", _("Recipient"));
		wprintf("</select>");
		wprintf("</td>");

		wprintf("<td>");
		wprintf("<select name=\"compare%d\" size=1 onChange=\"UpdateRules();\">", i);
		wprintf("<option value=\"match\">%s</option>", _("matches"));
		wprintf("<option value=\"notmatch\">%s</option>", _("does not match"));
		wprintf("</select>");
		wprintf("</td>");

		wprintf("<td>");
		wprintf("<input type=\"text\" name=\"htext%d\">", i);
		wprintf("</td>");

		wprintf("<td>");
		wprintf("<select id=\"action%d\" name=\"action%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		wprintf("<option value=\"fileinto\">%s</option>", _("file into"));
		wprintf("<option value=\"redirect\">%s</option>", _("forward to"));
		wprintf("<option value=\"reject\">%s</option>", _("reject"));
		wprintf("</select>");

		wprintf("<div id=\"div_fileinto%d\">", i);
		wprintf("<input type=\"text\" name=\"fileinto%d\">", i);
		wprintf("</div>");

		wprintf("<div id=\"div_redirect%d\">", i);
		wprintf("<input type=\"text\" name=\"redirect%d\">", i);
		wprintf("</div>");
		wprintf("</td>");



		wprintf("<td>%s</td>", _("and then") );

		wprintf("<td>");
		wprintf("<select name=\"final%d\" size=1 onChange=\"UpdateRules();\">", i);
		wprintf("<option value=\"stop\">%s</option>", _("stop"));
		wprintf("<option value=\"continue\">%s</option>", _("continue processing"));
		wprintf("</select>");
		wprintf("</td>");

		wprintf("</tr>\n");

	}

	wprintf("</table>");




	wprintf("<script type=\"text/javascript\">					\n"
		"UpdateRules();								\n"
		"</script>								\n"
	);

}






/*@}*/
