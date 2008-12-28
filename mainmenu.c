/*
 * $Id$
 */

#include "webcit.h"

/*
 * The Main Menu
 */
void display_main_menu(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("display_main_menu"), NULL, NULL, 0);
	end_burst();

/*
	char buf[SIZ];
	output_headers(1, 1, 1, 0, 0, 0);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table width=\"100%%\" cellspacing=\"10\" cellpadding=\"0\">"
		"<tr><td colspan=\"2\" class=\"advanced\">\n");

	svput("BOXTITLE", WCS_STRING, _("Basic commands"));
	do_template("beginbox", NULL);

	/ * start of first column * /
	wprintf("<ul class=\"adminitems col1\">");

	wprintf("<li><a href=\"knrooms\">");
	wprintf(_("List known rooms"));
	wprintf("</a><span>");
	wprintf(_("Where can I go from here?"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"gotonext\">");
	wprintf(_("Goto next room"));
	wprintf("</a><span>");
	wprintf(_("...with <em>unread</em> messages"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"skip\">");
	wprintf(_("Skip to next room"));
	wprintf("</a><span>");
	wprintf(_("(come back here later)"));
	wprintf("</span></li>\n");

	if ((!IsEmptyStr(WC->ugname)) && (strcasecmp(WC->ugname, WC->wc_roomname))) {
		wprintf("<li><a href=\"ungoto\">");
		wprintf(_("Ungoto"));
		wprintf("</a><span>");
		wprintf(_("(oops! Back to %s)"), WC->ugname);
		wprintf("</span></li>\n");
	}

	wprintf("</ul>\n");

	/ * start of second column * /

	wprintf("<ul class=\"adminitems col2\">");

	wprintf("<li><a href=\"readnew\">");
	wprintf(_("Read new messages"));
	wprintf("</a><span>");
	wprintf(_("...in this room"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"readfwd\">");
	wprintf(_("Read all messages"));
	wprintf("</a><span>");
	wprintf(_("...old <EM>and</EM> new"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"display_enter\">");
	wprintf(_("Enter a message"));
	wprintf("</a><span>");
	wprintf(_("(post in this room)"));
	wprintf("</span></li>\n");

	if (WC->room_flags & QR_VISDIR) {
		wprintf("<li><a href=\"display_room_directory\">");
		wprintf(_("File library"));
		wprintf("</a><span>");
		wprintf(_("(List files available for download)"));
		wprintf("</span></li>\n");
	}

	wprintf("</ul>\n");

	/ * start of third column * /

	wprintf("<ul class=\"adminitems lastcol\">");

	wprintf("<li><a href=\"summary\">");
	wprintf(_("Summary page"));
	wprintf("</a><span>");
	wprintf(_("Summary of my account"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"userlist\">\n");
	wprintf(_("User list"));
	wprintf("</a><span>");
	wprintf(_("(all registered users)"));
	wprintf("</span></li>\n");

	wprintf("<li><a href=\"termquit\" TARGET=\"_top\">");
	wprintf(_("Log off"));
	wprintf("</a><span>");
	wprintf(_("Bye!"));
	wprintf("</span></li>\n");

	wprintf("</ul>\n");

	wprintf("&nbsp;");

	do_template("endbox", NULL);

	wprintf("</td></tr>"
		"<tr valign=top><td width=50%%>");

	print_menu_box(_("Your info"), "adminitems", 8,
		       "display_preferences", _("Change your preferences and settings"),
		       "display_reg", _("Update your contact information"),
		       "display_changepw", _("Change your password"),
		       "display_editbio", _("Enter your 'bio'"),
		       "display_editpic", _("Edit your online photo"), 
		       "display_sieve", _("View/edit server-side mail filters"),
		       "display_pushemail", _("Edit your push email settings"),
		       "display_openids", _("Manage your OpenIDs")
	);

	wprintf("</td><td width=50%%>");

	snprintf(buf, SIZ, _("Zap (forget) this room (%s)"), WC->wc_roomname);

	if ( (WC->axlevel >= 6) || (WC->is_room_aide) || (WC->is_mailbox) )
		print_menu_box(_("Advanced room commands"),"adminitems", 5,
			       "display_editroom", _("Edit or delete this room"),
			       "display_private", _("Go to a 'hidden' room"),
			       "display_entroom", _("Create a new room"),
			       "display_zap",buf,
			       "zapped_list",_("List all forgotten rooms"));
	else
		print_menu_box(_("Advanced room commands"),"adminitems", 4,
			       "display_private", _("Go to a 'hidden' room"),
			       "display_entroom", _("Create a new room"),
			       "display_zap",buf,
			       "zapped_list",_("List all forgotten rooms"));

	wprintf("</td></tr></table></div>");
	wDumpContent(2);
*/
}


/*
 * System administration menu
 */
