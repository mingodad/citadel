/*
 * room listings and filters.
 */

#include "webcit.h"
#include "webserver.h"

typedef enum __eRoomParamType {
	eNotSet,
	eDomain,
	eAlias
}eRoomParamType;

HashList *GetWhoKnowsHash(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Line;
	StrBuf *Token;
	long State;
	HashList *Whok = NULL;
	int Done = 0;
	int n = 0;

	serv_puts("WHOK");
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		Whok = NewHash(1, Flathash);
		while(!Done && (StrBuf_ServGetln(Line) >= 0) )
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
		AppendImportantMessage(_("Higher access is required to access this function."), -1);


	FreeStrBuf(&Line);
	return Whok;
}


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
		while(!Done && StrBuf_ServGetln(Buf) >= 0)
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

HashList *GetZappedRoomListHash(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	if (WCC->Floors == NULL)
		GetFloorListHash(Target, TP);
	serv_puts("LZRM -1");
	return GetRoomListHash(Target, TP);
}
HashList *GetRoomListHashLKRA(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;

	if (WCC->Floors == NULL)
		GetFloorListHash(Target, TP);
	if (WCC->Rooms == NULL) 
	{
		serv_puts("LKRA");
		WCC->Rooms =  GetRoomListHash(Target, TP);
	}
	return WCC->Rooms;
}

HashList *GetRoomListHashLPRM(StrBuf *Target, WCTemplputParams *TP) 
{
	serv_puts("LPRM");
	return GetRoomListHash(Target, TP);
}


void FlushIgnetCfgs(folder *room)
{
	int i;
	if (room->IgnetCfgs[maxRoomNetCfg] == (HashList*) StrBufNOTNULL)
	{
		for (i = ignet_push_share; i < maxRoomNetCfg; i++)
			DeleteHash(&room->IgnetCfgs[i]);
	}
	memset(&room->IgnetCfgs, 0, sizeof(HashList *) * (maxRoomNetCfg + 1));
	room->RoomAlias = NULL;

}

void FlushFolder(folder *room)
{
	int i;

	FreeStrBuf(&room->XAPass);
	FreeStrBuf(&room->Directory);
	FreeStrBuf(&room->RoomAide);
	FreeStrBuf(&room->XInfoText);
	room->XHaveInfoTextLoaded = 0;

	FreeStrBuf(&room->name);

	FlushIgnetCfgs(room);

	if (room->RoomNameParts != NULL)
	{
		for (i=0; i < room->nRoomNameParts; i++)
			FreeStrBuf(&room->RoomNameParts[i]);
		free(room->RoomNameParts);
	}
	memset(room, 0, sizeof(folder));
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
		while(!Done && (StrBuf_ServGetln(Buf) >= 0))
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
				room->Order = StrBufExtractNext_long(Buf, &Pos, '|');
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

HashList *GetThisRoomMAlias(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	StrBuf *Line;
	StrBuf *Token;
	HashList *Aliases = NULL;
	const char *pComma;
	long aliaslen;
	long locallen;
	long State;
	
	serv_puts("GNET "FILE_MAILALIAS);
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		int Done = 0;
		int n = 0;

		Aliases = NewHash(1, NULL);
		while(!Done && (StrBuf_ServGetln(Line) >= 0))
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000"))
			{
				Done = 1;
			}
			else
			{
				pComma = strchr(ChrPtr(Line), ',');
				if (pComma == NULL)
					continue;
				aliaslen = pComma - ChrPtr(Line);
				locallen = StrLength(Line) - 1 - aliaslen;
				if (locallen - 5 != StrLength(WCC->CurRoom.name))
					continue;
				if (strncmp(pComma + 1, "room_", 5) != 0)
					continue;

				if (strcasecmp(pComma + 6, ChrPtr(WCC->CurRoom.name)) != 0)
					continue;
				Token = NewStrBufPlain(ChrPtr(Line), aliaslen);
				Put(Aliases, 
				    IKEY(n),
				    Token, 
				    HFreeStrBuf);
				n++;
			}
	}
	else if (State == 550)
		AppendImportantMessage(_("Higher access is required to access this function."), -1);

	FreeStrBuf(&Line);

	return Aliases;
}


