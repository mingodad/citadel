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
#define MAX_RULES	25
#define RULES_SCRIPT	"__WebCit_Generated_Script__"

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
	int rules_script_is_active = 0;
	

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

	wprintf("<option %s value=\"1\">", ((rules_script_is_active) ? "selected" : ""));
	wprintf(_("Filter it according to rules selected below"));
	wprintf("</option>\n");

	wprintf("<option %s value=\"2\">",
			(((active_script >= 0) && (!rules_script_is_active)) ? "selected" : ""));
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
			if (strcasecmp(script_names[i], RULES_SCRIPT)) {
				wprintf("<option %s value=\"%s\">%s</option>\n",
					((active_script == i) ? "selected" : ""),
					script_names[i],
					script_names[i]
				);
			}
		}
		wprintf("</select>\n");
	}

	wprintf("&nbsp;&nbsp;&nbsp;");
	wprintf("<a href=\"display_add_remove_scripts\">%s</a>\n", _("Add or delete scripts"));

	wprintf("<br />\n");

	if (num_scripts > 0) {
		for (i=0; i<num_scripts; ++i) {
			if (strcasecmp(script_names[i], RULES_SCRIPT)) {
				wprintf("<div id=\"script_%s\" style=\"display:none\">\n", script_names[i]);
				wprintf("<textarea name=\"text_%s\" wrap=soft rows=20 cols=80 width=80>\n",
					script_names[i]);
				serv_printf("MSIV getscript|%s", script_names[i]);
				serv_getln(buf, sizeof buf);
				if (buf[0] == '1') while(serv_getln(buf, sizeof (buf)), strcmp(buf, "000")) {
					wprintf("%s\n", buf);
				}
				wprintf("</textarea>\n");
				wprintf("</div>\n");
			}
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
 * \brief Translate the fields from the rule editor into something we can save...
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

	for (i=0; i<MAX_RULES; ++i) {
		
		strcpy(rule, "");

		sprintf(fname, "active%d", i);
		active = !strcasecmp(bstr(fname), "on") ;

		sprintf(fname, "hfield%d", i);
		safestrncpy(hfield, bstr(fname), sizeof hfield);

		sprintf(fname, "compare%d", i);
		safestrncpy(compare, bstr(fname), sizeof compare);

		sprintf(fname, "htext%d", i);
		safestrncpy(htext, bstr(fname), sizeof htext);

		sprintf(fname, "sizecomp%d", i);
		safestrncpy(sizecomp, bstr(fname), sizeof sizecomp);

		sprintf(fname, "sizeval%d", i);
		sizeval = atoi(bstr(fname));

		sprintf(fname, "action%d", i);
		safestrncpy(action, bstr(fname), sizeof action);

		sprintf(fname, "fileinto%d", i);
		safestrncpy(fileinto, bstr(fname), sizeof fileinto);

		sprintf(fname, "redirect%d", i);
		safestrncpy(redirect, bstr(fname), sizeof redirect);

		sprintf(fname, "automsg%d", i);
		safestrncpy(automsg, bstr(fname), sizeof automsg);

		sprintf(fname, "final%d", i);
		safestrncpy(final, bstr(fname), sizeof final);

		snprintf(rule, sizeof rule, "%d|%s|%s|%s|%s|%d|%s|%s|%s|%s|%s",
			active, hfield, compare, htext, sizecomp, sizeval, action, fileinto,
			redirect, automsg, final
		);

		CtdlEncodeBase64(encoded_rule, rule, strlen(rule)+1, 0);
		serv_printf("# WEBCIT_RULE|%d|%s|", i, encoded_rule);

		/* FIXME: this is where we need to generate Sieve code based on the rule */


	}

	serv_puts("000");

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

	bigaction = atoi(bstr("bigaction"));

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
					striplt(bstr(this_name));
					serv_printf("%s", bstr(this_name));
					serv_puts("000");
				}
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
			if ( (extract_int(buf, 1) == 0) && (strcasecmp(script_name, RULES_SCRIPT)) ) {
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
	wprintf("<script type=\"text/javascript\">					\n"
		"									\n"
		"var highest_active_rule = (-1);					\n"
		"									\n"
		"function UpdateRules() {						\n"
		"  for (i=0; i<%d; ++i) {						\n", MAX_RULES);
	wprintf("    d = ($('movedown'+i));						\n"
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
	wprintf("      $('div_addrule').style.display = 'block';			\n"
		"    } else {								\n"
		"      $('div_addrule').style.display = 'none';				\n"
		"    }									\n"
		"  }									\n"
	);
/*
 * Show only the active rows...
 */
	wprintf("  highest_active_rule = (-1);						\n");
	wprintf("  for (i=0; i<%d; ++i) {						\n", MAX_RULES);
	wprintf("   if ($('active'+i).checked) {					\n"
		"     $('rule' + i).style.display = 'block';				\n"
		"     highest_active_rule = i;						\n"
		"   }									\n"
		"   else {								\n"
		"     $('rule' + i).style.display = 'none';				\n"
		"   }									\n"
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
		"  for (i=rd; i<highest_active_rule; ++i) {				\n"
		"    SwapRules(i, (i+1));						\n"
		"  }									\n"
		"  $('active'+highest_active_rule).checked = false;			\n"
		"}									\n"
		"</script>								\n"
	);


	wprintf("<br />");

	wprintf("<table cellpadding=2 width=100%%>");

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

		wprintf("<tr id=\"rule%d\" bgcolor=\"#%s\">",
			i,
			((i%2) ? "DDDDDD" : "FFFFFF")
		);

		wprintf("<td width=5%% align=\"center\">");

		wprintf("<div style=\"display:none\">");
		wprintf("<input type=\"checkbox\" name=\"active%d\" id=\"active%d\" %s>",
			i, i,
			(active ? "checked" : "")
		);
		wprintf("</div>");

		if (i>0) wprintf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img border=\"0\" src=\"static/up_pointer.gif\" "
			"title=\"%s\"/></a>",
			i-1, i, _("Move rule up") );

		wprintf("<a href=\"javascript:SwapRules(%d,%d);UpdateRules();\">"
			"<img id=\"movedown%d\" border=\"0\" src=\"static/down_pointer.gif\" "
			"title=\"%s\"/></a>",
			i, i+1, i, _("Move rule down") );

		wprintf("<a href=\"javascript:DeleteRule(%d);UpdateRules();\">"
			"<img id=\"delete%d\" border=\"0\" src=\"static/delete.gif\" "
			"title=\"%s\"/></a>",
			i, i, _("Delete rule") );

		wprintf("</td>");

		wprintf("<td width=5%% align=\"center\">");
		wprintf("<font size=+2>%d</font>", i+1);
		wprintf("</td>");

		wprintf("<td width=20%%>%s ", _("If") );

		char *hfield_values[14][2] = {
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
			{	"size",		_("Message size")	},
			{	"all",		_("All")		}
		};

		wprintf("<select id=\"hfield%d\" name=\"hfield%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<14; ++j) {
			wprintf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(hfield, hfield_values[j][0])) ? "selected" : ""),
				hfield_values[j][0],
				hfield_values[j][1]
			);
		}

		wprintf("</select>");
		wprintf("</td>");

		wprintf("<td width=20%%>");

		char *compare_values[4][2] = {
			{	"contains",	_("contains")		},
			{	"notcontains",	_("does not contain")	},
			{	"is",		_("is")			},
			{	"isnot",	_("is not")		}
		};

		wprintf("<div id=\"div_compare%d\">", i);
		wprintf("<select id=\"compare%d\" name=\"compare%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<4; ++j) {
			wprintf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(compare, compare_values[j][0])) ? "selected" : ""),
				compare_values[j][0],
				compare_values[j][1]
			);
		}
		wprintf("</select>");

		wprintf("<input type=\"text\" id=\"htext%d\" name=\"htext%d\" value=\"", i, i);
		escputs(htext);
		wprintf("\"></div>");

		wprintf("<div id=\"div_nocompare%d\">", i);
		wprintf("%s", _("(All messages)"));
		wprintf("</div>");

		char *sizecomp_values[2][2] = {
			{	"larger",	_("is larger than")	},
			{	"smaller",	_("is smaller than")	}
		};

		wprintf("<div id=\"div_size%d\">", i);
		wprintf("<select id=\"sizecomp%d\" name=\"sizecomp%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<2; ++j) {
			wprintf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(sizecomp, sizecomp_values[j][0])) ? "selected" : ""),
				sizecomp_values[j][0],
				sizecomp_values[j][1]
			);
		}
		wprintf("</select>");

		wprintf("<input type=\"text\" id=\"sizeval%d\" name=\"sizeval%d\" value=\"%d\">",
			i, i, sizeval);
		wprintf("bytes");
		wprintf("</div>");

		wprintf("</td>");

		char *action_values[6][2] = {
			{	"keep",		_("Keep")		},
			{	"discard",	_("Discard silently")	},
			{	"reject",	_("Reject")		},
			{	"fileinto",	_("Move message to")	},
			{	"redirect",	_("Forward to")		},
			{	"vacation",	_("Vacation")		}
		};

		wprintf("<td width=20%%>");
		wprintf("<select id=\"action%d\" name=\"action%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<6; ++j) {
			wprintf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(action, action_values[j][0])) ? "selected" : ""),
				action_values[j][0],
				action_values[j][1]
			);
		}
		wprintf("</select>");

		wprintf("<div id=\"div_fileinto%d\">", i);
		wprintf("<select name=\"fileinto%d\" id=\"fileinto%d\">", i, i);
		for (j=0; j<num_roomnames; ++j) {
			wprintf("<option ");
			if (!strcasecmp(rooms[j].name, fileinto)) {
				wprintf("selected ");
			}
			wprintf("value=\"");
			urlescputs(rooms[j].name);
			wprintf("\">");
			escputs(rooms[j].name);
			wprintf("</option>\n");
		}
		wprintf("</select>\n");
		wprintf("</div>");

		wprintf("<div id=\"div_redirect%d\">", i);
		wprintf("<input type=\"text\" id=\"redirect%d\" name=\"redirect%d\" value=\"", i, i);
		escputs(redirect);
		wprintf("\"></div>");

		wprintf("<div id=\"div_automsg%d\">", i);
		wprintf(_("Message:"));
		wprintf("<br />");
		wprintf("<textarea name=\"automsg%d\" id=\"automsg%d\" wrap=soft rows=5>\n", i, i);
		escputs(automsg);
		wprintf("</textarea>");
		wprintf("</div>");

		wprintf("</td>");

		char *final_values[2][2] = {
			{	"continue",	_("continue processing")	},
			{	"stop",		_("stop")			}
		};

		wprintf("<td width=10%% align=\"center\">%s</td>", _("and then") );

		wprintf("<td width=20%%>");
		wprintf("<select name=\"final%d\" id=\"final%d\" size=1 onChange=\"UpdateRules();\">",
			i, i);
		for (j=0; j<2; ++j) {
			wprintf("<option %s value=\"%s\">%s</option>",
				( (!strcasecmp(final, final_values[j][0])) ? "selected" : ""),
				final_values[j][0],
				final_values[j][1]
			);
		}
		wprintf("</select>");
		wprintf("</td>");

		wprintf("</tr>\n");

	}

	wprintf("</table>");
	wprintf("<div id=\"div_addrule\"><a href=\"javascript:AddRule();\">Add rule</a><br /></div>\n");

	wprintf("<script type=\"text/javascript\">					\n");
	wprintf("UpdateRules();								\n");
	wprintf("</script>								\n");

	free(rooms);
}






/*@}*/
