/*
 * $Id: roomlist.c 7751 2009-08-28 21:13:28Z dothebart $
 * room listings and filters.
 */

#include "webcit.h"
#include "webserver.h"
#include "roomops.h"

void DeleteFloor(void *vFloor)
{
	floor *Floor;
	Floor = (floor*) vFloor;
	FreeStrBuf(&Floor->Name);
	free(Floor);
}

HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP) {
	const char *Err;
	StrBuf *Buf;
	HashList *floors;
	floor *Floor;
	const char *Pos;
	wcsession *WCC = WC;

	if (WCC->Floors != NULL)
		return WCC->Floors;
	WCC->Floors = floors = NewHash(1, NULL);
	Buf = NewStrBuf();
 
	Floor = malloc(sizeof(floor));
	Floor->ID = VIRTUAL_MY_FLOOR;
	Floor->Name = NewStrBufPlain(_("My Folders"), -1);
	Floor->NRooms = 0;
	
	Put(floors, IKEY(Floor->ID), Floor, DeleteFloor);
	serv_puts("LFLR"); /* get floors */
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err); /* '100', we hope */
	if (GetServerStatus(Buf, NULL) == 1) 
	{
		while(StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err), strcmp(ChrPtr(Buf), "000")) 
		{
			
			Pos = NULL;

			Floor = malloc(sizeof(floor));
			Floor->ID = StrBufExtractNext_long(Buf, &Pos, '|');
			Floor->Name = NewStrBufPlain(NULL, StrLength(Buf));
			StrBufExtract_NextToken(Floor->Name, Buf, &Pos, '|');
			Floor->NRooms = StrBufExtractNext_long(Buf, &Pos, '|');

			Put(floors, IKEY(Floor->ID), Floor, DeleteFloor);
		}
	}
	FreeStrBuf(&Buf);
	return floors;
}

void tmplput_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	floor *Floor = (floor *)(TP->Context);

	StrBufAppendPrintf(Target, "%d", Floor->ID);
}

void tmplput_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	floor *Floor = (floor *)(TP->Context);

	StrBufAppendTemplate(Target, TP, Floor->Name, 0);
}

void tmplput_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	floor *Floor = (floor *)(TP->Context);

	StrBufAppendPrintf(Target, "%d", Floor->NRooms);
}
HashList *GetRoomListHashLKRA(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	if (WCC->Floors == NULL)
		GetFloorListHash(Target, TP);
	serv_puts("LKRA");
	return GetRoomListHash(Target, TP);
}

void DeleteFolder(void *vFolder)
{
	folder *room;
	room = (folder*) vFolder;

	FreeStrBuf(&room->name);
	FreeStrBuf(&room->ACL);

	//// FreeStrBuf(&room->room);

	free(room);
}


HashList *GetRoomListHash(StrBuf *Target, WCTemplputParams *TP) 
{
	HashList *rooms;
	folder *room;
	StrBuf *Buf;
	const char *Pos;
	const char *Err;
	void *vFloor;
	wcsession *WCC = WC;

	Buf = NewStrBuf();
	rooms = NewHash(1, NULL);
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);
	if (GetServerStatus(Buf, NULL) == 1) 
	{
		while(StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err), 
		      strcmp(ChrPtr(Buf), "000")) 
		{

			Pos = NULL;
			room = (folder*) malloc (sizeof(folder));
			memset(room, 0, sizeof(folder));

			room->name = NewStrBufPlain(NULL, StrLength(Buf));
			StrBufExtract_NextToken(room->name, Buf, &Pos, '|');

			room->QRFlags = StrBufExtractNext_long(Buf, &Pos, '|');
			room->floorid = StrBufExtractNext_long(Buf, &Pos, '|');

			room->listorder = StrBufExtractNext_long(Buf, &Pos, '|');

			room->ACL = NewStrBufPlain(NULL, StrLength(Buf));
			StrBufExtract_NextToken(room->ACL, Buf, &Pos, '|');

			room->view = StrBufExtractNext_long(Buf, &Pos, '|');
			room->defview = StrBufExtractNext_long(Buf, &Pos, '|');
			room->lastchange = StrBufExtractNext_long(Buf, &Pos, '|');

			if ((room->QRFlags & QR_MAILBOX) && 
			    (room->floorid == 0))
				room->floorid = VIRTUAL_MY_FLOOR;
			GetHash(WCC->Floors, IKEY(room->floorid), &vFloor);
			room->Floor = (const floor*) vFloor;
			Put(rooms, SKEY(room->name), room, DeleteFolder);
		}
	}
	SortByHashKey(rooms, 1);
	/*SortByPayload(rooms, SortRoomsByListOrder);  */
	FreeStrBuf(&Buf);
	return rooms;
}

