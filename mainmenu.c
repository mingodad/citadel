/*
 * $Id$
 */
/**
 * \defgroup DispAdvancedMenu Displays the "advanced" (main) menu.
 * \ingroup MenuInfrastructure
 *
 */
/*@{*/
#include "webcit.h"

/**
 * \brief The Main Menu
 */
void display_main_menu(void)
{
	output_headers(1, 1, 1, 0, 0, 0);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<TABLE WIDTH=100%%>"
		"<TR><TD COLSPAN=2>\n");

	svprintf("BOXTITLE", WCS_STRING, _("Basic commands"));
	do_template("beginbox");

	wprintf("\n"
		"<TABLE border=0 cellspacing=1 cellpadding=1 width=100%%>"
		"<TR>"
		"<TD>");	/**< start of first column */

	wprintf("<a href=\"knrooms\"><span class=\"mainmenu\">");
	wprintf(_("List known rooms"));
	wprintf("</span></A><br /><span class=\"menudesc\">");
	wprintf(_("Where can I go from here?"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"gotonext\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Goto next room"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...with <EM>unread</EM> messages"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"skip\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Skip to next room"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("(come back here later)"));
	wprintf("</span>\n");

	if ((strlen(WC->ugname) > 0) && (strcasecmp(WC->ugname, WC->wc_roomname))) {
		wprintf("<br />"
			"<a href=\"ungoto\">"
			"<span class=\"mainmenu\">");
		wprintf(_("Ungoto"));
		wprintf("</span></A><br />"
			"<span class=\"menudesc\">");
		wprintf(_("(oops! Back to %s)"), WC->ugname);
		wprintf("</span>\n");
	}

	wprintf("</TD><TD>\n");	/* start of second column */

	wprintf("<a href=\"readnew\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Read new messages"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...in this room"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"readfwd\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Read all messages"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...old <EM>and</EM> new"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"display_enter\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Enter a message"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("(post in this room)"));
	wprintf("</span>\n");

	wprintf("</TD><TD>");	/* start of third column */

	wprintf("<a href=\"summary\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Summary page"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("Summary of my account"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"userlist\">\n"
		"<span class=\"mainmenu\">");
	wprintf(_("User list"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("(all registered users)"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"termquit\" TARGET=\"_top\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Log off"));
	wprintf("</span></A><br />"
		"<span class=\"menudesc\">");
	wprintf(_("Bye!"));
	wprintf("</span>\n");

	wprintf("</TD></TR></TABLE>\n");
	do_template("endbox");

	wprintf("</TD></TR>"
		"<TR VALIGN=TOP><TD>");

	svprintf("BOXTITLE", WCS_STRING, _("Your info"));
	do_template("beginbox");

	wprintf("<a href=\"display_preferences\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Change your preferences and settings"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"display_reg\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Update your contact information"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"display_changepw\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Change your password"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"display_editbio\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Enter your 'bio'"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_editpic\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Edit your online photo"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_sieve\">"
		"<span class=\"mainmenu\">");
	wprintf(_("View/edit server-side mail filters"));
	wprintf("</span></a>\n");

	do_template("endbox");

	wprintf("</TD><TD>");

	svprintf("BOXTITLE", WCS_STRING, _("Advanced room commands"));
	do_template("beginbox");

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<a href=\"display_editroom\">"
			"<span class=\"mainmenu\">");
		wprintf(_("Edit or delete this room"));
		wprintf("</span></A><br />\n");
	}

	wprintf("<a href=\"display_private\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Go to a 'hidden' room"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"display_entroom\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Create a new room"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"display_zap\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Zap (forget) this room (%s)"), WC->wc_roomname);
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"zapped_list\">"
		"<span class=\"mainmenu\">");
	wprintf(_("List all forgotten rooms"));
	wprintf("</span></A>\n");

	do_template("endbox");

	wprintf("</td></tr></table></div>");
	wDumpContent(2);
}


/**
 * \brief System administration menu
 */
