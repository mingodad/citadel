/*
 * $Id: roomlist.c 7751 2009-08-28 21:13:28Z dothebart $
 * room listings and filters.
 */

#include "webcit.h"
#include "webserver.h"


void DeleteFloor(void *vFloor)
{
	floor *Floor;
	Floor = (floor*) vFloor;
	FreeStrBuf(&Floor->Name);
	free(Floor);
}

int SortFloorsByNameOrder(const void *vfloor1, const void *vfloor2) 
{
	floor *f1 = (floor*) GetSearchPayload(vfloor1);
	floor *f2 = (floor*) GetSearchPayload(vfloor2);
	
	/* prefer My floor over alpabetical sort */
	if (f1->ID == VIRTUAL_MY_FLOOR)
		return 1;
	if (f2->ID == VIRTUAL_MY_FLOOR)
		return -1;

	return strcmp(ChrPtr(f1->Name), ChrPtr(f2->Name));
}

HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP) 
{
	int Done = 0;
	const char *Err;
	StrBuf *Buf;
	HashList *floors;
	HashPos *it;
	floor *Floor;
	void *vFloor;
	const char *Pos;
	int i;
	wcsession *WCC = WC;
	const char *HashKey;
	long HKLen;


	if (WCC->Floors != NULL)
		return WCC->Floors;
	WCC->Floors = floors = NewHash(1, Flathash);
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
		while(!Done && StrBuf_ServGetln(Buf))
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) 
			{
				Done = 1;
			}
			else
			{
			
				Pos = NULL;

				Floor = malloc(sizeof(floor));
				Floor->ID = StrBufExtractNext_int(Buf, &Pos, '|');
				Floor->Name = NewStrBufPlain(NULL, StrLength(Buf));
				StrBufExtract_NextToken(Floor->Name, Buf, &Pos, '|');
				Floor->NRooms = StrBufExtractNext_long(Buf, &Pos, '|');

				Put(floors, IKEY(Floor->ID), Floor, DeleteFloor);
			}
	}
	FreeStrBuf(&Buf);
	
	/* now lets pre-sort them alphabeticaly. */
	i = 1;
	SortByPayload(floors, SortFloorsByNameOrder);
	it = GetNewHashPos(floors, 0);
	while (	GetNextHashPos(floors, it, &HKLen, &HashKey, &vFloor)) 
		((floor*) vFloor)->AlphaN = i++;
	DeleteHashPos(&it);
	SortByHashKeyStr(floors);

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

void FlushFolder(folder *room)
{
	int i;

	FreeStrBuf(&room->name);
	if (room->RoomNameParts != NULL)
	{
		for (i=0; i < room->nRoomNameParts; i++)
			FreeStrBuf(&room->RoomNameParts[i]);
		free(room->RoomNameParts);
	}
}

void vDeleteFolder(void *vFolder)
{
	folder *room;

	room = (folder*) vFolder;
	FlushFolder(room);

	free(room);
}


