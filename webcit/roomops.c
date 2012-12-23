/*
 * Lots of different room-related operations.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"

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

void _DBG_QR(long QR)
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
	syslog(9, "DBG: QR-Vec [%ld] [%s]\n", QR, ChrPtr(QRVec));
	FreeStrBuf(&QRVec);
}



void _DBG_QR2(long QR2)
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
	syslog(9, "DBG: QR2-Vec [%ld] [%s]\n", QR2, ChrPtr(QR2Vec));
	FreeStrBuf(&QR2Vec);
}











/*******************************************************************************
 ***************************** Goto Commands ***********************************
 ******************************************************************************/

void dotskip(void) {
	smart_goto(sbstr("room"));
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

/*
 * goto next room
 */
void smart_goto(const StrBuf *next_room) {
	gotoroom(next_room);
	readloop(readnew, eUseDefault);
}

/*
 * goto a private room
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

/*
 * back end routine to take the session to a new room
 */
long gotoroom(const StrBuf *gname)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	static long ls = (-1L);
	long err = 0;
	int room_name_supplied = 0;
	int is_baseroom = 0;

	/* store ungoto information */
	if (StrLength(gname) > 0) {
		room_name_supplied = 1;
	}
	if (room_name_supplied) {
		strcpy(WCC->ugname, ChrPtr(WCC->CurRoom.name));
		if (!strcasecmp(ChrPtr(gname), "_BASEROOM_")) {
			is_baseroom = 1;
		}
	}
	WCC->uglsn = ls;
	Buf = NewStrBuf();

	/* move to the new room */
	if (room_name_supplied) {
		serv_printf("GOTO %s", ChrPtr(gname));
	}
	else {
		/* or just refresh the current state... */
		serv_printf("GOTO 00000000000000000000");
	}
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

	if (room_name_supplied) {
		remove_march(WCC->CurRoom.name);
		if (is_baseroom) {
			remove_march(gname);
		}
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

	StrBufExtract_NextToken(room->name, Line, &Pos, '|');

	room->nNewMessages = StrBufExtractNext_long(Line, &Pos, '|'); 
	if (room->nNewMessages > 0)
		room->RAFlags |= UA_HASNEWMSGS;

	room->nTotalMessages = StrBufExtractNext_long(Line, &Pos, '|');

	room->ShowInfo =  StrBufExtractNext_long(Line, &Pos, '|');
	
	room->QRFlags = StrBufExtractNext_long(Line, &Pos, '|');

	DBG_QR(room->QRFlags);

	room->HighestRead = StrBufExtractNext_long(Line, &Pos, '|');
	room->LastMessageRead = StrBufExtractNext_long(Line, &Pos, '|');

	room->is_inbox = StrBufExtractNext_long(Line, &Pos, '|');

	flag = StrBufExtractNext_long(Line, &Pos, '|');
	if (WCC->is_aide || flag) {
		room->RAFlags |= UA_ADMINALLOWED;
	}

	room->UsersNewMAilboxMessages = StrBufExtractNext_long(Line, &Pos, '|');

	room->floorid = StrBufExtractNext_int(Line, &Pos, '|');

	room->view = StrBufExtractNext_long(Line, &Pos, '|');

	room->defview = StrBufExtractNext_long(Line, &Pos, '|');

	flag = StrBufExtractNext_long(Line, &Pos, '|');
	if (flag)
		room->RAFlags |= UA_ISTRASH;

	room->QRFlags2 = StrBufExtractNext_long(Line, &Pos, '|');
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

/*
 * Delete the current room
 */
void delete_room(void)
{
	StrBuf *Line = NewStrBuf();
	const StrBuf *GoBstr;
	
	GoBstr = sbstr("go");

	if (GoBstr != NULL)
	{
		if (gotoroom(GoBstr) == 200)
		{
			serv_puts("KILL 1");
			StrBuf_ServGetln(Line);
			if (GetServerStatusMsg(Line, NULL, 1, 2) == 2) {
				StrBuf *Buf;
				
				FlushRoomlist ();
				Buf = NewStrBufPlain(HKEY("_BASEROOM_"));
				smart_goto(Buf);
				FreeStrBuf(&Buf);
				FreeStrBuf(&Line);
				return;
			}
		}
	}
	display_main_menu();
	FreeStrBuf(&Line);
}

/*
 * zap a room
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


/*
 * mark all messages in current room as having been read
 */
void slrp_highest(void)
{
	char buf[256];

	serv_puts("SLRP HIGHEST");
	serv_getln(buf, sizeof buf);
}














/*******************************************************************************
 ***************************** Modify Rooms ************************************
 ******************************************************************************/





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


int GetCurrentRoomFlags(folder *Room, int CareForStatusMessage)
{
	StrBuf *Buf;

	Buf = NewStrBuf();
	serv_puts("GETR");
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) != 2) {
		FlushStrBuf(Room->XAPass);
		FlushStrBuf(Room->Directory);
		StrBufCutLeft(Buf, 4);
		if (CareForStatusMessage)
			AppendImportantMessage (SKEY(Buf));
		FreeStrBuf(&Buf);
		Room->XALoaded = 2;
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
		
	if (WCC->CurRoom.XALoaded > 0)
		return;

	GetCurrentRoomFlags(&WCC->CurRoom, 0);
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


void LoadXRoomInfoText(void)
{
	wcsession *WCC = WC;
	StrBuf *Buf;
	int Done = 0;
	
	if (WCC->CurRoom.XHaveInfoTextLoaded) {
		return;
	}

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

	FreeStrBuf(&Buf);
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


/* 
 * Toggle self-service list subscription
 */
void toggle_self_service(void) {
	wcsession *WCC = WC;

	if (GetCurrentRoomFlags (&WCC->CurRoom, 1) == 0)
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

	output_headers(1, 1, 1, 0, 0, 0);	
	do_template("room_edit");
	wDumpContent(1);
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
	int succ1, succ2;

	if (!havebstr("ok_button")) {
		AppendImportantMessage(_("Cancelled.  Changes were not saved."), -1);
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		return;
	}

	if (GetCurrentRoomFlags (&WCC->CurRoom, 1) == 0) {
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		return;
	}

	LoadRoomAide();
	WCC->CurRoom.QRFlags &= !(QR_PRIVATE | QR_PASSWORDED | QR_GUESSNAME);

	Ptr = sbstr("type");
	if (!strcmp(ChrPtr(Ptr), "invonly")) {
		WCC->CurRoom.QRFlags |= (QR_PRIVATE);
	}
	if (!strcmp(ChrPtr(Ptr), "hidden")) {
		WCC->CurRoom.QRFlags |= (QR_PRIVATE | QR_GUESSNAME);
	}
	if (!strcmp(ChrPtr(Ptr), "passworded")) {
		WCC->CurRoom.QRFlags |= (QR_PRIVATE | QR_PASSWORDED);
	}
	if (!strcmp(ChrPtr(Ptr), "personal")) {
		WCC->CurRoom.QRFlags |= QR_MAILBOX;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_MAILBOX;
	}

	if (yesbstr("prefonly")) {
		WCC->CurRoom.QRFlags |= QR_PREFONLY;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_PREFONLY;
	}

	if (yesbstr("readonly")) {
		WCC->CurRoom.QRFlags |= QR_READONLY;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_READONLY;
	}

	if (yesbstr("collabdel")) {
		WCC->CurRoom.QRFlags2 |= QR2_COLLABDEL;
	} else {
		WCC->CurRoom.QRFlags2 &= ~QR2_COLLABDEL;
	}

	if (yesbstr("permanent")) {
		WCC->CurRoom.QRFlags |= QR_PERMANENT;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_PERMANENT;
	}

	if (yesbstr("subjectreq")) {
		WCC->CurRoom.QRFlags2 |= QR2_SUBJECTREQ;
	} else {
		WCC->CurRoom.QRFlags2 &= ~QR2_SUBJECTREQ;
	}

	if (yesbstr("network")) {
		WCC->CurRoom.QRFlags |= QR_NETWORK;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_NETWORK;
	}

	if (yesbstr("directory")) {
		WCC->CurRoom.QRFlags |= QR_DIRECTORY;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_DIRECTORY;
	}

	if (yesbstr("ulallowed")) {
		WCC->CurRoom.QRFlags |= QR_UPLOAD;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_UPLOAD;
	}

	if (yesbstr("dlallowed")) {
		WCC->CurRoom.QRFlags |= QR_DOWNLOAD;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_DOWNLOAD;
	}

	if (yesbstr("visdir")) {
		WCC->CurRoom.QRFlags |= QR_VISDIR;
	} else {
		WCC->CurRoom.QRFlags &= ~QR_VISDIR;
	}

	Ptr = sbstr("anon");

	WCC->CurRoom.QRFlags &= ~(QR_ANONONLY | QR_ANONOPT);
	if (!strcmp(ChrPtr(Ptr), "anononly"))
		WCC->CurRoom.QRFlags |= QR_ANONONLY;
	if (!strcmp(ChrPtr(Ptr), "anon2"))
		WCC->CurRoom.QRFlags |= QR_ANONOPT;

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
	
	if (succ1 + succ2 == 0) {
		AppendImportantMessage (_("Your changes have been saved."), -1);
	}
	output_headers(1, 1, 1, 0, 0, 0);	
	do_template("room_edit");
	wDumpContent(1);
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


	if (GetCurrentRoomFlags(&WCC->CurRoom, 1) == 1)
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
				if (StrBuf_ServGetln(Buf) < 0)
					break;
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
				if (StrBuf_ServGetln(Buf) < 0)
					break;
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

	output_headers(1, 1, 1, 0, 0, 0);	
	do_template("room_edit");
	wDumpContent(1);
}


/*
 * Create a new room
 */
void entroom(void)
{
	StrBuf *Line;
	const StrBuf *er_name;
	const StrBuf *er_type;
	const StrBuf *er_password;
	int er_floor;
	int er_num_type;
	int er_view;
	wcsession *WCC = WC;

	if (!havebstr("ok_button")) {
		AppendImportantMessage(_("Cancelled.  No new room was created."), -1);
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

	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 2) != 2) {
		FreeStrBuf(&Line);
		display_main_menu();
		return;
	}
	/** TODO: Room created, now update the left hand icon bar for this user */
	gotoroom(er_name);

	serv_printf("VIEW %d", er_view);
	StrBuf_ServGetln(Line);
	FreeStrBuf(&Line); /* TODO: should we care about errors? */
	WCC->CurRoom.view = er_view;

	if ( (WCC != NULL) && ( (WCC->CurRoom.RAFlags & UA_ADMINALLOWED) != 0) )  {
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
	} else {
		smart_goto(WCC->CurRoom.name);
	}
	FreeStrBuf(&Line);
}





/*
 * Change the view for this room
 */
void change_view(void) {
	int newview;
	char buf[SIZ];

	newview = lbstr("view");
	serv_printf("VIEW %d", newview);
	serv_getln(buf, sizeof buf);
	WC->CurRoom.view = newview;
	smart_goto(WC->CurRoom.name);
}



/*
 * Set the message expire policy for this room and/or floor
 */
void set_room_policy(void) {
	StrBuf *Line;

	if (!havebstr("ok_button")) {
		AppendImportantMessage(_("Cancelled.  Changes were not saved."), -1);
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		return;
	}

	Line = NewStrBuf();

	serv_printf("SPEX room|%d|%d", ibstr("roompolicy"), ibstr("roomvalue"));
	StrBuf_ServGetln(Line);
	GetServerStatusMsg(Line, NULL, 1, 0);
	if (WC->axlevel >= 6) {
		serv_printf("SPEX floor|%d|%d", ibstr("floorpolicy"), ibstr("floorvalue"));
		StrBuf_ServGetln(Line);
		GetServerStatusMsg(Line, NULL, 1, 0);
	}
	FreeStrBuf(&Line);
	ReloadCurrentRoom();

	output_headers(1, 1, 1, 0, 0, 0);	
	do_template("room_edit");
	wDumpContent(1);
}



/*
 * Perform changes to a room's network configuration
 */
void netedit(void) {
	char buf[SIZ];
	char line[SIZ];
	char cmpa0[SIZ];
	char cmpa1[SIZ];
	char cmpb0[SIZ];
	char cmpb1[SIZ];
	int i, num_addrs;
	StrBuf *Line;
	StrBuf *TmpBuf;
	int malias = 0;
	int malias_set_default = 0;
	char sepchar = '|';
	int Done;

	line[0] = '\0';
        if (havebstr("force_room")) {
                gotoroom(sbstr("force_room"));
	}
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
	else if (havebstr("alias")) {
		const char *domain;
		domain = bstr("aliasdomain");
		if ((domain == NULL) || IsEmptyStr(domain))
		{
			malias_set_default = 1;
			strcpy(line, bstr("prefix"));
			strcat(line, bstr("default_aliasdomain"));
		}
		else
		{
			malias = 1;
			sepchar = ',';
			strcat(line, bstr("prefix"));
			if (!IsEmptyStr(domain))
			{
				strcat(line, "@");
				strcat(line, domain);
			}
			strcat(line, ",");
			strcat(line, "room_");
			strcat(line, ChrPtr(WC->CurRoom.name));
		}
	}
	else {
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		return;
	}

	Line = NewStrBuf();
	TmpBuf = NewStrBuf();
	if (malias)
		serv_puts("GNET "FILE_MAILALIAS);
	else
		serv_puts("GNET");
	StrBuf_ServGetln(Line);
	if  (GetServerStatus(Line, NULL) != 1) {
		AppendImportantMessage(SRV_STATUS_MSG(Line));	
		FreeStrBuf(&Line);
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		return;
	}

	/** This loop works for add *or* remove.  Spiffy, eh? */
	Done = 0;
	extract_token(cmpb0, line, 0, sepchar, sizeof cmpb0);
	extract_token(cmpb1, line, 1, sepchar, sizeof cmpb1);
	while (!Done && StrBuf_ServGetln(Line)>=0) {
		if ( (StrLength(Line)==3) && 
		     !strcmp(ChrPtr(Line), "000")) 
		{
			Done = 1;
		}
		else
		{
			if (StrLength(Line) == 0)
				continue;

			if (malias_set_default)
			{
				if (strncmp(ChrPtr(Line), HKEY("roommailalias|")) != 0)
				{
					StrBufAppendBufPlain(Line, HKEY("\n"), 0);
					StrBufAppendBuf(TmpBuf, Line, 0);
				}
			}
			else
			{
				extract_token(cmpa0, ChrPtr(Line), 0, sepchar, sizeof cmpa0);
				extract_token(cmpa1, ChrPtr(Line), 1, sepchar, sizeof cmpa1);
				if ( (strcasecmp(cmpa0, cmpb0)) || (strcasecmp(cmpa1, cmpb1)) )
				{
					StrBufAppendBufPlain(Line, HKEY("\n"), 0);
					StrBufAppendBuf(TmpBuf, Line, 0);
				}
			}
		}
	}

	if (malias)
		serv_puts("SNET "FILE_MAILALIAS);
	else
		serv_puts("SNET");
	StrBuf_ServGetln(Line);
	if  (GetServerStatus(Line, NULL) != 4) {

		AppendImportantMessage(SRV_STATUS_MSG(Line));	
		output_headers(1, 1, 1, 0, 0, 0);	
		do_template("room_edit");
		wDumpContent(1);
		FreeStrBuf(&Line);
		FreeStrBuf(&TmpBuf);
		return;
	}

	serv_putbuf(TmpBuf);
	FreeStrBuf(&TmpBuf);

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
	FlushIgnetCfgs(&WC->CurRoom);
	FreeStrBuf(&Line);

	output_headers(1, 1, 1, 0, 0, 0);	
	do_template("room_edit");
	wDumpContent(1);
}

/*
 * Known rooms list (box style)
 */
void knrooms(void)
{
	DeleteHash(&WC->Rooms);
	output_headers(1, 1, 1, 0, 0, 0); 
	do_template("knrooms");
	wDumpContent(1);
}


















/*******************************************************************************
 ********************** FLOOR Coomands *****************************************
 ******************************************************************************/



/*
 * delete the actual floor
 */
void delete_floor(void) {
	int floornum;
	StrBuf *Buf;
	const char *Err;
		
	floornum = ibstr("floornum");
	Buf = NewStrBuf();
	serv_printf("KFLR %d|1", floornum);
	
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);

	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufPlain(Buf, _("Floor has been deleted."),-1);
	}
	else {
		StrBufCutLeft(Buf, 4);
	}
	AppendImportantMessage (SKEY(Buf));

	FlushRoomlist();
	http_transmit_thing(ChrPtr(do_template("floors")), 0);
	FreeStrBuf(&Buf);
}

