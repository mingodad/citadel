/*
 * $Id$
 */

#include "webcit.h"

typedef struct UserStateStruct {
	char *UserName;
	long UserNameLen;
	char *Room;
	long RoomLen;
	char *Host;
	long HostLen;
	char *RealRoom;
	long RealRoomLen;
	char *RealHost;
	long RealHostLen;
	long LastActive;
	int Session;
	int Idle;
	int SessionCount;
} UserStateStruct;

void DestroyUserStruct(void *vUser)
{
	UserStateStruct *User = (UserStateStruct*) vUser;
	free(User->UserName);
	free(User->Room);
	free(User->Host);
	free(User->RealRoom);
	free(User->RealHost);
	free(User);
}

int CompareUserStruct(const void *VUser1, const void *VUser2)
{
	const UserStateStruct *User1 = (UserStateStruct*) GetSearchPayload(VUser1);
	const UserStateStruct *User2 = (UserStateStruct*) GetSearchPayload(VUser2);
	
	if (User1->Idle != User2->Idle)
		return User1->Idle > User2->Idle;
	return strcasecmp(User1->UserName, User2->UserName);
}


int GetWholistSection(HashList *List, time_t now)
{
	struct wcsession *WCC = WC;	/* This is done to make it run faster; WC is a function */
	UserStateStruct *User, *OldUser;
	void *VOldUser;
	char buf[SIZ], user[SIZ], room[SIZ], host[SIZ],
		realroom[SIZ], realhost[SIZ];
	size_t BufLen;

	serv_puts("RWHO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (BufLen = serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (BufLen <= 0)
			    continue;
			User = (UserStateStruct*) malloc(sizeof(UserStateStruct));
			User->Session = extract_int(buf, 0);

			User->UserNameLen = extract_token(user, buf, 1, '|', sizeof user);
			User->UserName = malloc(User->UserNameLen + 1);
			memcpy(User->UserName, user, User->UserNameLen + 1);

			User->RoomLen = extract_token(room, buf, 2, '|', sizeof room);
			User->Room = malloc(User->RoomLen + 1);
			memcpy(User->Room, room, User->RoomLen + 1);

			User->HostLen = extract_token(host, buf, 3, '|', sizeof host);
			User->Host = malloc(User->HostLen + 1);
			memcpy(User->Host, host, User->HostLen + 1);

			User->RealRoomLen = extract_token(realroom, buf, 9, '|', sizeof realroom);
			User->RealRoom = malloc(User->RealRoomLen + 1);
			memcpy(User->RealRoom, realroom, User->RealRoomLen + 1);

			User->RealHostLen = extract_token(realhost, buf, 10, '|', sizeof realhost);
			User->RealHost = malloc(User->RealHostLen + 1);
			memcpy(User->RealHost, realhost, User->RealHostLen + 1);
			
			User->LastActive = extract_long(buf, 5);
			User->Idle = (now - User->LastActive) > 900L;
			User->SessionCount = 1;

			if (GetHash(List, User->UserName, User->UserNameLen, &VOldUser)) {
				OldUser = VOldUser;
				OldUser->SessionCount++;
				if (!User->Idle) {
					if (User->Session == WCC->ctdl_pid) 
						OldUser->Session = User->Session;

					OldUser->Idle = User->Idle;
					OldUser->LastActive = User->LastActive;
				}
				DestroyUserStruct(User);
			}
			else
				Put(List, User->UserName, User->UserNameLen, User, DestroyUserStruct);
		}
		SortByPayload(List, CompareUserStruct);
		return 1;
	}
	else
		return 0;
}

/*
 * Display inner div of Wholist
 */
void who_inner_div(void) {
	UserStateStruct *User;
	void *VUser;
	char buf[SIZ];
	struct wcsession *WCC = WC;	/* This is done to make it run faster; WC is a function */
	HashList *List;
	HashPos  *it;
	char *UserName;
	long len;
	time_t now;
	int bg = 0;

	wprintf("<table class=\"altern\">"
		"<tr>\n");
	wprintf("<th class=\"edit_col\"> </th>\n");
	wprintf("<th colspan=\"2\"> </th>\n");
	wprintf("<th>%s</th>\n", _("User name"));
	wprintf("<th>%s</th>", _("Room"));
	wprintf("<th class=\"host_col\">%s</th>\n</tr>\n", _("From host"));

	serv_puts("TIME");
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		now = extract_long(&buf[4], 0);
	}
	else {
		now = time(NULL);
	}

	List = NewHash(1, NULL);

	if (GetWholistSection(List, now)) {
		it = GetNewHashPos();
		while (GetNextHashPos(List, it, &len, &UserName, &VUser)) {
			User = VUser;
			bg = 1 - bg;
			wprintf("<tr class=\"%s\">",
				(bg ? "even" : "odd")
			);


			wprintf("<td class=\"edit_col\">");
			if ((WCC->is_aide) &&
			    (User->Session != WCC->ctdl_pid)) {
				wprintf(" <a href=\"terminate_session?which_session=%d", User->Session);
				wprintf("\" onClick=\"return ConfirmKill();\">%s</a>", _("(kill)"));
			}
			if (User->Session == WCC->ctdl_pid) {
				wprintf(" <a href=\"edit_me\">%s</a>", _("(edit)"));
			}
			wprintf("</td>");

			/* (link to page this user) */
			wprintf("<td width=\"5%%\"><a href=\"display_page?recp=");
			urlescputs(User->UserName);
			wprintf("\">"
				"<img align=\"middle\" "
				"src=\"static/citadelchat_24x.gif\" "
				"alt=\"(p)\""
				" border=\"0\" /></a> ");
			wprintf("</td>");

			/* (idle flag) */
			wprintf("<td width=\"5%%\">");
			if (User->Idle) {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"static/inactiveuser_24x.gif\" "
					"alt=\"(%s %ld %s)\" border=\"0\" />",
					_("idle since"),
					(now - User->LastActive) / 60,
					_("Minutes")
					);
				
			}
			else {
				wprintf(" "
					"<img align=\"middle\" "
					"src=\"static/activeuser_24x.gif\" "
					"alt=\"(active)\" border=\"0\" />");
			}
			wprintf("</td>\n<td>");

			/* username (link to user bio/photo page) */
			wprintf("<a href=\"showuser?who=");
			urlescputs(User->UserName);
			wprintf("\">");
			escputs(User->UserName);
			if (User->SessionCount > 1)
				wprintf(" [%d] ", User->SessionCount);
			wprintf("</a>");

			/* room */
			wprintf("</td>\n\t<td>");
			escputs(User->Room);
			if (!IsEmptyStr(User->RealRoom) ) {
				wprintf("<br /><i>");
				escputs(User->RealRoom);
				wprintf("</i>");
			}
			wprintf("</td>\n\t<td class=\"host_col\">");

			/* hostname */
			escputs(User->Host);
			if (!IsEmptyStr(User->RealHost)) {
				wprintf("<br /><i>");
				escputs(User->RealHost);
				wprintf("</i>");
			}
			wprintf("</td>\n</tr>");
		}
		DeleteHashPos(&it);
	}
	wprintf("</table>");
	DeleteHash(&List);

}