void AppendPossibleAliasWithDomain(
	HashList *PossibleAliases,
	long *nPossibleAliases,
	const HashList *Domains, 
	const char *prefix,
	long len,
	const char* Alias,
	long AliasLen)
{
	const StrBuf *OneDomain;
	StrBuf *Line;
	HashPos *It = NULL;
	const char *Key;
	long KLen;
	void *pV;
	int n;

	It = GetNewHashPos(Domains, 1);
	n = *nPossibleAliases;
	while (GetNextHashPos(Domains, It, &KLen, &Key, &pV))
	{
		OneDomain = (const StrBuf*) pV;
		Line = NewStrBuf();
		StrBufAppendBufPlain(Line, prefix, len, 0);
		StrBufAppendBufPlain(Line, Alias, AliasLen, 0);
		StrBufAppendBufPlain(Line, HKEY("@"), 0);
		StrBufAppendBuf(Line, OneDomain, 0);

		Put(PossibleAliases, 
		    IKEY(n),
		    Line, 
		    HFreeStrBuf);
		n++;
	}
	DeleteHashPos(&It);
	*nPossibleAliases = n;
}

HashList *GetThisRoomPossibleMAlias(StrBuf *Target, WCTemplputParams *TP) 
{
	wcsession *WCC = WC;
	HashList *Domains;
	StrBuf *Line;
	StrBuf *Token;
	StrBuf *RoomName;
	HashList *PossibleAliases = NULL;
	
	const char *pComma;
	const char *pAt;
	long aliaslen;
	long locallen;
	long State;
	long n = 0;

	Domains = GetValidDomainNames(Target, TP);
	if (Domains == NULL)
		return NULL;
	PossibleAliases = NewHash(1, NULL);
	Line = NewStrBuf();
	RoomName = NewStrBufDup(WCC->CurRoom.name);
	StrBufAsciify(RoomName, '_');
	StrBufReplaceChars(RoomName, ' ', '_');

	AppendPossibleAliasWithDomain(PossibleAliases,
				      &n,
				      Domains,
				      HKEY("room_"),
				      SKEY(RoomName));


	serv_puts("GNET "FILE_MAILALIAS);
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		int Done = 0;

		while(!Done && (StrBuf_ServGetln(Line) >= 0))
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000"))
			{
				Done = 1;
			}
			else
			{
				pComma = strchr(ChrPtr(Line), ',');
				if (pComma == NULL)
					continue;
				aliaslen = pComma - ChrPtr(Line);
				locallen = StrLength(Line) - 1 - aliaslen;
				if (locallen - 5 != StrLength(WCC->CurRoom.name))
					continue;
				if (strncmp(pComma + 1, "room_", 5) != 0)
					continue;

				if (strcasecmp(pComma + 6, ChrPtr(WCC->CurRoom.name)) != 0)
					continue;
				pAt = strchr(ChrPtr(Line), '@');
				if ((pAt == NULL) || (pAt > pComma))
				{
					AppendPossibleAliasWithDomain(PossibleAliases,
								      &n,
								      Domains,
								      HKEY(""),
								      ChrPtr(Line),
								      aliaslen);
					n++;
				}
				else
				{
					
					Token = NewStrBufPlain(ChrPtr(Line), aliaslen);
					Put(PossibleAliases,
					    IKEY(n),
					    Token,
					    HFreeStrBuf);
					n++;
				}
			}
	}
	else if (State == 550)
		AppendImportantMessage(_("Higher access is required to access this function."), -1);

	FreeStrBuf(&Line);
	FreeStrBuf(&RoomName);
	return PossibleAliases;
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
	
	WantThisOne = GetTemplateTokenNumber(Target, TP, 5, -1);
	if ((WantThisOne < 0) || (WantThisOne > maxRoomNetCfg))
		return NULL;
	if (WCC->CurRoom.IgnetCfgs[maxRoomNetCfg] == (HashList*) StrBufNOTNULL)
		return WCC->CurRoom.IgnetCfgs[WantThisOne];

	WCC->CurRoom.IgnetCfgs[maxRoomNetCfg] = (HashList*) StrBufNOTNULL;
	serv_puts("GNET");
	Line = NewStrBuf();
	Token = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatus(Line, &State) == 1) 
	{
		const char *Pos = NULL;
		int Done = 0;
		int HaveRoomMailAlias = 0;

		while(!Done && (StrBuf_ServGetln(Line) >= 0))
		{
			if (StrLength(Line) == 0)
				continue;
			if ( (StrLength(Line)==3) && 
			     !strcmp(ChrPtr(Line), "000"))
			{
				Done = 1;
			}
			else
			{
				StrBufExtract_NextToken(Token, Line, &Pos, '|');
				PutTo = GetTokenDefine(SKEY(Token), -1);
				if (PutTo == roommailalias)
				{
					if (HaveRoomMailAlias > 0)
						continue; /* Only ONE alias possible! */
					HaveRoomMailAlias++;
				}
				if ((PutTo >= 0) && 
				    (PutTo < maxRoomNetCfg) &&
				    (Pos != StrBufNOTNULL))
				{
					int n;
					HashList *SubH;
					
					if (WCC->CurRoom.IgnetCfgs[PutTo] == NULL)
					{
						n = 0;
						WCC->CurRoom.IgnetCfgs[PutTo] = NewHash(1, NULL);
					}
					else 
					{
						n = GetCount(WCC->CurRoom.IgnetCfgs[PutTo]);
					}
					SubH = NewHash(1, NULL);
					Put(WCC->CurRoom.IgnetCfgs[PutTo], 
					    IKEY(n),
					    SubH, 
					    HDeleteHash);
					n = 1; /* #0 is the type... */
					while (Pos != StrBufNOTNULL) {
						Content = NewStrBuf();
						StrBufExtract_NextToken(Content, Line, &Pos, '|');

						if ((PutTo == roommailalias) && n == 1)
							WCC->CurRoom.RoomAlias = Content;

						Put(SubH, 
						    IKEY(n),
						    Content, 
						    HFreeStrBuf);
						n++;
					}
				}
				Pos = NULL;
			}
		}
	}
	else if (State == 550)
		AppendImportantMessage(_("Higher access is required to access this function."), -1);

	FreeStrBuf(&Line);
	FreeStrBuf(&Token);

	return WCC->CurRoom.IgnetCfgs[WantThisOne];
}

