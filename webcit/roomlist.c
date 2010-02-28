/*
 * $Id$
 * room listings and filters.
 */

#include "webcit.h"
#include "webserver.h"


void DeleteFloor(void *vFloor)
{
	Floor *pFloor;
	pFloor = (Floor*) vFloor;
	FreeStrBuf(&pFloor->Name);
	free(pFloor);
}

int SortFloorsByNameOrder(const void *vfloor1, const void *vfloor2) 
{
	Floor *f1 = (Floor*) GetSearchPayload(vfloor1);
	Floor *f2 = (Floor*) GetSearchPayload(vfloor2);
	
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
	HashList *floorsbyname;
	HashPos *it;
	Floor *pFloor;
	void *vFloor;
	const char *Pos;
	int i;
	wcsession *WCC = WC;
	const char *HashKey;
	long HKLen;


	if (WCC->Floors != NULL)
		return WCC->Floors;
	WCC->Floors = floors = NewHash(1, Flathash);
	WCC->FloorsByName = floorsbyname = NewHash(1, NULL);
	Buf = NewStrBuf();

	pFloor = (Floor*) malloc(sizeof(Floor));
	pFloor->ID = VIRTUAL_MY_FLOOR;
	pFloor->Name = NewStrBufPlain(_("My Folders"), -1);
	pFloor->NRooms = 0;
	
	Put(floors, IKEY(pFloor->ID), pFloor, DeleteFloor);
	Put(floorsbyname, SKEY(pFloor->Name), pFloor, reference_free_handler);

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

				pFloor = (Floor*) malloc(sizeof(Floor));
				pFloor->ID = StrBufExtractNext_int(Buf, &Pos, '|');
				pFloor->Name = NewStrBufPlain(NULL, StrLength(Buf));
				StrBufExtract_NextToken(pFloor->Name, Buf, &Pos, '|');
				pFloor->NRooms = StrBufExtractNext_long(Buf, &Pos, '|');

				Put(floors, IKEY(pFloor->ID), pFloor, DeleteFloor);
				Put(floorsbyname, SKEY(pFloor->Name), pFloor, reference_free_handler);
			}
	}
	FreeStrBuf(&Buf);
	
	/* now lets pre-sort them alphabeticaly. */
	i = 1;
	SortByPayload(floors, SortFloorsByNameOrder);
	it = GetNewHashPos(floors, 0);
	while (	GetNextHashPos(floors, it, &HKLen, &HashKey, &vFloor)) 
		((Floor*) vFloor)->AlphaN = i++;
	DeleteHashPos(&it);
	SortByHashKeyStr(floors);

	return floors;
}

void tmplput_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendPrintf(Target, "%d", myFloor->ID);
}

void tmplput_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendTemplate(Target, TP, myFloor->Name, 0);
}

void tmplput_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	Floor *myFloor = (Floor *)CTX;

	StrBufAppendPrintf(Target, "%d", myFloor->NRooms);
}
HashList *GetRoomListHashLKRA(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	if (WCC->Floors == NULL)
		GetFloorListHash(Target, TP);
	serv_puts("LKRA");
	if (WCC->Rooms == NULL) 
		WCC->Rooms =  GetRoomListHash(Target, TP);
	return WCC->Rooms;
}