/*
 * Display a list of users currently logged in to the system
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
	wprintf("<div class=\"room_banner\">");
	wprintf("<img src=\"static/usermanag_48x.gif\">");
	wprintf("<h1>");
	snprintf(title, sizeof title, _("Users currently on %s"), serv_info.serv_humannode);
	escputs(title);
	wprintf("</h1></div>");
	wprintf("<ul class=\"room_actions\">\n");
	wprintf("<li class=\"start_page\">");
	offer_start_page();
	wprintf("</li></ul>");
	wprintf("</div>");

	wprintf("<div id=\"content\" class=\"fix_scrollbar_bug who_is_online\">\n");
	wprintf("<div class=\"box\">");
	wprintf("<div class=\"boxlabel\">");	
	snprintf(title, sizeof title, _("Users currently on %s"), serv_info.serv_humannode);
	escputs(title);
	wprintf("</div>");	
	wprintf("<div class=\"boxcontent\">");
        wprintf("<div id=\"who_inner\" >");
	who_inner_div();
	wprintf("</div>");

	wprintf("<div class=\"instructions\">");
	wprintf(_("Click on a name to read user info.  Click on %s "
		"to send an instant message to that user."),
		"<img align=\"middle\" src=\"static/citadelchat_16x.gif\" alt=\"(p)\" border=\"0\">"
	);
	wprintf("</div></div>\n");

	/*
	 * JavaScript to make the ajax refresh happen:
	 * See http://www.sergiopereira.com/articles/prototype.js.html for info on Ajax.PeriodicalUpdater
	 * It wants: 1. The div being updated
	 *           2. The URL of the update source
	 *           3. Other flags (such as the HTTP method and the refresh frequency)
	 */
	wprintf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('who_inner', 'who_inner_html',	"
		"                            { method: 'get', frequency: 30 }  );	"
		"</script>							 	\n"
	);
	wDumpContent(1);
}

/*
 * end session
 */
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

	if (havebstr("change_room_name_button")) {
		serv_printf("RCHG %s", bstr("fake_roomname"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (havebstr("change_host_name_button")) {
		serv_printf("HCHG %s", bstr("fake_hostname"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (havebstr("change_user_name_button")) {
		serv_printf("UCHG %s", bstr("fake_username"));
		serv_getln(buf, sizeof buf);
		http_redirect("who");
	} else if (havebstr("cancel_button")) {
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
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);

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

/*
 * Wholist section
 */
void wholist_section(void) {
	UserStateStruct *User;
	void *VUser;
	HashList *List;
	HashPos  *it;
	char *UserName;
	long len;
	char buf[SIZ];
        time_t now;

	serv_puts("TIME");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '2') {
		now = extract_long(&buf[4], 0);
	}
	else {
		now = time(NULL);
	}

	List = NewHash(1, NULL);

	if (GetWholistSection(List, now)) {
		SortByPayload(List, CompareUserStruct);
		it = GetNewHashPos();
		while (GetNextHashPos(List, it, &len, &UserName, &VUser)) {
			User = VUser;
			if (strcmp(User->UserName, NLI)) {
				wprintf("<li class=\"");
				if (User->Idle) {
					wprintf("inactiveuser");
				}
				else {
					wprintf("activeuser");
				}
				wprintf("\"><a href=\"showuser?who=");
				urlescputs(User->UserName);
				wprintf("\">");
				escputs(User->UserName);
				wprintf("</a></li>");
			}
		}
		DeleteHashPos(&it);
	}
	DeleteHash(&List);
}

void _terminate_session(void) {
	slrp_highest();
	terminate_session();
}

void 
InitModule_WHO
(void)
{
	WebcitAddUrlHandler(HKEY("who"), who, 0);
	WebcitAddUrlHandler(HKEY("who_inner_html"), who_inner_div, AJAX);
	WebcitAddUrlHandler(HKEY("wholist_section"), wholist_section, AJAX);
	WebcitAddUrlHandler(HKEY("terminate_session"), _terminate_session, 0);
	WebcitAddUrlHandler(HKEY("edit_me"), edit_me, 0);
}