/** Unused function that orders rooms by the listorder flag */
int SortRoomsByListOrder(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) room1;
	folder *r2 = (folder*) room2;
  
	if (r1->listorder == r2->listorder) return 0;
	if (r1->listorder > r2->listorder) return 1;
	return -1;
}

int SortRoomsByFloorAndName(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) room1;
	folder *r2 = (folder*) room2;
  
	if (r1->Floor != r2->Floor)
		return strcmp(ChrPtr(r1->Floor->Name), 
			      ChrPtr(r2->Floor->Name));
	return strcmp (ChrPtr(r1->name), 
		       ChrPtr(r2->name));
}



void tmplput_ROOM_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);

	StrBufAppendTemplate(Target, TP, Folder->name, 0);
}

void tmplput_ROOM_ACL(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);

	StrBufAppendTemplate(Target, TP, Folder->ACL, 0);
}


void tmplput_ROOM_QRFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->QRFlags);
}



void tmplput_ROOM_FLOORID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->floorid);
}

void tmplput_ROOM_LISTORDER(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->listorder);
}
void tmplput_ROOM_VIEW(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->view);
}
void tmplput_ROOM_DEFVIEW(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->defview);
}
void tmplput_ROOM_LASTCHANGE(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->lastchange);
}
void tmplput_ROOM_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	const floor *Floor = Folder->Floor;

	if (Floor == NULL)
		return;

	StrBufAppendPrintf(Target, "%d", Floor->ID);
}

void tmplput_ROOM_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	const floor *Floor = Folder->Floor;

	if (Floor == NULL)
		return;

	StrBufAppendTemplate(Target, TP, Floor->Name, 0);
}

void tmplput_ROOM_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	const floor *Floor = Folder->Floor;

	if (Floor == NULL)
		return;
	StrBufAppendPrintf(Target, "%d", Floor->NRooms);
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



void 
InitModule_ROOMLIST
(void)
{
	WebcitAddUrlHandler(HKEY("json_roomflr"), jsonRoomFlr, 0);


	RegisterNamespace("FLOOR:ID", 0, 0, tmplput_FLOOR_ID, CTX_FLOORS);
	RegisterNamespace("FLOOR:NAME", 0, 1, tmplput_FLOOR_NAME, CTX_FLOORS);
	RegisterNamespace("FLOOR:NROOMS", 0, 0, tmplput_FLOOR_NROOMS, CTX_FLOORS);



	RegisterIterator("LKRA", 0, NULL, GetRoomListHashLKRA, NULL, DeleteHash, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);

	RegisterNamespace("ROOM:INFO:FLOORID", 0, 1, tmplput_ROOM_FLOORID, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:NAME", 0, 1, tmplput_ROOM_NAME, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:ACL", 0, 1, tmplput_ROOM_ACL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:QRFLAGS", 0, 1, tmplput_ROOM_QRFLAGS, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LISTORDER", 0, 1, tmplput_ROOM_LISTORDER, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:VIEW", 0, 1, tmplput_ROOM_VIEW, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:DEFVIEW", 0, 1, tmplput_ROOM_DEFVIEW, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LASTCHANGE", 0, 1, tmplput_ROOM_LASTCHANGE, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:ID", 0, 0, tmplput_ROOM_FLOOR_ID, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:NAME", 0, 1, tmplput_ROOM_FLOOR_NAME, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:NROOMS", 0, 0, tmplput_ROOM_FLOOR_NROOMS, CTX_ROOMS);

/*
	RegisterSortFunc(HKEY("byfloorroom"),
			 NULL, 0,
			 CompareRoomListByFloorRoomPrivFirst,
			 CompareRoomListByFloorRoomPrivFirstRev,
			 GroupchangeRoomListByFloorRoomPrivFirst,
			 CTX_ROOMS);
*/
}
