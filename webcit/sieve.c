/*
 * Copyright (c) 1996-2011 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 3 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include "webcit.h"

#define MAX_SCRIPTS	100
#define MAX_RULES	50
#define RULES_SCRIPT	"__WebCit_Generated_Script__"
/*#define FOO 1*/
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

void display_add_remove_scripts(char *message);
void display_rules_editor_inner_div(void);






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
	int active_script = (-1);	/* this throws a 'set but not used' warning , check this ! */
	int i;
	char this_name[64];
	char buf[256];

	if (!havebstr("save_button")) {
		AppendImportantMessage(_("Cancelled.  Changes were not saved."), -1);
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

	AppendImportantMessage(_("Your changes have been saved."), -1);
	display_main_menu();
	return;
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
#if FOO
		display_add_remove_scripts(_("A script by that name already exists."));
#endif
		return;
	}
	
	serv_printf("MSIV putscript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
	if (buf[0] == '4') {
		serv_puts("keep;");
		serv_puts("000");
#if FOO
		display_add_remove_scripts(_("A new script has been created.  Return to the script editing screen to edit and activate it."));
#else
	output_headers(1, 1, 2, 0, 0, 0);
	do_template("sieve_add");
	wDumpContent(1);
#endif
		return;
	}

#if FOO
	display_add_remove_scripts(&buf[4]);
#else
	output_headers(1, 1, 2, 0, 0, 0);
	do_template("sieve_add");
	wDumpContent(1);
#endif
}




/*
 * delete a script
 */
void delete_script(void) {
	char buf[256];

	serv_printf("MSIV deletescript|%s", bstr("script_name"));
	serv_getln(buf, sizeof buf);
#if FOO
	display_add_remove_scripts(&buf[4]);
#else
	output_headers(1, 1, 2, 0, 0, 0);
	do_template("sieve_add");
	wDumpContent(1);
#endif
}
		


/*
 * dummy panel indicating to the user that the server doesn't support Sieve
 */
void display_no_sieve(void) {

	output_headers(1, 1, 2, 0, 0, 0);
	do_template("sieve_none");
	wDumpContent(1);
}

