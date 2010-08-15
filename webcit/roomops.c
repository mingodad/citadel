/*
 * $Id$
 * Lots of different room-related operations.
 */

#include "webcit.h"
#include "webserver.h"
#define MAX_FLOORS 128

char floorlist[MAX_FLOORS][SIZ];	/* list of our floor names */

/* See GetFloorListHash and GetRoomListHash for info on these.
 * Basically we pull LFLR/LKRA etc. and set up a room HashList with these keys.
 */

void display_whok(void);
int ConditionalHaveRoomeditRights(StrBuf *Target, WCTemplputParams *TP);


char *viewdefs[VIEW_MAX];			/* the different kinds of available views */

ROOM_VIEWS exchangeable_views[VIEW_MAX][VIEW_MAX] = {	/* the different kinds of available views for a view */
{VIEW_BBS, VIEW_MAILBOX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX }, 
{VIEW_BBS, VIEW_MAILBOX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX }, 
{VIEW_MAX, VIEW_MAX, VIEW_ADDRESSBOOK, VIEW_CALENDAR, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX }, 
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_CALENDAR, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX /*VIEW_CALBRIEF*/, VIEW_MAX, VIEW_MAX }, 
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_TASKS, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, },
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_NOTES, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, },
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_WIKI, VIEW_MAX, VIEW_MAX, VIEW_MAX}, 
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_CALENDAR, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX/*VIEW_CALBRIEF*/, VIEW_MAX, VIEW_MAX},
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_JOURNAL, VIEW_MAX }, 
{VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_MAX, VIEW_BLOG }, 
	};
/* the brief calendar view is disabled: VIEW_CALBRIEF */

ROOM_VIEWS allowed_default_views[VIEW_MAX] = {
	1, /* VIEW_BBS		Bulletin board view */
	1, /* VIEW_MAILBOX		Mailbox summary */
	1, /* VIEW_ADDRESSBOOK	Address book view */
	1, /* VIEW_CALENDAR		Calendar view */
	1, /* VIEW_TASKS		Tasks view */
	1, /* VIEW_NOTES		Notes view */
	1, /* VIEW_WIKI		Wiki view */
	0, /* VIEW_CALBRIEF		Brief Calendar view */
	0, /* VIEW_JOURNAL		Journal view */
	0  /* VIEW_BLOG		Blog view (not yet implemented) */
};


/*
 * Initialize the viewdefs with localized strings
 */
void initialize_viewdefs(void) {
	viewdefs[VIEW_BBS] = _("Bulletin Board");
	viewdefs[VIEW_MAILBOX] = _("Mail Folder");
	viewdefs[VIEW_ADDRESSBOOK] = _("Address Book");
	viewdefs[VIEW_CALENDAR] = _("Calendar");
	viewdefs[VIEW_TASKS] = _("Task List");
	viewdefs[VIEW_NOTES] = _("Notes List");
	viewdefs[VIEW_WIKI] = _("Wiki");
	viewdefs[VIEW_CALBRIEF] = _("Calendar List");
	viewdefs[VIEW_JOURNAL] = _("Journal");
	viewdefs[VIEW_BLOG] = _("Blog");
}



/*
 * load the list of floors
 * /
void load_floorlist(StrBuf *Buf)
{
	int a;
	int Done = 0;

	for (a = 0; a < MAX_FLOORS; ++a)
		floorlist[a][0] = 0;

	serv_puts("LFLR");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 1) {
		strcpy(floorlist[0], "Main Floor");
		return;
	}
	while (!Done && (StrBuf_ServGetln(Buf)>=0)) {
		if ( (StrLength(Buf)==3) && 
		     !strcmp(ChrPtr(Buf), "000")) {
			Done = 1;
			break;
		}
		extract_token(floorlist[StrBufExtract_int(Buf, 0, '|')], ChrPtr(Buf), 1, '|', sizeof floorlist[0]);
	}
}
*/


/*
 * Embed the room banner
 *
 * got			The information returned from a GOTO server command
 * navbar_style 	Determines which navigation buttons to display
 *
 */

void embed_room_banner(void) 
{
	wcsession *WCC = WC;
	char buf[256];

	/* refresh current room states... */
	/* dosen't work??? gotoroom(NULL); */

	/* The browser needs some information for its own use */
	wc_printf("<script type=\"text/javascript\">	\n"
		  "	room_is_trash = %d;		\n"
		  "</script>\n",
		  ((WC->CurRoom.RAFlags & UA_ISTRASH) != 0)
		);

	/*
	 * If the user happens to select the "make this my start page" link,
	 * we want it to remember the URL as a "/dotskip" one instead of
	 * a "skip" or "gotonext" or something like that.
	 */
	if (WCC->Hdr->this_page == NULL) {
		WCC->Hdr->this_page = NewStrBuf();
	}
	StrBufPrintf(WCC->Hdr->this_page, 
		     "dotskip?room=%s",
		     ChrPtr(WC->CurRoom.name)
		);

	do_template("roombanner", NULL);
	/* roombanner contains this for mobile */
	if (WC->is_mobile)
		return;


	wc_printf("<div id=\"navbar\"><ul>");

	wc_printf(
		"<li class=\"ungoto\">"
		"<a href=\"ungoto\">"
		"<img src=\"static/ungoto2_24x.gif\" alt=\"\" width=\"24\" height=\"24\">"
		"<span class=\"navbar_link\">%s</span></A>"
		"</li>\n", _("Ungoto")
		);
	
	if (WC->CurRoom.view == VIEW_BBS) {
		wc_printf(
			"<li class=\"newmess\">"
			"<a href=\"readnew\">"
			"<img src=\"static/newmess2_24x.gif\" alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">%s</span></A>"
			"</li>\n", _("Read new messages")
			);
	}

	switch(WC->CurRoom.view) {
	case VIEW_ADDRESSBOOK:
		wc_printf(
			"<li class=\"viewcontacts\">"
			"<a href=\"readfwd\">"
			"<img src=\"static/viewcontacts_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("View contacts")
			);
		wc_printf(
			"<li class=\"addnewcontact\">"
			"<a href=\"display_enter\">"
			"<img src=\"static/addnewcontact_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Add new contact")
			);
		break;
	case VIEW_CALENDAR:
		wc_printf(
			"<li class=\"staskday\">"
			"<a href=\"readfwd?calview=day\">"
			"<img src=\"static/taskday2_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Day view")
			);
		wc_printf(
			"<li class=\"monthview\">"
			"<a href=\"readfwd?calview=month\">"
			"<img src=\"static/monthview2_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Month view")
			);
		wc_printf("<li class=\"addevent\"><a href=\"display_enter");
		if (havebstr("year" )) wc_printf("?year=%s", bstr("year"));
		if (havebstr("month")) wc_printf("?month=%s", bstr("month"));
		if (havebstr("day"  )) wc_printf("?day=%s", bstr("day"));
		wc_printf("\">"
			  "<img  src=\"static/addevent_24x.gif\" "
			  "alt=\"\" width=\"24\" height=\"24\">"
			  "<span class=\"navbar_link\">"
			  "%s"
			  "</span></a></li>\n", _("Add new event")
			);
		break;
	case VIEW_CALBRIEF:
		wc_printf(
			"<li class=\"monthview\">"
			"<a href=\"readfwd?calview=month\">"
			"<img src=\"static/monthview2_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Calendar list")
			);
		break;
	case VIEW_TASKS:
		wc_printf(
			"<li class=\"taskmanag\">"
			"<a href=\"readfwd\">"
			"<img src=\"static/taskmanag_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("View tasks")
			);
		wc_printf(
			"<li class=\"newmess\">"
			"<a href=\"display_enter\">"
			"<img  src=\"static/newmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Add new task")
			);
		break;
	case VIEW_NOTES:
		wc_printf(
			"<li class=\"viewnotes\">"
			"<a href=\"readfwd\">"
			"<img src=\"static/viewnotes_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("View notes")
			);
		wc_printf(
			"<li class=\"enternewnote\">"
			"<a href=\"add_new_note\">"
			"<img  src=\"static/enternewnote_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Add new note")
			);
		break;
	case VIEW_MAILBOX:
		wc_printf(
			"<li class=\"readallmess\">"
			"<a id=\"m_refresh\" href=\"readfwd\">"
			"<img src=\"static/readallmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Refresh message list")
			);
		wc_printf(
			"<li class=\"readallmess\">"
			"<a href=\"readfwd\">"
			"<img src=\"static/readallmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Read all messages")
			);
		wc_printf(
			"<li class=\"newmess\">"
			"<a href=\"display_enter\">"
			"<img  src=\"static/newmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Write mail")
			);
		break;
	case VIEW_WIKI:
		wc_printf(
			"<li class=\"readallmess\">"
			"<a href=\"wiki?page=home\">"
			"<img src=\"static/readallmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Wiki home")
			);
		safestrncpy(buf, bstr("page"), sizeof buf);
		if (IsEmptyStr(buf)) {
			safestrncpy(buf, "home", sizeof buf);
		}
		str_wiki_index(buf);
		wc_printf(
			"<li class=\"newmess\">"
			"<a href=\"display_enter?page=%s\">"
			"<img  src=\"static/newmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", buf, _("Edit this page")
			);
		
		if (bmstrcasestr((char *)ChrPtr(WCC->Hdr->HR.ReqLine), "wiki_history")) {
			/* already viewing history; display a link to the current page */
			wc_printf(
				"<li class=\"newmess\">"
				"<a href=\"wiki?page=%s\">"
				"<img  src=\"static/newmess3_24x.gif\" "
				"alt=\"\" width=\"24\" height=\"24\">"
				"<span class=\"navbar_link\">"
				"%s"
				"</span></a></li>\n", buf, _("Current version")
				);
		}
		else {
			/* display a link to the history */
			wc_printf(
				"<li class=\"newmess\">"
				"<a href=\"wiki_history?page=%s\">"
				"<img  src=\"static/newmess3_24x.gif\" "
				"alt=\"\" width=\"24\" height=\"24\">"
				"<span class=\"navbar_link\">"
				"%s"
				"</span></a></li>\n", buf, _("History")
				);
		}
		break;
		break;
	default:
		wc_printf(
			"<li class=\"readallmess\">"
			"<a href=\"readfwd\">"
			"<img src=\"static/readallmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Read all messages")
			);
		wc_printf(
			"<li class=\"newmess\">"
			"<a href=\"display_enter\">"
			"<img  src=\"static/newmess3_24x.gif\" "
			"alt=\"\" width=\"24\" height=\"24\">"
			"<span class=\"navbar_link\">"
			"%s"
			"</span></a></li>\n", _("Enter a message")
			);
		break;
	}
	
	wc_printf(
		"<li class=\"skipthisroom\">"
		"<a href=\"skip\" "
		"title=\"%s\">"
		"<img  src=\"static/skipthisroom_24x.gif\" alt=\"\" "
		"width=\"24\" height=\"24\">"
		"<span class=\"navbar_link\">%s</span></a>"
		"</li>\n",
		_("Leave all messages marked as unread, go to next room with unread messages"),
		_("Skip this room")
		);
	
	wc_printf(
		"<li class=\"markngo\">"
		"<a href=\"gotonext\" "
		"title=\"%s\">"
		"<img  src=\"static/markngo_24x.gif\" alt=\"\" "
		"width=\"24\" height=\"24\">"
		"<span class=\"navbar_link\">%s</span></a>"
		"</li>\n",
		_("Mark all messages as read, go to next room with unread messages"),
		_("Goto next room")
		);
	
	wc_printf("</ul></div>\n");
}


/*
 * back end routine to take the session to a new room
 */
long gotoroom(const StrBuf *gname)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	static long ls = (-1L);
	long err = 0;

	/* store ungoto information */
	if (StrLength(gname) > 0)
		strcpy(WCC->ugname, ChrPtr(WCC->CurRoom.name));
	WCC->uglsn = ls;
	Buf = NewStrBuf();

	/* move to the new room */
	if (StrLength(gname) > 0)
		serv_printf("GOTO %s", ChrPtr(gname));
	else /* or just refresh the current state... */
		serv_printf("GOTO 00000000000000000000");
	StrBuf_ServGetln(Buf);
	if  (GetServerStatus(Buf, &err) != 2) {
		serv_puts("GOTO _BASEROOM_");
		StrBuf_ServGetln(Buf);
		/* 
		 * well, we know that this is the fallback case, 
		 * but we're interested that the first command 
		 * didn't work out in first place.
		 */
		if (GetServerStatus(Buf, NULL) != 2) {
			FreeStrBuf(&Buf);
			return err;
		}
	}
	FlushFolder(&WCC->CurRoom);
	ParseGoto(&WCC->CurRoom, Buf);

	if (StrLength(gname) > 0)
	{
		remove_march(WCC->CurRoom.name);
		if (!strcasecmp(ChrPtr(gname), "_BASEROOM_"))
			remove_march(gname);
	}
	FreeStrBuf(&Buf);

	return err;
}

