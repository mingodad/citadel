/*
 * $Id$
 *
 * Displays the "advanced" (main) menu.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"



/*
 * The Main Menu
 */
void display_main_menu(void)
{
	output_headers(1, 1, 1, 0, 0, 0, 0);

	wprintf("<center><TABLE WIDTH=99%%>"
		"<TR><TD COLSPAN=2>\n");

	svprintf("BOXTITLE", WCS_STRING, "Basic commands");
	do_template("beginbox");

	wprintf("\n"
		"<TABLE border=0 cellspacing=1 cellpadding=1 width=100%%>"
		"<TR>"
		"<TD>");	/* start of first column */

	wprintf("<A HREF=\"/knrooms\">"
		"<span class=\"mainmenu\">"
		"List known rooms</span></A><br />"
		"<span class=\"menudesc\">"
		"Where can I go from here?</span><br />\n"
	);

	wprintf("<A HREF=\"/gotonext\">"
		"<span class=\"mainmenu\">"
		"Goto next room</span></A><br />"
		"<span class=\"menudesc\">"
		"...with <EM>unread</EM> messages"
		"</span><br />\n"
	);

	wprintf("<A HREF=\"/skip\">"
		"<span class=\"mainmenu\">"
		"Skip to next room</span></a><br />"
		"<span class=\"menudesc\">"
		"(come back here later)"
		"</span>\n"
	);

	if ((strlen(WC->ugname) > 0) && (strcasecmp(WC->ugname, WC->wc_roomname))) {
		wprintf("<br />"
			"<A HREF=\"/ungoto\">"
			"<span class=\"mainmenu\">"
			"Ungoto</span></A><br />"
			"<span class=\"menudesc\">"
			"(oops! Back to %s)"
			"</span>\n", WC->ugname
		);
	}

	wprintf("</TD><TD>\n");	/* start of second column */

	wprintf("<A HREF=\"/readnew\">"
		"<span class=\"mainmenu\">"
		"Read new messages</span></A><br />"
		"<span class=\"menudesc\">"
		"...in this room</span><br />\n"
	);

	wprintf("<A HREF=\"/readfwd\">"
		"<span class=\"mainmenu\">"
		"Read all messages</span></A><br />"
		"<span class=\"menudesc\">"
		"...old <EM>and</EM> new</span><br />\n"
	);

	wprintf("<A HREF=\"/display_enter\">"
		"<span class=\"mainmenu\">"
		"Enter a message</span></A><br />"
		"<span class=\"menudesc\">"
		"(post in this room)</span>\n"
	);

	wprintf("</TD><TD>");	/* start of third column */

	wprintf("<A HREF=\"/summary\">"
		"<span class=\"mainmenu\">"
		"Summary page</span></A><br />"
		"<span class=\"menudesc\">"
		"Summary of my account</span><br />\n"
	);

	wprintf("<A HREF=\"/userlist\">\n"
		"<span class=\"mainmenu\">"
		"User list</span></A><br />"
		"<span class=\"menudesc\">"
		"(all registered users)</span><br />\n"
	);

	wprintf("<A HREF=\"/termquit\" TARGET=\"_top\">"
		"<span class=\"mainmenu\">"
		"Log off</span></A><br />"
		"<span class=\"menudesc\">"
		"Bye!</span>\n"
	);

	wprintf("</TD></TR></TABLE>\n");
	do_template("endbox");

	wprintf("</TD></TR>"
		"<TR VALIGN=TOP><TD>");

	svprintf("BOXTITLE", WCS_STRING, "Your info");
	do_template("beginbox");

	wprintf("<A HREF=\"/display_reg\">"
		"<span class=\"mainmenu\">"
		"Update your contact information "
		"</span><br />\n");

	wprintf("<A HREF=\"/display_changepw\">"
		"<span class=\"mainmenu\">"
		"Change your password</span></A><br />\n");

	wprintf("<A HREF=\"/display_editbio\">"
		"<span class=\"mainmenu\">"
		"Enter your 'bio'</span></a><br />\n");

	wprintf("<A HREF=\"/display_editpic\">"
		"<span class=\"mainmenu\">"
		"Edit your online photo</span></a>\n");

	do_template("endbox");

	wprintf("</TD><TD>");

	svprintf("BOXTITLE", WCS_STRING, "Advanced room commands");
	do_template("beginbox");

	if ((WC->axlevel >= 6) || (WC->is_room_aide)) {
		wprintf("<A HREF=\"/display_editroom\">"
			"<span class=\"mainmenu\">"
			"Edit or delete this room</span></A><br />\n");
	}

	wprintf("<A HREF=\"/display_private\">"
		"<span class=\"mainmenu\">"
		"Go to a &quot;hidden&quot; room</span></A><br />\n");

	wprintf("<A HREF=\"/display_entroom\">"
		"<span class=\"mainmenu\">"
		"Create a new room</span></A><br />\n");

	wprintf("<A HREF=\"/display_zap\">"
		"<span class=\"mainmenu\">"
		"Zap (forget) this room (%s)</span></A><br />\n",
		WC->wc_roomname);

	wprintf("<A HREF=\"/zapped_list\">"
		"<span class=\"mainmenu\">"
		"List all forgotten rooms</span></A>\n");

	do_template("endbox");

	wprintf("</td></tr></table></center>");
	wDumpContent(2);
}