#if FOO
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

	wc_printf("<script type=\"text/javascript\">\n"
		  "\n"
		  "var previously_active_script;\n"
		  "\n"
		  "function ToggleSievePanels() {\n"
		  " d = ($('sieveform').bigaction.options[$('sieveform').bigaction.selectedIndex].value);\n"
		  " for (i=0; i<3; ++i) {\n"
		  "  if (i == d) {\n"
		  "   $('sievediv' + i).style.display = 'block';\n"
		  "  }\n"
		  "  else {\n"
		  "   $('sievediv' + i).style.display = 'none';\n"
		  "  }\n"
		  " }\n"
		  "}\n"
		  "\n"
		  "function ToggleScriptPanels() {\n"
		  " d = ($('sieveform').active_script.options[$('sieveform').active_script.selectedIndex].value);\n"
		  " if ($('script_' + previously_active_script)) {\n"
		  "  $('script_' + previously_active_script).style.display = 'none';\n"
		  " }\n"
		  " $('script_' + d).style.display = 'block';\n"
		  " previously_active_script = d;\n"
		  "}\n"
		  "\n"
		  "</script>\n"
);

	wc_printf("<div id=\"banner\">\n");
	wc_printf("<img src=\"static/webcit_icons/essen/32x32/config.png\">");
	wc_printf("<h1>");
	wc_printf(_("View/edit server-side mail filters"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<table class=\"sieve_background\">\n"
		"<tr>\n<td valign=top>\n");


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

	wc_printf("</select>\n\n");



	/* The "no filtering" div */

	wc_printf("<div id=\"sievediv0\" style=\"display:none\">\n");
	wc_printf("<div align=\"center\"><br><br>");
	wc_printf(_("Your incoming mail will not be filtered through any scripts."));
	wc_printf("<br><br></div>\n");
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
		wc_printf("</select>\n\n");
	}

	wc_printf("&nbsp;&nbsp;&nbsp;");
	wc_printf("<a href=\"display_add_remove_scripts\">%s</a>\n", _("Add or delete scripts"));

	wc_printf("<br>\n");

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

	wc_printf("</td></tr></table>\n");

	wc_printf("<script type=\"text/javascript\">	\n"
		"ToggleSievePanels();			\n"
		"</script>				\n"
	);

	wDumpContent(1);

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
	wc_printf("<img src=\"static/webcit_icons/essen/32x32/config.png\">");
	wc_printf(_("Add or delete scripts"));
	wc_printf("</h1>\n");
	wc_printf("</div>\n");
	
	wc_printf("<div id=\"content\" class=\"service\">\n");

	if (message != NULL) {
		wc_printf("%s", message);
	}

	wc_printf("<table border=0 cellspacing=10><tr valign=top><td>\n");

	do_template("box_begin_1");
	StrBufAppendBufPlain(WC->WBuf, _("Add a new script"), -1, 0);
	do_template("box_begin_2");

	wc_printf(_("To create a new script, enter the desired "
		"script name in the box below and click 'Create'."));
	wc_printf("<br><br>");

        wc_printf("<center><form method=\"POST\" action=\"create_script\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wc_printf(_("Script name: "));
        wc_printf("<input type=\"text\" name=\"script_name\"><br>\n"
        	"<input type=\"submit\" name=\"create_button\" value=\"%s\">"
		"</form></center>\n", _("Create"));

	do_template("box_end");

	do_template("box_begin_1");
	StrBufAppendBufPlain(WC->WBuf, _("Edit scripts"), -1, 0);
	do_template("box_begin_2");
	wc_printf("<br><div align=center><a href=\"display_sieve\">%s</a><br><br>\n",
		_("Return to the script editing screen")
	);
	do_template("box_end");

	wc_printf("</td><td>");

	do_template("box_begin_1");
	StrBufAppendBufPlain(WC->WBuf, _("Delete scripts"), -1, 0);
	do_template("box_begin_2");

	wc_printf(_("To delete an existing script, select the script "
		"name from the list and click 'Delete'."));
	wc_printf("<br><br>");
	
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
        wc_printf("</select>\n\n<br>\n");

        wc_printf("<input type=\"submit\" name=\"delete_button\" value=\"%s\" "
		"onClick=\"return confirm('%s');\">", _("Delete script"), _("Delete this script?"));
        wc_printf("</form></center>\n");
	do_template("box_end");

	wc_printf("</td></tr></table>\n");

	wDumpContent(1);
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
	wc_printf("<script type=\"text/javascript\">\n"
		  "\n"
		  "var highest_active_rule = (-1);\n"
		  "\n"
		  "function UpdateRules() {\n");
/*
 * Show only the active rows...
 */
	wc_printf("  highest_active_rule = (-1);\n");
	wc_printf("  for (i=0; i<%d; ++i) {\n", MAX_RULES);
	wc_printf("   if ($('active'+i).checked) {\n"
		  "     $('rule' + i).style.display = 'block';\n"
		  "     highest_active_rule = i;\n"
		  "   }\n"
		  "   else {\n"
		  "     $('rule' + i).style.display = 'none';\n"
		  "   }\n"
		  "  }\n");
/*
 * Show only the fields relevant to the rules...
 */
	wc_printf("  for (i=0; i<=highest_active_rule; ++i) {\n"
		  "    d = ($('movedown'+i));\n"
		  "    if (i < highest_active_rule) {\n"
		  "      d.style.display = 'block';\n"
		  "    }\n"
		  "    else {\n"
		  "      d.style.display = 'none';\n"
		  "    }\n"
		  "    d = ($('hfield'+i).options[$('hfield'+i).selectedIndex].value);\n"
		  "    if (d == 'all') {\n"
		  "      $('div_size'+i).style.display = 'none'; \n"
		  "      $('div_compare'+i).style.display = 'none';\n"
		  "      $('div_nocompare'+i).style.display = 'block';\n"
		  "    }\n"
		  "    else if (d == 'size') {\n"
		  "      $('div_size'+i).style.display = 'block'; \n"
		  "      $('div_compare'+i).style.display = 'none';\n"
		  "      $('div_nocompare'+i).style.display = 'none';\n"
		  "    }\n"
		  "    else {\n"
		  "      $('div_size'+i).style.display = 'none'; \n"
		  "      $('div_compare'+i).style.display = 'block';\n"
		  "      $('div_nocompare'+i).style.display = 'none';\n"
		  "    }\n"
		  "    d = ($('action'+i).options[$('action'+i).selectedIndex].value);\n"
		  "    if (d == 'fileinto') {\n"
		  "      $('div_fileinto'+i).style.display = 'block';\n"
		  "      $('div_redirect'+i).style.display = 'none';\n"
		  "      $('div_automsg'+i).style.display = 'none';\n"
		  "    } else if (d == 'redirect') {\n"
		  "      $('div_fileinto'+i).style.display = 'none';\n"
		  "      $('div_redirect'+i).style.display = 'block';\n"
		  "      $('div_automsg'+i).style.display = 'none';\n"
		  "    } else if ((d == 'reject') || (d == 'vacation'))  {\n"
		  "      $('div_fileinto'+i).style.display = 'none';\n"
		  "      $('div_redirect'+i).style.display = 'none';\n"
		  "      $('div_automsg'+i).style.display = 'block';\n"
		  "    } else {\n"
		  "      $('div_fileinto'+i).style.display = 'none';\n"
		  "      $('div_redirect'+i).style.display = 'none';\n"
		  "      $('div_automsg'+i).style.display = 'none';\n"
		  "    }\n"
		  "    if (highest_active_rule < %d) {\n", MAX_RULES-1 );
	wc_printf("      $('div_addrule').style.display = 'block';\n"
		  "    } else {\n"
		  "      $('div_addrule').style.display = 'none';\n"
		  "    }\n"
		  "  }\n"
		  "}\n"
/*
 * Add a rule (really, just un-hide it)
 */
		  "function AddRule() {\n"
		  "  highest_active_rule = highest_active_rule + 1;\n"
		  "  $('active'+highest_active_rule).checked = true;\n"
		  "  UpdateRules();\n"
		  "}\n"
/*
 * Swap two rules
 */
		  "function SwapRules(ra, rb) {\n"
		  "\n"
		  "  var things = new Array();\n"
		  "  things[0] = 'hfield';\n"
		  "  things[1] = 'compare';\n"
		  "  things[2] = 'htext';\n"
		  "  things[3] = 'action';\n"
		  "  things[4] = 'fileinto';\n"
		  "  things[5] = 'redirect';\n"
		  "  things[6] = 'final';\n"
		  "  things[7] = 'sizecomp';\n"
		  "  things[8] = 'sizeval';\n"
		  "  things[9] = 'automsg';\n"
		  "\n"
		  "  for (i=0; i<=9; ++i) {\n"
		  "    tempval=$(things[i]+ra).value;\n"
		  "    $(things[i]+ra).value = $(things[i]+rb).value;\n"
		  "    $(things[i]+rb).value = tempval;\n"
		  "  }\n"
		  "}\n"
/*
 * Delete a rule (percolate the deleted rule out to the end, then deactivate it)
 */
		  "function DeleteRule(rd) {\n"
		  "  for (j=rd; j<=highest_active_rule; ++j) {\n"
		  "    SwapRules(j, (j+1));\n"
		  "  }\n"
		  "  $('active'+highest_active_rule).checked = false;\n"
		  "}\n"
		  "</script>\n"
		);


	wc_printf("<br>");

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

		wc_printf("<tr id=\"rule%d\" class=\"%s\">",
			i,
			((i%2) ? "odd" : "even")
		);

		wc_printf("<td width=5%% align=\"center\">\n");

		wc_printf("<div style=\"display:none\">\n");
		wc_printf("<input type=\"checkbox\" name=\"active%d\" id=\"active%d\" %s>\n",
			i, i,
			(active ? "checked" : "")
		);
		wc_printf("</div>\n");

		if (i>0) wc_printf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img border=\"0\" src=\"static/webcit_icons/up_pointer.gif\" "
			"title=\"%s\"/></a>\n",
			i-1, i, _("Move rule up") );

		wc_printf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img id=\"movedown%d\" border=\"0\" src=\"static/webcit_icons/down_pointer.gif\" "
			"title=\"%s\"/></a>\n",
			i, i+1, i, _("Move rule down") );

		wc_printf("<a href=\"javascript:DeleteRule(%d);UpdateRules();\">"
			"<img id=\"delete%d\" border=\"0\" src=\"static/webcit_icons/delete.gif\" "
			"title=\"%s\"/></a>\n",
			i, i, _("Delete rule") );

		wc_printf("</td>\n\n\n");

		wc_printf("<td width=5%% align=\"center\">\n");
		wc_printf("<font size=+2>%d</font>\n", i+1);
		wc_printf("</td>\n");

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
			wc_printf("<option %s value=\"%s\">%s</option>\n",
				( (!strcasecmp(hfield, hfield_values[j][0])) ? "selected" : ""),
				hfield_values[j][0],
				hfield_values[j][1]
			);
		}

		wc_printf("</select>\n\n");
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

		wc_printf("<div id=\"div_compare%d\">\n", i);
		wc_printf("<select id=\"compare%d\" name=\"compare%d\" size=1 onChange=\"UpdateRules();\">\n",
			i, i);
		for (j=0; j<6; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>\n",
				( (!strcasecmp(compare, compare_values[j][0])) ? "selected" : ""),
				compare_values[j][0],
				compare_values[j][1]
			);
		}
		wc_printf("</select>\n\n");

		wc_printf("<input type=\"text\" id=\"htext%d\" name=\"htext%d\" value=\"", i, i);
		escputs(htext);
		wc_printf("\">\n</div>\n");

		wc_printf("<div id=\"div_nocompare%d\">", i);
		wc_printf("%s", _("(All messages)"));
		wc_printf("</div>\n");

		char *sizecomp_values[2][2] = {
			{	"larger",	_("is larger than")	},
			{	"smaller",	_("is smaller than")	}
		};

		wc_printf("<div id=\"div_size%d\">\n", i);
		wc_printf("<select id=\"sizecomp%d\" name=\"sizecomp%d\" size=1 onChange=\"UpdateRules();\">\n",
			i, i);
		for (j=0; j<2; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>\n",
				( (!strcasecmp(sizecomp, sizecomp_values[j][0])) ? "selected" : ""),
				sizecomp_values[j][0],
				sizecomp_values[j][1]
			);
		}
		wc_printf("</select>\n\n");

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

		wc_printf("<td width=20%%>\n");
		wc_printf("<select id=\"action%d\" name=\"action%d\" size=1 onChange=\"UpdateRules();\">\n",
			i, i);
		for (j=0; j<6; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>\n",
				( (!strcasecmp(action, action_values[j][0])) ? "selected" : ""),
				action_values[j][0],
				action_values[j][1]
			);
		}
		wc_printf("</select>\n\n");

		wc_printf("<div id=\"div_fileinto%d\">\n", i);
		wc_printf("<select name=\"fileinto%d\" id=\"fileinto%d\">\n", i, i);
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
		wc_printf("</select>\n\n");
		wc_printf("</div>");

		wc_printf("<div id=\"div_redirect%d\">\n", i);
		wc_printf("<input type=\"text\" id=\"redirect%d\" name=\"redirect%d\" value=\"", i, i);
		escputs(redirect);
		wc_printf("\">\n</div>\n");

		wc_printf("<div id=\"div_automsg%d\">\n", i);
		wc_printf(_("Message:"));
		wc_printf("<br>\n");
		wc_printf("<textarea name=\"automsg%d\" id=\"automsg%d\" wrap=soft rows=5>\n", i, i);
		escputs(automsg);
		wc_printf("</textarea>");
		wc_printf("</div>\n");

		wc_printf("</td>\n");

		char *final_values[2][2] = {
			{	"continue",	_("continue processing")	},
			{	"stop",		_("stop")			}
		};

		wc_printf("<td width=10%% align=\"center\">%s</td>\n", _("and then") );

		wc_printf("<td width=20%%>\n");
		wc_printf("<select name=\"final%d\" id=\"final%d\" size=1 onChange=\"UpdateRules();\">\n",
			i, i);
		for (j=0; j<2; ++j) {
			wc_printf("<option %s value=\"%s\">%s</option>\n",
				( (!strcasecmp(final, final_values[j][0])) ? "selected" : ""),
				final_values[j][0],
				final_values[j][1]
			);
		}
		wc_printf("</select>\n\n");
		wc_printf("</td>\n");

		wc_printf("</tr>\n");

	}

	wc_printf("</table>\n");
	wc_printf("<div id=\"div_addrule\"><a href=\"javascript:AddRule();\">%s</a><br></div>\n",
		_("Add rule")
	);

	wc_printf("<script type=\"text/javascript\">\n");
	wc_printf("UpdateRules();\n");
	wc_printf("</script>\n");

	free(rooms);
}
void _display_add_remove_scripts(void) {display_add_remove_scripts(NULL);}
#endif