void DBG_QR(long QR)
{
	const char *QRFlagList[15] = {
		strof(QR_PERMANENT),
		strof(QR_INUSE),
		strof(QR_PRIVATE),
		strof(QR_PASSWORDED),
		strof(QR_GUESSNAME),
		strof(QR_DIRECTORY),
		strof(QR_UPLOAD),
		strof(QR_DOWNLOAD),
		strof(QR_VISDIR),
		strof(QR_ANONONLY),
		strof(QR_ANONOPT),
		strof(QR_NETWORK),
		strof(QR_PREFONLY),
		strof(QR_READONLY),
		strof(QR_MAILBOX)
	};
	int i = 1;
	int j=0;
	StrBuf *QRVec;

	QRVec = NewStrBufPlain(NULL, 256);
	while (i != 0)
	{
		if ((QR & i) != 0) {
			if (StrLength(QRVec) > 0)
				StrBufAppendBufPlain(QRVec, HKEY(" | "), 0);
			StrBufAppendBufPlain(QRVec, QRFlagList[j], -1, 0);
		}
		i = i << 1;
		j++;
	}
	lprintf(9, "DBG: QR-Vec [%ld] [%s]\n", QR, ChrPtr(QRVec));
	FreeStrBuf(&QRVec);
}



void DBG_QR2(long QR2)
{
	const char *QR2FlagList[15] = {
		strof(QR2_SYSTEM),
		strof(QR2_SELFLIST),
		strof(QR2_COLLABDEL),
		strof(QR2_SUBJECTREQ),
		strof(QR2_SMTP_PUBLIC),
		strof(QR2_MODERATED),
		"", 
		"", 
		"", 
		"", 
		"", 
		"", 
		"", 
		"", 
		""
	};
	int i = 1;
	int j=0;
	StrBuf *QR2Vec;

	QR2Vec = NewStrBufPlain(NULL, 256);
	while (i != 0)
	{
		if ((QR2 & i) != 0) {
			if (StrLength(QR2Vec) > 0)
				StrBufAppendBufPlain(QR2Vec, HKEY(" | "), 0);
			StrBufAppendBufPlain(QR2Vec, QR2FlagList[j], -1, 0);
		}
		i = i << 1;
		j++;
	}
	lprintf(9, "DBG: QR2-Vec [%ld] [%s]\n", QR2, ChrPtr(QR2Vec));
	FreeStrBuf(&QR2Vec);
}



void ParseGoto(folder *room, StrBuf *Line)
{
	wcsession *WCC = WC;
	const char *Pos;
	int flag;
	void *vFloor = NULL;
	StrBuf *pBuf;

	if (StrLength(Line) < 4) {
		return;
	}
	
	/* ignore the commandstate... */
	Pos = ChrPtr(Line) + 4;

	if (room->RoomNameParts != NULL)
	{
		int i;
		for (i=0; i < room->nRoomNameParts; i++)
			FreeStrBuf(&room->RoomNameParts[i]);
		free(room->RoomNameParts);
		room->RoomNameParts = NULL;
	}

	pBuf = room->name;  
	if (pBuf == NULL)
		pBuf = NewStrBufPlain(NULL, StrLength(Line));
	else
		FlushStrBuf(pBuf);
	memset(room, 0, sizeof(folder));
	room->name = pBuf;

	StrBufExtract_NextToken(room->name, Line, &Pos, '|'); // WC->CurRoom->name

	room->nNewMessages = StrBufExtractNext_long(Line, &Pos, '|'); 
	if (room->nNewMessages > 0)
		room->RAFlags |= UA_HASNEWMSGS;

	room->nTotalMessages = StrBufExtractNext_long(Line, &Pos, '|');

	room->ShowInfo =  StrBufExtractNext_long(Line, &Pos, '|');
	
	room->QRFlags = StrBufExtractNext_long(Line, &Pos, '|'); //CurRoom->QRFlags

	DBG_QR(room->QRFlags);

	room->HighestRead = StrBufExtractNext_long(Line, &Pos, '|');
	room->LastMessageRead = StrBufExtractNext_long(Line, &Pos, '|');

	room->is_inbox = StrBufExtractNext_long(Line, &Pos, '|'); // is_mailbox

	flag = StrBufExtractNext_long(Line, &Pos, '|');
	if (WCC->is_aide || flag) {
		room->RAFlags |= UA_ADMINALLOWED;
	}

	room->UsersNewMAilboxMessages = StrBufExtractNext_long(Line, &Pos, '|');

	room->floorid = StrBufExtractNext_int(Line, &Pos, '|'); // wc_floor

	room->view = StrBufExtractNext_long(Line, &Pos, '|'); // CurRoom->view

	room->defview = StrBufExtractNext_long(Line, &Pos, '|'); // CurRoom->defview

	flag = StrBufExtractNext_long(Line, &Pos, '|');
	if (flag)
		room->RAFlags |= UA_ISTRASH; //	wc_is_trash

	room->QRFlags2 = StrBufExtractNext_long(Line, &Pos, '|'); // CurRoom->QRFlags2
	DBG_QR2(room->QRFlags2);

	/* find out, whether we are in a sub-room */
	room->nRoomNameParts = StrBufNum_tokens(room->name, '\\');
	if (room->nRoomNameParts > 1)
	{
		int i;
		
		Pos = NULL;
		room->RoomNameParts = malloc(sizeof(StrBuf*) * (room->nRoomNameParts + 1));
		memset(room->RoomNameParts, 0, sizeof(StrBuf*) * (room->nRoomNameParts + 1));
		for (i=0; i < room->nRoomNameParts; i++)
		{
			room->RoomNameParts[i] = NewStrBuf();
			StrBufExtract_NextToken(room->RoomNameParts[i],
						room->name, &Pos, '\\');
		}
	}

	/* Private mailboxes on the main floor get remapped to the personal folder */
	if ((room->QRFlags & QR_MAILBOX) && 
	    (room->floorid == 0))
	{
		room->floorid = VIRTUAL_MY_FLOOR;
		if ((room->nRoomNameParts == 1) && 
		    (StrLength(room->name) == 4) && 
		    (strcmp(ChrPtr(room->name), "Mail") == 0))
		{
			room->is_inbox = 1;
		}
		
	}
	/* get a pointer to the floor we're on: */
	if (WCC->Floors == NULL)
		GetFloorListHash(NULL, NULL);

	GetHash(WCC->Floors, IKEY(room->floorid), &vFloor);
	room->Floor = (const Floor*) vFloor;
}

void LoadRoomAide(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	
	if (WCC->CurRoom.RoomAideLoaded)
		return;

	WCC->CurRoom.RoomAideLoaded = 1;
	Buf = NewStrBuf();
	serv_puts("GETA");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		FlushStrBuf(WCC->CurRoom.RoomAide);
		AppendImportantMessage (ChrPtr(Buf) + 4, 
					StrLength(Buf) - 4);
	} else {
		const char *Pos;

		Pos = ChrPtr(Buf) + 4;

		FreeStrBuf(&WCC->CurRoom.RoomAide);
		WCC->CurRoom.RoomAide = NewStrBufPlain (NULL, StrLength (Buf));

		StrBufExtract_NextToken(WCC->CurRoom.RoomAide, Buf, &Pos, '|'); 
	}
	FreeStrBuf (&Buf);
}
void tmplput_CurrentRoomFloorName(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	folder *Folder = &WCC->CurRoom;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;

	StrBufAppendTemplate(Target, TP, pFloor->Name, 0);
}

void tmplput_CurrentRoomAide(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomAide();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.RoomAide, 0);
}


void LoadRoomXA (void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	
	if (WCC->CurRoom.XALoaded)
		return;

	WCC->CurRoom.XALoaded = 1;
	Buf = NewStrBuf();
	serv_puts("GETR");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		FlushStrBuf(WCC->CurRoom.XAPass);
		FlushStrBuf(WCC->CurRoom.Directory);

		AppendImportantMessage (ChrPtr(Buf) + 4, 
					StrLength(Buf) - 4);
	} else {
		const char *Pos;

		Pos = ChrPtr(Buf) + 4;

		FreeStrBuf(&WCC->CurRoom.XAPass);
		FreeStrBuf(&WCC->CurRoom.Directory);

		WCC->CurRoom.XAPass = NewStrBufPlain (NULL, StrLength (Buf));
		WCC->CurRoom.Directory = NewStrBufPlain (NULL, StrLength (Buf));

		StrBufSkip_NTokenS(Buf, &Pos, '|', 1); /* The Name, we already know... */
		StrBufExtract_NextToken(WCC->CurRoom.XAPass, Buf, &Pos, '|'); 
		StrBufExtract_NextToken(WCC->CurRoom.Directory, Buf, &Pos, '|'); 
		StrBufSkip_NTokenS(Buf, &Pos, '|', 2); /* QRFlags, FloorNum we already know... */
		WCC->CurRoom.Order = StrBufExtractNext_long(Buf, &Pos, '|');
		/* defview, we already know you. */
		/* QR2Flags, we already know them... */

	}
	FreeStrBuf (&Buf);
}


void LoadXRoomPic(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	
	if (WCC->CurRoom.XHaveRoomPicLoaded)
		return;

	WCC->CurRoom.XHaveRoomPicLoaded = 1;
	Buf = NewStrBuf();
	serv_puts("OIMG _roompic_");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		WCC->CurRoom.XHaveRoomPic = 0;
	} else {
		WCC->CurRoom.XHaveRoomPic = 1;
	}
	serv_puts("CLOS");
	StrBuf_ServGetln(Buf);
	GetServerStatus(Buf, NULL);
	FreeStrBuf (&Buf);
}

int ConditionalThisRoomXHavePic(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC == NULL)
		return 0;

	LoadXRoomPic();
	return WCC->CurRoom.XHaveRoomPic == 1;
}

void LoadXRoomInfoText(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	int Done = 0;
	
	if (WCC->CurRoom.XHaveInfoTextLoaded)
		return;

	WCC->CurRoom.XHaveInfoTextLoaded = 1;
	Buf = NewStrBuf();

	serv_puts("RINF");

	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		WCC->CurRoom.XInfoText = NewStrBuf ();
		
		while (!Done && StrBuf_ServGetln(Buf)>=0) {
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) 
				Done = 1;
			else 
				StrBufAppendBuf(WCC->CurRoom.XInfoText, Buf, 0);
		}
	}

	FreeStrBuf (&Buf);
}

int ConditionalThisRoomXHaveInfoText(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	if (WCC == NULL)
		return 0;

	LoadXRoomInfoText();
	return (StrLength(WCC->CurRoom.XInfoText)>0);
}

void tmplput_CurrentRoomInfoText(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadXRoomInfoText();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.XAPass, 1);
}

void LoadXRoomXCountFiles(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	int Done = 0;
	
	if (WCC->CurRoom.XHaveDownloadCount)
		return;

	WCC->CurRoom.XHaveDownloadCount = 1;

	Buf = NewStrBuf();
	serv_puts("RDIR");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		
		while (!Done && StrBuf_ServGetln(Buf)>=0) {
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) 
				Done = 1;
			else 
				WCC->CurRoom.XDownloadCount++;
		}
	}

	FreeStrBuf (&Buf);
}

void tmplput_CurrentRoomXNFiles(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadXRoomXCountFiles();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.XDownloadCount);
}

void tmplput_CurrentRoomX_FileString(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadXRoomXCountFiles();

	if (WCC->CurRoom.XDownloadCount == 1)
		StrBufAppendBufPlain(Target, _("file"), -1, 0);
	else
		StrBufAppendBufPlain(Target, _("files"), -1, 0);
}

void tmplput_CurrentRoomPass(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.XAPass, 0);
}
void tmplput_CurrentRoomDirectory(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendTemplate(Target, TP, WCC->CurRoom.Directory, 0);
}
void tmplput_CurrentRoomOrder(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.Order);
}
void tmplput_CurrentRoomDefView(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.defview);
}

void tmplput_CurrentRoom_nNewMessages(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.nNewMessages);
}

void tmplput_CurrentRoom_nTotalMessages(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	LoadRoomXA();

	StrBufAppendPrintf(Target, "%d", WCC->CurRoom.nTotalMessages);
}

int ConditionalThisRoomOrder(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;

	if (WCC == NULL)
		return 0;

	LoadRoomXA();

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	return CheckThis == WCC->CurRoom.Order;
}

int ConditionalThisRoomDefView(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;

	if (WCC == NULL)
		return 0;

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	return CheckThis == WCC->CurRoom.defview;
}

int ConditionalThisRoomCurrView(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;

	if (WCC == NULL)
		return 0;

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	return CheckThis == WCC->CurRoom.view;
}

int ConditionalThisRoomHaveView(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;
	
	if (WCC == NULL)
		return 0;

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	if ((CheckThis >= VIEW_MAX) || (CheckThis < VIEW_BBS))
	{
		LogTemplateError(Target, "Conditional", ERR_PARM2, TP,
				 "Roomview [%ld] not valid\n", 
				 CheckThis);
		return 0;
	}

	return exchangeable_views [WCC->CurRoom.defview][CheckThis] != VIEW_MAX;
}