void FlushFolder(folder *room)
{
	int i;

	FreeStrBuf(&room->name);
	if (room->IgnetCfgs[0] == (HashList*) StrBufNOTNULL)
	{
		room->IgnetCfgs[0] = NULL;
		for (i = ignet_push_share; i < maxRoomNetCfg; i++)
			DeleteHash(&room->IgnetCfgs[i]);
	}
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
	void *vFloor;
	wcsession *WCC = WC;
	CompareFunc SortIt;
	WCTemplputParams SubTP;

	Buf = NewStrBuf();
	rooms = NewHash(1, NULL);
	StrBuf_ServGetln(Buf);
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
				room->Floor = (const Floor*) vFloor;



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

HashList *GetNetConfigHash(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	StrBuf *Line;
	StrBuf *Token;
	StrBuf *Content;
	long WantThisOne;
	long PutTo;
	long State;
	
	WantThisOne = GetTemplateTokenNumber(Target, TP, 5, 0);
	if (WantThisOne == 0)
		return NULL;
	if (WCC->CurRoom.IgnetCfgs[0] == (HashList*) StrBufNOTNULL)
		return WCC->CurRoom.IgnetCfgs[WantThisOne];

	WCC->CurRoom.IgnetCfgs[0] = (HashList*) StrBufNOTNULL;
	serv_puts("GNET");
	Line = NewStrBuf();
	Token = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		const char *Pos = NULL;
		StrBuf_ServGetln(Line);
		StrBufExtract_NextToken(Token, Line, &Pos, '|');
		PutTo = GetTokenDefine(SKEY(Token), -1);
		if ((PutTo > 0) && (PutTo < maxRoomNetCfg))
		{
			int n;

			if (WCC->CurRoom.IgnetCfgs[PutTo] == NULL)
				WCC->CurRoom.IgnetCfgs[PutTo] = NewHash(1, NULL);
			Content = NewStrBuf();
			StrBufExtract_NextToken(Content, Line, &Pos, '|');
			n = GetCount(WCC->CurRoom.IgnetCfgs[PutTo]) + 1;
			Put(WCC->CurRoom.IgnetCfgs[PutTo], 
			    IKEY(n),
			    Content, 
			    HFreeStrBuf);
		}
	}
	else if (State == 550)
		StrBufAppendBufPlain(WCC->ImportantMsg,
				     _("Higher access is required to access this function."), -1, 0);


	return WCC->CurRoom.IgnetCfgs[WantThisOne];
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
	folder *Folder = (folder *)CTX;

	StrBufAppendTemplate(Target, TP, Folder->name, 0);
}
void tmplput_ROOM_BASENAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)CTX;

	if (room->nRoomNameParts > 1)
		StrBufAppendTemplate(Target, TP, 
				      room->RoomNameParts[room->nRoomNameParts - 1], 0);
	else 
		StrBufAppendTemplate(Target, TP, room->name, 0);
}
void tmplput_ROOM_LEVEL_N_TIMES(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *room = (folder *)CTX;
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
	folder *Folder = (folder *)CTX;

	StrBufAppendPrintf(Target, "%ld", Folder->RAFlags, 0);
}


void tmplput_ROOM_QRFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->QRFlags);
}

void tmplput_ROOM_RAFLAGS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)(TP->Context);
	StrBufAppendPrintf(Target, "%d", Folder->RAFlags);
}


void tmplput_ROOM_FLOORID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->floorid);
}

void tmplput_ROOM_LISTORDER(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->listorder);
}
void tmplput_ROOM_VIEW(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->view);
}
void tmplput_ROOM_DEFVIEW(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->defview);
}
void tmplput_ROOM_LASTCHANGE(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	StrBufAppendPrintf(Target, "%d", Folder->lastchange);
}
void tmplput_ROOM_FLOOR_ID(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;

	StrBufAppendPrintf(Target, "%d", pFloor->ID);
}

void tmplput_ROOM_FLOOR_NAME(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;

	StrBufAppendTemplate(Target, TP, pFloor->Name, 0);
}

void tmplput_ROOM_FLOOR_NROOMS(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	const Floor *pFloor = Folder->Floor;

	if (pFloor == NULL)
		return;
	StrBufAppendPrintf(Target, "%d", pFloor->NRooms);
}



int ConditionalRoomHas_UA_KNOWN(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_KNOWN) != 0;
}

int ConditionalRoomHas_UA_GOTOALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_GOTOALLOWED) != 0;
}

int ConditionalRoomHas_UA_HASNEWMSGS(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_HASNEWMSGS) != 0;
}

int ConditionalRoomHas_UA_ZAPPED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_ZAPPED) != 0;
}

int ConditionalRoomHas_UA_POSTALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_POSTALLOWED) != 0;
}

int ConditionalRoomHas_UA_ADMINALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_ADMINALLOWED) != 0;
}

int ConditionalRoomHas_UA_DELETEALLOWED(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return (Folder->RAFlags & UA_DELETEALLOWED) != 0;
}


int ConditionalRoomIsInbox(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;
	return Folder->is_inbox;
}

void tmplput_ROOM_COLLECTIONTYPE(StrBuf *Target, WCTemplputParams *TP) 
{
	folder *Folder = (folder *)CTX;
	
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
	case VIEW_WIKI:
		StrBufAppendBufPlain(Target, HKEY("wiki"), 0);
		break;
	}
}




int ConditionalRoomHasGroupdavContent(StrBuf *Target, WCTemplputParams *TP)
{
	folder *Folder = (folder *)CTX;

	lprintf(0, "-> %s: %ld\n", ChrPtr(Folder->name), Folder->view);

	return ((Folder->view == VIEW_CALENDAR) || 
		(Folder->view == VIEW_TASKS) || 
		(Folder->view == VIEW_ADDRESSBOOK) ||
		(Folder->view == VIEW_NOTES) ||
		(Folder->view == VIEW_JOURNAL) );
}



