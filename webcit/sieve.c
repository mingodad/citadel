/* 
 * $Id$
 */

#include "webcit.h"

#define MAX_SCRIPTS	100
#define MAX_RULES	50
#define RULES_SCRIPT	"__WebCit_Generated_Script__"


/*
 * dummy panel indicating to the user that the server doesn't support Sieve
 */
void display_no_sieve(void) {

	output_headers(1, 1, 2, 0, 0, 0);

	wc_printf("<div id=\"banner\">\n");
	wc_printf("<img src=\"static/advanpage2_48x.gif\">");
	wc_printf("<h1>");
	wc_printf(_("View/edit server-side mail filters"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"sieve_background\">"
		"<tr><td valign=top>\n");

	wc_printf(_("This installation of Citadel was built without support for server-side mail filtering."
		"<br>Please contact your system administrator if you require this feature.<br>"));

	wc_printf("</td></tr></table></div>\n");
	wDumpContent(1);
}


/*
 * view/edit sieve config
 */
void display_sieve(void)
{
	char script_names[MAX_SCRIPTS][64];
	int num_scripts = 0;
	int active_script = (-1);
	char buf[SIZ];		/* Don't make this buffer smaller or it will restrict line length */
	int i;
	int rules_script_is_active = 0;

	if (!WC->serv_info->serv_supports_sieve) {
		display_no_sieve();
		return;
	}

	memset(script_names, 0, sizeof script_names);

	serv_puts("MSIV listscripts");
	serv_getln(buf, sizeof(buf));
	if (buf[0] == '1') while (serv_getln(buf, sizeof(buf)), strcmp(buf, "000")) {
		if (num_scripts < MAX_SCRIPTS) {
			extract_token(script_names[num_scripts], buf, 0, '|', 64);
			if (extract_int(buf, 1) > 0) {
				active_script = num_scripts;
				if (!strcasecmp(script_names[num_scripts], RULES_SCRIPT)) {
					rules_script_is_active = 1;
				}
			}
			++num_scripts;
		}
	}

	output_headers(1, 1, 2, 0, 0, 0);

	wc_printf("<script type=\"text/javascript\">					\n"
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

	wc_printf("<div id=\"banner\">\n");
	wc_printf("<img src=\"static/advanpage2_48x.gif\">");
	wc_printf("<h1>");
	wc_printf(_("View/edit server-side mail filters"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"sieve_background\">"
		"<tr><td valign=top>\n");


	wc_printf("<form id=\"sieveform\" method=\"post\" action=\"save_sieve\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

	wc_printf(_("When new mail arrives: "));
        wc_printf("<select name=\"bigaction\" size=1 onChange=\"ToggleSievePanels();\">\n");

	wc_printf("<option %s value=\"0\">", ((active_script < 0) ? "selected" : ""));
	wc_printf(_("Leave it in my inbox without filtering"));
	wc_printf("</option>\n");

	wc_printf("<option %s value=\"1\">", ((rules_script_is_active) ? "selected" : ""));
	wc_printf(_("Filter it according to rules selected below"));
	wc_printf("</option>\n");

	wc_printf("<option %s value=\"2\">",
			(((active_script >= 0) && (!rules_script_is_active)) ? "selected" : ""));
	wc_printf(_("Filter it through a manually edited script (advanced users only)"));
	wc_printf("</option>\n");

	wc_printf("</select>");



	/* The "no filtering" div */

	wc_printf("<div id=\"sievediv0\" style=\"display:none\">\n");
	wc_printf("<div align=\"center\"><br /><br />");
	wc_printf(_("Your incoming mail will not be filtered through any scripts."));
	wc_printf("<br /><br /></div>\n");
	wc_printf("</div>\n");

	/* The "webcit managed scripts" div */

	wc_printf("<div id=\"sievediv1\" style=\"display:none\">\n");
	display_rules_editor_inner_div();
	wc_printf("</div>\n");

	/* The "I'm smart and can write my own Sieve scripts" div */

	wc_printf("<div id=\"sievediv2\" style=\"display:none\">\n");

	if (num_scripts > 0) {
		wc_printf(_("The currently active script is: "));
        	wc_printf("<select name=\"active_script\" size=1 onChange=\"ToggleScriptPanels();\">\n");
		for (i=0; i<num_scripts; ++i) {
			if (strcasecmp(script_names[i], RULES_SCRIPT)) {
				wc_printf("<option %s value=\"%s\">%s</option>\n",
					((active_script == i) ? "selected" : ""),
					script_names[i],
					script_names[i]
				);
			}
		}
		wc_printf("</select>\n");
	}

	wc_printf("&nbsp;&nbsp;&nbsp;");
	wc_printf("<a href=\"display_add_remove_scripts\">%s</a>\n", _("Add or delete scripts"));

	wc_printf("<br />\n");

	if (num_scripts > 0) {
		for (i=0; i<num_scripts; ++i) {
			if (strcasecmp(script_names[i], RULES_SCRIPT)) {
				wc_printf("<div id=\"script_%s\" style=\"display:none\">\n", script_names[i]);
				wc_printf("<textarea name=\"text_%s\" wrap=soft rows=20 cols=80 width=80>\n",
					script_names[i]);
				serv_printf("MSIV getscript|%s", script_names[i]);
				serv_getln(buf, sizeof buf);
				if (buf[0] == '1') while(serv_getln(buf, sizeof (buf)), strcmp(buf, "000")) {
					wc_printf("%s\n", buf);
				}
				wc_printf("</textarea>\n");
				wc_printf("</div>\n");
			}
		}
	}

	wc_printf("<script type=\"text/javascript\">	\n"
		"ToggleScriptPanels();			\n"
		"</script>				\n"
	);

	wc_printf("</div>\n");


	/* The rest of this is common for all panels... */

	wc_printf("<div align=\"center\"><br>");
	wc_printf("<input type=\"submit\" name=\"save_button\" value=\"%s\">", _("Save changes"));
	wc_printf("&nbsp;");
	wc_printf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wc_printf("</div></form>\n");

	wc_printf("</td></tr></table></div>\n");

	wc_printf("<script type=\"text/javascript\">	\n"
		"ToggleSievePanels();			\n"
		"</script>				\n"
	);

	wDumpContent(1);

}



/*
 * Helper function for output_sieve_rule() to output strings with quotes escaped
 */
void osr_sanitize(char *str) {
	int i, len;

	if (str == NULL) return;
	len = strlen(str);
	for (i=0; i<len; ++i) {
		if (str[i]=='\"') {
			str[i] = '\'' ;
		}
		else if (isspace(str[i])) {
			str[i] = ' ';
		}
	}
}


/*
 * Output parseable Sieve script code based on rules input
 */
void output_sieve_rule(char *hfield, char *compare, char *htext, char *sizecomp, int sizeval,
			char *action, char *fileinto, char *redirect, char *automsg, char *final,
			char *my_addresses)
{
	char *comp1 = "";
	char *comp2 = "";

	osr_sanitize(htext);
	osr_sanitize(fileinto);
	osr_sanitize(redirect);
	osr_sanitize(automsg);

	/* Prepare negation and match operators that will be used iff we apply a conditional */

	if (!strcasecmp(compare, "contains")) {
		comp1 = "";
		comp2 = ":contains";
	}
	else if (!strcasecmp(compare, "notcontains")) {
		comp1 = "not";
		comp2 = ":contains";
	}
	else if (!strcasecmp(compare, "is")) {
		comp1 = "";
		comp2 = ":is";
	}
	else if (!strcasecmp(compare, "isnot")) {
		comp1 = "not";
		comp2 = ":is";
	}
	else if (!strcasecmp(compare, "matches")) {
		comp1 = "";
		comp2 = ":matches";
	}
	else if (!strcasecmp(compare, "notmatches")) {
		comp1 = "not";
		comp2 = ":matches";
	}

	/* Now do the conditional */

	if (!strcasecmp(hfield, "from")) {
		serv_printf("if%s header %s \"From\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "tocc")) {
		serv_printf("if%s header %s [\"To\", \"Cc\"] \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "subject")) {
		serv_printf("if%s header %s \"Subject\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "replyto")) {
		serv_printf("if%s header %s \"Reply-to\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "sender")) {
		serv_printf("if%s header %s \"Sender\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "resentfrom")) {
		serv_printf("if%s header %s \"Resent-from\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "resentto")) {
		serv_printf("if%s header %s \"Resent-to\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "xmailer")) {
		serv_printf("if%s header %s \"X-Mailer\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "xspamflag")) {
		serv_printf("if%s header %s \"X-Spam-Flag\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "xspamstatus")) {
		serv_printf("if%s header %s \"X-Spam-Status\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "listid")) {
		serv_printf("if%s header %s \"List-ID\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "envfrom")) {
		serv_printf("if%s envelope %s \"From\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "envto")) {
		serv_printf("if%s envelope %s \"To\" \"%s\"",
			comp1, comp2,
			htext
		);
	}

	else if (!strcasecmp(hfield, "size")) {
		if (!strcasecmp(sizecomp, "larger")) {
			serv_printf("if size :over %d", sizeval);
		}
		else if (!strcasecmp(sizecomp, "smaller")) {
			serv_printf("if size :under %d", sizeval);
		}
		else {	/* failsafe - should never get here, but just in case... */
			serv_printf("if size :over 1");
		}
	}

	/* Open braces if we're in a conditional loop */

	if (strcasecmp(hfield, "all")) {
		serv_printf("{");
	}


	/* Do action */

	if (!strcasecmp(action, "keep")) {
		serv_printf("keep;");
	}

	else if (!strcasecmp(action, "discard")) {
		serv_printf("discard;");
	}

	else if (!strcasecmp(action, "reject")) {
		serv_printf("reject \"%s\";", automsg);
	}

	else if (!strcasecmp(action, "fileinto")) {
		serv_printf("fileinto \"%s\";", fileinto);
	}

	else if (!strcasecmp(action, "redirect")) {
		serv_printf("redirect \"%s\";", redirect);
	}

	else if (!strcasecmp(action, "vacation")) {
		serv_printf("vacation :addresses [%s]\n\"%s\";", my_addresses, automsg);
	}


	/* Do 'final' action */

	if (!strcasecmp(final, "stop")) {
		serv_printf("stop;");
	}


	/* Close the braces if we're in a conditional loop */

	if (strcasecmp(hfield, "all")) {
		serv_printf("}");
	}


	/* End of rule. */
}



/*
 * Translate the fields from the rule editor into something we can save...
 */
void parse_fields_from_rule_editor(void) {

	int active;
	char hfield[256];
	char compare[32];
	char htext[256];
	char sizecomp[32];
	int sizeval;
	char action[32];
	char fileinto[128];
	char redirect[256];
	char automsg[1024];
	char final[32];
	int i;
	char buf[256];
	char fname[256];
	char rule[2048];
	char encoded_rule[4096];
	char my_addresses[4096];
	
	/* Enumerate my email addresses in case they are needed for a vacation rule */
	my_addresses[0] = 0;
	serv_puts("GVEA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (!IsEmptyStr(my_addresses)) {
			strcat(my_addresses, ",\n");
		}
		strcat(my_addresses, "\"");
		strcat(my_addresses, buf);
		strcat(my_addresses, "\"");
	}

	/* Now generate the script and write it to the Citadel server */
	serv_printf("MSIV putscript|%s|", RULES_SCRIPT);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		return;
	}

	serv_puts("# THIS SCRIPT WAS AUTOMATICALLY GENERATED BY WEBCIT.");
	serv_puts("# ");
	serv_puts("# Do not attempt to manually edit it.  If you do so,");
	serv_puts("# your changes will be overwritten the next time WebCit");
	serv_puts("# saves its mail filtering rule set.  If you really want");
	serv_puts("# to use these rules as the basis for another script,");
	serv_puts("# copy them to another script and save that instead.");
	serv_puts("");
	serv_puts("require \"fileinto\";");
	serv_puts("require \"reject\";");
	serv_puts("require \"vacation\";");
	serv_puts("require \"envelope\";");
	serv_puts("");

	for (i=0; i<MAX_RULES; ++i) {
		
		strcpy(rule, "");

		sprintf(fname, "active%d", i);
		active = !strcasecmp(BSTR(fname), "on") ;

		if (active) {

			sprintf(fname, "hfield%d", i);
			safestrncpy(hfield, BSTR(fname), sizeof hfield);
	
			sprintf(fname, "compare%d", i);
			safestrncpy(compare, BSTR(fname), sizeof compare);
	
			sprintf(fname, "htext%d", i);
			safestrncpy(htext, BSTR(fname), sizeof htext);
	
			sprintf(fname, "sizecomp%d", i);
			safestrncpy(sizecomp, BSTR(fname), sizeof sizecomp);
	
			sprintf(fname, "sizeval%d", i);
			sizeval = IBSTR(fname);
	
			sprintf(fname, "action%d", i);
			safestrncpy(action, BSTR(fname), sizeof action);
	
			sprintf(fname, "fileinto%d", i);
			safestrncpy(fileinto, BSTR(fname), sizeof fileinto);
	
			sprintf(fname, "redirect%d", i);
			safestrncpy(redirect, BSTR(fname), sizeof redirect);
	
			sprintf(fname, "automsg%d", i);
			safestrncpy(automsg, BSTR(fname), sizeof automsg);
	
			sprintf(fname, "final%d", i);
			safestrncpy(final, BSTR(fname), sizeof final);
	
			snprintf(rule, sizeof rule, "%d|%s|%s|%s|%s|%d|%s|%s|%s|%s|%s",
				active, hfield, compare, htext, sizecomp, sizeval, action, fileinto,
				redirect, automsg, final
			);
	
			CtdlEncodeBase64(encoded_rule, rule, strlen(rule)+1, 0);
			serv_printf("# WEBCIT_RULE|%d|%s|", i, encoded_rule);
			output_sieve_rule(hfield, compare, htext, sizecomp, sizeval,
					action, fileinto, redirect, automsg, final, my_addresses);
			serv_puts("");
		}


	}

	serv_puts("stop;");
	serv_puts("000");
}



/*
 * save sieve config
 */
void save_sieve(void) {
	int bigaction;
	char script_names[MAX_SCRIPTS][64];
	int num_scripts = 0;
	int active_script = (-1);
	int i;
	char this_name[64];
	char buf[256];

	if (!havebstr("save_button")) {
		strcpy(WC->ImportantMessage,
			_("Cancelled.  Changes were not saved."));
		display_main_menu();
		return;
	}

	parse_fields_from_rule_editor();

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

	bigaction = ibstr("bigaction");

	if (bigaction == 0) {
		serv_puts("MSIV setactive||");
		serv_getln(buf, sizeof buf);
	}

	else if (bigaction == 1) {
		serv_printf("MSIV setactive|%s|", RULES_SCRIPT);
		serv_getln(buf, sizeof buf);
	}

	else if (bigaction == 2) {
		serv_printf("MSIV setactive|%s|", bstr("active_script"));
		serv_getln(buf, sizeof buf);
	}

	if (num_scripts > 0) {
		for (i=0; i<num_scripts; ++i) {
			/*
			 * We only want to save the scripts from the "manually edited scripts"
			 * screen.  The script that WebCit generates from its ruleset will be
			 * auto-generated by parse_fields_from_rule_editor() and saved there.
			 */
			if (strcasecmp(script_names[i], RULES_SCRIPT)) {
				serv_printf("MSIV putscript|%s|", script_names[i]);
				serv_getln(buf, sizeof buf);
				if (buf[0] == '4') {
					snprintf(this_name, sizeof this_name, "text_%s", script_names[i]);
					striplt((char *)BSTR(this_name)); /* TODO: get rid of typecast*/
					serv_write(BSTR(this_name), strlen(BSTR(this_name)));
					serv_puts("\n000");
				}
			}
		}
	}

	strcpy(WC->ImportantMessage, _("Your changes have been saved."));
	display_main_menu();
	return;
}


/*
 * show a list of available scripts to add/remove them
 */
void display_add_remove_scripts(char *message)
{
	char buf[256];
	char script_name[256];

	output_headers(1, 1, 2, 0, 0, 0);
	wc_printf("<div id=\"banner\">\n");
	wc_printf("<img src=\"static/advanpage2_48x.gif\">");
	wc_printf(_("Add or delete scripts"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");
	
	wc_printf("<div id=\"content\" class=\"service\">\n");

	if (message != NULL) {
		wc_printf("%s", message);
	}

	wc_printf("<table border=0 cellspacing=10><tr valign=top><td>\n");

	svput("BOXTITLE", WCS_STRING, _("Add a new script"));
	do_template("beginboxx", NULL);

	wc_printf(_("To create a new script, enter the desired "
		"script name in the box below and click 'Create'."));
	wc_printf("<br /><br />");

        wc_printf("<center><form method=\"POST\" action=\"create_script\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wc_printf(_("Script name: "));
        wc_printf("<input type=\"text\" name=\"script_name\"><br />\n"
        	"<input type=\"submit\" name=\"create_button\" value=\"%s\">"
		"</form></center>\n", _("Create"));

	do_template("endbox", NULL);

	svput("BOXTITLE", WCS_STRING, _("Edit scripts"));
	do_template("beginboxx", NULL);
	wc_printf("<br /><div align=center><a href=\"display_sieve\">%s</a><br /><br />\n",
		_("Return to the script editing screen")
	);
	do_template("endbox", NULL);

	wc_printf("</td><td>");

	svput("BOXTITLE", WCS_STRING, _("Delete scripts"));
	do_template("beginboxx", NULL);

	wc_printf(_("To delete an existing script, select the script "
		"name from the list and click 'Delete'."));
	wc_printf("<br /><br />");
	
        wc_printf("<center>"
		"<form method=\"POST\" action=\"delete_script\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wc_printf("<select name=\"script_name\" size=10 style=\"width:100%%\">\n");

        serv_puts("MSIV listscripts");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(script_name, buf, 0, '|', sizeof script_name);
			if ( (extract_int(buf, 1) == 0) && (strcasecmp(script_name, RULES_SCRIPT)) ) {
                        	wc_printf("<option>");
                        	escputs(script_name);
                        	wc_printf("</option>\n");
			}
                }
        }
        wc_printf("</select><br />\n");

        wc_printf("<input type=\"submit\" name=\"delete_button\" value=\"%s\" "
		"onClick=\"return confirm('%s');\">", _("Delete script"), _("Delete this script?"));
        wc_printf("</form></center>\n");
	do_template("endbox", NULL);

	wc_printf("</td></tr></table>\n");

	wDumpContent(1);
}



/*
 * delete a script
 */
void delete_script(void) {
	char buf[256];

	serv_printf("MSIV deletescript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
	display_add_remove_scripts(&buf[4]);
}
		


/*
 * create a new script
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
	int i, j;
	char buf[4096];
	char rules[MAX_RULES][2048];

	struct {
		char name[128];
	} *rooms = NULL;
	int num_roomnames = 0;
	int num_roomnames_alloc = 0;

	int active;
	char hfield[256];
	char compare[32];
	char htext[256];
	char sizecomp[32];
	int sizeval;
	char action[32];
	char fileinto[128];
	char redirect[256];
	char automsg[1024];
	char final[32];

	/* load the rules */
	memset(rules, 0, sizeof rules);
	serv_printf("MSIV getscript|%s", RULES_SCRIPT);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(serv_getln(buf, sizeof (buf)), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "# WEBCIT_RULE|", 14)) {
			j = extract_int(buf, 1);
			remove_token(buf, 0, '|');
			remove_token(buf, 0, '|');
			CtdlDecodeBase64(rules[j], buf, strlen(buf));
		}
	}

	/* load the roomnames */
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			++num_roomnames;
			if (num_roomnames > num_roomnames_alloc) {
				num_roomnames_alloc += 250;
				rooms = realloc(rooms, (num_roomnames_alloc * 128));
			}
			extract_token(rooms[num_roomnames-1].name, buf, 0, '|', 128);
		}
	}


/*
 * This script should get called by every onChange event...
 *
 */
	wc_printf("<script type=\"text/javascript\">					\n"
		"									\n"
		"var highest_active_rule = (-1);					\n"
		"									\n"
		"function UpdateRules() {						\n");
/*
 * Show only the active rows...
 */
	wc_printf("  highest_active_rule = (-1);						\n");
	wc_printf("  for (i=0; i<%d; ++i) {						\n", MAX_RULES);
	wc_printf("   if ($('active'+i).checked) {					\n"
		"     $('rule' + i).style.display = 'block';				\n"
		"     highest_active_rule = i;						\n"
		"   }									\n"
		"   else {								\n"
		"     $('rule' + i).style.display = 'none';				\n"
		"   }									\n"
		"  }									\n");
/*
 * Show only the fields relevant to the rules...
 */
	wc_printf("  for (i=0; i<=highest_active_rule; ++i) {				\n"
		"    d = ($('movedown'+i));						\n"
		"    if (i < highest_active_rule) {					\n"
		"      d.style.display = 'block';					\n"
		"    }									\n"
		"    else {								\n"
		"      d.style.display = 'none';					\n"
		"    }									\n"
		"    d = ($('hfield'+i).options[$('hfield'+i).selectedIndex].value);	\n"
		"    if (d == 'all') {							\n"
		"      $('div_size'+i).style.display = 'none';	 			\n"
		"      $('div_compare'+i).style.display = 'none';			\n"
		"      $('div_nocompare'+i).style.display = 'block';			\n"
		"    }									\n"
		"    else if (d == 'size') {						\n"
		"      $('div_size'+i).style.display = 'block';	 			\n"
		"      $('div_compare'+i).style.display = 'none';			\n"
		"      $('div_nocompare'+i).style.display = 'none';			\n"
		"    }									\n"
		"    else {								\n"
		"      $('div_size'+i).style.display = 'none';	 			\n"
		"      $('div_compare'+i).style.display = 'block';			\n"
		"      $('div_nocompare'+i).style.display = 'none';			\n"
		"    }									\n"
		"    d = ($('action'+i).options[$('action'+i).selectedIndex].value);	\n"
		"    if (d == 'fileinto') {						\n"
		"      $('div_fileinto'+i).style.display = 'block';			\n"
		"      $('div_redirect'+i).style.display = 'none';			\n"
		"      $('div_automsg'+i).style.display = 'none';			\n"
		"    } else if (d == 'redirect') {					\n"
		"      $('div_fileinto'+i).style.display = 'none';			\n"
		"      $('div_redirect'+i).style.display = 'block';			\n"
		"      $('div_automsg'+i).style.display = 'none';			\n"
		"    } else if ((d == 'reject') || (d == 'vacation'))  {		\n"
		"      $('div_fileinto'+i).style.display = 'none';			\n"
		"      $('div_redirect'+i).style.display = 'none';			\n"
		"      $('div_automsg'+i).style.display = 'block';			\n"
		"    } else {								\n"
		"      $('div_fileinto'+i).style.display = 'none';			\n"
		"      $('div_redirect'+i).style.display = 'none';			\n"
		"      $('div_automsg'+i).style.display = 'none';			\n"
		"    }									\n"
		"    if (highest_active_rule < %d) {					\n", MAX_RULES-1 );
	wc_printf("      $('div_addrule').style.display = 'block';			\n"
		"    } else {								\n"
		"      $('div_addrule').style.display = 'none';				\n"
		"    }									\n"
		"  }									\n"
		"}									\n"
/*
 * Add a rule (really, just un-hide it)
 */
		"function AddRule() {							\n"
		"  highest_active_rule = highest_active_rule + 1;			\n"
		"  $('active'+highest_active_rule).checked = true;			\n"
		"  UpdateRules();							\n"
		"}									\n"
/*
 * Swap two rules
 */
		"function SwapRules(ra, rb) {						\n"
		"									\n"
		"  var things = new Array();						\n"
		"  things[0] = 'hfield';						\n"
		"  things[1] = 'compare';						\n"
		"  things[2] = 'htext';							\n"
		"  things[3] = 'action';						\n"
		"  things[4] = 'fileinto';						\n"
		"  things[5] = 'redirect';						\n"
		"  things[6] = 'final';							\n"
		"  things[7] = 'sizecomp';						\n"
		"  things[8] = 'sizeval';						\n"
		"  things[9] = 'automsg';						\n"
		"									\n"
		"  for (i=0; i<=9; ++i) {						\n"
		"    tempval=$(things[i]+ra).value;					\n"
		"    $(things[i]+ra).value = $(things[i]+rb).value;			\n"
		"    $(things[i]+rb).value = tempval;					\n"
		"  }									\n"
		"}									\n"
/*
 * Delete a rule (percolate the deleted rule out to the end, then deactivate it)
 */
		"function DeleteRule(rd) {						\n"
		"  for (j=rd; j<=highest_active_rule; ++j) {				\n"
		"    SwapRules(j, (j+1));						\n"
		"  }									\n"
		"  $('active'+highest_active_rule).checked = false;			\n"
		"}									\n"
		"</script>								\n"
	);


	wc_printf("<br />");

	wc_printf("<table cellpadding=2 width=100%%>");

	for (i=0; i<MAX_RULES; ++i) {

		/* Grab our existing values to populate */
		active = extract_int(rules[i], 0);
		extract_token(hfield, rules[i], 1, '|', sizeof hfield);
		extract_token(compare, rules[i], 2, '|', sizeof compare);
		extract_token(htext, rules[i], 3, '|', sizeof htext);
		extract_token(sizecomp, rules[i], 4, '|', sizeof sizecomp);
		sizeval = extract_int(rules[i], 5);
		extract_token(action, rules[i], 6, '|', sizeof action);
		extract_token(fileinto, rules[i], 7, '|', sizeof fileinto);
		extract_token(redirect, rules[i], 8, '|', sizeof redirect);
		extract_token(automsg, rules[i], 9, '|', sizeof automsg);
		extract_token(final, rules[i], 10, '|', sizeof final);
		
		/* now generate the table row */

		wc_printf("<tr id=\"rule%d\" bgcolor=\"#%s\">",
			i,
			((i%2) ? "DDDDDD" : "FFFFFF")
		);

		wc_printf("<td width=5%% align=\"center\">");

		wc_printf("<div style=\"display:none\">");
		wc_printf("<input type=\"checkbox\" name=\"active%d\" id=\"active%d\" %s>",
			i, i,
			(active ? "checked" : "")
		);
		wc_printf("</div>");

		if (i>0) wc_printf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img border=\"0\" src=\"static/up_pointer.gif\" "
			"title=\"%s\"/></a>",
			i-1, i, _("Move rule up") );

		wc_printf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img id=\"movedown%d\" border=\"0\" src=\"static/down_pointer.gif\" "
			"title=\"%s\"/></a>",
			i, i+1, i, _("Move rule down") );

		wc_printf("<a href=\"javascript:DeleteRule(%d);UpdateRules();\">"
			"<img id=\"delete%d\" border=\"0\" src=\"static/delete.gif\" "
			"title=\"%s\"/></a>",
			i, i, _("Delete rule") );

		wc_printf("</td>");

		wc_printf("<td width=5%% align=\"center\">");
		wc_printf("<font size=+2>%d</font>", i+1);
		wc_printf("</td>");

		wc_printf("<td width=20%%>%s ", _("If") );

		char *hfield_values[15][2] = {
			{	"from",		_("From")		},
			{	"tocc",		_("To or Cc")		},
			{	"subject",	_("Subject")		},
			{	"replyto",	_("Reply-to")		},
			{	"sender",	_("Sender")		},
			{	"resentfrom",	_("Resent-From")	},
			{	"resentto",	_("Resent-To")		},
			{	"envfrom",	_("Envelope From")	},
			{	"envto",	_("Envelope To")	},
			{	"xmailer",	_("X-Mailer")		},
			{	"xspamflag",	_("X-Spam-Flag")	},
			{	"xspamstatus",	_("X-Spam-Status")	},
			{	"listid",	_("List-ID")		},
			{	"size",		_("Message size")	},
			{	"all",		_("All")		}
		};

		wc_printf("<select id=\"hfield%d\" name=\"hfield%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<15; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(hfield, hfield_values[j][0])) ? "selected" : ""),
				hfield_values[j][0],
				hfield_values[j][1]
			);
		}

		wc_printf("</select>");
		wc_printf("</td>");

		wc_printf("<td width=20%%>");

		char *compare_values[6][2] = {
			{	"contains",	_("contains")		},
			{	"notcontains",	_("does not contain")	},
			{	"is",		_("is")			},
			{	"isnot",	_("is not")		},
			{	"matches",	_("matches")		},
			{	"notmatches",	_("does not match")	}
		};

		wc_printf("<div id=\"div_compare%d\">", i);
		wc_printf("<select id=\"compare%d\" name=\"compare%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<6; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(compare, compare_values[j][0])) ? "selected" : ""),
				compare_values[j][0],
				compare_values[j][1]
			);
		}
		wc_printf("</select>");

		wc_printf("<input type=\"text\" id=\"htext%d\" name=\"htext%d\" value=\"", i, i);
		escputs(htext);
		wc_printf("\"></div>");

		wc_printf("<div id=\"div_nocompare%d\">", i);
		wc_printf("%s", _("(All messages)"));
		wc_printf("</div>");

		char *sizecomp_values[2][2] = {
			{	"larger",	_("is larger than")	},
			{	"smaller",	_("is smaller than")	}
		};

		wc_printf("<div id=\"div_size%d\">", i);
		wc_printf("<select id=\"sizecomp%d\" name=\"sizecomp%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<2; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(sizecomp, sizecomp_values[j][0])) ? "selected" : ""),
				sizecomp_values[j][0],
				sizecomp_values[j][1]
			);
		}
		wc_printf("</select>");

		wc_printf("<input type=\"text\" id=\"sizeval%d\" name=\"sizeval%d\" value=\"%d\">",
			i, i, sizeval);
		wc_printf("bytes");
		wc_printf("</div>");

		wc_printf("</td>");

		char *action_values[6][2] = {
			{	"keep",		_("Keep")		},
			{	"discard",	_("Discard silently")	},
			{	"reject",	_("Reject")		},
			{	"fileinto",	_("Move message to")	},
			{	"redirect",	_("Forward to")		},
			{	"vacation",	_("Vacation")		}
		};

		wc_printf("<td width=20%%>");
		wc_printf("<select id=\"action%d\" name=\"action%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<6; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(action, action_values[j][0])) ? "selected" : ""),
				action_values[j][0],
				action_values[j][1]
			);
		}
		wc_printf("</select>");

		wc_printf("<div id=\"div_fileinto%d\">", i);
		wc_printf("<select name=\"fileinto%d\" id=\"fileinto%d\">", i, i);
		for (j=0; j<num_roomnames; ++j) {
			wc_printf("<option ");
			if (!strcasecmp(rooms[j].name, fileinto)) {
				wc_printf("selected ");
			}
			wc_printf("value=\"");
			escputs(rooms[j].name);
			wc_printf("\">");
			escputs(rooms[j].name);
			wc_printf("</option>\n");
		}
		wc_printf("</select>\n");
		wc_printf("</div>");

		wc_printf("<div id=\"div_redirect%d\">", i);
		wc_printf("<input type=\"text\" id=\"redirect%d\" name=\"redirect%d\" value=\"", i, i);
		escputs(redirect);
		wc_printf("\"></div>");

		wc_printf("<div id=\"div_automsg%d\">", i);
		wc_printf(_("Message:"));
		wc_printf("<br />");
		wc_printf("<textarea name=\"automsg%d\" id=\"automsg%d\" wrap=soft rows=5>\n", i, i);
		escputs(automsg);
		wc_printf("</textarea>");
		wc_printf("</div>");

		wc_printf("</td>");

		char *final_values[2][2] = {
			{	"continue",	_("continue processing")	},
			{	"stop",		_("stop")			}
		};

		wc_printf("<td width=10%% align=\"center\">%s</td>", _("and then") );

		wc_printf("<td width=20%%>");
		wc_printf("<select name=\"final%d\" id=\"final%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<2; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(final, final_values[j][0])) ? "selected" : ""),
				final_values[j][0],
				final_values[j][1]
			);
		}
		wc_printf("</select>");
		wc_printf("</td>");

		wc_printf("</tr>\n");

	}

	wc_printf("</table>");
	wc_printf("<div id=\"div_addrule\"><a href=\"javascript:AddRule();\">%s</a><br /></div>\n",
		_("Add rule")
	);

	wc_printf("<script type=\"text/javascript\">					\n");
	wc_printf("UpdateRules();								\n");
	wc_printf("</script>								\n");

	free(rooms);
}

void _display_add_remove_scripts(void) {display_add_remove_scripts(NULL);}

void 
InitModule_SIEVE
(void)
{
	WebcitAddUrlHandler(HKEY("display_sieve"), "", 0, display_sieve, 0);
	WebcitAddUrlHandler(HKEY("save_sieve"), "", 0, save_sieve, 0);
	WebcitAddUrlHandler(HKEY("display_add_remove_scripts"), "", 0, _display_add_remove_scripts, 0);
	WebcitAddUrlHandler(HKEY("create_script"), "", 0, create_script, 0);
	WebcitAddUrlHandler(HKEY("delete_script"), "", 0, delete_script, 0);
}