void tmplput_CurrentRoomViewString(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	StrBuf *Buf;

	if ((WCC == NULL) ||
	    (WCC->CurRoom.defview >= VIEW_MAX) || 
	    (WCC->CurRoom.defview < VIEW_BBS))
	{
		LogTemplateError(Target, "Token", ERR_PARM2, TP,
				 "Roomview [%ld] not valid\n", 
				 (WCC != NULL)? 
				 WCC->CurRoom.defview : -1);
		return;
	}

	Buf = NewStrBufPlain(_(viewdefs[WCC->CurRoom.defview]), -1);
	StrBufAppendTemplate(Target, TP, Buf, 0);
	FreeStrBuf(&Buf);
}

void tmplput_RoomViewString(StrBuf *Target, WCTemplputParams *TP) 
{
	long CheckThis;
	StrBuf *Buf;

	CheckThis = GetTemplateTokenNumber(Target, TP, 0, 0);
	if ((CheckThis >= VIEW_MAX) || (CheckThis < VIEW_BBS))
	{
		LogTemplateError(Target, "Token", ERR_PARM2, TP,
				 "Roomview [%ld] not valid\n", 
				 CheckThis);
		return;
	}

	Buf = NewStrBufPlain(_(viewdefs[CheckThis]), -1);
	StrBufAppendTemplate(Target, TP, Buf, 0);
	FreeStrBuf(&Buf);
}


int ConditionalIsAllowedDefaultView(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	long CheckThis;
	
	if (WCC == NULL)
		return 0;

	CheckThis = GetTemplateTokenNumber(Target, TP, 2, 0);
	if ((CheckThis >= VIEW_MAX) || (CheckThis < VIEW_BBS))
	{
		LogTemplateError(Target, "Conditional", ERR_PARM2, TP,
				 "Roomview [%ld] not valid\n", 
				 CheckThis);
		return 0;
	}

	return allowed_default_views[CheckThis] != 0;
}

/*
 * goto next room
 */
void smart_goto(const StrBuf *next_room) {
	gotoroom(next_room);
	readloop(readnew, eUseDefault);
}



/*
 * mark all messages in current room as having been read
 */
void slrp_highest(void)
{
	char buf[256];

	serv_puts("SLRP HIGHEST");
	serv_getln(buf, sizeof buf);
}


typedef struct __room_states {
	char password[SIZ];
	char dirname[SIZ];
	char name[SIZ];
	int flags;
	int floor;
	int order;
	int view;
	int flags2;
} room_states;




/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * set newval to 0 to clear, 1 to set, any other value to leave unchanged.
 * returns the new value.
 */

int self_service(int newval) {
	int current_value = 0;
	char buf[SIZ];
	
	char name[SIZ];
	char password[SIZ];
	char dirname[SIZ];
        int flags, floor, order, view, flags2;

	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return(0);

	extract_token(name, &buf[4], 0, '|', sizeof name);
	extract_token(password, &buf[4], 1, '|', sizeof password);
	extract_token(dirname, &buf[4], 2, '|', sizeof dirname);
	flags = extract_int(&buf[4], 3);
	floor = extract_int(&buf[4], 4);
	order = extract_int(&buf[4], 5);
	view = extract_int(&buf[4], 6);
	flags2 = extract_int(&buf[4], 7);

	if (flags2 & QR2_SELFLIST) {
		current_value = 1;
	}
	else {
		current_value = 0;
	}

	if (newval == 1) {
		flags2 = flags2 | QR2_SELFLIST;
	}
	else if (newval == 0) {
		flags2 = flags2 & ~QR2_SELFLIST;
	}
	else {
		return(current_value);
	}

	if (newval != current_value) {
		serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
			    name, password, dirname, flags,
			    floor, order, view, flags2);
		serv_getln(buf, sizeof buf);
	}

	FlushRoomlist ();
	return(newval);

}

int is_selflist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_SELFLIST) != 0);
}

int is_publiclist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_SMTP_PUBLIC) != 0);
}

int is_moderatedlist(room_states *RoomFlags)
{
	return ((RoomFlags->flags2 & QR2_MODERATED) != 0);
}

/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * set newval to 0 to clear, 1 to set, any other value to leave unchanged.
 * returns the new value.
 */

int get_roomflags(room_states *RoomOps) 
{
	char buf[SIZ];
	
	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return(0);

	extract_token(RoomOps->name, &buf[4], 0, '|', sizeof RoomOps->name);
	extract_token(RoomOps->password, &buf[4], 1, '|', sizeof RoomOps->password);
	extract_token(RoomOps->dirname, &buf[4], 2, '|', sizeof RoomOps->dirname);
	RoomOps->flags = extract_int(&buf[4], 3);
	RoomOps->floor = extract_int(&buf[4], 4);
	RoomOps->order = extract_int(&buf[4], 5);
	RoomOps->view = extract_int(&buf[4], 6);
	RoomOps->flags2 = extract_int(&buf[4], 7);
	return (1);
}

int set_roomflags(room_states *RoomOps)
{
	char buf[SIZ];

	serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
		    RoomOps->name, 
		    RoomOps->password, 
		    RoomOps->dirname, 
		    RoomOps->flags,
		    RoomOps->floor, 
		    RoomOps->order, 
		    RoomOps->view, 
		    RoomOps->flags2);
	serv_getln(buf, sizeof buf);
	FlushRoomlist ();

	return (1);
}






/*
 * display the form for editing a room
 */