HashList *GetRoomListHash(StrBuf *Target, WCTemplputParams *TP) 
{
	int Done = 0;
	HashList *rooms;
	folder *room;
	StrBuf *Buf;
	const char *Pos;
	const char *Err;
	void *vFloor;
	wcsession *WCC = WC;
	CompareFunc SortIt;
	WCTemplputParams SubTP;

	Buf = NewStrBuf();
	rooms = NewHash(1, NULL);
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);
	if (GetServerStatus(Buf, NULL) == 1) 
	{
		while(!Done && StrBuf_ServGetln(Buf))
			if ( (StrLength(Buf)==3) && 
			     !strcmp(ChrPtr(Buf), "000")) 
			{
				Done = 1;
			}
			else
			{				
				Pos = NULL;
				room = (folder*) malloc (sizeof(folder));
				memset(room, 0, sizeof(folder));

				/* Load the base data from the server reply */
				room->name = NewStrBufPlain(NULL, StrLength(Buf));
				StrBufExtract_NextToken(room->name, Buf, &Pos, '|');

				room->QRFlags = StrBufExtractNext_long(Buf, &Pos, '|');
				room->floorid = StrBufExtractNext_int(Buf, &Pos, '|');
				room->listorder = StrBufExtractNext_long(Buf, &Pos, '|');
				room->QRFlags2 = StrBufExtractNext_long(Buf, &Pos, '|');

				room->RAFlags = StrBufExtractNext_long(Buf, &Pos, '|');

/*
  ACWHUT?
  room->ACL = NewStrBufPlain(NULL, StrLength(Buf));
  StrBufExtract_NextToken(room->ACL, Buf, &Pos, '|');
*/

				room->view = StrBufExtractNext_long(Buf, &Pos, '|');
				room->defview = StrBufExtractNext_long(Buf, &Pos, '|');
				room->lastchange = StrBufExtractNext_long(Buf, &Pos, '|');

				/* Evaluate the Server sent data for later use */
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
				GetHash(WCC->Floors, IKEY(room->floorid), &vFloor);
				room->Floor = (const floor*) vFloor;



				/* now we know everything, remember it... */
				Put(rooms, SKEY(room->name), room, vDeleteFolder);
			}
	}

	SubTP.Filter.ContextType = CTX_ROOMS;
	SortIt = RetrieveSort(&SubTP, NULL, 0, HKEY("fileunsorted"), 0);
	if (SortIt != NULL)
		SortByPayload(rooms, SortIt);
	else 
		SortByPayload(rooms, SortRoomsByListOrder);
	FreeStrBuf(&Buf);
	return rooms;
}

/** Unused function that orders rooms by the listorder flag */
int SortRoomsByListOrder(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) GetSearchPayload(room1);
	folder *r2 = (folder*) GetSearchPayload(room2);
  
	if (r1->listorder == r2->listorder) return 0;
	if (r1->listorder > r2->listorder) return 1;
	return -1;
}

int CompareRoomListByFloorRoomPrivFirst(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) GetSearchPayload(room1);
	folder *r2 = (folder*) GetSearchPayload(room2);
  
	if ((r1->Floor == NULL)  ||
	    (r2->Floor == NULL))
		return 0;
		
	/**
	 * are we on the same floor? else sort by floor.
	 */
	if (r1->Floor != r2->Floor)
	{
		/**
		 * the private rooms are first in any case.
		 */
		if (r1->Floor->ID == VIRTUAL_MY_FLOOR)
			return -1;
		if (r2->Floor->ID == VIRTUAL_MY_FLOOR)
			return 1;
		/**
		 * else decide alpaheticaly by floorname
		 */
		return (r1->Floor->AlphaN > r2->Floor->AlphaN)? 1 : -1;
	}

	/**
	 * if we have different levels of subdirectories, 
	 * we want the toplevel to be first, regardless of sort
	 * sequence.
	 */
	if (((r1->nRoomNameParts > 1) || 
	    (r2->nRoomNameParts > 1)    )&&
	    (r1->nRoomNameParts != r2->nRoomNameParts))
	{
		int i, ret;
		int nparts = (r1->nRoomNameParts > r2->nRoomNameParts)?
			r2->nRoomNameParts : r1->nRoomNameParts;

		for (i=0; i < nparts; i++)
		{
			ret = strcmp (ChrPtr(r1->name), 
				      ChrPtr(r2->name));
			/**
			 * Deltas in common parts? exit here.
			 */
			if (ret != 0) 
				return ret;
		}

		/**
		 * who's a subdirectory of whom?
		 */
		if (r1->nRoomNameParts > r2->nRoomNameParts)
			return 1;
		else
			return -1;

	}

	/**
	 * else just sort alphabeticaly.
	 */
	return strcmp (ChrPtr(r1->name), 
		       ChrPtr(r2->name));
}