/*
 * start creating a new floor
 */
void create_floor(void) {
	StrBuf *Buf;
	const char *Err;

	Buf = NewStrBuf();
	serv_printf("CFLR %s|1", bstr("floorname"));
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);

	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufPlain(Buf, _("New floor has been created."),-1);
	}
	else {
		StrBufCutLeft(Buf, 4);
	}
	AppendImportantMessage (SKEY(Buf));
	FlushRoomlist();
	http_transmit_thing(ChrPtr(do_template("floors")), 0);
	FreeStrBuf(&Buf);
}


/*
 * rename this floor
 */
void rename_floor(void) {
	StrBuf *Buf;

	Buf = NewStrBuf();
	FlushRoomlist();

	serv_printf("EFLR %d|%s", ibstr("floornum"), bstr("floorname"));
	StrBuf_ServGetln(Buf);

	StrBufCutLeft(Buf, 4);
	AppendImportantMessage (SKEY(Buf));

	http_transmit_thing(ChrPtr(do_template("floors")), 0);
	FreeStrBuf(&Buf);
}



void jsonRoomFlr(void) 
{
	/* Send as our own (application/json) content type */
	hprintf("HTTP/1.1 200 OK\r\n");
	hprintf("Content-type: application/json; charset=utf-8\r\n");
	hprintf("Server: %s / %s\r\n", PACKAGE_STRING, ChrPtr(WC->serv_info->serv_software));
	hprintf("Connection: close\r\n");
	hprintf("Pragma: no-cache\r\nCache-Control: no-store\r\nExpires:-1\r\n");
	begin_burst();
	DoTemplate(HKEY("json_roomflr"),NULL,&NoCtx);
	end_burst(); 
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
	RegisterPreference("roomlistview",
                           _("Room list view"),
                           PRF_STRING,
                           NULL);
        RegisterPreference("emptyfloors", _("Show empty floors"), PRF_YESNO, NULL);

	WebcitAddUrlHandler(HKEY("json_roomflr"), "", 0, jsonRoomFlr, 0);

	WebcitAddUrlHandler(HKEY("delete_floor"), "", 0, delete_floor, 0);
	WebcitAddUrlHandler(HKEY("rename_floor"), "", 0, rename_floor, 0);
	WebcitAddUrlHandler(HKEY("create_floor"), "", 0, create_floor, 0);

	WebcitAddUrlHandler(HKEY("knrooms"), "", 0, knrooms, ANONYMOUS);
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
	REGISTERTokenParamDefine(UA_REPLYALLOWED);
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
	REGISTERTokenParamDefine(VIEW_QUEUE);

	/* GNET types: */
	/* server internal, we need to know but ignore them. */
	REGISTERTokenParamDefine(subpending);
	REGISTERTokenParamDefine(unsubpending);
	REGISTERTokenParamDefine(lastsent);

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
	REGISTERTokenParamDefine(roommailalias);



}


void 
SessionDestroyModule_ROOMOPS
(wcsession *sess)
{
	_FlushRoomList (sess);
}