typedef struct __SieveListing {
	int IsActive;
	int IsRulesScript;
	StrBuf *Name;
	StrBuf *Content;
} SieveListing;

int ConditionalSieveScriptIsActive(StrBuf *Target, WCTemplputParams *TP)
{
	SieveListing     *SieveList = (SieveListing *)CTX;
	return SieveList->IsActive;
}
int ConditionalSieveScriptIsRulesScript(StrBuf *Target, WCTemplputParams *TP)
{
	SieveListing     *SieveList = (SieveListing *)CTX;
	return SieveList->IsActive;
}
void tmplput_SieveScriptName(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveListing     *SieveList = (SieveListing *)CTX;
	StrBufAppendTemplate(Target, TP, SieveList->Name, 0);
}
void tmplput_SieveScriptContent(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveListing     *SieveList = (SieveListing *)CTX;
	StrBufAppendTemplate(Target, TP, SieveList->Content, 0);
}
void FreeSieveListing(void *vSieveListing)
{
	SieveListing *List = (SieveListing*) vSieveListing;

	FreeStrBuf(&List->Name);
	free(List);
}

HashList *GetSieveScriptListing(StrBuf *Target, WCTemplputParams *TP)
{
        wcsession *WCC = WC;
	StrBuf *Line;
	int num_scripts = 0;
	int rules_script_active = 0;
	int have_rules_script = 0;
	const char *pch;
	HashPos  *it;
	int Done = 0;
	SieveListing *Ruleset;

	if (WCC->KnownSieveScripts != NULL)
		return WCC->KnownSieveScripts;

	serv_puts("MSIV listscripts");
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, NULL) == 1) 
	{
		WCC->KnownSieveScripts = NewHash(1, Flathash);

		while(!Done && (StrBuf_ServGetln(Line) >= 0) )
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000")) 
			{
				Done = 1;
			}
			else
			{
				pch = NULL;
				Ruleset = (SieveListing *) malloc(sizeof(SieveListing));
				Ruleset->Name = NewStrBufPlain(NULL, StrLength(Line));
				StrBufExtract_NextToken(Ruleset->Name, Line, &pch, '|');
				Ruleset->IsActive = StrBufExtractNext_int(Line, &pch, '|'); 
				Ruleset->Content = NULL;

				if (!strcasecmp(ChrPtr(Ruleset->Name), RULES_SCRIPT))
				{
					Ruleset->IsRulesScript = 1;
					have_rules_script = 1;
					if (Ruleset->IsActive)
					{
						rules_script_active = 1;
						PutBstr(HKEY("__SIEVE:RULESSCRIPT"), NewStrBufPlain(HKEY("1")));
					}
				}
				Put(WCC->KnownSieveScripts, IKEY(num_scripts), Ruleset, FreeSieveListing);

				++num_scripts;
			}
	}
	if ((num_scripts > 0) && (rules_script_active == 0))
		PutBstr(HKEY("__SIEVE:EXTERNAL_SCRIPT"), NewStrBufPlain(HKEY("1")));

	if (num_scripts > have_rules_script)
	{
		long rc = 0;
		long len;
		const char *Key;
		void *vRuleset;

		/* 
		 * ok; we have custom scripts, expose that via bstr, and load the payload.
		 */
		PutBstr(HKEY("__SIEVE:HAVE_EXTERNAL_SCRIPT"), NewStrBufPlain(HKEY("1")));

		it = GetNewHashPos(WCC->KnownSieveScripts, 0);
		while (GetNextHashPos(WCC->KnownSieveScripts, it, &len, &Key, &vRuleset) && 
		       (vRuleset != NULL))
		{
			Ruleset = (SieveListing *) vRuleset;

			/*
			 * its the webcit rule? we don't need to load that here.
			 */
			if (Ruleset->IsRulesScript)
				continue;

			if (!serv_printf("MSIV getscript|%s", ChrPtr(Ruleset->Name)))
				break;
			StrBuf_ServGetln(Line);
			if (GetServerStatus(Line, NULL) == 1) 
			{
				Ruleset->Content = NewStrBuf();
				while(!Done && (rc = StrBuf_ServGetln(Line), rc >= 0) )
					if ( (StrLength(Line)==3) && 
					     !strcmp(ChrPtr(Line), "000")) 
					{
						Done = 1;
					}
					else
					{
						if (StrLength(Ruleset->Content)>0)
							StrBufAppendBufPlain(Ruleset->Content, HKEY("\n"), 0);
						StrBufAppendBuf(Ruleset->Content, Line, 0);
					}
				if (rc < 0) break;
			}
		}
	}
	FreeStrBuf(&Line);
	return WCC->KnownSieveScripts;
}