int ConditionalFloorIsRESTSubFloor(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	Floor *MyFloor = (Floor *)CTX;
	/** if we have dav_depth the client just wants the subfloors */
	if ((WCC->Hdr->HR.dav_depth == 1) && 
	    (GetCount(WCC->Directory) == 0))
		return 1;
	return WCC->CurrentFloor == MyFloor;
}


int ConditionalFloorIsSUBROOM(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	Floor *MyFloor = (Floor *)CTX;

	return WCC->CurRoom.floorid == MyFloor->ID;
}


int ConditionalRoomIsRESTSubRoom(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	folder     *Folder = (folder *)CTX;
	HashPos    *it;
	StrBuf     * Dir;
	void       *vDir;
	long        len;
        const char *Key;
	int i, j, urlp;
	int delta;


	/* list only folders relative to the current floor... */
	if (Folder->Floor != WCC->CurrentFloor)
		return 0;

	urlp = GetCount(WCC->Directory);
	delta = Folder->nRoomNameParts - urlp + 1;

	lprintf(0, "\n->%s: %ld - %ld ", ChrPtr(Folder->name), urlp, 
		Folder->nRoomNameParts);
	/* list only the floors which are in relation to the dav_depth header */
	if (WCC->Hdr->HR.dav_depth != delta) {
		lprintf(0, "1\n");
		return 0;
	}


	it = GetNewHashPos(WCC->Directory, 0);
	/* Fast forward the floorname we checked above... */
	GetNextHashPos(WCC->Directory, it, &len, &Key, &vDir);

	if (Folder->nRoomNameParts > 1) {		
		for (i = 0, j = 1; 
		     (i > Folder->nRoomNameParts) && (j > urlp); 
		     i++, j++)
		{
			if (!GetNextHashPos(WCC->Directory, 
					    it, &len, &Key, &vDir) ||
			    (vDir == NULL))
			{
				DeleteHashPos(&it);

				lprintf(0, "3\n");
				return 0;
			}
			Dir = (StrBuf*) vDir;
			if (strcmp(ChrPtr(Folder->RoomNameParts[i]), 
				   ChrPtr(Dir)) != 0)
			{
				DeleteHashPos(&it);
				lprintf(0, "4\n");
				return 0;
			}
		}
		DeleteHashPos(&it);
		return 1;
	}
	else {
		if (!GetNextHashPos(WCC->Directory, 
				    it, &len, &Key, &vDir) ||
		    (vDir == NULL))
		{
			DeleteHashPos(&it);
			
			lprintf(0, "5\n");
			return WCC->Hdr->HR.dav_depth == 1;
		}
		DeleteHashPos(&it);
		Dir = (StrBuf*) vDir;
		if (WCC->Hdr->HR.dav_depth == 0) {
			return (strcmp(ChrPtr(Folder->name), 
				       ChrPtr(Dir))
				== 0);

		}
		return 0;
	}
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
	DeleteHash(&sess->Rooms);
	DeleteHash(&sess->FloorsByName);
}

void 
InitModule_ROOMLIST
(void)
{
	WebcitAddUrlHandler(HKEY("json_roomflr"), "", 0, jsonRoomFlr, 0);


	RegisterNamespace("FLOOR:ID", 0, 0, tmplput_FLOOR_ID, NULL, CTX_FLOORS);
	RegisterNamespace("FLOOR:NAME", 0, 1, tmplput_FLOOR_NAME, NULL, CTX_FLOORS);
	RegisterNamespace("FLOOR:NROOMS", 0, 0, tmplput_FLOOR_NROOMS, NULL, CTX_FLOORS);
	RegisterConditional(HKEY("COND:FLOOR:ISSUBROOM"), 0, ConditionalFloorIsSUBROOM, CTX_FLOORS);
	RegisterConditional(HKEY("COND:ROOM:REST:ISSUBFLOOR"), 0, ConditionalFloorIsRESTSubFloor, CTX_FLOORS);

	RegisterIterator("ITERATE:THISROOM:GNET", 1, NULL, GetNetConfigHash, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);

	RegisterIterator("LFLR", 0, NULL, GetFloorListHash, NULL, NULL, CTX_FLOORS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);

	RegisterIterator("LKRA", 0, NULL, GetRoomListHashLKRA, NULL, NULL, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);

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