/** Unused function that orders rooms by the listorder flag */
int SortRoomsByListOrder(const void *room1, const void *room2) 
{
	folder *r1 = (folder*) GetSearchPayload(room1);
	folder *r2 = (folder*) GetSearchPayload(room2);
  
	if (r1->Order == r2->Order) return 0;
	if (r1->Order > r2->Order) return 1;
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


int CompareRooms(const folder *room1, const folder *room2) 
{
	if ((room1 == NULL) || (room2 == NULL))
		return -1;
	return CompareRoomListByFloorRoomPrivFirst(room1, room2);
}

int ConditionalThisRoomIsStrBufContextAlias(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession       *WCC = WC;
	const char      *pVal;
	long             len;
	eRoomParamType   ParamType;

	ParamType = GetTemplateTokenNumber(Target, TP, 2, eNotSet);
	GetTemplateTokenString(Target, TP, 3, &pVal, &len);

	if (ParamType == eNotSet)
	{
		return StrLength(WCC->CurRoom.RoomAlias) == 0;
	}
	else if (ParamType == eDomain)
	{
		const StrBuf *CtxStr = (const StrBuf*) CTX(CTX_STRBUF);
		const char *pAt;

		if (CtxStr == NULL) 
			return 0;
		
		if (StrLength(WCC->CurRoom.RoomAlias) == 0)
			return 0;

		if (strncmp(ChrPtr(WCC->CurRoom.RoomAlias), "room_", 5) != 0)
			return 0;

		pAt = strchr(ChrPtr(WCC->CurRoom.RoomAlias), '@');
		if (pAt == NULL)
			return 0;
		return strcmp(pAt + 1, ChrPtr(CtxStr)) == 0;
	}
	else if (ParamType == eAlias)
	{
		const StrBuf *CtxStr = (const StrBuf*) CTX(CTX_STRBUF);

		if (CtxStr == NULL) 
			return 0;
		
		if (StrLength(WCC->CurRoom.RoomAlias) == 0)
			return 0;

		return strcmp(ChrPtr(WCC->CurRoom.RoomAlias), ChrPtr(CtxStr)) == 0;
	}
	else
	{
		LogTemplateError(Target, "TokenParameter", 2, TP, 
				 "Invalid paramtype; need one of [eNotSet|eDomain|eAlias]");
		return 0;
	}

}

int ConditionalRoomIsRESTSubRoom(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession  *WCC = WC;
	folder     *Folder = (folder *)CTX(CTX_ROOMS);
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

	syslog(LOG_DEBUG, "\n->%s: %d - %ld ", 
	       ChrPtr(Folder->name), 
	       urlp, 
	       Folder->nRoomNameParts);
	/* list only the floors which are in relation to the dav_depth header */
	if (WCC->Hdr->HR.dav_depth != delta) {
		syslog(LOG_DEBUG, "1\n");
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

				syslog(LOG_DEBUG, "3\n");
				return 0;
			}
			Dir = (StrBuf*) vDir;
			if (strcmp(ChrPtr(Folder->RoomNameParts[i]), 
				   ChrPtr(Dir)) != 0)
			{
				DeleteHashPos(&it);
				syslog(LOG_DEBUG, "4\n");
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
			
			syslog(LOG_DEBUG, "5\n");
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


void 
InitModule_ROOMLIST
(void)
{
	/* we duplicate this, just to be shure its already done. */
	RegisterCTX(CTX_ROOMS);
	RegisterCTX(CTX_FLOORS);

	RegisterIterator("ITERATE:THISROOM:WHO_KNOWS", 0, NULL, GetWhoKnowsHash, NULL, DeleteHash, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
	RegisterIterator("ITERATE:THISROOM:GNET", 1, NULL, GetNetConfigHash, NULL, NULL, CTX_STRBUFARR, CTX_NONE, IT_NOFLAG);
	RegisterIterator("ITERATE:THISROOM:MALIAS", 1, NULL, GetThisRoomMAlias, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);
	RegisterIterator("ITERATE:THISROOM:POSSIBLE:MALIAS", 1, NULL, GetThisRoomPossibleMAlias, NULL, NULL, CTX_STRBUF, CTX_NONE, IT_NOFLAG);

	RegisterIterator("LFLR", 0, NULL, GetFloorListHash, NULL, NULL, CTX_FLOORS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);
	RegisterIterator("LKRA", 0, NULL, GetRoomListHashLKRA, NULL, NULL, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);
	RegisterIterator("LZRM", 0, NULL, GetZappedRoomListHash, NULL, DeleteHash, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);
	RegisterIterator("LPRM", 0, NULL, GetRoomListHashLPRM, NULL, DeleteHash, CTX_ROOMS, CTX_NONE, IT_FLAG_DETECT_GROUPCHANGE);


	REGISTERTokenParamDefine(eNotSet);
	REGISTERTokenParamDefine(eDomain);
	REGISTERTokenParamDefine(eAlias);


	RegisterConditional("COND:ROOM:REST:ISSUBROOM", 0, ConditionalRoomIsRESTSubRoom, CTX_ROOMS);

	RegisterConditional("COND:THISROOM:ISALIAS:CONTEXTSTR", 0, ConditionalThisRoomIsStrBufContextAlias, CTX_NONE);

	RegisterSortFunc(HKEY("byfloorroom"),
			 NULL, 0,
			 CompareRoomListByFloorRoomPrivFirst,
			 CompareRoomListByFloorRoomPrivFirstRev,
			 GroupchangeRoomListByFloorRoomPrivFirst,
			 CTX_ROOMS);

}