int CompareRoomListByFloorRoomPrivFirstRev(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) GetSearchPayload(room1);
	folder *r2 = (folder*) GetSearchPayload(room2);

	if ((r1->Floor == NULL)  ||
	    (r2->Floor == NULL))
		return 0;

	/**
	 * are we on the same floor? else sort by floor.
	 */
	if (r2->Floor != r1->Floor)
	{
		/**
		 * the private rooms are first in any case.
		 */
		if (r1->Floor->ID == VIRTUAL_MY_FLOOR)
			return -1;
		if (r2->Floor->ID == VIRTUAL_MY_FLOOR)
			return 1;
		/**
		 * else decide alpaheticaly by floorname
		 */

		return (r1->Floor->AlphaN < r2->Floor->AlphaN)? 1 : -1;
	}

	/**
	 * if we have different levels of subdirectories, 
	 * we want the toplevel to be first, regardless of sort
	 * sequence.
	 */
	if (((r1->nRoomNameParts > 1) || 
	    (r2->nRoomNameParts > 1)    )&&
	    (r1->nRoomNameParts != r2->nRoomNameParts))
	{
		int i, ret;
		int nparts = (r1->nRoomNameParts > r2->nRoomNameParts)?
			r2->nRoomNameParts : r1->nRoomNameParts;

		for (i=0; i < nparts; i++)
		{
			/**
			 * special cases if one room is top-level...
			 */
			if (r2->nRoomNameParts == 1)
				ret = strcmp (ChrPtr(r2->name), 
					      ChrPtr(r1->RoomNameParts[i]));
			else if (r1->nRoomNameParts == 1)
				ret = strcmp (ChrPtr(r2->RoomNameParts[i]),
					      ChrPtr(r1->name));
			else 
				ret = strcmp (ChrPtr(r2->RoomNameParts[i]), 
					      ChrPtr(r1->RoomNameParts[i]));
			/**
			 * Deltas in common parts? exit here.
			 */
			if (ret != 0) 
				return ret;
		}

		/**
		 * who's a subdirectory of whom?
		 */
		if (r1->nRoomNameParts > r2->nRoomNameParts)
			return 1;
		else
			return -1;
	}

	return strcmp (ChrPtr(r2->name), 
		       ChrPtr(r1->name));
}

int GroupchangeRoomListByFloorRoomPrivFirst(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) room1;
	folder *r2 = (folder*) room2;
  

	if ((r1->Floor == NULL)  ||
	    (r2->Floor == NULL))
		return 0;
		
	if (r1->Floor == r2->Floor)
		return 0;
	else 
	{
		wcsession *WCC = WC;
		static int columns = 3;
		int boxes_per_column = 0;
		int nf;

		nf = GetCount(WCC->Floors);
		while (nf % columns != 0) ++nf;
		boxes_per_column = (nf / columns);
		if (boxes_per_column < 1)
			boxes_per_column = 1;
		if (r1->Floor->AlphaN % boxes_per_column == 0)
			return 2;
		else 
			return 1;
	}
}






void tmplput_ROOM_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);

	StrBufAppendTemplate(Target, TP, Folder->name, 0);
}
void tmplput_ROOM_BASENAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)(TP->Context);

	if (room->nRoomNameParts > 1)
		StrBufAppendTemplate(Target, TP, 
				      room->RoomNameParts[room->nRoomNameParts - 1], 0);
	else 
		StrBufAppendTemplate(Target, TP, room->name, 0);
}
void tmplput_ROOM_LEVEL_N_TIMES(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)(TP->Context);
	int i;
        const char *AppendMe;
        long AppendMeLen;


	if (room->nRoomNameParts > 1)
	{
		GetTemplateTokenString(Target, TP, 0, &AppendMe, &AppendMeLen);
		for (i = 0; i < room->nRoomNameParts; i++)
			StrBufAppendBufPlain(Target, AppendMe, AppendMeLen, 0);
	}
}

void tmplput_ROOM_ACL(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);

	StrBufAppendPrintf(Target, "%ld", Folder->RAFlags, 0);
}