void display_editroom(void)
{
	char buf[SIZ];
	char cmd[1024];
	char node[256];
	char remote_room[128];
	char recp[1024];
	char er_name[128];
	char er_password[10];
	char er_dirname[15];
	char er_roomaide[26];
	unsigned er_flags;
	unsigned er_flags2;
	int er_floor;
	int i, j;
	char *tab;
	char *shared_with;
	char *not_shared_with = NULL;
	int roompolicy = 0;
	int roomvalue = 0;
	int floorpolicy = 0;
	int floorvalue = 0;
	char pop3_host[128];
	char pop3_user[32];
	int bg = 0;

	tab = bstr("tab");
	if (IsEmptyStr(tab)) tab = "admin";

//	Buf = NewStrBuf();
//	load_floorlist(Buf);
//	FreeStrBuf(&Buf);
	output_headers(1, 1, 1, 0, 0, 0);

	wc_printf("<div class=\"fix_scrollbar_bug\">");

	wc_printf("<br />\n");

	/* print the tabbed dialog */
	wc_printf("<div align=\"center\">");
	wc_printf("<table id=\"AdminTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">"
		"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
		);

	wc_printf("<td class=\"");
	if (!strcmp(tab, "admin")) {
		wc_printf(" tab_cell_label\">");
		wc_printf(_("Administration"));
	}
	else {
		wc_printf("< tab_cell_edit\"><a href=\"display_editroom?tab=admin\">");
		wc_printf(_("Administration"));
		wc_printf("</a>");
	}
	wc_printf("</td>\n");
	wc_printf("<td>&nbsp;</td>\n");

	if ( ConditionalHaveRoomeditRights(NULL, NULL)) {

		wc_printf("<td class=\"");
		if (!strcmp(tab, "config")) {
			wc_printf(" tab_cell_label\">");
			wc_printf(_("Configuration"));
		}
		else {
			wc_printf(" tab_cell_edit\"><a href=\"display_editroom?tab=config\">");
			wc_printf(_("Configuration"));
			wc_printf("</a>");
		}
		wc_printf("</td>\n");
		wc_printf("<td>&nbsp;</td>\n");

		wc_printf("<td class=\"");
		if (!strcmp(tab, "expire")) {
			wc_printf(" tab_cell_label\">");
			wc_printf(_("Message expire policy"));
		}
		else {
			wc_printf(" tab_cell_edit\"><a href=\"display_editroom?tab=expire\">");
			wc_printf(_("Message expire policy"));
			wc_printf("</a>");
		}
		wc_printf("</td>\n");
		wc_printf("<td>&nbsp;</td>\n");
	
		wc_printf("<td class=\"");
		if (!strcmp(tab, "access")) {
			wc_printf(" tab_cell_label\">");
			wc_printf(_("Access controls"));
		}
		else {
			wc_printf(" tab_cell_edit\"><a href=\"display_editroom?tab=access\">");
			wc_printf(_("Access controls"));
			wc_printf("</a>");
		}
		wc_printf("</td>\n");
		wc_printf("<td>&nbsp;</td>\n");

		wc_printf("<td class=\"");
		if (!strcmp(tab, "sharing")) {
			wc_printf(" tab_cell_label\">");
			wc_printf(_("Sharing"));
		}
		else {
			wc_printf(" tab_cell_edit\"><a href=\"display_editroom?tab=sharing\">");
			wc_printf(_("Sharing"));
			wc_printf("</a>");
		}
		wc_printf("</td>\n");
		wc_printf("<td>&nbsp;</td>\n");

		wc_printf("<td class=\"");
		if (!strcmp(tab, "listserv")) {
			wc_printf(" tab_cell_label\">");
			wc_printf(_("Mailing list service"));
		}
		else {
			wc_printf("< tab_cell_edit\"><a href=\"display_editroom?tab=listserv\">");
			wc_printf(_("Mailing list service"));
			wc_printf("</a>");
		}
		wc_printf("</td>\n");
		wc_printf("<td>&nbsp;</td>\n");

	}

	wc_printf("<td class=\"");
	if (!strcmp(tab, "feeds")) {
		wc_printf(" tab_cell_label\">");
		wc_printf(_("Remote retrieval"));
	}
	else {
		wc_printf("< tab_cell_edit\"><a href=\"display_editroom?tab=feeds\">");
		wc_printf(_("Remote retrieval"));
		wc_printf("</a>");
	}
	wc_printf("</td>\n");
	wc_printf("<td>&nbsp;</td>\n");

	wc_printf("</tr></table>\n");
	wc_printf("</div>\n");
	/* end tabbed dialog */	

	wc_printf("<script type=\"text/javascript\">"
		" Nifty(\"table#AdminTabs td\", \"small transparent top\");"
		"</script>"
		);

	/* begin content of whatever tab is open now */

	if (!strcmp(tab, "admin")) {
		wc_printf("<div class=\"tabcontent\">");
		wc_printf("<ul>"
			"<li><a href=\"delete_room\" "
			"onClick=\"return confirm('");
		wc_printf(_("Are you sure you want to delete this room?"));
		wc_printf("');\">\n");
		wc_printf(_("Delete this room"));
		wc_printf("</a>\n"
			"<li><a href=\"display_editroompic?which_room=");
		urlescputs(ChrPtr(WC->CurRoom.name));
		wc_printf("\">\n");
		wc_printf(_("Set or change the icon for this room's banner"));
		wc_printf("</a>\n"
			"<li><a href=\"display_editinfo\">\n");
		wc_printf(_("Edit this room's Info file"));
		wc_printf("</a>\n"
			"</ul>");
		wc_printf("</div>");
	}

	if (!strcmp(tab, "config")) {
		wc_printf("<div class=\"tabcontent\">");
		serv_puts("GETR");
		serv_getln(buf, sizeof buf);

		if (!strncmp(buf, "550", 3)) {
			wc_printf("<br><br><div align=center>%s</div><br><br>\n",
				_("Higher access is required to access this function.")
				);
		}
		else if (buf[0] != '2') {
			wc_printf("<br><br><div align=center>%s</div><br><br>\n", &buf[4]);
		}
		else {
			extract_token(er_name, &buf[4], 0, '|', sizeof er_name);
			extract_token(er_password, &buf[4], 1, '|', sizeof er_password);
			extract_token(er_dirname, &buf[4], 2, '|', sizeof er_dirname);
			er_flags = extract_int(&buf[4], 3);
			er_floor = extract_int(&buf[4], 4);
			er_flags2 = extract_int(&buf[4], 7);
	
			wc_printf("<form method=\"POST\" action=\"editroom\">\n");
			wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		
			wc_printf("<ul><li>");
			wc_printf(_("Name of room: "));
			wc_printf("<input type=\"text\" NAME=\"er_name\" VALUE=\"%s\" MAXLENGTH=\""ULONG_FMT"\">\n",
				er_name,
				(sizeof(er_name)-1)
				);
		
			wc_printf("<li>");
			wc_printf(_("Resides on floor: "));
			wc_printf("<select NAME=\"er_floor\" SIZE=\"1\"");
			if (er_flags & QR_MAILBOX)
				wc_printf("disabled >\n");
			for (i = 0; i < 128; ++i)
				if (!IsEmptyStr(floorlist[i])) {
					wc_printf("<OPTION ");
					if (i == er_floor )
						wc_printf("SELECTED ");
					wc_printf("VALUE=\"%d\">", i);
					escputs(floorlist[i]);
					wc_printf("</OPTION>\n");
				}
			wc_printf("</select>\n");

			wc_printf("<li>");
			wc_printf(_("Type of room:"));
			wc_printf("<ul>\n");
	
			wc_printf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"public\" ");
			if ((er_flags & (QR_PRIVATE + QR_MAILBOX)) == 0)
				wc_printf("CHECKED ");
			wc_printf("OnChange=\""
				"	if (this.form.type[0].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wc_printf(_("Public (automatically appears to everyone)"));
			wc_printf("\n");
	
			wc_printf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"hidden\" ");
			if ((er_flags & QR_PRIVATE) &&
			    (er_flags & QR_GUESSNAME))
				wc_printf("CHECKED ");
			wc_printf(" OnChange=\""
				"	if (this.form.type[1].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wc_printf(_("Private - hidden (accessible to anyone who knows its name)"));
		
			wc_printf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"passworded\" ");
			if ((er_flags & QR_PRIVATE) &&
			    (er_flags & QR_PASSWORDED))
				wc_printf("CHECKED ");
			wc_printf(" OnChange=\""
				"	if (this.form.type[2].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wc_printf(_("Private - require password: "));
			wc_printf("\n<input type=\"text\" NAME=\"er_password\" VALUE=\"%s\" MAXLENGTH=\"9\">\n",
				er_password);
		
			wc_printf("<li><input type=\"radio\" NAME=\"type\" VALUE=\"invonly\" ");
			if ((er_flags & QR_PRIVATE)
			    && ((er_flags & QR_GUESSNAME) == 0)
			    && ((er_flags & QR_PASSWORDED) == 0))
				wc_printf("CHECKED ");
			wc_printf(" OnChange=\""
				"	if (this.form.type[3].checked == true) {	"
				"		this.form.er_floor.disabled = false;	"
				"	}						"
				"\"> ");
			wc_printf(_("Private - invitation only"));
		
			wc_printf("\n<li><input type=\"radio\" NAME=\"type\" VALUE=\"personal\" ");
			if (er_flags & QR_MAILBOX)
				wc_printf("CHECKED ");
			wc_printf (" OnChange=\""
				 "	if (this.form.type[4].checked == true) {	"
				 "		this.form.er_floor.disabled = true;	"
				 "	}						"
				 "\"> ");
			wc_printf(_("Personal (mailbox for you only)"));
			
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"bump\" VALUE=\"yes\" ");
			wc_printf("> ");
			wc_printf(_("If private, cause current users to forget room"));
		
			wc_printf("\n</ul>\n");
		
			wc_printf("<li><input type=\"checkbox\" NAME=\"prefonly\" VALUE=\"yes\" ");
			if (er_flags & QR_PREFONLY)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Preferred users only"));
		
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"readonly\" VALUE=\"yes\" ");
			if (er_flags & QR_READONLY)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Read-only room"));
		
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"collabdel\" VALUE=\"yes\" ");
			if (er_flags2 & QR2_COLLABDEL)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("All users allowed to post may also delete messages"));
		
			/** directory stuff */
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"directory\" VALUE=\"yes\" ");
			if (er_flags & QR_DIRECTORY)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("File directory room"));
	
			wc_printf("\n<ul><li>");
			wc_printf(_("Directory name: "));
			wc_printf("<input type=\"text\" NAME=\"er_dirname\" VALUE=\"%s\" MAXLENGTH=\"14\">\n",
				er_dirname);
	
			wc_printf("<li><input type=\"checkbox\" NAME=\"ulallowed\" VALUE=\"yes\" ");
			if (er_flags & QR_UPLOAD)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Uploading allowed"));
		
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"dlallowed\" VALUE=\"yes\" ");
			if (er_flags & QR_DOWNLOAD)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Downloading allowed"));
		
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"visdir\" VALUE=\"yes\" ");
			if (er_flags & QR_VISDIR)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Visible directory"));
			wc_printf("</ul>\n");
		
			/** end of directory stuff */
	
			wc_printf("<li><input type=\"checkbox\" NAME=\"network\" VALUE=\"yes\" ");
			if (er_flags & QR_NETWORK)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Network shared room"));
	
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"permanent\" VALUE=\"yes\" ");
			if (er_flags & QR_PERMANENT)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Permanent (does not auto-purge)"));
	
			wc_printf("\n<li><input type=\"checkbox\" NAME=\"subjectreq\" VALUE=\"yes\" ");
			if (er_flags2 & QR2_SUBJECTREQ)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Subject Required (Force users to specify a message subject)"));
	
			/** start of anon options */
		
			wc_printf("\n<li>");
			wc_printf(_("Anonymous messages"));
			wc_printf("<ul>\n");
		
			wc_printf("<li><input type=\"radio\" NAME=\"anon\" VALUE=\"no\" ");
			if (((er_flags & QR_ANONONLY) == 0)
			    && ((er_flags & QR_ANONOPT) == 0))
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("No anonymous messages"));
	
			wc_printf("\n<li><input type=\"radio\" NAME=\"anon\" VALUE=\"anononly\" ");
			if (er_flags & QR_ANONONLY)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("All messages are anonymous"));
		
			wc_printf("\n<li><input type=\"radio\" NAME=\"anon\" VALUE=\"anon2\" ");
			if (er_flags & QR_ANONOPT)
				wc_printf("CHECKED ");
			wc_printf("> ");
			wc_printf(_("Prompt user when entering messages"));
			wc_printf("</ul>\n");
		
			/* end of anon options */
		
			wc_printf("<li>");
			wc_printf(_("Room aide: "));
			serv_puts("GETA");
			serv_getln(buf, sizeof buf);
			if (buf[0] != '2') {
				wc_printf("<em>%s</em>\n", &buf[4]);
			} else {
				extract_token(er_roomaide, &buf[4], 0, '|', sizeof er_roomaide);
				wc_printf("<input type=\"text\" NAME=\"er_roomaide\" VALUE=\"%s\" MAXLENGTH=\"25\">\n", er_roomaide);
			}
		
			wc_printf("</ul><CENTER>\n");
			wc_printf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"config\">\n"
				"<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">"
				"&nbsp;"
				"<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">"
				"</CENTER>\n",
				_("Save changes"),
				_("Cancel")
				);
		}
		wc_printf("</div>");
	}


	/* Sharing the room with other Citadel nodes... */
	if (!strcmp(tab, "sharing")) {
		wc_printf("<div class=\"tabcontent\">");

		shared_with = strdup("");
		not_shared_with = strdup("");

		/** Learn the current configuration */
		serv_puts("CONF getsys|application/x-citadel-ignet-config");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(node, buf, 0, '|', sizeof node);
				not_shared_with = realloc(not_shared_with,
							  strlen(not_shared_with) + 32);
				strcat(not_shared_with, node);
				strcat(not_shared_with, "\n");
			}

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cmd, buf, 0, '|', sizeof cmd);
				extract_token(node, buf, 1, '|', sizeof node);
				extract_token(remote_room, buf, 2, '|', sizeof remote_room);
				if (!strcasecmp(cmd, "ignet_push_share")) {
					shared_with = realloc(shared_with,
							      strlen(shared_with) + 32);
					strcat(shared_with, node);
					if (!IsEmptyStr(remote_room)) {
						strcat(shared_with, "|");
						strcat(shared_with, remote_room);
					}
					strcat(shared_with, "\n");
				}
			}

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n', sizeof buf);
			extract_token(node, buf, 0, '|', sizeof node);
			for (j=0; j<num_tokens(not_shared_with, '\n'); ++j) {
				extract_token(cmd, not_shared_with, j, '\n', sizeof cmd);
				if (!strcasecmp(node, cmd)) {
					remove_token(not_shared_with, j, '\n');
				}
			}
		}

		/* Display the stuff */
		wc_printf("<CENTER><br />"
			"<table border=1 cellpadding=5><tr>"
			"<td><B><I>");
		wc_printf(_("Shared with"));
		wc_printf("</I></B></td>"
			"<td><B><I>");
		wc_printf(_("Not shared with"));
		wc_printf("</I></B></td></tr>\n"
			"<tr><td VALIGN=TOP>\n");

		wc_printf("<table border=0 cellpadding=5><tr class=\"tab_cell\"><td>");
		wc_printf(_("Remote node name"));
		wc_printf("</td><td>");
		wc_printf(_("Remote room name"));
		wc_printf("</td><td>");
		wc_printf(_("Actions"));
		wc_printf("</td></tr>\n");

		for (i=0; i<num_tokens(shared_with, '\n'); ++i) {
			extract_token(buf, shared_with, i, '\n', sizeof buf);
			extract_token(node, buf, 0, '|', sizeof node);
			extract_token(remote_room, buf, 1, '|', sizeof remote_room);
			if (!IsEmptyStr(node)) {
				wc_printf("<form method=\"POST\" action=\"netedit\">");
				wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
				wc_printf("<tr><td>%s</td>\n", node);

				wc_printf("<td>");
				if (!IsEmptyStr(remote_room)) {
					escputs(remote_room);
				}
				wc_printf("</td>");

				wc_printf("<td>");
		
				wc_printf("<input type=\"hidden\" NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				if (!IsEmptyStr(remote_room)) {
					wc_printf("|");
					urlescputs(remote_room);
				}
				wc_printf("\">");
				wc_printf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"sharing\">\n");
				wc_printf("<input type=\"hidden\" NAME=\"cmd\" VALUE=\"remove\">\n");
				wc_printf("<input type=\"submit\" "
					"NAME=\"unshare_button\" VALUE=\"%s\">", _("Unshare"));
				wc_printf("</td></tr></form>\n");
			}
		}

		wc_printf("</table>\n");
		wc_printf("</td><td VALIGN=TOP>\n");
		wc_printf("<table border=0 cellpadding=5><tr class=\"tab_cell\"><td>");
		wc_printf(_("Remote node name"));
		wc_printf("</td><td>");
		wc_printf(_("Remote room name"));
		wc_printf("</td><td>");
		wc_printf(_("Actions"));
		wc_printf("</td></tr>\n");

		for (i=0; i<num_tokens(not_shared_with, '\n'); ++i) {
			extract_token(node, not_shared_with, i, '\n', sizeof node);
			if (!IsEmptyStr(node)) {
				wc_printf("<form method=\"POST\" action=\"netedit\">");
				wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
				wc_printf("<tr><td>");
				escputs(node);
				wc_printf("</td><td>"
					"<input type=\"INPUT\" "
					"NAME=\"suffix\" "
					"MAXLENGTH=128>"
					"</td><td>");
				wc_printf("<input type=\"hidden\" "
					"NAME=\"line\" "
					"VALUE=\"ignet_push_share|");
				urlescputs(node);
				wc_printf("|\">");
				wc_printf("<input type=\"hidden\" NAME=\"tab\" "
					"VALUE=\"sharing\">\n");
				wc_printf("<input type=\"hidden\" NAME=\"cmd\" "
					"VALUE=\"add\">\n");
				wc_printf("<input type=\"submit\" "
					"NAME=\"add_button\" VALUE=\"%s\">", _("Share"));
				wc_printf("</td></tr></form>\n");
			}
		}

		wc_printf("</table>\n");
		wc_printf("</td></tr>"
			"</table></CENTER><br />\n"
			"<I><B>%s</B><ul><li>", _("Notes:"));
		wc_printf(_("When sharing a room, "
			  "it must be shared from both ends.  Adding a node to "
			  "the 'shared' list sends messages out, but in order to"
			  " receive messages, the other nodes must be configured"
			  " to send messages out to your system as well. "
			  "<li>If the remote room name is blank, it is assumed "
			  "that the room name is identical on the remote node."
			  "<li>If the remote room name is different, the remote "
			  "node must also configure the name of the room here."
			  "</ul></I><br />\n"
				));

		wc_printf("</div>");
	}

	if (not_shared_with != NULL)
		free (not_shared_with);

	/* Mailing list management */
	if (!strcmp(tab, "listserv")) {
		room_states RoomFlags;
		wc_printf("<div class=\"tabcontent\">");

		wc_printf("<br /><center>"
			"<table BORDER=0 WIDTH=100%% CELLPADDING=5>"
			"<tr><td VALIGN=TOP>");

		wc_printf(_("<i>The contents of this room are being "
			  "mailed <b>as individual messages</b> "
			  "to the following list recipients:"
			  "</i><br /><br />\n"));

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cmd, buf, 0, '|', sizeof cmd);
				if (!strcasecmp(cmd, "listrecp")) {
					extract_token(recp, buf, 1, '|', sizeof recp);
			
					escputs(recp);
					wc_printf(" <a href=\"netedit?cmd=remove?tab=listserv?line=listrecp|");
					urlescputs(recp);
					wc_printf("\">");
					wc_printf(_("(remove)"));
					wc_printf("</A><br />");
				}
			}
		wc_printf("<br /><form method=\"POST\" action=\"netedit\">\n"
			"<input type=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<input type=\"hidden\" NAME=\"prefix\" VALUE=\"listrecp|\">\n");
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<input type=\"text\" id=\"add_as_listrecp\" NAME=\"line\">\n");
		wc_printf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wc_printf("</form>\n");

		wc_printf("</td><td VALIGN=TOP>\n");
		
		wc_printf(_("<i>The contents of this room are being "
			  "mailed <b>in digest form</b> "
			  "to the following list recipients:"
			  "</i><br /><br />\n"));

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cmd, buf, 0, '|', sizeof cmd);
				if (!strcasecmp(cmd, "digestrecp")) {
					extract_token(recp, buf, 1, '|', sizeof recp);
			
					escputs(recp);
					wc_printf(" <a href=\"netedit?cmd=remove?tab=listserv?line="
						"digestrecp|");
					urlescputs(recp);
					wc_printf("\">");
					wc_printf(_("(remove)"));
					wc_printf("</A><br />");
				}
			}
		wc_printf("<br /><form method=\"POST\" action=\"netedit\">\n"
			"<input type=\"hidden\" NAME=\"tab\" VALUE=\"listserv\">\n"
			"<input type=\"hidden\" NAME=\"prefix\" VALUE=\"digestrecp|\">\n");
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<input type=\"text\" id=\"add_as_digestrecp\" NAME=\"line\">\n");
		wc_printf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wc_printf("</form>\n");
		
		wc_printf("</td></tr></table>\n");

		/** Pop open an address book -- begin **/
		wc_printf("<div align=right>"
			"<a href=\"javascript:PopOpenAddressBook('add_as_listrecp|%s|add_as_digestrecp|%s');\" "
			"title=\"%s\">"
			"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
			"&nbsp;%s</a>"
			"</div>",
			_("List"),
			_("Digest"),
			_("Add recipients from Contacts or other address books"),
			_("Add recipients from Contacts or other address books")
			);
		/* Pop open an address book -- end **/

		wc_printf("<br />\n<form method=\"GET\" action=\"toggle_self_service\">\n");

		get_roomflags (&RoomFlags);
		
		/* Self Service subscription? */
		wc_printf("<table><tr><td>\n");
		wc_printf(_("Allow self-service subscribe/unsubscribe requests."));
		wc_printf("</td><td><input type=\"checkbox\" name=\"QR2_SelfList\" value=\"yes\" %s></td></tr>\n"
			" <tr><td colspan=\"2\">\n",
			(is_selflist(&RoomFlags))?"checked":"");
		wc_printf(_("The URL for subscribe/unsubscribe is: "));
		wc_printf("<TT>%s://%s/listsub</TT></td></tr>\n",
			(is_https ? "https" : "http"),
			ChrPtr(WC->Hdr->HR.http_host));
		/* Public posting? */
		wc_printf("<tr><td>");
		wc_printf(_("Allow non-subscribers to mail to this room."));
		wc_printf("</td><td><input type=\"checkbox\" name=\"QR2_SubsOnly\" value=\"yes\" %s></td></tr>\n",
			(is_publiclist(&RoomFlags))?"checked":"");
		
		/* Moderated List? */
		wc_printf("<tr><td>");
		wc_printf(_("Room post publication needs Aide permission."));
		wc_printf("</td><td><input type=\"checkbox\" name=\"QR2_Moderated\" value=\"yes\" %s></td></tr>\n",
			(is_moderatedlist(&RoomFlags))?"checked":"");


		wc_printf("<tr><td colspan=\"2\" align=\"center\">"
			"<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\"></td></tr>", _("Save changes"));
		wc_printf("</table></form>");
			

		wc_printf("</CENTER>\n");
		wc_printf("</div>");
	}


	/* Configuration of The Dreaded Auto-Purger */
	if (!strcmp(tab, "expire")) {
		wc_printf("<div class=\"tabcontent\">");

		serv_puts("GPEX room");
		serv_getln(buf, sizeof buf);
		if (!strncmp(buf, "550", 3)) {
			wc_printf("<br><br><div align=center>%s</div><br><br>\n",
				_("Higher access is required to access this function.")
				);
		}
		else if (buf[0] != '2') {
			wc_printf("<br><br><div align=center>%s</div><br><br>\n", &buf[4]);
		}
		else {
			roompolicy = extract_int(&buf[4], 0);
			roomvalue = extract_int(&buf[4], 1);
		
			serv_puts("GPEX floor");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				floorpolicy = extract_int(&buf[4], 0);
				floorvalue = extract_int(&buf[4], 1);
			}
			
			wc_printf("<br /><form method=\"POST\" action=\"set_room_policy\">\n");
			wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
			wc_printf("<table border=0 cellspacing=5>\n");
			wc_printf("<tr><td>");
			wc_printf(_("Message expire policy for this room"));
			wc_printf("<br />(");
			escputs(ChrPtr(WC->CurRoom.name));
			wc_printf(")</td><td>");
			wc_printf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"0\" %s>",
				((roompolicy == 0) ? "CHECKED" : "") );
			wc_printf(_("Use the default policy for this floor"));
			wc_printf("<br />\n");
			wc_printf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"1\" %s>",
				((roompolicy == 1) ? "CHECKED" : "") );
			wc_printf(_("Never automatically expire messages"));
			wc_printf("<br />\n");
			wc_printf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"2\" %s>",
				((roompolicy == 2) ? "CHECKED" : "") );
			wc_printf(_("Expire by message count"));
			wc_printf("<br />\n");
			wc_printf("<input type=\"radio\" NAME=\"roompolicy\" VALUE=\"3\" %s>",
				((roompolicy == 3) ? "CHECKED" : "") );
			wc_printf(_("Expire by message age"));
			wc_printf("<br />");
			wc_printf(_("Number of messages or days: "));
			wc_printf("<input type=\"text\" NAME=\"roomvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">", roomvalue);
			wc_printf("</td></tr>\n");
	
			if (WC->axlevel >= 6) {
				wc_printf("<tr><td COLSPAN=2><hr /></td></tr>\n");
				wc_printf("<tr><td>");
				wc_printf(_("Message expire policy for this floor"));
				wc_printf("<br />(");
				escputs(floorlist[WC->CurRoom.floorid]);
				wc_printf(")</td><td>");
				wc_printf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"0\" %s>",
					((floorpolicy == 0) ? "CHECKED" : "") );
				wc_printf(_("Use the system default"));
				wc_printf("<br />\n");
				wc_printf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"1\" %s>",
					((floorpolicy == 1) ? "CHECKED" : "") );
				wc_printf(_("Never automatically expire messages"));
				wc_printf("<br />\n");
				wc_printf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"2\" %s>",
					((floorpolicy == 2) ? "CHECKED" : "") );
				wc_printf(_("Expire by message count"));
				wc_printf("<br />\n");
				wc_printf("<input type=\"radio\" NAME=\"floorpolicy\" VALUE=\"3\" %s>",
					((floorpolicy == 3) ? "CHECKED" : "") );
				wc_printf(_("Expire by message age"));
				wc_printf("<br />");
				wc_printf(_("Number of messages or days: "));
				wc_printf("<input type=\"text\" NAME=\"floorvalue\" MAXLENGTH=\"5\" VALUE=\"%d\">",
					floorvalue);
			}
	
			wc_printf("<CENTER>\n");
			wc_printf("<tr><td COLSPAN=2><hr /><CENTER>\n");
			wc_printf("<input type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">", _("Save changes"));
			wc_printf("&nbsp;");
			wc_printf("<input type=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\">", _("Cancel"));
			wc_printf("</CENTER></td><tr>\n");
	
			wc_printf("</table>\n"
				"<input type=\"hidden\" NAME=\"tab\" VALUE=\"expire\">\n"
				"</form>\n"
				);
		}

		wc_printf("</div>");
	}

	/* Access controls */
	if (!strcmp(tab, "access")) {
		wc_printf("<div class=\"tabcontent\">");
		display_whok();
		wc_printf("</div>");
	}

	/* Fetch messages from remote locations */
	if (!strcmp(tab, "feeds")) {
		wc_printf("<div class=\"tabcontent\">");

		wc_printf("<i>");
		wc_printf(_("Retrieve messages from these remote POP3 accounts and store them in this room:"));
		wc_printf("</i><br />\n");

		wc_printf("<table class=\"altern\" border=0 cellpadding=5>"
			"<tr class=\"even\"><th>");
		wc_printf(_("Remote host"));
		wc_printf("</th><th>");
		wc_printf(_("User name"));
		wc_printf("</th><th>");
		wc_printf(_("Password"));
		wc_printf("</th><th>");
		wc_printf(_("Keep messages on server?"));
		wc_printf("</th><th>");
		wc_printf(_("Interval"));
		wc_printf("</th><th> </th></tr>");

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		bg = 1;
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cmd, buf, 0, '|', sizeof cmd);
				if (!strcasecmp(cmd, "pop3client")) {
					safestrncpy(recp, &buf[11], sizeof recp);

					bg = 1 - bg;
					wc_printf("<tr class=\"%s\">",
						(bg ? "even" : "odd")
						);

					wc_printf("<td>");
					extract_token(pop3_host, buf, 1, '|', sizeof pop3_host);
					escputs(pop3_host);
					wc_printf("</td>");

					wc_printf("<td>");
					extract_token(pop3_user, buf, 2, '|', sizeof pop3_user);
					escputs(pop3_user);
					wc_printf("</td>");

					wc_printf("<td>*****</td>");		/* Don't show the password */

					wc_printf("<td>%s</td>", extract_int(buf, 4) ? _("Yes") : _("No"));

					wc_printf("<td>%ld</td>", extract_long(buf, 5));	/* Fetching interval */
			
					wc_printf("<td class=\"button_link\">");
					wc_printf(" <a href=\"netedit?cmd=remove?tab=feeds?line=pop3client|");
					urlescputs(recp);
					wc_printf("\">");
					wc_printf(_("(remove)"));
					wc_printf("</a></td>");
			
					wc_printf("</tr>");
				}
			}

		wc_printf("<form method=\"POST\" action=\"netedit\">\n"
			"<tr>"
			"<input type=\"hidden\" name=\"tab\" value=\"feeds\">"
			"<input type=\"hidden\" name=\"prefix\" value=\"pop3client|\">\n");
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<td>");
		wc_printf("<input type=\"text\" id=\"add_as_pop3host\" NAME=\"line_pop3host\">\n");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"text\" id=\"add_as_pop3user\" NAME=\"line_pop3user\">\n");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"password\" id=\"add_as_pop3pass\" NAME=\"line_pop3pass\">\n");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"checkbox\" id=\"add_as_pop3keep\" NAME=\"line_pop3keep\" VALUE=\"1\">");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"text\" id=\"add_as_pop3int\" NAME=\"line_pop3int\" MAXLENGTH=\"5\">");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"submit\" NAME=\"add_button\" VALUE=\"%s\">", _("Add"));
		wc_printf("</td></tr>");
		wc_printf("</form></table>\n");

		wc_printf("<hr>\n");

		wc_printf("<i>");
		wc_printf(_("Fetch the following RSS feeds and store them in this room:"));
		wc_printf("</i><br />\n");

		wc_printf("<table class=\"altern\" border=0 cellpadding=5>"
			"<tr class=\"even\"><th>");
		wc_printf("<img src=\"static/rss_16x.png\" width=\"16\" height=\"16\" alt=\" \"> ");
		wc_printf(_("Feed URL"));
		wc_printf("</th><th>");
		wc_printf("</th></tr>");

		serv_puts("GNET");
		serv_getln(buf, sizeof buf);
		bg = 1;
		if (buf[0]=='1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(cmd, buf, 0, '|', sizeof cmd);
				if (!strcasecmp(cmd, "rssclient")) {
					safestrncpy(recp, &buf[10], sizeof recp);

					bg = 1 - bg;
					wc_printf("<tr class=\"%s\">",
						(bg ? "even" : "odd")
						);

					wc_printf("<td>");
					extract_token(pop3_host, buf, 1, '|', sizeof pop3_host);
					escputs(pop3_host);
					wc_printf("</td>");

					wc_printf("<td class=\"button_link\">");
					wc_printf(" <a href=\"netedit?cmd=remove?tab=feeds?line=rssclient|");
					urlescputs(recp);
					wc_printf("\">");
					wc_printf(_("(remove)"));
					wc_printf("</a></td>");
			
					wc_printf("</tr>");
				}
			}

		wc_printf("<form method=\"POST\" action=\"netedit\">\n"
			"<tr>"
			"<input type=\"hidden\" name=\"tab\" value=\"feeds\">"
			"<input type=\"hidden\" name=\"prefix\" value=\"rssclient|\">\n");
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<td>");
		wc_printf("<input type=\"text\" id=\"add_as_pop3host\" size=\"72\" "
			"maxlength=\"256\" name=\"line_pop3host\">\n");
		wc_printf("</td>");
		wc_printf("<td>");
		wc_printf("<input type=\"submit\" name=\"add_button\" value=\"%s\">", _("Add"));
		wc_printf("</td></tr>");
		wc_printf("</form></table>\n");

		wc_printf("</div>");
	}


	/* end content of whatever tab is open now */
	wc_printf("</div>\n");

	address_book_popup();
	wDumpContent(1);
}


