/*
 * $Id$
 */

#include "webcit.h"

typedef struct UserStateStruct {
	StrBuf *UserName;
	StrBuf *Room;
	StrBuf *Host;
	StrBuf *RealRoom;
	StrBuf *RealHost;
	long LastActive;
	int Session;
	int Idle;
	int SessionCount;
} UserStateStruct;

void DestroyUserStruct(void *vUser)
{
	UserStateStruct *User = (UserStateStruct*) vUser;
	FreeStrBuf(&User->UserName);
	FreeStrBuf(&User->Room);
	FreeStrBuf(&User->Host);
	FreeStrBuf(&User->RealRoom);
	FreeStrBuf(&User->RealHost);
	free(User);
}

int CompareUserStruct(const void *VUser1, const void *VUser2)
{
	const UserStateStruct *User1 = (UserStateStruct*) GetSearchPayload(VUser1);
	const UserStateStruct *User2 = (UserStateStruct*) GetSearchPayload(VUser2);
	
	if (User1->Idle != User2->Idle)
		return User1->Idle > User2->Idle;
	return strcasecmp(ChrPtr(User1->UserName), 
			  ChrPtr(User2->UserName));
}


int GetWholistSection(HashList *List, time_t now)
{
	StrBuf *Buf, *XBuf;
	struct wcsession *WCC = WC;	/* This is done to make it run faster; WC is a function */
	UserStateStruct *User, *OldUser;
	void *VOldUser;
	size_t BufLen;
	char buf[SIZ];

	serv_puts("RWHO");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		Buf = NewStrBuf();
		XBuf = NewStrBuf();
		while (BufLen = StrBuf_ServGetln(Buf), strcmp(ChrPtr(Buf), "000")) {
			if (BufLen <= 0)
			    continue;
			User = (UserStateStruct*) malloc(sizeof(UserStateStruct));
			User->Session = StrBufExtract_int(Buf, 0, '|');

			StrBufExtract_token(XBuf, Buf, 1, '|');
			User->UserName = NewStrBufDup(XBuf);

			StrBufExtract_token(XBuf, Buf, 2, '|');
			User->Room = NewStrBufDup(XBuf);

			StrBufExtract_token(XBuf, Buf, 3, '|');
			User->Host = NewStrBufDup(XBuf);

			StrBufExtract_token(XBuf, Buf, 9, '|');
			User->RealRoom = NewStrBufDup(XBuf);

			StrBufExtract_token(XBuf, Buf, 10, '|');
			User->RealHost = NewStrBufDup(XBuf);
			
			User->LastActive = StrBufExtract_long(Buf, 5, '|');
			User->Idle = (now - User->LastActive) > 900L;
			User->SessionCount = 1;

			if (GetHash(List, 
				    ChrPtr(User->UserName), 
				    StrLength(User->UserName), 
				    &VOldUser)) {
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
				Put(List, 
				    ChrPtr(User->UserName), 
				    StrLength(User->UserName), 
				    User, DestroyUserStruct);
		}
		SortByPayload(List, CompareUserStruct);

		FreeStrBuf(&XBuf);
		FreeStrBuf(&Buf);
		return 1;
	}
	else
		return 0;
}

/*
 * Display inner div of Wholist
 * /
void who_inner_div(void) {
	UserStateStruct *User;
	void *VUser;
	char buf[SIZ];
	struct wcsession *WCC = WC;
	HashList *List;
	HashPos  *it;
	const char *UserName;
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

			/ * (link to page this user) * /
			wprintf("<td width=\"5%%\"><a href=\"display_page?recp=");
			UrlescPutStrBuf(User->UserName);
			wprintf("\">"
				"<img align=\"middle\" "
				"src=\"static/citadelchat_24x.gif\" "
				"alt=\"(p)\""
				" border=\"0\" /></a> ");
			wprintf("</td>");

			/ * (idle flag) * /
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

			/ * username (link to user bio/photo page) * /
			wprintf("<a href=\"showuser?who=");
			UrlescPutStrBuf(User->UserName);
			wprintf("\">");
			StrEscPuts(User->UserName);
			if (User->SessionCount > 1)
				wprintf(" [%d] ", User->SessionCount);
			wprintf("</a>");

			/ * room * /
			wprintf("</td>\n\t<td>");
			StrEscPuts(User->Room);
			if (StrLength(User->RealRoom) > 0) {
				wprintf("<br /><i>");
				StrEscPuts(User->RealRoom);
				wprintf("</i>");
			}
			wprintf("</td>\n\t<td class=\"host_col\">");

			/ * hostname * /
			StrEscPuts(User->Host);
			if (StrLength(User->RealHost) > 0) {
				wprintf("<br /><i>");
				StrEscPuts(User->RealHost);
				wprintf("</i>");
			}
			wprintf("</td>\n</tr>");
		}
		DeleteHashPos(&it);
	}
	wprintf("</table>");
	DeleteHash(&List);
}
*/


