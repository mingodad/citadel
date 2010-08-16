/*
 * $Id$
 * Lots of different room-related operations.
 */

#include "webcit.h"
#include "webserver.h"

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

ConstStr QRFlagList[] = {
	{HKEY(strof(QR_PERMANENT))},
	{HKEY(strof(QR_INUSE))},
	{HKEY(strof(QR_PRIVATE))},
	{HKEY(strof(QR_PASSWORDED))},
	{HKEY(strof(QR_GUESSNAME))},
	{HKEY(strof(QR_DIRECTORY))},
	{HKEY(strof(QR_UPLOAD))},
	{HKEY(strof(QR_DOWNLOAD))},
	{HKEY(strof(QR_VISDIR))},
	{HKEY(strof(QR_ANONONLY))},
	{HKEY(strof(QR_ANONOPT))},
	{HKEY(strof(QR_NETWORK))},
	{HKEY(strof(QR_PREFONLY))},
	{HKEY(strof(QR_READONLY))},
	{HKEY(strof(QR_MAILBOX))}
};
ConstStr QR2FlagList[] = {
	{HKEY(strof(QR2_SYSTEM))},
	{HKEY(strof(QR2_SELFLIST))},
	{HKEY(strof(QR2_COLLABDEL))},
	{HKEY(strof(QR2_SUBJECTREQ))},
	{HKEY(strof(QR2_SMTP_PUBLIC))},
	{HKEY(strof(QR2_MODERATED))},
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}, 
	{HKEY("")}
};

void DBG_QR(long QR)
{
	int i = 1;
	int j=0;
	StrBuf *QRVec;

	QRVec = NewStrBufPlain(NULL, 256);
	while (i != 0)
	{
		if ((QR & i) != 0) {
			if (StrLength(QRVec) > 0)
				StrBufAppendBufPlain(QRVec, HKEY(" | "), 0);
			StrBufAppendBufPlain(QRVec, CKEY(QRFlagList[j]), 0);
		}
		i = i << 1;
		j++;
	}
	lprintf(9, "DBG: QR-Vec [%ld] [%s]\n", QR, ChrPtr(QRVec));
	FreeStrBuf(&QRVec);
}



void DBG_QR2(long QR2)
{
	int i = 1;
	int j=0;
	StrBuf *QR2Vec;

	QR2Vec = NewStrBufPlain(NULL, 256);
	while (i != 0)
	{
		if ((QR2 & i) != 0) {
			if (StrLength(QR2Vec) > 0)
				StrBufAppendBufPlain(QR2Vec, HKEY(" | "), 0);
			StrBufAppendBufPlain(QR2Vec, CKEY(QR2FlagList[j]), 0);
		}
		i = i << 1;
		j++;
	}
	lprintf(9, "DBG: QR2-Vec [%ld] [%s]\n", QR2, ChrPtr(QR2Vec));
	FreeStrBuf(&QR2Vec);
}


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

int SaveRoomAide(folder *Room)
{
	StrBuf *Buf;
	Buf = NewStrBuf ();
	serv_printf("SETA %s", ChrPtr(Room->RoomAide));
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		StrBufCutLeft(Buf, 4);
		AppendImportantMessage (SKEY(Buf));
		FreeStrBuf(&Buf);
		return 0;
	}
	FreeStrBuf(&Buf);
	return 1;
}

void tmplput_CurrentRoomFloorName(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	folder *Folder = &WCC->CurRoom;
	const Floor *pFloor;

	if (Folder == NULL)
		return;

	pFloor = Folder->Floor;
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

int GetCurrentRoomFlags(folder *Room)
{
	StrBuf *Buf;

	Buf = NewStrBuf();
	serv_puts("GETR");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		FlushStrBuf(Room->XAPass);
		FlushStrBuf(Room->Directory);
		StrBufCutLeft(Buf, 4);
		AppendImportantMessage (SKEY(Buf));
		FreeStrBuf(&Buf);
		return 0;
	} else {
		const char *Pos;

		Pos = ChrPtr(Buf) + 4;

		FreeStrBuf(&Room->XAPass);
		FreeStrBuf(&Room->Directory);

		Room->XAPass = NewStrBufPlain (NULL, StrLength (Buf));
		Room->Directory = NewStrBufPlain (NULL, StrLength (Buf));

		FreeStrBuf(&Room->name);
		Room->name = NewStrBufPlain(NULL, StrLength(Buf));
		StrBufExtract_NextToken(Room->name, Buf, &Pos, '|'); 
					
		StrBufExtract_NextToken(Room->XAPass, Buf, &Pos, '|'); 
		StrBufExtract_NextToken(Room->Directory, Buf, &Pos, '|'); 
		
		Room->QRFlags = StrBufExtractNext_long(Buf, &Pos, '|');
		Room->floorid = StrBufExtractNext_long(Buf, &Pos, '|');
		Room->Order = StrBufExtractNext_long(Buf, &Pos, '|');
		Room->defview = StrBufExtractNext_long(Buf, &Pos, '|');
		Room->QRFlags2 = StrBufExtractNext_long(Buf, &Pos, '|');
		FreeStrBuf (&Buf);
		Room->XALoaded = 1;
		return 1;
	}
}


