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
		"<table width=100%%>"
		"<tr><td colspan=\"2\">\n");

	svprintf("BOXTITLE", WCS_STRING, _("Basic commands"));
	do_template("beginbox");

	wprintf("\n"
		"<table border=0 cellspacing=1 cellpadding=1 width=100%%>"
		"<tr>"
		"<td>");	/**< start of first column */

	wprintf("<a href=\"knrooms\"><span class=\"mainmenu\">");
	wprintf(_("List known rooms"));
	wprintf("</span></a><br /><span class=\"menudesc\">");
	wprintf(_("Where can I go from here?"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"gotonext\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Goto next room"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...with <em>unread</em> messages"));
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
		wprintf("</span></a><br />"
			"<span class=\"menudesc\">");
		wprintf(_("(oops! Back to %s)"), WC->ugname);
		wprintf("</span>\n");
	}

	wprintf("</td><td>\n");	/* start of second column */

	wprintf("<a href=\"readnew\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Read new messages"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...in this room"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"readfwd\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Read all messages"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("...old <EM>and</EM> new"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"display_enter\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Enter a message"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("(post in this room)"));
	wprintf("</span>\n");

	if (WC->room_flags & QR_VISDIR) {
		wprintf("<br /><a href=\"display_room_directory\">"
			"<span class=\"mainmenu\">");
		wprintf(_("File library"));
		wprintf("</span></a><br />"
			"<span class=\"menudesc\">");
		wprintf(_("(List files available for download)"));
		wprintf("</span>\n");
	}

	wprintf("</td><td>");	/* start of third column */

	wprintf("<a href=\"summary\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Summary page"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("Summary of my account"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"userlist\">\n"
		"<span class=\"mainmenu\">");
	wprintf(_("User list"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("(all registered users)"));
	wprintf("</span><br />\n");

	wprintf("<a href=\"termquit\" TARGET=\"_top\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Log off"));
	wprintf("</span></a><br />"
		"<span class=\"menudesc\">");
	wprintf(_("Bye!"));
	wprintf("</span>\n");

	wprintf("</td></tr></table>\n");
	do_template("endbox");

	wprintf("</td></tr>"
		"<tr><td colspan=2></td></tr>"
		"<tr valign=top><td width=50%%>");

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
	wprintf("</span></a><br />\n");

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

	wprintf("</td><td width=50%%>");

	svprintf("BOXTITLE", WCS_STRING, _("Advanced room commands"));
	do_template("beginbox");

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<a href=\"display_editroom\">"
			"<span class=\"mainmenu\">");
		wprintf(_("Edit or delete this room"));
		wprintf("</span></a><br />\n");
	}

	wprintf("<a href=\"display_private\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Go to a 'hidden' room"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_entroom\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Create a new room"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_zap\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Zap (forget) this room (%s)"), WC->wc_roomname);
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"zapped_list\">"
		"<span class=\"mainmenu\">");
	wprintf(_("List all forgotten rooms"));
	wprintf("</span></a>\n");

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
		"<table class=\"mainmenu_banner\"><tr><td>"
		"<span class=\"titlebar\">");
	wprintf(_("System Administration Menu"));
	wprintf("</span>"
		"</td></tr></table>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%%><tr valign=top><td>");

	svprintf("BOXTITLE", WCS_STRING, _("Global Configuration"));
	do_template("beginbox");

	wprintf("<a href=\"display_siteconfig\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Edit site-wide configuration"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_inetconf\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Domain names and Internet mail configuration"));
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"display_netconf\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Configure replication with other Citadel servers"));
	wprintf("</span></a><br />\n");

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
	wprintf("</span></a><br />\n");

	wprintf("<a href=\"validate\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Validate new users"));
	wprintf("</span></a><br />\n");

	do_template("endbox");

	svprintf("BOXTITLE", WCS_STRING, _("Rooms and Floors"));
	do_template("beginbox");

	wprintf("<a href=\"display_floorconfig\">"
		"<span class=\"mainmenu\">");
	wprintf(_("Add, change, or delete floors"));
	wprintf("</span></a>\n");

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
		"<table class=\"mainmenu_banner\"><tr><td>"
		"<span class=\"titlebar\">");
	wprintf(_("Enter a server command"));
	wprintf("</span></td></tr></table>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"mainmenu_background\"><tr><td>\n");

	wprintf("<center>");
	wprintf(_("This screen allows you to enter Citadel server commands which are "
		"not supported by WebCit.  If you do not know what that means, "
		"then this screen will not be of much use to you."));
	wprintf("<br />\n");

	wprintf("<form method=\"post\" action=\"do_generic\">\n");

	wprintf(_("Enter command:"));
	wprintf("<br /><input type=\"text\" name=\"g_cmd\" size=80 maxlength=\"250\"><br />\n");

	wprintf(_("Command input (if requesting SEND_LISTING transfer mode):"));
	wprintf("<br /><textarea name=\"g_input\" rows=10 cols=80 width=80></textarea><br />\n");

	wprintf("<font size=-2>");
	wprintf(_("Detected host header is %s://%s"), (is_https ? "https" : "http"), WC->http_host);
	wprintf("</font>\n");
	wprintf("<input type=\"submit\" name=\"sc_button\" value=\"%s\">", _("Send command"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\"><br />\n", _("Cancel"));

	wprintf("</form></center>\n");
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

	wprintf("<table border=0><tr><td>Command:</td><td><tt>");
	escputs(bstr("g_cmd"));
	wprintf("</tt></td></tr><tr><td>Result:</td><td><tt>");
	escputs(buf);
	wprintf("</tt></td></tr></table><br />\n");

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
	wprintf("<a href=\"display_generic\">Enter another command</a><br />\n");
	wprintf("<a href=\"display_advanced\">Return to menu</a>\n");
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
		wprintf("<html>\n"
			"<head>\n"
			"<title>MenuBar</title>\n"
			"<style type=\"text/css\">\n"
			"body	{ text-decoration: none; }\n"
			"</style>\n"
			"</head>\n");
		do_template("background");
	}

	do_template("menubar");

	if (as_single_page) {
		wDumpContent(2);
	}


}


/*@}*/