typedef enum __eSieveHfield 
{
	from,		
	tocc,		
	subject,	
	replyto,	
	sender,	
	resentfrom,	
	resentto,	
	envfrom,	
	envto,	
	xmailer,	
	xspamflag,	
	xspamstatus,	
	listid,	
	size,		
	all
} eSieveHfield;

typedef enum __eSieveCompare {
	contains,
	notcontains,
	is,
	isnot,
	matches,
	notmatches
} eSieveCompare;

typedef enum __eSieveAction {
	keep,
	discard,
	reject,
	fileinto,
	redirect,
	vacation
} eSieveAction;


typedef enum __eSieveSizeComp {
	larger,
	smaller
} eSieveSizeComp;

typedef enum __eSieveFinal {
	econtinue,
	estop
} eSieveFinal;


typedef struct __SieveRule {
	int active;
	int sizeval;
	eSieveHfield hfield;
	eSieveCompare compare;
	StrBuf *htext;
	eSieveSizeComp sizecomp;
	eSieveAction Action;
	StrBuf *fileinto;
	StrBuf *redirect;
	StrBuf *automsg;
	eSieveFinal final;
}SieveRule;



int ConditionalSieveRule_hfield(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
	
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      from)
                ==
                Rule->hfield;
}
int ConditionalSieveRule_compare(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      contains)
                ==
		Rule->compare;
}
int ConditionalSieveRule_action(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      keep)
                ==
		Rule->Action; 
}
int ConditionalSieveRule_sizecomp(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      larger)
                ==
		Rule->sizecomp;
}
int ConditionalSieveRule_final(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      econtinue)
                ==
		Rule->final;
}
int ConditionalSieveRule_ThisRoom(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return GetTemplateTokenNumber(Target, 
                                      TP, 
                                      3, 
                                      econtinue)
                ==
		Rule->final;
}
int ConditionalSieveRule_Active(StrBuf *Target, WCTemplputParams *TP)
{
	SieveRule     *Rule = (SieveRule *)CTX;
        return Rule->active;
}
void tmplput_SieveRule_htext(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveRule     *Rule = (SieveRule *)CTX;
	StrBufAppendTemplate(Target, TP, Rule->htext, 0);
}
void tmplput_SieveRule_fileinto(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveRule     *Rule = (SieveRule *)CTX;
	StrBufAppendTemplate(Target, TP, Rule->fileinto, 0);
}
void tmplput_SieveRule_redirect(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveRule     *Rule = (SieveRule *)CTX;
	StrBufAppendTemplate(Target, TP, Rule->redirect, 0);
}
void tmplput_SieveRule_automsg(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveRule     *Rule = (SieveRule *)CTX;
	StrBufAppendTemplate(Target, TP, Rule->automsg, 0);
}
void tmplput_SieveRule_sizeval(StrBuf *Target, WCTemplputParams *TP) 
{
	SieveRule     *Rule = (SieveRule *)CTX;
	StrBufAppendPrintf(Target, "%d", Rule->sizeval);
}

