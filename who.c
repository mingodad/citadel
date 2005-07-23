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
 * Display inner div of Wholist
 */
void who_inner_div(void) {
	char buf[SIZ], user[SIZ], room[SIZ], host[SIZ],
		realroom[SIZ], realhost[SIZ];
	int sess;
	time_t last_activity;
	time_t now;
	int bg = 0;

	wprintf("<table border=\"0\" cellspacing=\"0\" width=\"100%%\" bgcolor=\"#FFFFFF\">"
		"<tr>\n");
	wprintf("<th colspan=\"3\"> </th>\n");
	wprintf("<th>User Name</th>\n");
	wprintf("<th>Room</th>");
	wprintf("<th>From host</th>\n</tr>\n");

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
			wprintf("<tr bgcolor=\"#%s\">",
				(bg ? "DDDDDD" : "FFFFFF")
			);


			wprintf("<td>");
			if ((WC->is_aide) &&
			    (sess != WC->ctdl_pid)) {
				wprintf(" <a href=\"/terminate_session?which_session=%d", sess);
				wprintf("\" onClick=\"return ConfirmKill();\" "
				">[kill]</a>");
			}
			if (sess == WC->ctdl_pid) {
				wprintf(" <a href=\"/edit_me\" "
					">[edit]</a>");
			}
			wprintf("</td>");

			/* (link to page this user) */
			wprintf("<td><a href=\"/display_page?recp=");
			urlescputs(user);
			wprintf("\">"
				"<img align=\"middle\" "
				"src=\"/static/citadelchat_24x.gif\" "
				"alt=\"(p)\""
				" border=\"0\" /></a> ");
			wprintf("</td>");

			/* (idle flag) */
			wprintf("<td>");
			if ((now - last_activity) > 900L) {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"/static/inactiveuser_24x.gif\" "
					"alt=\"[idle]\" border=\"0\" />");
			}
			else {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"/static/activeuser_24x.gif\" "
					"alt=\"[active]\" border=\"0\" />");
			}
			wprintf("</td>\n<td>");



			/* username (link to user bio/photo page) */
			wprintf("<a href=\"/showuser?who=");
			urlescputs(user);
			wprintf("\">");
			escputs(user);
			wprintf("</a>");

			/* room */
			wprintf("</td>\n\t<td>");
			escputs(room);
			if (strlen(realroom) > 0) {
				wprintf("<br /><i>");
				escputs(realroom);
				wprintf("</i>");
			}
			wprintf("</td>\n\t<td>");

			/* hostname */
			escputs(host);
			if (strlen(realhost) > 0) {
				wprintf("<br /><i>");
				escputs(realhost);
				wprintf("</i>");
			}
			wprintf("</td>\n</tr>");
		}
	}
	wprintf("</table>");
}


/*
 * XML-encapsulated version of wholist inner html
 */
void who_inner_html(void) {
	output_headers(0, 0, 0, 0, 0, 0, 0);

	wprintf("Content-type: text/xml;charset=UTF-8\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-store\r\n",
		SERVER);
	begin_burst();

	wprintf("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
		"<ajax-response>\r\n"
		"<response type=\"element\" id=\"fix_scrollbar_bug\">\r\n"
	);

	who_inner_div();

	wprintf("</response>\r\n"
		"</ajax-response>\r\n"
		"\r\n"
	);

	wDumpContent(0);
}


/*
 * who is on?
 */
void who(void)
{
	/*
	output_headers(1, 1, 2, 0, 1, 0, 0); old refresh30 version
	*/
	output_headers(1, 1, 2, 0, 0, 0, 0);

	wprintf("<script type=\"text/javascript\">\n"
		"function ConfirmKill() { \n"
		"return confirm('Do you really want to kill this session?');\n"
		"}\n"
		"</script>\n"
	);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<IMG SRC=\"/static/usermanag_48x.gif\" ALT=\" \" "
		"ALIGN=MIDDLE "
		">");
		/* "onLoad=\"javascript:bodyOnLoad()\" " */
	wprintf("<SPAN CLASS=\"titlebar\"> Users currently on ");
	escputs(serv_info.serv_humannode);
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n");

	wprintf("<div id=\"content\">\n");

	wprintf("<div style=\"display:inline\" id=\"fix_scrollbar_bug\">");
	who_inner_div();	/* Actual data handled by another function */
	wprintf("</div>\n");

	wprintf("<div id=\"instructions\" align=center>"
		"Click on a name to read user info.  Click on "
		"<IMG ALIGN=MIDDLE SRC=\"/static/citadelchat_16x.gif\" "
		"ALT=\"(p)\" BORDER=0>"
		" to send an instant message to that user.</div>\n");

	/* JavaScript to make the ajax refresh happen:
	 * 1. Register the request 'getWholist' which calls the WebCit action 'who_inner_html'
	 * 2. Register the 'fix_scrollbar_bug' div as one we're interested in ajaxifying
	 * 3. setInterval to make the ajax refresh happen every 30 seconds.  The random number
	 *    in the request is there to prevent IE from caching the XML even though it's been
	 *    told not to.  Die, Microsoft, Die.
	 */
	wprintf(
"									\n"
" <script type=\"text/javascript\">					\n"
"	ajaxEngine.registerRequest('getWholist', 'who_inner_html');\n"
"	ajaxEngine.registerAjaxElement('fix_scrollbar_bug');	\n"
"	setInterval(\"ajaxEngine.sendRequest('getWholist', 'junk='+Math.random());\", 30000);	\n"
"</script>\n"
	);


	wDumpContent(1);
}


void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
	who();
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
		http_redirect("/who");
	} else if (!strcasecmp(bstr("sc"), "Change host name")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_getln(buf, sizeof buf);
		http_redirect("/who");
	} else if (!strcasecmp(bstr("sc"), "Change user name")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_getln(buf, sizeof buf);
		http_redirect("/who");
	} else if (!strcasecmp(bstr("sc"), "Cancel")) {
		http_redirect("/who");
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
		wprintf("<TR><TD> </TD><TD> </TD><TD ALIGN=center>");
		wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\">");
		wprintf("</TD></TR></TABLE>\n");
		wprintf("</FORM></CENTER>\n");
		wDumpContent(1);
	}
}