void display_aide_menu(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("System Administration Menu"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%%><tr valign=top><td>");

	svprintf("BOXTITLE", WCS_STRING, _("Global Configuration"));
	do_template("beginbox");

	wprintf("<a href=\"display_siteconfig\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Edit site-wide configuration"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"display_inetconf\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Domain names and Internet mail configuration"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_netconf\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Configure replication with other Citadel servers"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"display_smtpqueue\">"
		"<span class=\"mainmenu\">");
	wprintf(_("View the outbound SMTP queue"));
	wprintf("</span></A>\n");

	do_template("endbox");

	wprintf("</td><td>");

	svprintf("BOXTITLE", WCS_STRING, _("User account management"));
	do_template("beginbox");

	wprintf("<a href=\"select_user_to_edit\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Add, change, delete user accounts"));
	wprintf("</span></A><br />\n");

	wprintf("<a href=\"validate\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Validate new users"));
	wprintf("</span></A><br />\n");

	do_template("endbox");

	svprintf("BOXTITLE", WCS_STRING, _("Rooms and Floors"));
	do_template("beginbox");

	wprintf("<a href=\"display_floorconfig\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Add, change, or delete floors"));
	wprintf("</span></A>\n");

	do_template("endbox");

	wprintf("</td></tr></table></div>");
	wDumpContent(2);
}





/**
 * \brief Display the screen to enter a generic server command
 */
void display_generic(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Enter a server command"));
	wprintf("</SPAN></TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf("<CENTER>");
	wprintf(_("This screen allows you to enter Citadel server commands which are "
		"not supported by WebCit.  If you do not know what that means, "
		"then this screen will not be of much use to you."));
	wprintf("<br />\n");

	wprintf("<FORM METHOD=\"POST\" action=\"do_generic\">\n");

	wprintf(_("Enter command:"));
	wprintf("<br /><INPUT TYPE=\"text\" NAME=\"g_cmd\" SIZE=80 MAXLENGTH=\"250\"><br />\n");

	wprintf(_("Command input (if requesting SEND_LISTING transfer mode):"));
	wprintf("<br /><TEXTAREA NAME=\"g_input\" ROWS=10 COLS=80 WIDTH=80></TEXTAREA><br />\n");

	wprintf("<FONT SIZE=-2>");
	wprintf(_("Detected host header is %s://%s"), (is_https ? "https" : "http"), WC->http_host);
	wprintf("</FONT>\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc_button\" VALUE=\"%s\">", _("Send command"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\"><br />\n", _("Cancel"));

	wprintf("</FORM></CENTER>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/**
 * \brief Interactive window to perform generic Citadel server commands.
 */
void do_generic(void)
{
	char buf[SIZ];
	char gcontent[SIZ];
	char *junk;
	size_t len;

	if (strlen(bstr("sc_button")) == 0) {
		display_main_menu();
		return;
	}

	output_headers(1, 1, 0, 0, 0, 0);

	serv_printf("%s", bstr("g_cmd"));
	serv_getln(buf, sizeof buf);

	svprintf("BOXTITLE", WCS_STRING, _("Server command results"));
	do_template("beginbox");

	wprintf("<TABLE border=0><TR><TD>Command:</TD><TD><TT>");
	escputs(bstr("g_cmd"));
	wprintf("</TT></TD></TR><TR><TD>Result:</TD><TD><TT>");
	escputs(buf);
	wprintf("</TT></TD></TR></TABLE><br />\n");

	if (buf[0] == '8') {
		serv_printf("\n\n000");
	}
	if ((buf[0] == '1') || (buf[0] == '8')) {
		while (serv_getln(gcontent, sizeof gcontent), strcmp(gcontent, "000")) {
			escputs(gcontent);
			wprintf("<br />\n");
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"));
		serv_puts("000");
	}
	if (buf[0] == '6') {
		len = atol(&buf[4]);
		junk = malloc(len);
		serv_read(junk, len);
		free(junk);
	}
	if (buf[0] == '7') {
		len = atol(&buf[4]);
		junk = malloc(len);
		memset(junk, 0, len);
		serv_write(junk, len);
		free(junk);
	}
	wprintf("<hr />");
	wprintf("<a href=\"display_generic\">Enter another command</A><br />\n");
	wprintf("<a href=\"display_advanced\">Return to menu</A>\n");
	do_template("endbox");
	wDumpContent(1);
}


/**
 * \brief Display the menubar.  
 * \param as_single_page Set to display HTML headers and footers -- otherwise it's assumed
 * that the menubar is being embedded in another page.
 */
void display_menubar(int as_single_page) {

	if (as_single_page) {
		output_headers(0, 0, 0, 0, 0, 0);
		wprintf("<HTML>\n"
			"<HEAD>\n"
			"<TITLE>MenuBar</TITLE>\n"
			"<STYLE TYPE=\"text/css\">\n"
			"BODY	{ text-decoration: none; }\n"
			"</STYLE>\n"
			"</HEAD>\n");
		do_template("background");
	}

	do_template("menubar");

	if (as_single_page) {
		wDumpContent(2);
	}


}


/*@}*/