void tmplput_ROOM_QRFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->QRFlags);
}

void tmplput_ROOM_RAFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->RAFlags);
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



int ConditionalRoomHas_UA_KNOWN(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_KNOWN) != 0;
}

int ConditionalRoomHas_UA_GOTOALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_GOTOALLOWED) != 0;
}

int ConditionalRoomHas_UA_HASNEWMSGS(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_HASNEWMSGS) != 0;
}

int ConditionalRoomHas_UA_ZAPPED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_ZAPPED) != 0;
}

int ConditionalRoomHas_UA_POSTALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_POSTALLOWED) != 0;
}

int ConditionalRoomHas_UA_ADMINALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_ADMINALLOWED) != 0;
}

int ConditionalRoomHas_UA_DELETEALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return (Folder->RAFlags & UA_DELETEALLOWED) != 0;
}


int ConditionalRoomIsInbox(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);
	return Folder->is_inbox;
}

void tmplput_ROOM_COLLECTIONTYPE(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	
	switch(Folder->view) {
	case VIEW_CALENDAR:
		StrBufAppendBufPlain(Target, HKEY("vevent"), 0);
		break;
	case VIEW_TASKS:
		StrBufAppendBufPlain(Target, HKEY("vtodo"), 0);
		break;
	case VIEW_ADDRESSBOOK:
		StrBufAppendBufPlain(Target, HKEY("vcard"), 0);
		break;
	case VIEW_NOTES:
		StrBufAppendBufPlain(Target, HKEY("vnotes"), 0);
		break;
	case VIEW_JOURNAL:
		StrBufAppendBufPlain(Target, HKEY("vjournal"), 0);
		break;
	}
}




int ConditionalRoomHasGroupdavContent(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)(TP->Context);

	return ((Folder->view == VIEW_CALENDAR) || 
		(Folder->view == VIEW_TASKS) || 
		(Folder->view == VIEW_ADDRESSBOOK) ||
		(Folder->view == VIEW_NOTES) ||
		(Folder->view == VIEW_JOURNAL) );
}



int ConditionalFloorIsRESTSubFloor(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;

	/** If we have dav_depth the client just wants the _current_ room without subfloors */
	if (WCC->Hdr->HR.dav_depth == 0)
		return 0;
	    
	return 1;
}