/* 
 * Toggle self-service list subscription
 */
void toggle_self_service(void) {
	room_states RoomFlags;

	get_roomflags (&RoomFlags);

	if (yesbstr("QR2_SelfList")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SELFLIST;
	else 
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SELFLIST;

	if (yesbstr("QR2_SMTP_PUBLIC")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SMTP_PUBLIC;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SMTP_PUBLIC;

	if (yesbstr("QR2_Moderated")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_MODERATED;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_MODERATED;
	if (yesbstr("QR2_SubsOnly")) 
		RoomFlags.flags2 = RoomFlags.flags2 | QR2_SMTP_PUBLIC;
	else
		RoomFlags.flags2 = RoomFlags.flags2 & ~QR2_SMTP_PUBLIC;

	set_roomflags (&RoomFlags);
	
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
}



/*
 * save new parameters for a room
 */
void editroom(void)
{
	const StrBuf *Ptr;
	StrBuf *Buf;
	StrBuf *er_name;
	StrBuf *er_password;
	StrBuf *er_dirname;
	StrBuf *er_roomaide;
	int er_floor;
	unsigned er_flags;
	int er_listingorder;
	int er_defaultview;
	unsigned er_flags2;
	int bump;


	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
		       _("Cancelled.  Changes were not saved."));
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}
	serv_puts("GETR");
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		StrBufCutLeft(Buf, 4);
		strcpy(WC->ImportantMessage, ChrPtr(Buf));
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		FreeStrBuf(&Buf);
		return;
	}

	FlushRoomlist ();

	er_name = NewStrBuf();
	er_password = NewStrBuf();
	er_dirname = NewStrBuf();
	er_roomaide = NewStrBuf();

	StrBufCutLeft(Buf, 4);
	StrBufExtract_token(er_name, Buf, 0, '|');
	StrBufExtract_token(er_password, Buf, 1, '|');
	StrBufExtract_token(er_dirname, Buf, 2, '|');
	er_flags = StrBufExtract_int(Buf, 3, '|');
	er_listingorder = StrBufExtract_int(Buf, 5, '|');
	er_defaultview = StrBufExtract_int(Buf, 6, '|');
	er_flags2 = StrBufExtract_int(Buf, 7, '|');

	er_roomaide = NewStrBufDup(sbstr("er_roomaide"));
	if (StrLength(er_roomaide) == 0) {
		serv_puts("GETA");
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) != 2) {
			FlushStrBuf(er_roomaide);
		} else {
			StrBufCutLeft(Buf, 4);
			StrBufExtract_token(er_roomaide, Buf, 0, '|');
		}
	}
	Ptr = sbstr("er_name");
	if (StrLength(Ptr) > 0) {
		FlushStrBuf(er_name);
		StrBufAppendBuf(er_name, Ptr, 0);
	}

	Ptr = sbstr("er_password");
	if (StrLength(Ptr) > 0) {
		FlushStrBuf(er_password);
		StrBufAppendBuf(er_password, Ptr, 0);
	}
		

	Ptr = sbstr("er_dirname");
	if (StrLength(Ptr) > 0) { /* todo: cut 15 */
		FlushStrBuf(er_dirname);
		StrBufAppendBuf(er_dirname, Ptr, 0);
	}


	Ptr = sbstr("type");
	er_flags &= !(QR_PRIVATE | QR_PASSWORDED | QR_GUESSNAME);

	if (!strcmp(ChrPtr(Ptr), "invonly")) {
		er_flags |= (QR_PRIVATE);
	}
	if (!strcmp(ChrPtr(Ptr), "hidden")) {
		er_flags |= (QR_PRIVATE | QR_GUESSNAME);
	}
	if (!strcmp(ChrPtr(Ptr), "passworded")) {
		er_flags |= (QR_PRIVATE | QR_PASSWORDED);
	}
	if (!strcmp(ChrPtr(Ptr), "personal")) {
		er_flags |= QR_MAILBOX;
	} else {
		er_flags &= ~QR_MAILBOX;
	}
	
	if (yesbstr("prefonly")) {
		er_flags |= QR_PREFONLY;
	} else {
		er_flags &= ~QR_PREFONLY;
	}

	if (yesbstr("readonly")) {
		er_flags |= QR_READONLY;
	} else {
		er_flags &= ~QR_READONLY;
	}

	
	if (yesbstr("collabdel")) {
		er_flags2 |= QR2_COLLABDEL;
	} else {
		er_flags2 &= ~QR2_COLLABDEL;
	}

	if (yesbstr("permanent")) {
		er_flags |= QR_PERMANENT;
	} else {
		er_flags &= ~QR_PERMANENT;
	}

	if (yesbstr("subjectreq")) {
		er_flags2 |= QR2_SUBJECTREQ;
	} else {
		er_flags2 &= ~QR2_SUBJECTREQ;
	}

	if (yesbstr("network")) {
		er_flags |= QR_NETWORK;
	} else {
		er_flags &= ~QR_NETWORK;
	}

	if (yesbstr("directory")) {
		er_flags |= QR_DIRECTORY;
	} else {
		er_flags &= ~QR_DIRECTORY;
	}

	if (yesbstr("ulallowed")) {
		er_flags |= QR_UPLOAD;
	} else {
		er_flags &= ~QR_UPLOAD;
	}

	if (yesbstr("dlallowed")) {
		er_flags |= QR_DOWNLOAD;
	} else {
		er_flags &= ~QR_DOWNLOAD;
	}

	if (yesbstr("visdir")) {
		er_flags |= QR_VISDIR;
	} else {
		er_flags &= ~QR_VISDIR;
	}

	Ptr = sbstr("anon");

	er_flags &= ~(QR_ANONONLY | QR_ANONOPT);
	if (!strcmp(ChrPtr(Ptr), "anononly"))
		er_flags |= QR_ANONONLY;
	if (!strcmp(ChrPtr(Ptr), "anon2"))
		er_flags |= QR_ANONOPT;

	bump = yesbstr("bump");

	er_floor = ibstr("er_floor");

	StrBufPrintf(Buf, "SETR %s|%s|%s|%u|%d|%d|%d|%d|%u",
		     ChrPtr(er_name), 
		     ChrPtr(er_password), 
		     ChrPtr(er_dirname), 
		     er_flags, 
		     bump, 
		     er_floor,
		     er_listingorder, 
		     er_defaultview, 
		     er_flags2);
	serv_putbuf(Buf);
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		strcpy(WC->ImportantMessage, &ChrPtr(Buf)[4]);
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		FreeStrBuf(&Buf);
		FreeStrBuf(&er_name);
		FreeStrBuf(&er_password);
		FreeStrBuf(&er_dirname);
		FreeStrBuf(&er_roomaide);
		return;
	}
	gotoroom(er_name);

	if (StrLength(er_roomaide) > 0) {
		serv_printf("SETA %s", ChrPtr(er_roomaide));
		StrBuf_ServGetln(Buf);
		if (GetServerStatus(Buf, NULL) != 2) {
			strcpy(WC->ImportantMessage, &ChrPtr(Buf)[4]);
			display_main_menu();
			FreeStrBuf(&Buf);
			FreeStrBuf(&er_name);
			FreeStrBuf(&er_password);
			FreeStrBuf(&er_dirname);
			FreeStrBuf(&er_roomaide);
			return;
		}
	}
	gotoroom(er_name);
	strcpy(WC->ImportantMessage, _("Your changes have been saved."));
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
	FreeStrBuf(&Buf);
	FreeStrBuf(&er_name);
	FreeStrBuf(&er_password);
	FreeStrBuf(&er_dirname);
	FreeStrBuf(&er_roomaide);
	return;
}