void tmplput_SieveRule_lookup_FileIntoRoom(StrBuf *Target, WCTemplputParams *TP) 
{
	void *vRoom;
	SieveRule     *Rule = (SieveRule *)CTX;
        wcsession *WCC = WC;
	HashList *Rooms = GetRoomListHashLKRA(Target, TP);

	GetHash(Rooms, SKEY(Rule->fileinto), &vRoom);
	WCC->ThisRoom = (folder*) vRoom;
}

void FreeSieveRule(void *vRule)
{
	SieveRule *Rule = (SieveRule*) vRule;

	FreeStrBuf(&Rule->htext);
	FreeStrBuf(&Rule->fileinto);
	FreeStrBuf(&Rule->redirect);
	FreeStrBuf(&Rule->automsg);
	
	free(Rule);
}

#define WC_RULE_HEADER "# WEBCIT_RULE|"
HashList *GetSieveRules(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Line = NULL;
	StrBuf *EncodedRule = NULL;
	int n = 0;
	const char *pch = NULL;
	HashList *SieveRules = NULL;
	int Done = 0;
	SieveRule *Rule = NULL;

	SieveRules = NewHash(1, Flathash);
	serv_printf("MSIV getscript|"RULES_SCRIPT);
	Line = NewStrBuf();
	EncodedRule = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, NULL) == 1) 
	{
		while(!Done && (StrBuf_ServGetln(Line) >= 0) )
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000")) 
			{
				Done = 1;
			}
			else
			{
				pch = NULL;
				/* We just care for our encoded header and skip everything else */
				if ((StrLength(Line) > sizeof(WC_RULE_HEADER) - 1) &&
				    (!strncasecmp(ChrPtr(Line), HKEY(WC_RULE_HEADER))))
				{
					StrBufSkip_NTokenS(Line, &pch, '|', 1);
					n = StrBufExtractNext_int(Line, &pch, '|'); 
					StrBufExtract_NextToken(EncodedRule, Line, &pch, '|');
					StrBufDecodeBase64(EncodedRule);

					Rule = (SieveRule*) malloc(sizeof(SieveRule));

					Rule->htext = NewStrBufPlain (NULL, StrLength(EncodedRule));

					Rule->fileinto = NewStrBufPlain (NULL, StrLength(EncodedRule));
					Rule->redirect = NewStrBufPlain (NULL, StrLength(EncodedRule));
					Rule->automsg = NewStrBufPlain (NULL, StrLength(EncodedRule));

					/* Grab our existing values to populate */
					pch = NULL;
					Rule->active = StrBufExtractNext_int(EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Line, EncodedRule, &pch, '|');
					
					Rule->hfield = (eSieveHfield) GetTokenDefine(SKEY(Line), tocc);
					StrBufExtract_NextToken(Line, EncodedRule, &pch, '|');
					Rule->compare = (eSieveCompare) GetTokenDefine(SKEY(Line), contains);
					StrBufExtract_NextToken(Rule->htext, EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Line, EncodedRule, &pch, '|');
					Rule->sizecomp = (eSieveSizeComp) GetTokenDefine(SKEY(Line), larger);
					Rule->sizeval = StrBufExtractNext_int(EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Line, EncodedRule, &pch, '|');
					Rule->Action = (eSieveAction) GetTokenDefine(SKEY(Line), keep);
					StrBufExtract_NextToken(Rule->fileinto, EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Rule->redirect, EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Rule->automsg, EncodedRule, &pch, '|');
					StrBufExtract_NextToken(Line, EncodedRule, &pch, '|');
					Rule->final = (eSieveFinal) GetTokenDefine(SKEY(Line), econtinue);
					Put(SieveRules, IKEY(n), Rule, FreeSieveRule);
					n++;
				}
			}
	}

	while (n < MAX_RULES) {
		Rule = (SieveRule*) malloc(sizeof(SieveRule));
		memset(Rule, 0, sizeof(SieveRule));
		Put(SieveRules, IKEY(n), Rule, FreeSieveRule);
	    
		n++;
	}


	FreeStrBuf(&EncodedRule);
	FreeStrBuf(&Line);
	return SieveRules;
}