int ConditionalRoomIsRESTSubRoom(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	folder     *Folder = (folder *)(TP->Context);
	HashPos    *it;
	StrBuf     * Dir;
	void       *vDir;
	long        len;
        const char *Key;
	int i;



	if (Folder->Floor != WCC->CurrentFloor)
		return 0;

	if (GetCount(WCC->Directory) != Folder->nRoomNameParts)
		return 0;

	it = GetNewHashPos(WCC->Directory, 0);
	for (i = 0; i < Folder->nRoomNameParts; i++)
	{
		if (!GetNextHashPos(WCC->Directory, it, &len, &Key, &vDir) ||
		    (vDir == NULL))
		{
			DeleteHashPos(&it);
			return 0;
		}
		Dir = (StrBuf*) vDir;
		if (strcmp(ChrPtr(Folder->RoomNameParts[i]), 
			   ChrPtr(Dir)) != 0)
		{
			DeleteHashPos(&it);
			return 0;
		}
	}
	DeleteHashPos(&it);

	/** If we have dav_depth the client just wants the _current_ room without subfloors */
	if ((WCC->Hdr->HR.dav_depth == 0) &&
	    (i != Folder->nRoomNameParts))
		return 0;

	return 1;
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
SessionDetachModule_ROOMLIST
(wcsession *sess)
{
	DeleteHash(&sess->Floors);
}


void 
InitModule_ROOMLIST
(void)
{
	WebcitAddUrlHandler(HKEY("json_roomflr"), "", 0, jsonRoomFlr, 0);


	RegisterNamespace("FLOOR:ID", 0, 0, tmplput_FLOOR_ID, NULL, CTX_FLOORS);
	RegisterNamespace("FLOOR:NAME", 0, 1, tmplput_FLOOR_NAME, NULL, CTX_FLOORS);
	RegisterNamespace("FLOOR:NROOMS", 0, 0, tmplput_FLOOR_NROOMS, NULL, CTX_FLOORS);
	RegisterConditional(HKEY("COND:ROOM:REST:ISSUBFLOOR"), 0, ConditionalFloorIsRESTSubFloor, CTX_FLOORS);

	RegisterIterator("LFLR", 0, NULL, GetFloorListHash, NULL, NULL, CTX_FLOORS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);

	RegisterIterator("LKRA", 0, NULL, GetRoomListHashLKRA, NULL, DeleteHash, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);

	RegisterNamespace("ROOM:INFO:FLOORID", 0, 1, tmplput_ROOM_FLOORID, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:NAME", 0, 1, tmplput_ROOM_NAME, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:PRINT_NAME", 0, 1, tmplput_ROOM_NAME, NULL, CTX_ROOMS);/// TODO!
	RegisterNamespace("ROOM:INFO:BASENAME", 0, 1, tmplput_ROOM_BASENAME, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LEVELNTIMES", 1, 2, tmplput_ROOM_LEVEL_N_TIMES, NULL, CTX_ROOMS);

	RegisterNamespace("ROOM:INFO:ACL", 0, 1, tmplput_ROOM_ACL, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:QRFLAGS", 0, 1, tmplput_ROOM_QRFLAGS, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:RAFLAGS", 0, 1, tmplput_ROOM_RAFLAGS, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LISTORDER", 0, 1, tmplput_ROOM_LISTORDER, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:VIEW", 0, 1, tmplput_ROOM_VIEW, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:DEFVIEW", 0, 1, tmplput_ROOM_DEFVIEW, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:LASTCHANGE", 0, 1, tmplput_ROOM_LASTCHANGE, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:COLLECTIONTYPE", 0, 1, tmplput_ROOM_COLLECTIONTYPE, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:ID", 0, 0, tmplput_ROOM_FLOOR_ID, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:NAME", 0, 1, tmplput_ROOM_FLOOR_NAME, NULL, CTX_ROOMS);
	RegisterNamespace("ROOM:INFO:FLOOR:NROOMS", 0, 0, tmplput_ROOM_FLOOR_NROOMS, NULL, CTX_ROOMS);

	RegisterConditional(HKEY("COND:ROOM:REST:ISSUBROOM"), 0, ConditionalRoomIsRESTSubRoom, CTX_ROOMS);

	RegisterConditional(HKEY("COND:ROOM:INFO:IS_INBOX"), 0, ConditionalRoomIsInbox, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_KNOWN"), 0, ConditionalRoomHas_UA_KNOWN, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_GOTOALLOWED"), 0, ConditionalRoomHas_UA_GOTOALLOWED, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_HASNEWMSGS"), 0, ConditionalRoomHas_UA_HASNEWMSGS, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_ZAPPED"), 0, ConditionalRoomHas_UA_ZAPPED, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_POSTALLOWED"), 0, ConditionalRoomHas_UA_POSTALLOWED, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_ADMINALLOWED"), 0, ConditionalRoomHas_UA_ADMINALLOWED, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:FLAGS:UA_DELETEALLOWED"), 0, ConditionalRoomHas_UA_DELETEALLOWED, CTX_ROOMS);
	RegisterConditional(HKEY("COND:ROOM:GROUPDAV_CONTENT"), 0, ConditionalRoomHasGroupdavContent, CTX_ROOMS);



	RegisterSortFunc(HKEY("byfloorroom"),
			 NULL, 0,
			 CompareRoomListByFloorRoomPrivFirst,
			 CompareRoomListByFloorRoomPrivFirstRev,
			 GroupchangeRoomListByFloorRoomPrivFirst,
			 CTX_ROOMS);

}