/*
 * Display form for Invite, Kick, and show Who Knows a room
 */
void do_invt_kick(void) {
        char buf[SIZ], room[SIZ], username[SIZ];

        serv_puts("GETR");
        serv_getln(buf, sizeof buf);

        if (buf[0] != '2') {
		escputs(&buf[4]);
		return;
        }
        extract_token(room, &buf[4], 0, '|', sizeof room);

        strcpy(username, bstr("username"));

        if (havebstr("kick_button")) {
                sprintf(buf, "KICK %s", username);
                serv_puts(buf);
                serv_getln(buf, sizeof buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
				_("<B><I>User %s kicked out of room %s.</I></B>\n"), 
                                username, room);
                }
        }

	if (havebstr("invite_button")) {
                sprintf(buf, "INVT %s", username);
                serv_puts(buf);
                serv_getln(buf, sizeof buf);

                if (buf[0] != '2') {
                        strcpy(WC->ImportantMessage, &buf[4]);
                } else {
                        sprintf(WC->ImportantMessage,
                        	_("<B><I>User %s invited to room %s.</I></B>\n"), 
                                username, room);
                }
        }

	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
}



/*
 * Display form for Invite, Kick, and show Who Knows a room
 */
void display_whok(void)
{
        char buf[SIZ], room[SIZ], username[SIZ];

        serv_puts("GETR");
        serv_getln(buf, sizeof buf);

        if (buf[0] != '2') {
		escputs(&buf[4]);
		return;
        }
        extract_token(room, &buf[4], 0, '|', sizeof room);

        
	wc_printf("<table border=0 CELLSPACING=10><tr VALIGN=TOP><td>");
	wc_printf(_("The users listed below have access to this room.  "
		  "To remove a user from the access list, select the user "
		  "name from the list and click 'Kick'."));
	wc_printf("<br /><br />");
	
        wc_printf("<CENTER><form method=\"POST\" action=\"do_invt_kick\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
        wc_printf("<select NAME=\"username\" SIZE=\"10\" style=\"width:100%%\">\n");
        serv_puts("WHOK");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(username, buf, 0, '|', sizeof username);
                        wc_printf("<OPTION>");
                        escputs(username);
                        wc_printf("\n");
                }
        }
        wc_printf("</select><br />\n");

        wc_printf("<input type=\"submit\" name=\"kick_button\" value=\"%s\">", _("Kick"));
        wc_printf("</form></CENTER>\n");

	wc_printf("</td><td>");
	wc_printf(_("To grant another user access to this room, enter the "
		  "user name in the box below and click 'Invite'."));
	wc_printf("<br /><br />");

        wc_printf("<CENTER><form method=\"POST\" action=\"do_invt_kick\">\n");
	wc_printf("<input type=\"hidden\" NAME=\"tab\" VALUE=\"access\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
        wc_printf(_("Invite:"));
	wc_printf(" ");
        wc_printf("<input type=\"text\" name=\"username\" id=\"username_id\" style=\"width:100%%\"><br />\n"
        	"<input type=\"hidden\" name=\"invite_button\" value=\"Invite\">"
        	"<input type=\"submit\" value=\"%s\">"
		"</form></CENTER>\n", _("Invite"));
	/* Pop open an address book -- begin **/
	wc_printf(
		"<a href=\"javascript:PopOpenAddressBook('username_id|%s');\" "
		"title=\"%s\">"
		"<img align=middle border=0 width=24 height=24 src=\"static/viewcontacts_24x.gif\">"
		"&nbsp;%s</a>",
		_("User"), 
		_("Users"), _("Users")
		);
	/* Pop open an address book -- end **/

	wc_printf("</td></tr></table>\n");
	address_book_popup();
        wDumpContent(1);
}

/*
 * support function for entroom() -- sets the default view 
 */
void er_set_default_view(int newview) {

	char buf[SIZ];

	char rm_name[SIZ];
	char rm_pass[SIZ];
	char rm_dir[SIZ];
	int rm_bits1;
	int rm_floor;
	int rm_listorder;
	int rm_bits2;

	serv_puts("GETR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') return;

	extract_token(rm_name, &buf[4], 0, '|', sizeof rm_name);
	extract_token(rm_pass, &buf[4], 1, '|', sizeof rm_pass);
	extract_token(rm_dir, &buf[4], 2, '|', sizeof rm_dir);
	rm_bits1 = extract_int(&buf[4], 3);
	rm_floor = extract_int(&buf[4], 4);
	rm_listorder = extract_int(&buf[4], 5);
	rm_bits2 = extract_int(&buf[4], 7);

	serv_printf("SETR %s|%s|%s|%d|0|%d|%d|%d|%d",
		    rm_name, rm_pass, rm_dir, rm_bits1, rm_floor,
		    rm_listorder, newview, rm_bits2
		);
	serv_getln(buf, sizeof buf);
}



/*
 * Create a new room
 */
void entroom(void)
{
	char buf[SIZ];
	const StrBuf *er_name;
	const StrBuf *er_type;
	const StrBuf *er_password;
	int er_floor;
	int er_num_type;
	int er_view;
	wcsession *WCC = WC;

	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
		       _("Cancelled.  No new room was created."));
		display_main_menu();
		return;
	}
	er_name = sbstr("er_name");
	er_type = sbstr("type");
	er_password = sbstr("er_password");
	er_floor = ibstr("er_floor");
	er_view = ibstr("er_view");

	er_num_type = 0;
	if (!strcmp(ChrPtr(er_type), "hidden"))
		er_num_type = 1;
	else if (!strcmp(ChrPtr(er_type), "passworded"))
		er_num_type = 2;
	else if (!strcmp(ChrPtr(er_type), "invonly"))
		er_num_type = 3;
	else if (!strcmp(ChrPtr(er_type), "personal"))
		er_num_type = 4;

	serv_printf("CRE8 1|%s|%d|%s|%d|%d|%d", 
		    ChrPtr(er_name), 
		    er_num_type, 
		    ChrPtr(er_password), 
		    er_floor, 
		    0, 
		    er_view);

	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WCC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	/** TODO: Room created, now update the left hand icon bar for this user */
	burn_folder_cache(0);	/* burn the old folder cache */

	gotoroom(er_name);

	serv_printf("VIEW %d", er_view);
	serv_getln(buf, sizeof buf);
	WCC->CurRoom.view = er_view;

	if ( (WCC != NULL) && ( (WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) )  {
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
	} else {
        	do_change_view(er_view);                /* Now go there */
	}

}


