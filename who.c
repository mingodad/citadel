/*
 * $Id$
 *
 * Display a list of all users currently logged on to the Citadel server.
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
 * who is on?
 */
void whobbs(void)
{
	char buf[SIZ], user[SIZ], room[SIZ], host[SIZ],
		realroom[SIZ], realhost[SIZ];
	int sess;
	time_t last_activity;
	time_t now;
	int bg = 0;

	output_headers(1, 1, 2, 0, 1, 0, 0);

	wprintf("<script type=\"text/javascript\">\n"
		"function ConfirmKill() { \n"
		"return confirm('Do you really want to kill this session?');\n"
		"}\n"
		"</script>\n"
	);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<IMG SRC=\"/static/usermanag_48x.gif\" ALT=\" \" ALIGN=MIDDLE>");
	wprintf("<SPAN CLASS=\"titlebar\">&nbsp;Users currently on ");
	escputs(serv_info.serv_humannode);
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 cellspacing=0 width=100%% bgcolor=\"#FFFFFF\">"
		"<tr>\n");
	wprintf("<TH COLSPAN=3>&nbsp;</TH>\n");
	wprintf("<TH>User Name</TH>\n");
	wprintf("<TH>Room</TH>");
	wprintf("<TH>From host</TH>\n</TR>\n");

	serv_puts("TIME");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		now = extract_long(&buf[4], 0);
	}
	else {
		now = time(NULL);
	}

	serv_puts("RWHO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			sess = extract_int(buf, 0);
			extract_token(user, buf, 1, '|', sizeof user);
			extract_token(room, buf, 2, '|', sizeof room);
			extract_token(host, buf, 3, '|', sizeof host);
			extract_token(realroom, buf, 9, '|', sizeof realroom);
			extract_token(realhost, buf, 10, '|', sizeof realhost);
			last_activity = extract_long(buf, 5);

			bg = 1 - bg;
			wprintf("<TR BGCOLOR=\"#%s\">",
				(bg ? "DDDDDD" : "FFFFFF")
			);


			wprintf("<td>");
			if ((WC->is_aide) &&
			    (sess != WC->ctdl_pid)) {
				wprintf(" <A HREF=\"/terminate_session&which_session=%d&session_owner=", sess);
				urlescputs(user);
				wprintf("\" onClick=\"return ConfirmKill();\" "
				">[kill]</A>");
			}
			if (sess == WC->ctdl_pid) {
				wprintf(" <A HREF=\"/edit_me\" "
					">[edit]</A>");
			}
			wprintf("</TD>");

			/* (link to page this user) */
			wprintf("<TD><A HREF=\"/display_page?recp=");
			urlescputs(user);
			wprintf("\">"
				"<IMG ALIGN=MIDDLE WIDTH=20 HEIGHT=15 "
				"SRC=\"/static/page.gif\" "
				"ALT=\"(p)\""
				" BORDER=0></A>&nbsp;");
			wprintf("</TD>");

			/* (idle flag) */
			wprintf("<TD>");
			if ((now - last_activity) > 900L) {
				wprintf("&nbsp;"
					"<IMG ALIGN=MIDDLE "
					"SRC=\"/static/inactiveuser_24x.gif\" "
					"ALT=\"[idle]\" BORDER=0>");
			}
			else {
				wprintf("&nbsp;"
					"<IMG ALIGN=MIDDLE "
					"SRC=\"/static/activeuser_24x.gif\" "
					"ALT=\"[active]\" BORDER=0>");
			}
			wprintf("</TD>\n\t<TD>");



			/* username (link to user bio/photo page) */
			wprintf("<A HREF=\"/showuser&who=");
			urlescputs(user);
			wprintf("\">");
			escputs(user);
			wprintf("</A>");

			/* room */
			wprintf("</TD>\n\t<TD>");
			escputs(room);
			if (strlen(realroom) > 0) {
				wprintf("<br /><I>");
				escputs(realroom);
				wprintf("</I>");
			}
			wprintf("</TD>\n\t<TD>");

			/* hostname */
			escputs(host);
			if (strlen(realhost) > 0) {
				wprintf("<br /><I>");
				escputs(realhost);
				wprintf("</I>");
			}
			wprintf("</TD>\n</TR>");
		}
	}
	wprintf("</TABLE></div>\n"
		"<div align=center>"
		"Click on a name to read user info.  Click on "
		"<IMG ALIGN=MIDDLE SRC=\"/static/page.gif\" ALT=\"(p)\" "
		"BORDER=0> to send an instant message to that user.</div>\n");
	wDumpContent(1);
}


void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
	whobbs();
}


/*
 * Change your session info (fake roomname and hostname)
 */
void edit_me(void)
{
	char buf[SIZ];

	if (!strcasecmp(bstr("sc"), "Change room name")) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_getln(buf, sizeof buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Change host name")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_getln(buf, sizeof buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Change user name")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_getln(buf, sizeof buf);
		http_redirect("/whobbs");
	} else if (!strcasecmp(bstr("sc"), "Cancel")) {
		http_redirect("/whobbs");
	} else {

		output_headers(1, 1, 0, 0, 0, 0, 0);

		wprintf("<div id=\"banner\">\n");
		wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
		wprintf("<SPAN CLASS=\"titlebar\">");
		wprintf("Edit your session display");
		wprintf("</SPAN></TD></TR></TABLE>\n");
		wprintf("</div>\n<div id=\"content\">\n");

		wprintf("This screen allows you to change the way your\n");
		wprintf("session appears in the 'Who is online' listing.\n");
		wprintf("To turn off any 'fake' name you've previously\n");
		wprintf("set, simply click the appropriate 'change' button\n");
		wprintf("without typing anything in the corresponding box.\n");
		wprintf("<br />\n");

		wprintf("<FORM METHOD=\"POST\" ACTION=\"/edit_me\">\n");

		wprintf("<TABLE border=0 width=100%%>\n");

		wprintf("<TR><TD><B>Room name:</B></TD>\n<TD>");
		wprintf("<INPUT TYPE=\"text\" NAME=\"fake_roomname\" MAXLENGTH=\"64\">\n");
		wprintf("</TD>\n<TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change room name\">");
		wprintf("</TD>\n</TR>\n");

		wprintf("<TR><TD><B>Host name:</B></TD><TD>");
		wprintf("<INPUT TYPE=\"text\" NAME=\"fake_hostname\" MAXLENGTH=\"64\">\n");
		wprintf("</TD>\n<TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change host name\">");
		wprintf("</TD>\n</TR>\n");

		if (WC->is_aide) {
			wprintf("<TR><TD><B>User name:</B></TD><TD>");
			wprintf("<INPUT TYPE=\"text\" NAME=\"fake_username\" MAXLENGTH=\"64\">\n");
			wprintf("</TD>\n<TD ALIGN=center>");
			wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Change user name\">");
			wprintf("</TD>\n</TR>\n");
		}
		wprintf("<TR><TD>&nbsp;</TD><TD>&nbsp;</TD><TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
		wprintf("</TD></TR></TABLE>\n");

		wprintf("</FORM></CENTER>\n");
		wDumpContent(1);
	}
}