void display_aide_menu(void)
{
	begin_burst();
	output_headers(1, 0, 0, 0, 1, 0);
	DoTemplate(HKEY("display_aide_menu"), NULL, NULL, 0);
	end_burst();
/*
	output_headers(1, 1, 2, 0, 0, 0);

        wprintf("<div id=\"banner\">\n");
        wprintf("<h1>");
	wprintf(_("System Administration Menu"));
        wprintf("</h1>");
        wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table> "
		"<tr valign=top><td width=50%% valign=\"top\">");

	print_menu_box(_("Global Configuration"), "adminitems", 4,
		       "display_siteconfig", _("Edit site-wide configuration"), 
		       "display_inetconf",_("Domain names and Internet mail configuration"),
		       "display_netconf", _("Configure replication with other Citadel servers"), 
		       "display_smtpqueue", _("View the outbound SMTP queue"));
	
	wprintf("</td><td width=50%% valign=\"top\">");

	print_menu_box(_("User account management"), "adminitems", 2, 
		       "select_user_to_edit", _("Add, change, delete user accounts"),
		       "validate", _("Validate new users"));

	wprintf("</td></tr><tr><td width=50%% valign=\"top\">");


	print_menu_box(_("Shutdown Citadel"), "adminitems", 3, 
		       "server_shutdown?when=now", _("Restart Now"),
		       "server_shutdown?when=page", _("Restart after paging users"),
		       "server_shutdown?when=idle", _("Restart when all users are idle"));

	wprintf("</td><td width=50%% valign=\"top\">");

	print_menu_box(_("Rooms and Floors"), "adminitems", 1, 
		       "display_floorconfig", _("Add, change, or delete floors"));

	wprintf("</td></tr></table></div>");
	wDumpContent(2);
*/
}



/*
 * Display the screen to enter a generic server command
 */
void display_generic(void)
{
	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Enter a server command"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"mainmenu_background\"><tr><td>\n");

	wprintf("<center>");
	wprintf(_("This screen allows you to enter Citadel server commands which are "
		"not supported by WebCit.  If you do not know what that means, "
		"then this screen will not be of much use to you."));
	wprintf("<br />\n");

	wprintf("<form method=\"post\" action=\"do_generic\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

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

/*
 * Interactive window to perform generic Citadel server commands.
 */
void do_generic(void)
{
	char buf[SIZ];
	char gcontent[SIZ];
	char *junk;
	size_t len;

	if (!havebstr("sc_button")) {
		display_main_menu();
		return;
	}

	output_headers(1, 1, 0, 0, 0, 0);

	serv_printf("%s", bstr("g_cmd"));
	serv_getln(buf, sizeof buf);

	svput("BOXTITLE", WCS_STRING, _("Server command results"));
	do_template("beginboxx", NULL);

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
	do_template("endbox", NULL);
	wDumpContent(1);
}


/*
 * Display the menubar.  
 *
 * Set 'as_single_page' to display HTML headers and footers -- otherwise it's assumed
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
		do_template("background", NULL);
	}

	do_template("menubar", NULL);

	if (as_single_page) {
		wDumpContent(2);
	}


}


/*
 * Display the wait / input dialog while restarting the server.
 */
void display_shutdown(void)
{
	char buf[SIZ];
	char *when;
	
	when=bstr("when");
	if (strcmp(when, "now") == 0){
		serv_printf("DOWN 1");
		serv_getln(buf, sizeof buf);
		if (atol(buf) == 500)
		{ /* upsie. maybe the server is not running as daemon? */
			
			safestrncpy(WC->ImportantMessage,
				    &buf[4],
				    sizeof WC->ImportantMessage);
		}
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(HKEY("display_serverrestart"), NULL, NULL, 0);
		end_burst();
		lingering_close(WC->http_sock);
		sleeeeeeeeeep(10);
		serv_printf("NOOP");
		serv_printf("NOOP");
	}
	else if (strcmp(when, "page") == 0) {
		char *message;
	       
		message = bstr("message");
		if ((message == NULL) || (IsEmptyStr(message)))
		{
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("display_serverrestartpage"), NULL, NULL, 0);
			end_burst();
		}
		else
		{
			serv_printf("SEXP broadcast|%s", message);
			serv_getln(buf, sizeof buf); /* TODO: should we care? */
			begin_burst();
			output_headers(1, 0, 0, 0, 1, 0);
			DoTemplate(HKEY("display_serverrestartpagedo"), NULL, NULL, 0);
			end_burst();			
		}
	}
	else if (!strcmp(when, "idle")) {
		serv_printf("SCDN 3");
		serv_getln(buf, sizeof buf);

		if (atol(buf) == 500)
		{ /* upsie. maybe the server is not running as daemon? */
			safestrncpy(WC->ImportantMessage,
				    &buf[4],
				    sizeof WC->ImportantMessage);
		}
		begin_burst();
		output_headers(1, 0, 0, 0, 1, 0);
		DoTemplate(HKEY("display_aide_menu"), NULL, NULL, 0);
		end_burst();			
	}
}

void _display_menubar(void) { display_menubar(0); }

void 
InitModule_MAINMENU
(void)
{
	WebcitAddUrlHandler(HKEY("display_aide_menu"), display_aide_menu, 0);
	WebcitAddUrlHandler(HKEY("server_shutdown"), display_shutdown, 0);
	WebcitAddUrlHandler(HKEY("display_main_menu"), display_main_menu, 0);
	WebcitAddUrlHandler(HKEY("display_generic"), display_generic, 0);
	WebcitAddUrlHandler(HKEY("do_generic"), do_generic, 0);
	WebcitAddUrlHandler(HKEY("display_menubar"), _display_menubar, 0);
}