/**
 * \brief goto a private room
 */
void goto_private(void)
{
	char hold_rm[SIZ];
	char buf[SIZ];

	if (!havebstr("ok_button")) {
		display_main_menu();
		return;
	}
	FlushRoomlist();
	strcpy(hold_rm, ChrPtr(WC->CurRoom.name));
	serv_printf("GOTO %s|%s",
		    bstr("gr_name"),
		    bstr("gr_pass"));
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		smart_goto(sbstr("gr_name"));
		return;
	}
	if (!strncmp(buf, "540", 3)) {
		DoTemplate(HKEY("room_display_private"), NULL, &NoCtx);
		return;
	}
	output_headers(1, 1, 1, 0, 0, 0);
	wc_printf("%s\n", &buf[4]);
	wDumpContent(1);
	return;
}



/**
 * \brief zap a room
 */
void zap(void)
{
	char buf[SIZ];
	StrBuf *final_destination;

	/**
	 * If the forget-room routine fails for any reason, we fall back
	 * to the current room; otherwise, we go to the Lobby
	 */
	final_destination = NewStrBufDup(WC->CurRoom.name);

	if (havebstr("ok_button")) {
		serv_printf("GOTO %s", ChrPtr(WC->CurRoom.name));
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			serv_puts("FORG");
			serv_getln(buf, sizeof buf);
			if (buf[0] == '2') {
				FlushStrBuf(final_destination);
				StrBufAppendBufPlain(final_destination, HKEY("_BASEROOM_"), 0);
			}
		}
		FlushRoomlist ();
	}
	smart_goto(final_destination);
	FreeStrBuf(&final_destination);
}



/**
 * \brief Delete the current room
 */
void delete_room(void)
{
	char buf[SIZ];

	
	serv_puts("KILL 1");
	serv_getln(buf, sizeof buf);
	burn_folder_cache(0);	/* Burn the cahce of known rooms to update the icon bar */
	FlushRoomlist ();
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	} else {
		StrBuf *Buf;
		
		Buf = NewStrBufPlain(HKEY("_BASEROOM_"));
		smart_goto(Buf);
		FreeStrBuf(&Buf);
	}
}



/**
 * \brief Perform changes to a room's network configuration
 */
void netedit(void) {
	FILE *fp;
	char buf[SIZ];
	char line[SIZ];
	char cmpa0[SIZ];
	char cmpa1[SIZ];
	char cmpb0[SIZ];
	char cmpb1[SIZ];
	int i, num_addrs;
	/*/ TODO: do line dynamic! */
	if (havebstr("line_pop3host")) {
		strcpy(line, bstr("prefix"));
		strcat(line, bstr("line_pop3host"));
		strcat(line, "|");
		strcat(line, bstr("line_pop3user"));
		strcat(line, "|");
		strcat(line, bstr("line_pop3pass"));
		strcat(line, "|");
		strcat(line, ibstr("line_pop3keep") ? "1" : "0" );
		strcat(line, "|");
		sprintf(&line[strlen(line)],"%ld", lbstr("line_pop3int"));
		strcat(line, bstr("suffix"));
	}
	else if (havebstr("line")) {
		strcpy(line, bstr("prefix"));
		strcat(line, bstr("line"));
		strcat(line, bstr("suffix"));
	}
	else {
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}


	fp = tmpfile();
	if (fp == NULL) {
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}

	serv_puts("GNET");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
		fclose(fp);
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}

	/** This loop works for add *or* remove.  Spiffy, eh? */
	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		extract_token(cmpa0, buf, 0, '|', sizeof cmpa0);
		extract_token(cmpa1, buf, 1, '|', sizeof cmpa1);
		extract_token(cmpb0, line, 0, '|', sizeof cmpb0);
		extract_token(cmpb1, line, 1, '|', sizeof cmpb1);
		if ( (strcasecmp(cmpa0, cmpb0)) 
		     || (strcasecmp(cmpa1, cmpb1)) ) {
			fprintf(fp, "%s\n", buf);
		}
	}

	rewind(fp);
	serv_puts("SNET");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		fclose(fp);
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}

	while (fgets(buf, sizeof buf, fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		serv_puts(buf);
	}

	if (havebstr("add_button")) {
		num_addrs = num_tokens(bstr("line"), ',');
		if (num_addrs < 2) {
			/* just adding one node or address */
			serv_puts(line);
		}
		else {
			/* adding multiple addresses separated by commas */
			for (i=0; i<num_addrs; ++i) {
				strcpy(line, bstr("prefix"));
				extract_token(buf, bstr("line"), i, ',', sizeof buf);
				striplt(buf);
				strcat(line, buf);
				strcat(line, bstr("suffix"));
				serv_puts(line);
			}
		}
	}

	serv_puts("000");
	fclose(fp);
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
}



/**
 * \brief Convert a room name to a folder-ish-looking name.
 * \param folder the folderish name
 * \param room the room name
 * \param floor the floor name
 * \param is_mailbox is it a mailbox?
 */
void room_to_folder(char *folder, char *room, int floor, int is_mailbox)
{
	int i, len;

	/**
	 * For mailboxes, just do it straight...
	 */
	if (is_mailbox) {
		sprintf(folder, "My folders|%s", room);
	}

	/**
	 * Otherwise, prefix the floor name as a "public folders" moniker
	 */
	else {
		if (floor > MAX_FLOORS) {
			wc_backtrace ();
			sprintf(folder, "%%%%%%|%s", room);
		}
		else {
			sprintf(folder, "%s|%s", floorlist[floor], room);
		}
	}

	/**
	 * Replace "\" characters with "|" for pseudo-folder-delimiting
	 */
	len = strlen (folder);
	for (i=0; i<len; ++i) {
		if (folder[i] == '\\') folder[i] = '|';
	}
}




/**
 * \brief Back end for change_view()
 * \param newview set newview???
 */
void do_change_view(int newview) {
	char buf[SIZ];

	serv_printf("VIEW %d", newview);
	serv_getln(buf, sizeof buf);
	WC->CurRoom.view = newview;
	smart_goto(WC->CurRoom.name);
}



/**
 * \brief Change the view for this room
 */
void change_view(void) {
	int view;

	view = lbstr("view");
	do_change_view(view);
}

/**
 * \brief Burn the cached folder list.  
 * \param age How old the cahce needs to be before we burn it.
 */

void burn_folder_cache(time_t age)
{
	/** If our cached folder list is very old, burn it. */
	if (WC->cache_fold != NULL) {
		if ((time(NULL) - WC->cache_timestamp) > age) {
			free(WC->cache_fold);
			WC->cache_fold = NULL;
		}
	}
}



/**
 * \brief Do either a known rooms list or a folders list, depending on the
 * user's preference
 */
void knrooms(void)
{
	StrBuf *ListView = NULL;

	/** Determine whether the user is trying to change views */
	if (havebstr("view")) {
		ListView = NewStrBufDup(SBSTR("view"));
		set_preference("roomlistview", ListView, 1);
	}
	/** Sanitize the input so its safe */
	if((get_preference("roomlistview", &ListView) != 0)||
	   ((strcasecmp(ChrPtr(ListView), "folders") != 0) &&
	    (strcasecmp(ChrPtr(ListView), "table") != 0))) 
	{
		if (ListView == NULL) {
			ListView = NewStrBufPlain(HKEY("rooms"));
			set_preference("roomlistview", ListView, 0);
			ListView = NULL;
		}
		else {
			ListView = NewStrBufPlain(HKEY("rooms"));
			set_preference("roomlistview", ListView, 0);
			ListView = NULL;
		}
	}
	FreeStrBuf(&ListView);
	url_do_template();
}



/**
 * \brief Set the message expire policy for this room and/or floor
 */
void set_room_policy(void) {
	char buf[SIZ];

	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
		       _("Cancelled.  Changes were not saved."));
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}

	serv_printf("SPEX roompolicy|%d|%d", ibstr("roompolicy"), ibstr("roomvalue"));
	serv_getln(buf, sizeof buf);
	strcpy(WC->ImportantMessage, &buf[4]);

	if (WC->axlevel >= 6) {
		strcat(WC->ImportantMessage, "<br />\n");
		serv_printf("SPEX floorpolicy|%d|%d", ibstr("floorpolicy"), ibstr("floorvalue"));
		serv_getln(buf, sizeof buf);
		strcat(WC->ImportantMessage, &buf[4]);
	}
	FlushRoomlist();
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
}

void tmplput_RoomName(StrBuf *Target, WCTemplputParams *TP)
{
	StrBufAppendTemplate(Target, TP, WC->CurRoom.name, 0);
}

void dotgoto(void) {
	if (!havebstr("room")) {
		readloop(readnew, eUseDefault);
		return;
	}
	if (WC->CurRoom.view != VIEW_MAILBOX) {	/* dotgoto acts like dotskip when we're in a mailbox view */
		slrp_highest();
	}
	smart_goto(sbstr("room"));
}



void tmplput_current_room(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if (WCC != NULL)
		StrBufAppendTemplate(Target, TP, 
				     WCC->CurRoom.name, 
				     0); 
}

void tmplput_roombanner(StrBuf *Target, WCTemplputParams *TP)
{
	wc_printf("<div id=\"banner\">\n");
	embed_room_banner();
	wc_printf("</div>\n");
}


void tmplput_ungoto(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if ((WCC!=NULL) && 
	    (!IsEmptyStr(WCC->ugname)))
		StrBufAppendBufPlain(Target, WCC->ugname, -1, 0);
}

int ConditionalRoomAide(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return (WCC != NULL)? 
		((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) : 0;
}

int ConditionalRoomAcessDelete(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	return (WCC == NULL)? 0 : 
		( ((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) ||
		   (WCC->CurRoom.is_inbox) || 
		   (WCC->CurRoom.QRFlags2 & QR2_COLLABDEL) );
}

int ConditionalHaveUngoto(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	
	return ((WCC!=NULL) && 
		(!IsEmptyStr(WCC->ugname)) && 
		(strcasecmp(WCC->ugname, ChrPtr(WCC->CurRoom.name)) == 0));
}


int ConditionalRoomHas_UAFlag(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	long UA_CheckFlag;
		
	UA_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (UA_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"UA_*\"- defines or an integer flag 0 is invalid!");

	return ((Folder->RAFlags & UA_CheckFlag) != 0);
}



int ConditionalCurrentRoomHas_QRFlag(StrBuf *Target, WCTemplputParams *TP)
{
	long QR_CheckFlag;
	wcsession *WCC = WC;
	
	QR_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR*\"- defines or an integer flag 0 is invalid!");
	
	if (WCC == NULL)
		return 0;

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (WCC->CurRoom.QRFlags & QR_CheckFlag) != 0;
	else
		return (WCC->CurRoom.QRFlags & QR_CheckFlag) == QR_CheckFlag;
}

int ConditionalRoomHas_QRFlag(StrBuf *Target, WCTemplputParams *TP)
{
	long QR_CheckFlag;
	folder *Folder = (folder *)(TP->Context);

	QR_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR*\"- defines or an integer flag 0 is invalid!");

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (Folder->QRFlags & QR_CheckFlag) != 0;
	else
		return (Folder->QRFlags & QR_CheckFlag) == QR_CheckFlag;
}


int ConditionalCurrentRoomHas_QRFlag2(StrBuf *Target, WCTemplputParams *TP)
{
	long QR2_CheckFlag;
	wcsession *WCC = WC;
	
	QR2_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR2_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR2*\"- defines or an integer flag 0 is invalid!");

	
	if (WCC == NULL)
		return 0;

	if ((TP->Tokens->Params[2]->MaskBy == eOR) ||
	    (TP->Tokens->Params[2]->MaskBy == eNO))
		return (WCC->CurRoom.QRFlags2 & QR2_CheckFlag) != 0;
	else
		return (WCC->CurRoom.QRFlags2 & QR2_CheckFlag) == QR2_CheckFlag;
}