/*
 * System administration menu
 */
void display_aide_menu(void)
{
	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">System Administration Menu</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<center><table border=0 width=99%%><tr valign=top><td>");

	svprintf("BOXTITLE", WCS_STRING, "Global Configuration");
	do_template("beginbox");

	wprintf("<A HREF=\"/display_siteconfig\">"
		"<span class=\"mainmenu\">"
		"Edit site-wide configuration</span></A><br />\n");

	wprintf("<A HREF=\"/display_inetconf\">"
		"<span class=\"mainmenu\">"
		"Domain names and Internet mail configuration</span></a><br />\n");

	wprintf("<A HREF=\"/display_netconf\">"
		"<span class=\"mainmenu\">"
		"Configure replication with other Citadel servers"
		"</span></A>\n");

	do_template("endbox");

	wprintf("</td><td>");

	svprintf("BOXTITLE", WCS_STRING, "User account management");
	do_template("beginbox");

	wprintf("<A HREF=\"/select_user_to_edit\">"
		"<span class=\"mainmenu\">"
		"Add, change, delete user accounts"
		"</span></A><br />\n");

	wprintf("<A HREF=\"/validate\">"
		"<span class=\"mainmenu\">"
		"Validate new users</span></A><br />\n");

	do_template("endbox");

	svprintf("BOXTITLE", WCS_STRING, "Rooms and Floors");
	do_template("beginbox");

	wprintf("<A HREF=\"/display_floorconfig\">"
		"<span class=\"mainmenu\">"
		"Add, change, or delete floors"
		"</span></A>\n");

	do_template("endbox");

	wprintf("</td></tr></table></center>");
	wDumpContent(2);
}





/*
 * Display the screen to enter a generic server command
 */
void display_generic(void)
{
	output_headers(1, 1, 2, 0, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Enter a server command</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<center><table border=0 width=99%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf("<CENTER>");
	wprintf("This screen allows you to enter Citadel server commands which are\n");
	wprintf("not supported by WebCit.  If you do not know what that means,\n");
	wprintf("then this screen will not be of much use to you.<br />\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/do_generic\">\n");

	wprintf("Enter command:<br />\n");
	wprintf("<INPUT TYPE=\"text\" NAME=\"g_cmd\" SIZE=80 MAXLENGTH=\"250\"><br />\n");

	wprintf("Command input (if requesting SEND_LISTING transfer mode):<br />\n");
	wprintf("<TEXTAREA NAME=\"g_input\" ROWS=10 COLS=80 WIDTH=80></TEXTAREA><br />\n");

	wprintf("<FONT SIZE=-2>Detected host header is http://%s</FONT>\n",
		WC->http_host);
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Send command\">");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><br />\n");

	wprintf("</FORM></CENTER>\n");
	wprintf("</td></tr></table></center>\n");
	wDumpContent(1);
}

void do_generic(void)
{
	char buf[SIZ];
	char gcontent[SIZ];
	char *junk;
	size_t len;

	if (strcasecmp(bstr("sc"), "Send command")) {
		display_main_menu();
		return;
	}

	output_headers(1, 1, 0, 0, 0, 0, 0);

	serv_printf("%s", bstr("g_cmd"));
	serv_gets(buf);

	svprintf("BOXTITLE", WCS_STRING, "Server command results");
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
		while (serv_gets(gcontent), strcmp(gcontent, "000")) {
			escputs(gcontent);
			wprintf("<br />\n");
		}
		wprintf("000");
	}
	if (buf[0] == '4') {
		text_to_server(bstr("g_input"), 0);
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
	wprintf("<A HREF=\"/display_generic\">Enter another command</A><br />\n");
	wprintf("<A HREF=\"/display_advanced\">Return to menu</A>\n");
	do_template("endbox");
	wDumpContent(1);
}




/*
 * Display the menubar.  Set as_single_page to
 * display HTML headers and footers -- otherwise it's assumed
 * that the menubar is being embedded in another page.
 */
void display_menubar(int as_single_page) {

	if (as_single_page) {
		output_headers(0, 0, 0, 0, 0, 0, 0);
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


