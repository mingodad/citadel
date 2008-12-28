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
	int IdleSince;
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
	wcsession *WCC = WC;
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
			User->IdleSince = (now - User->LastActive) / 60;
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
 * end session
 */
void terminate_session(void)
{
	char buf[SIZ];

	serv_printf("TERM %s", bstr("which_session"));
	serv_getln(buf, sizeof buf);
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

void _terminate_session(void) {
	slrp_highest();
	terminate_session();
}

HashList *GetWholistHash(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType)
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


void DeleteWholistHash(HashList **KillMe)
{
	DeleteHash(KillMe);
}

void tmplput_who_username(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendTemplate(Target, nArgs, Tokens, vContext, ContextType, User->UserName, 0);
}

void tmplput_who_room(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendTemplate(Target, nArgs, Tokens, vContext, ContextType, User->Room, 0);
}

void tmplput_who_host(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendTemplate(Target, nArgs, Tokens, vContext, ContextType, User->Host, 0);
}

void tmplput_who_realroom(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendTemplate(Target, nArgs, Tokens, vContext, ContextType, User->RealRoom, 0);
}
int conditional_who_realroom(WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	return StrLength(User->RealRoom) > 0;
}

void tmplput_who_realhost(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendTemplate(Target, nArgs, Tokens, vContext, ContextType, User->RealHost, 0);
}

void tmplput_who_lastactive(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendPrintf(Target, "%d", User->LastActive);
}

void tmplput_who_idlesince(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendPrintf(Target, "%d", User->IdleSince);
}

void tmplput_who_session(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendPrintf(Target, "%d", User->Session);
}

int conditional_who_idle(WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	return User->Idle;
}

int conditional_who_nsessions(WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	return User->SessionCount;
}

void tmplput_who_nsessions(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	StrBufAppendPrintf(Target, "%d", User->SessionCount);
}

int conditional_who_isme(WCTemplateToken *Tokens, void *vContext, int ContextType)
{
	UserStateStruct *User = (UserStateStruct*) vContext;
	return (User->Session == WC->ctdl_pid);
}

void 
InitModule_WHO
(void)
{
	WebcitAddUrlHandler(HKEY("terminate_session"), _terminate_session, 0);
	WebcitAddUrlHandler(HKEY("edit_me"), edit_me, 0);

	RegisterIterator("WHOLIST", 0, NULL, GetWholistHash, NULL, DeleteWholistHash, CTX_WHO, CTX_NONE);

	RegisterNamespace("WHO:NAME",        0, 1, tmplput_who_username, CTX_WHO);
	RegisterNamespace("WHO:ROOM",        0, 1, tmplput_who_room, CTX_WHO);
	RegisterNamespace("WHO:HOST",        0, 1, tmplput_who_host, CTX_WHO);
	RegisterNamespace("WHO:REALROOM",    0, 1, tmplput_who_realroom, CTX_WHO);
	RegisterNamespace("WHO:REALHOST",    0, 1, tmplput_who_realhost, CTX_WHO);
	RegisterNamespace("WHO:LASTACTIVE",  0, 1, tmplput_who_lastactive, CTX_WHO);
	RegisterNamespace("WHO:IDLESINCE",   0, 1, tmplput_who_idlesince, CTX_WHO);
	RegisterNamespace("WHO:SESSION",     0, 1, tmplput_who_session, CTX_WHO);
	RegisterNamespace("WHO:NSESSIONS",   0, 1, tmplput_who_nsessions, CTX_WHO);
	RegisterNamespace("WHO:NSESSIONS",   0, 1, tmplput_who_nsessions, CTX_WHO);

	RegisterConditional(HKEY("WHO:IDLE"),      1, conditional_who_idle, CTX_WHO);
	RegisterConditional(HKEY("WHO:NSESSIONS"), 1, conditional_who_nsessions, CTX_WHO);
	RegisterConditional(HKEY("WHO:ISME"),      1, conditional_who_isme, CTX_WHO);
	RegisterConditional(HKEY("WHO:REALROOM"),  1, conditional_who_realroom, CTX_WHO);
}
