/*
 * $Id$
 */
/**
 * \defgroup DislpayWho Display a list of all users currently logged on to the Citadel server.
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"



/**
 * \brief Display inner div of Wholist
 */
void who_inner_div(void) {
	char buf[SIZ], user[SIZ], room[SIZ], host[SIZ],
		realroom[SIZ], realhost[SIZ];
	int sess;
	time_t last_activity;
	time_t now;
	int bg = 0;

	wprintf("<table class=\"who_background\">"
		"<tr>\n");
	wprintf("<th colspan=\"3\"> </th>\n");
	wprintf("<th>%s</th>\n", _("User name"));
	wprintf("<th>%s</th>", _("Room"));
	wprintf("<th class=\"from_host\">%s</th>\n</tr>\n", _("From host"));

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
				wprintf(" <a href=\"terminate_session?which_session=%d", sess);
				wprintf("\" onClick=\"return ConfirmKill();\">%s</a>", _("(kill)"));
			}
			if (sess == WC->ctdl_pid) {
				wprintf(" <a href=\"edit_me\">%s</a>", _("(edit)"));
			}
			wprintf("</td>");

			/** (link to page this user) */
			wprintf("<td><a href=\"display_page?recp=");
			urlescputs(user);
			wprintf("\">"
				"<img align=\"middle\" "
				"src=\"static/citadelchat_24x.gif\" "
				"alt=\"(p)\""
				" border=\"0\" /></a> ");
			wprintf("</td>");

			/** (idle flag) */
			wprintf("<td>");
			if ((now - last_activity) > 900L) {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"static/inactiveuser_24x.gif\" "
					"alt=\"(idle)\" border=\"0\" />");
			}
			else {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"static/activeuser_24x.gif\" "
					"alt=\"(active)\" border=\"0\" />");
			}
			wprintf("</td>\n<td>");



			/** username (link to user bio/photo page) */
			wprintf("<a href=\"showuser?who=");
			urlescputs(user);
			wprintf("\">");
			escputs(user);
			wprintf("</a>");

			/** room */
			wprintf("</td>\n\t<td>");
			escputs(room);
			if (strlen(realroom) > 0) {
				wprintf("<br /><i>");
				escputs(realroom);
				wprintf("</i>");
			}
			wprintf("</td>\n\t<td class=\"from_host\">");

			/** hostname */
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


/**
 * \brief who is on?
 */
void who(void)
{
	char title[256];

	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<script type=\"text/javascript\">\n"
		"function ConfirmKill() { \n"
		"return confirm('%s');\n"
		"}\n"
		"</script>\n", _("Do you really want to kill this session?")
	);

	wprintf("<div id=\"banner\">\n");
	wprintf("<table class=\"who_banner\"><tr><td>");
	wprintf("<img src=\"static/usermanag_48x.gif\" alt=\" \" "
		"align=middle "
		">");
	wprintf("<span class=\"titlebar\"> ");

	snprintf(title, sizeof title, _("Users currently on %s"), serv_info.serv_humannode);
	escputs(title);

	wprintf("</span></td><td align=right>");
	offer_start_page();
	wprintf("</td></tr></table>\n");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"who_is_online\">\n");

	wprintf("<div style=\"display:inline\" id=\"fix_scrollbar_bug\">");
	who_inner_div();
	wprintf("</div>\n");

	wprintf("<div id=\"instructions\" align=center>");
	wprintf(_("Click on a name to read user info.  Click on %s "
		"to send an instant message to that user."),
		"<img align=\"middle\" src=\"static/citadelchat_16x.gif\" alt=\"(p)\" border=\"0\">"
	);
	wprintf("</div>\n");

	/**
	 * JavaScript to make the ajax refresh happen:
	 * See http://www.sergiopereira.com/articles/prototype.js.html for info on Ajax.PeriodicalUpdater
	 * It wants: 1. The div being updated
	 *           2. The URL of the update source
	 *           3. Other flags (such as the HTTP method and the refresh frequency)
	 */
	wprintf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('fix_scrollbar_bug', 'who_inner_html',	"
		"                            { method: 'get', frequency: 30 }  );	"
		"</script>							 	\n"
	);
	wDumpContent(1);
}

/**
 * \brief end session \todo what??? does this belong here? 
 */
void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
	who();
}


/**
 * \brief Change your session info (fake roomname and hostname)
 */
void edit_me(void)
{
	char buf[SIZ];

	if (strlen(bstr("change_room_name_button")) > 0) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (strlen(bstr("change_host_name_button")) > 0) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (strlen(bstr("change_user_name_button")) > 0) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (strlen(bstr("cancel_button")) > 0) {
		http_redirect("who");
	} else {
		output_headers(1, 1, 0, 0, 0, 0);

		wprintf("<div id=\"banner\">\n");
		wprintf("<table class=\"who_banner\"><tr><td>");
		wprintf("<span class=\"titlebar\">");
		wprintf(_("Edit your session display"));
		wprintf("</span></td></tr></table>\n");
		wprintf("</div>\n<div id=\"content\">\n");

		wprintf(_("This screen allows you to change the way your "
			"session appears in the 'Who is online' listing. "
			"To turn off any 'fake' name you've previously "
			"set, simply click the appropriate 'change' button "
			"without typing anything in the corresponding box. "));
		wprintf("<br />\n");

		wprintf("<form method=\"POST\" action=\"edit_me\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);

		wprintf("<table border=0 width=100%%>\n");

		wprintf("<tr><td><b>");
		wprintf(_("Room name:"));
		wprintf("</b></td>\n<td>");
		wprintf("<input type=\"text\" name=\"fake_roomname\" maxlength=\"64\">\n");
		wprintf("</td>\n<td align=center>");
		wprintf("<input type=\"submit\" name=\"change_room_name_button\" value=\"%s\">",
			_("Change room name"));
		wprintf("</td>\n</tr>\n");

		wprintf("<tr><td><b>");
		wprintf(_("Host name:"));
		wprintf("</b></td><td>");
		wprintf("<input type=\"text\" name=\"fake_hostname\" maxlength=\"64\">\n");
		wprintf("</td>\n<td align=center>");
		wprintf("<input type=\"submit\" name=\"change_host_name_button\" value=\"%s\">",
			_("Change host name"));
		wprintf("</td>\n</tr>\n");

		if (WC->is_aide) {
			wprintf("<tr><td><b>");
			wprintf(_("User name:"));
			wprintf("</b></td><td>");
			wprintf("<input type=\"text\" name=\"fake_username\" maxlength=\"64\">\n");
			wprintf("</td>\n<td align=center>");
			wprintf("<input type=\"submit\" name \"change_user_name_button\" value=\"%s\">",
				_("Change user name"));
			wprintf("</td>\n</tr>\n");
		}
		wprintf("<tr><td> </td><td> </td><td align=center>");
		wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">",
			_("Cancel"));
		wprintf("</td></tr></table>\n");
		wprintf("</form></center>\n");
		wDumpContent(1);
	}
}


/*@}*/