void
SessionDetachModule_SIEVE
(wcsession *sess)
{
	DeleteHash(&sess->KnownSieveScripts);
}

void 
InitModule_SIEVE
(void)
{
	REGISTERTokenParamDefine(from);		
	REGISTERTokenParamDefine(tocc);		
	REGISTERTokenParamDefine(subject);	
	REGISTERTokenParamDefine(replyto);	
	REGISTERTokenParamDefine(sender);	
	REGISTERTokenParamDefine(resentfrom);	
	REGISTERTokenParamDefine(resentto);	
	REGISTERTokenParamDefine(envfrom);	
	REGISTERTokenParamDefine(envto);	
	REGISTERTokenParamDefine(xmailer);	
	REGISTERTokenParamDefine(xspamflag);	
	REGISTERTokenParamDefine(xspamstatus);	
	REGISTERTokenParamDefine(listid);	
	REGISTERTokenParamDefine(size);		
	REGISTERTokenParamDefine(all);

	REGISTERTokenParamDefine(contains);
	REGISTERTokenParamDefine(notcontains);
	REGISTERTokenParamDefine(is);
	REGISTERTokenParamDefine(isnot);
	REGISTERTokenParamDefine(matches);
	REGISTERTokenParamDefine(notmatches);

	REGISTERTokenParamDefine(keep);
	REGISTERTokenParamDefine(discard);
	REGISTERTokenParamDefine(reject);
	REGISTERTokenParamDefine(fileinto);
	REGISTERTokenParamDefine(redirect);
	REGISTERTokenParamDefine(vacation);

	REGISTERTokenParamDefine(larger);
	REGISTERTokenParamDefine(smaller);

	/* these are c-keyworads, so do it by hand. */
	RegisterTokenParamDefine(HKEY("continue"), econtinue);
	RegisterTokenParamDefine(HKEY("stop"), estop);

	RegisterIterator("SIEVE:SCRIPTS", 0, NULL, GetSieveScriptListing, NULL, NULL, CTX_SIEVELIST, CTX_NONE, IT_NOFLAG);

	RegisterConditional(HKEY("COND:SIEVE:SCRIPT:ACTIVE"), 0, ConditionalSieveScriptIsActive, CTX_SIEVELIST);
	RegisterConditional(HKEY("COND:SIEVE:SCRIPT:ISRULES"), 0, ConditionalSieveScriptIsRulesScript, CTX_SIEVELIST);
	RegisterNamespace("SIEVE:SCRIPT:NAME", 0, 1, tmplput_SieveScriptName, NULL, CTX_SIEVELIST);
	RegisterNamespace("SIEVE:SCRIPT:CONTENT", 0, 1, tmplput_SieveScriptContent, NULL, CTX_SIEVELIST);

 
	RegisterIterator("SIEVE:RULES", 0, NULL, GetSieveRules, NULL, DeleteHash, CTX_SIEVESCRIPT, CTX_NONE, IT_NOFLAG);

	RegisterConditional(HKEY("COND:SIEVE:ACTIVE"), 1, ConditionalSieveRule_Active, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:HFIELD"), 1, ConditionalSieveRule_hfield, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:COMPARE"), 1, ConditionalSieveRule_compare, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:ACTION"), 1, ConditionalSieveRule_action, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:SIZECOMP"), 1, ConditionalSieveRule_sizecomp, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:FINAL"), 1, ConditionalSieveRule_final, CTX_SIEVESCRIPT);
	RegisterConditional(HKEY("COND:SIEVE:THISROOM"), 1, ConditionalSieveRule_ThisRoom, CTX_SIEVESCRIPT);

	RegisterNamespace("SIEVE:SCRIPT:HTEXT", 0, 1, tmplput_SieveRule_htext, NULL, CTX_SIEVESCRIPT);
	RegisterNamespace("SIEVE:SCRIPT:SIZE", 0, 1, tmplput_SieveRule_sizeval, NULL, CTX_SIEVESCRIPT);
	RegisterNamespace("SIEVE:SCRIPT:FILEINTO", 0, 1, tmplput_SieveRule_fileinto, NULL, CTX_SIEVESCRIPT);
	RegisterNamespace("SIEVE:SCRIPT:REDIRECT", 0, 1, tmplput_SieveRule_redirect, NULL, CTX_SIEVESCRIPT);
	RegisterNamespace("SIEVE:SCRIPT:AUTOMSG", 0, 1, tmplput_SieveRule_automsg, NULL, CTX_SIEVESCRIPT);

	/* fetch our room into WCC->ThisRoom, to evaluate while iterating over rooms with COND:THIS:THAT:ROOM */
	RegisterNamespace("SIEVE:SCRIPT:LOOKUP_FILEINTO", 0, 1, tmplput_SieveRule_lookup_FileIntoRoom, NULL, CTX_SIEVESCRIPT);

#if FOO
	WebcitAddUrlHandler(HKEY("display_sieve"), "", 0, display_sieve, 0);
	WebcitAddUrlHandler(HKEY("display_add_remove_scripts"), "", 0, _display_add_remove_scripts, 0);
#endif
	WebcitAddUrlHandler(HKEY("save_sieve"), "", 0, save_sieve, 0);

	WebcitAddUrlHandler(HKEY("create_script"), "", 0, create_script, 0);
	WebcitAddUrlHandler(HKEY("delete_script"), "", 0, delete_script, 0);
}