/*
 * Display a list of users currently logged in to the system
 * /
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

	/ *
	 * JavaScript to make the ajax refresh happen:
	 * See http://www.sergiopereira.com/articles/prototype.js.html for info on Ajax.PeriodicalUpdater
	 * It wants: 1. The div being updated
	 *           2. The URL of the update source
	 *           3. Other flags (such as the HTTP method and the refresh frequency)
	 * /
	wprintf(
		"<script type=\"text/javascript\">					"
		" new Ajax.PeriodicalUpdater('who_inner', 'who_inner_html',	"
		"                            { method: 'get', frequency: 30 }  );	"
		"</script>							 	\n"
	);
	wDumpContent(1);
}
*/

/*
 * end session
 */
void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
	///who();
	url_do_template();
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
 * /
void wholist_section(void) {
	UserStateStruct *User;
	void *VUser;
	HashList *List;
	HashPos  *it;
	const char *UserName;
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
			if (strcmp(ChrPtr(User->UserName), NLI)) {
				wprintf("<li class=\"");
				if (User->Idle) {
					wprintf("inactiveuser");
				}
				else {
					wprintf("activeuser");
				}
				wprintf("\"><a href=\"showuser?who=");
				UrlescPutStrBuf(User->UserName);
				wprintf("\">");
				StrEscPuts(User->UserName);
				wprintf("</a></li>");
			}
		}
		DeleteHashPos(&it);
	}
	DeleteHash(&List);
}
*/

void _terminate_session(void) {
	slrp_highest();
	terminate_session();
}

HashList *GetWholistHash(WCTemplateToken *Token)
{
	HashList *List;
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
	GetWholistSection(List, now);
	return List;
}

void WholistSubst(StrBuf *TemplBuffer, void *vContext, WCTemplateToken *Token)
{
	UserStateStruct *User = (UserStateStruct*) vContext;

	SVPutBuf("WHO:NAME", User->UserName, 1);
	SVPutBuf("WHO:ROOM", User->Room, 1);
	SVPutBuf("WHO:HOST", User->Host, 1);
	SVPutBuf("WHO:REALROOM", User->RealRoom, 1);
	SVPutBuf("WHO:REALHOST", User->RealHost, 1);
	svputlong("WHO:LASTACTIVE", User->LastActive);
	///svputlong("WHO:IDLESINCE",(now - User->LastActive) / 60);//// todo
	svputlong("WHO:SESSION", User->Session);
	svputlong("WHO:IDLE", User->Idle);
	svputlong("WHO:NSESSIONS", User->SessionCount);
	svputlong("WHO:ISME", (User->Session == WC->ctdl_pid));
}

void DeleteWholistHash(HashList **KillMe)
{
	DeleteHash(KillMe);
}

void 
InitModule_WHO
(void)
{
///	WebcitAddUrlHandler(HKEY("who"), who, 0);
//	WebcitAddUrlHandler(HKEY("who_inner_html"), who_inner_div, AJAX);
//	WebcitAddUrlHandler(HKEY("wholist_section"), wholist_section, AJAX);
	WebcitAddUrlHandler(HKEY("terminate_session"), _terminate_session, 0);
	WebcitAddUrlHandler(HKEY("edit_me"), edit_me, 0);

	RegisterIterator("WHOLIST", 0, NULL, GetWholistHash, WholistSubst, DeleteWholistHash);
}