int SetCurrentRoomFlags(folder *Room)
{
	StrBuf *Buf;

	Buf = NewStrBuf();
	DBG_QR(Room->QRFlags);
	DBG_QR2(Room->QRFlags2);

	serv_printf("SETR %s|%s|%s|%ld|%d|%d|%ld|%ld|%ld",
		    ChrPtr(Room->name),
		    ChrPtr(Room->XAPass),
		    ChrPtr(Room->Directory),
		    Room->QRFlags, 
		    Room->BumpUsers,
		    Room->floorid, 
		    Room->Order,
		    Room->defview,
		    Room->QRFlags2);

	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		StrBufCutLeft(Buf, 4);
		AppendImportantMessage (SKEY(Buf));
		FreeStrBuf(&Buf);
		return 0;
	} else {
		FreeStrBuf(&Buf);
		return 1;
	}
}

void LoadRoomXA (void)
{
	wcsession *WCC = WC;
		
	if (WCC->CurRoom.XALoaded)
		return;

	GetCurrentRoomFlags(&WCC->CurRoom);
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



/*
 * Set/clear/read the "self-service list subscribe" flag for a room
 * 
 * set newval to 0 to clear, 1 to set, any other value to leave unchanged.
 * returns the new value.
 */

int self_service(int newval) {
	int current_value = 0;
	wcsession *WCC = WC;

	if (GetCurrentRoomFlags (&WCC->CurRoom) == 0)
	{
		return 0;
	}

	if ((WCC->CurRoom.QRFlags2 & QR2_SELFLIST) != 0) {
		current_value = 1;
	}
	else {
		current_value = 0;
	}

	if (newval == 1) {
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 | QR2_SELFLIST;
	}
	else if (newval == 0) {
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 & ~QR2_SELFLIST;
	}
	else {
		return(current_value);
	}

	if (newval != current_value) {
		SetCurrentRoomFlags(&WCC->CurRoom);
	}

	return(newval);

}



/* 
 * Toggle self-service list subscription
 */
void toggle_self_service(void) {
	wcsession *WCC = WC;

	if (GetCurrentRoomFlags (&WCC->CurRoom) == 0)
		return;

	if (yesbstr("QR2_SelfList")) 
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 | QR2_SELFLIST;
	else 
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 & ~QR2_SELFLIST;

	if (yesbstr("QR2_SMTP_PUBLIC")) 
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 | QR2_SMTP_PUBLIC;
	else
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 & ~QR2_SMTP_PUBLIC;

	if (yesbstr("QR2_Moderated")) 
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 | QR2_MODERATED;
	else
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 & ~QR2_MODERATED;
	if (yesbstr("QR2_SubsOnly")) 
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 | QR2_SMTP_PUBLIC;
	else
		WCC->CurRoom.QRFlags2 = WCC->CurRoom.QRFlags2 & ~QR2_SMTP_PUBLIC;

	SetCurrentRoomFlags (&WCC->CurRoom);
	
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
}



/*
 * save new parameters for a room
 */
void editroom(void)
{
	wcsession *WCC = WC;
	const StrBuf *Ptr;
	const StrBuf *er_name;
	const StrBuf *er_password;
	const StrBuf *er_dirname;
	const StrBuf *er_roomaide;
	unsigned er_flags;
	unsigned er_flags2;
	int succ1, succ2;

	if (!havebstr("ok_button")) {
		strcpy(WC->ImportantMessage,
		       _("Cancelled.  Changes were not saved."));
		http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
		return;
	}
	if (GetCurrentRoomFlags (&WCC->CurRoom) == 0)
		return;

	LoadRoomAide();

	er_flags = WCC->CurRoom.QRFlags;
	er_flags &= !(QR_PRIVATE | QR_PASSWORDED | QR_GUESSNAME);

	er_flags2 = WCC->CurRoom.QRFlags2;

	Ptr = sbstr("type");
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

	er_name     = sbstr("er_name");
	er_dirname  = sbstr("er_dirname");
	er_roomaide = sbstr("er_roomaide");
	er_password = sbstr("er_password");

	FlushStrBuf(WCC->CurRoom.name);
	StrBufAppendBuf(WCC->CurRoom.name, er_name, 0);

	FlushStrBuf(WCC->CurRoom.Directory);
	StrBufAppendBuf(WCC->CurRoom.Directory, er_dirname, 0);

	FlushStrBuf(WCC->CurRoom.RoomAide);
	StrBufAppendBuf(WCC->CurRoom.RoomAide, er_roomaide, 0);

	FlushStrBuf(WCC->CurRoom.XAPass);
	StrBufAppendBuf(WCC->CurRoom.XAPass, er_password, 0);

	WCC->CurRoom.BumpUsers = yesbstr("bump");

	WCC->CurRoom.floorid = ibstr("er_floor");

	succ1 = SetCurrentRoomFlags(&WCC->CurRoom);

	succ2 = SaveRoomAide (&WCC->CurRoom);
	
	if (succ1 + succ2 == 0)
		AppendImportantMessage (_("Your changes have been saved."), -1);
	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
	return;
}


/*
 * Display form for Invite, Kick, and show Who Knows a room
 */
void do_invt_kick(void) 
{
	StrBuf *Buf, *User;
	const StrBuf *UserNames;
	int Kick, Invite;
	wcsession *WCC = WC;


	if (GetCurrentRoomFlags(&WCC->CurRoom) == 1)
	{
		const char *Pos;
		UserNames = sbstr("username");
		Kick = havebstr("kick_button");
		Invite = havebstr("invite_button");

		User = NewStrBufPlain(NULL, StrLength(UserNames));
		Buf = NewStrBuf();
		
		Pos = ChrPtr(UserNames);
		while (Pos != StrBufNOTNULL)
		{
			StrBufExtract_NextToken(User, UserNames, &Pos, ',');
			StrBufTrim(User);
			if ((StrLength(User) > 0) && (Kick))
			{
				serv_printf("KICK %s", ChrPtr(User));
				StrBuf_ServGetln(Buf);
				if (GetServerStatus(Buf, NULL) != 2) {
					StrBufCutLeft(Buf, 4);
					AppendImportantMessage(SKEY(Buf));
				} else {
					StrBufPrintf(Buf, 
						     _("User '%s' kicked out of room '%s'."), 
						     ChrPtr(User), 
						     ChrPtr(WCC->CurRoom.name)
						);
					AppendImportantMessage(SKEY(Buf));
				}
			}
			else if ((StrLength(User) > 0) && (Invite))
			{
				serv_printf("INVT %s", ChrPtr(User));
				StrBuf_ServGetln(Buf);
				if (GetServerStatus(Buf, NULL) != 2) {
					StrBufCutLeft(Buf, 4);
					AppendImportantMessage(SKEY(Buf));
				} else {
					StrBufPrintf(Buf, 
						     _("User '%s' invited to room '%s'."), 
						     ChrPtr(User), 
						     ChrPtr(WCC->CurRoom.name)
						);
					AppendImportantMessage(SKEY(Buf));
				}
			}
                }
        }

	http_transmit_thing(ChrPtr(do_template("room_edit", NULL)), 0);
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
	StrBuf *Buf;
	const StrBuf *gr_name;
	long err;

	if (!havebstr("ok_button")) {
		display_main_menu();
		return;
	}
	gr_name = sbstr("gr_name");
	Buf = NewStrBuf();
	strcpy(hold_rm, ChrPtr(WC->CurRoom.name));
	serv_printf("GOTO %s|%s",
		    ChrPtr(gr_name),
		    bstr("gr_pass"));
	StrBuf_ServGetln(Buf);
	if  (GetServerStatus(Buf, &err) == 2) {
		FlushRoomlist();
		smart_goto(gr_name);
		FreeStrBuf(&Buf);
		return;
	}
	if (err == 540) {
		DoTemplate(HKEY("room_display_private"), NULL, &NoCtx);
		FreeStrBuf(&Buf);
		return;
	}
	StrBufCutLeft(Buf, 4);
	AppendImportantMessage (SKEY(Buf));
	Buf = NewStrBufPlain(HKEY("_BASEROOM_"));
	smart_goto(Buf);
	FreeStrBuf(&Buf);
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

	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	} else {
		StrBuf *Buf;
		
		FlushRoomlist ();
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
	ReloadCurrentRoom();
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
				n++;
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
	FlushFolder(&WCC->CurRoom);
}

void ReloadCurrentRoom(void)
{
	wcsession *WCC = WC;
	StrBuf *CurRoom;

	CurRoom = WCC->CurRoom.name;
	WCC->CurRoom.name = NULL;
	_FlushRoomList(WCC);
	gotoroom(CurRoom);
	FreeStrBuf(&CurRoom);
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
	_FlushRoomList (sess);
}


/*@}*/