int ConditionalRoomHas_QRFlag2(StrBuf *Target, WCTemplputParams *TP)
{
	long QR2_CheckFlag;
	folder *Folder = (folder *)(TP->Context);

	QR2_CheckFlag = GetTemplateTokenNumber(Target, TP, 2, 0);
	if (QR2_CheckFlag == 0)
		LogTemplateError(Target, "Conditional", ERR_PARM1, TP,
				 "requires one of the #\"QR2*\"- defines or an integer flag 0 is invalid!");
	return ((Folder->QRFlags2 & QR2_CheckFlag) != 0);
}


int ConditionalHaveRoomeditRights(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	return ( (WCC!= NULL) && 
		 ((WCC->axlevel >= 6) || 
		  ((WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) ||
		  (WCC->CurRoom.is_inbox) ));
}

int ConditionalIsRoomtype(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;

	if ((WCC == NULL) ||
	    (TP->Tokens->nParameters < 3))
	{
		return ((WCC->CurRoom.view < VIEW_BBS) || 
			(WCC->CurRoom.view > VIEW_MAX));
	}

	return WCC->CurRoom.view == GetTemplateTokenNumber(Target, TP, 2, VIEW_BBS);
}


HashList *GetWhoKnowsHash(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	StrBuf *Line;
	StrBuf *Token;
	long State;
	HashList *Whok = NULL;
	int Done = 0;
	int n;

	serv_puts("WHOK");
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		Whok = NewHash(1, Flathash);
		while(!Done && StrBuf_ServGetln(Line))
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000")) 
			{
				Done = 1;
			}
			else
			{
			
				const char *Pos = NULL;
				Token = NewStrBufPlain (NULL, StrLength(Line));
				StrBufExtract_NextToken(Token, Line, &Pos, '|');

				Put(Whok, 
				    IKEY(n),
				    Token, 
				    HFreeStrBuf);
			}
	}
	else if (State == 550)
		StrBufAppendBufPlain(WCC->ImportantMsg,
				     _("Higher access is required to access this function."), -1, 0);


	FreeStrBuf(&Line);
	return Whok;
}

void _FlushRoomList(wcsession *WCC)
{
	free_march_list(WCC);
	DeleteHash(&WCC->Floors);
	DeleteHash(&WCC->Rooms);
	DeleteHash(&WCC->FloorsByName);
}

void FlushRoomlist(void)
{
	wcsession *WCC = WC;
	_FlushRoomList(WCC);
}


void 
InitModule_ROOMOPS
(void)
{
	initialize_viewdefs();
	RegisterPreference("roomlistview",
                           _("Room list view"),
                           PRF_STRING,
                           NULL);
        RegisterPreference("emptyfloors", _("Show empty floors"), PRF_YESNO, NULL);

	RegisterNamespace("ROOMNAME", 0, 1, tmplput_RoomName, NULL, CTX_NONE);


	WebcitAddUrlHandler(HKEY("knrooms"), "", 0, knrooms, 0);
	WebcitAddUrlHandler(HKEY("dotgoto"), "", 0, dotgoto, NEED_URL);
	WebcitAddUrlHandler(HKEY("dotskip"), "", 0, dotskip, NEED_URL);

	WebcitAddUrlHandler(HKEY("goto_private"), "", 0, goto_private, NEED_URL);
	WebcitAddUrlHandler(HKEY("zap"), "", 0, zap, 0);
	WebcitAddUrlHandler(HKEY("entroom"), "", 0, entroom, 0);
	WebcitAddUrlHandler(HKEY("do_invt_kick"), "", 0, do_invt_kick, 0);
	WebcitAddUrlHandler(HKEY("display_editroom"), "", 0, display_editroom, 0);
	WebcitAddUrlHandler(HKEY("netedit"), "", 0, netedit, 0);
	WebcitAddUrlHandler(HKEY("editroom"), "", 0, editroom, 0);
	WebcitAddUrlHandler(HKEY("delete_room"), "", 0, delete_room, 0);
	WebcitAddUrlHandler(HKEY("set_room_policy"), "", 0, set_room_policy, 0);
	WebcitAddUrlHandler(HKEY("changeview"), "", 0, change_view, 0);
	WebcitAddUrlHandler(HKEY("toggle_self_service"), "", 0, toggle_self_service, 0);
	RegisterNamespace("ROOMBANNER", 0, 1, tmplput_roombanner, NULL, CTX_NONE);

	RegisterConditional(HKEY("COND:ROOM:TYPE_IS"), 0, ConditionalIsRoomtype, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:FLAG:QR"), 0, ConditionalCurrentRoomHas_QRFlag, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAG:QR"), 0, ConditionalRoomHas_QRFlag, CTX_ROOMS);

	RegisterConditional(HKEY("COND:THISROOM:FLAG:QR2"), 0, ConditionalCurrentRoomHas_QRFlag2, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:FLAG:QR2"), 0, ConditionalRoomHas_QRFlag2, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAG:UA"), 0, ConditionalRoomHas_UAFlag, CTX_ROOMS);

	RegisterIterator("ITERATE:THISROOM:WHO_KNOWS", 0, NULL, GetWhoKnowsHash, NULL, DeleteHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
	RegisterNamespace("THISROOM:MSGS:NEW", 0, 0, tmplput_CurrentRoom_nNewMessages, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:MSGS:TOTAL", 0, 0, tmplput_CurrentRoom_nTotalMessages, NULL, CTX_NONE);

	RegisterNamespace("THISROOM:FLOOR:NAME", 0, 1, tmplput_CurrentRoomFloorName, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:AIDE", 0, 1, tmplput_CurrentRoomAide, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:PASS", 0, 1, tmplput_CurrentRoomPass, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:DIRECTORY", 0, 1, tmplput_CurrentRoomDirectory, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:ORDER", 0, 0, tmplput_CurrentRoomOrder, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:DEFAULT_VIEW", 0, 0, tmplput_CurrentRoomDefView, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:HAVE_VIEW"), 0, ConditionalThisRoomHaveView, CTX_NONE);
	RegisterConditional(HKEY("COND:ALLOWED_DEFAULT_VIEW"), 0, ConditionalIsAllowedDefaultView, CTX_NONE);

	RegisterNamespace("THISROOM:VIEW_STRING", 0, 1, tmplput_CurrentRoomViewString, NULL, CTX_NONE);
	RegisterNamespace("ROOM:VIEW_STRING", 1, 2, tmplput_RoomViewString, NULL, CTX_NONE);

	RegisterNamespace("THISROOM:INFOTEXT", 1, 2, tmplput_CurrentRoomInfoText, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:ORDER"), 0, ConditionalThisRoomOrder, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:DEFAULT_VIEW"), 0, ConditionalThisRoomDefView, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:CURR_VIEW"), 0, ConditionalThisRoomCurrView, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:HAVE_PIC"), 0, ConditionalThisRoomXHavePic, CTX_NONE);
	RegisterConditional(HKEY("COND:THISROOM:HAVE_INFOTEXT"), 0, ConditionalThisRoomXHaveInfoText, CTX_NONE);
	RegisterNamespace("THISROOM:FILES:N", 0, 1, tmplput_CurrentRoomXNFiles, NULL, CTX_NONE);
	RegisterNamespace("THISROOM:FILES:STR", 0, 1, tmplput_CurrentRoomX_FileString, NULL, CTX_NONE);

	REGISTERTokenParamDefine(QR_PERMANENT);
	REGISTERTokenParamDefine(QR_INUSE);
	REGISTERTokenParamDefine(QR_PRIVATE);
	REGISTERTokenParamDefine(QR_PASSWORDED);
	REGISTERTokenParamDefine(QR_GUESSNAME);
	REGISTERTokenParamDefine(QR_DIRECTORY);
	REGISTERTokenParamDefine(QR_UPLOAD);
	REGISTERTokenParamDefine(QR_DOWNLOAD);
	REGISTERTokenParamDefine(QR_VISDIR);
	REGISTERTokenParamDefine(QR_ANONONLY);
	REGISTERTokenParamDefine(QR_ANONOPT);
	REGISTERTokenParamDefine(QR_NETWORK);
	REGISTERTokenParamDefine(QR_PREFONLY);
	REGISTERTokenParamDefine(QR_READONLY);
	REGISTERTokenParamDefine(QR_MAILBOX);
	REGISTERTokenParamDefine(QR2_SYSTEM);
	REGISTERTokenParamDefine(QR2_SELFLIST);
	REGISTERTokenParamDefine(QR2_COLLABDEL);
	REGISTERTokenParamDefine(QR2_SUBJECTREQ);
	REGISTERTokenParamDefine(QR2_SMTP_PUBLIC);
	REGISTERTokenParamDefine(QR2_MODERATED);

	REGISTERTokenParamDefine(UA_KNOWN);
	REGISTERTokenParamDefine(UA_GOTOALLOWED);
	REGISTERTokenParamDefine(UA_HASNEWMSGS);
	REGISTERTokenParamDefine(UA_ZAPPED);
	REGISTERTokenParamDefine(UA_POSTALLOWED);
	REGISTERTokenParamDefine(UA_ADMINALLOWED);
	REGISTERTokenParamDefine(UA_DELETEALLOWED);
	REGISTERTokenParamDefine(UA_ISTRASH);

	REGISTERTokenParamDefine(US_NEEDVALID);
	REGISTERTokenParamDefine(US_PERM);
	REGISTERTokenParamDefine(US_LASTOLD);
	REGISTERTokenParamDefine(US_EXPERT);
	REGISTERTokenParamDefine(US_UNLISTED);
	REGISTERTokenParamDefine(US_NOPROMPT);
	REGISTERTokenParamDefine(US_PROMPTCTL);
	REGISTERTokenParamDefine(US_DISAPPEAR);
	REGISTERTokenParamDefine(US_REGIS);
	REGISTERTokenParamDefine(US_PAGINATOR);
	REGISTERTokenParamDefine(US_INTERNET);
	REGISTERTokenParamDefine(US_FLOORS);
	REGISTERTokenParamDefine(US_COLOR);
	REGISTERTokenParamDefine(US_USER_SET);

	REGISTERTokenParamDefine(VIEW_BBS);
	REGISTERTokenParamDefine(VIEW_MAILBOX);	
	REGISTERTokenParamDefine(VIEW_ADDRESSBOOK);
	REGISTERTokenParamDefine(VIEW_CALENDAR);	
	REGISTERTokenParamDefine(VIEW_TASKS);	
	REGISTERTokenParamDefine(VIEW_NOTES);		
	REGISTERTokenParamDefine(VIEW_WIKI);		
	REGISTERTokenParamDefine(VIEW_CALBRIEF);
	REGISTERTokenParamDefine(VIEW_JOURNAL);
	REGISTERTokenParamDefine(VIEW_BLOG);

	/* GNET types: */
	REGISTERTokenParamDefine(ignet_push_share);
	{ /* these are the parts of an IGNET push config */
		REGISTERTokenParamDefine(GNET_IGNET_NODE);
		REGISTERTokenParamDefine(GNET_IGNET_ROOM);
	}
	REGISTERTokenParamDefine(listrecp);
	REGISTERTokenParamDefine(digestrecp);
	REGISTERTokenParamDefine(pop3client);
	{ /* These are the parts of a pop3 client line... */
		REGISTERTokenParamDefine(GNET_POP3_HOST);
		REGISTERTokenParamDefine(GNET_POP3_USER);
		REGISTERTokenParamDefine(GNET_POP3_DONT_DELETE_REMOTE);
		REGISTERTokenParamDefine(GNET_POP3_INTERVAL);
	}
	REGISTERTokenParamDefine(rssclient);
	REGISTERTokenParamDefine(participate);

	RegisterConditional(HKEY("COND:ROOMAIDE"), 2, ConditionalRoomAide, CTX_NONE);
	RegisterConditional(HKEY("COND:ACCESS:DELETE"), 2, ConditionalRoomAcessDelete, CTX_NONE);

	RegisterConditional(HKEY("COND:UNGOTO"), 0, ConditionalHaveUngoto, CTX_NONE);
	RegisterConditional(HKEY("COND:ROOM:EDITACCESS"), 0, ConditionalHaveRoomeditRights, CTX_NONE);

	RegisterNamespace("CURRENT_ROOM", 0, 1, tmplput_current_room, NULL, CTX_NONE);
	RegisterNamespace("ROOM:UNGOTO", 0, 0, tmplput_ungoto, NULL, CTX_NONE);
	RegisterIterator("FLOORS", 0, NULL, GetFloorListHash, NULL, NULL, CTX_FLOORS, CTX_NONE, IT_NOFLAG);


}


void 
SessionDestroyModule_ROOMOPS
(wcsession *sess)
{
	FlushFolder(&sess->CurRoom);
	if (sess->cache_fold != NULL) {
		free(sess->cache_fold);
	}
	
	_FlushRoomList (sess);
}


/*@}*/
