/*
 * Copyright (c) 1996-2013 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#define VIRTUAL_MY_FLOOR -1

/*
 * This struct holds a list of rooms for "Goto" operations.
 */
struct march {
	struct march *next;       /* pointer to next in linked list */
	char march_name[128];     /* name of room */
	int march_floor;          /* floor number of room */
	int march_order;          /* sequence in which we are to visit this room */
};

/*
 * This struct holds a list of rooms for client display. It is a binary tree.
 */
struct roomlisting {
	struct roomlisting *lnext;	/* pointer to 'left' tree node */
	struct roomlisting *rnext;	/* pointer to 'right' tree node */
	char rlname[128];		/* name of room */
	unsigned rlflags;		/* room flags */
	int rlfloor;			/* the floor it resides on */
	int rlorder;			/* room listing order */
};





typedef struct _floor {
	int ID;
	StrBuf *Name;
	long NRooms;
	long AlphaN;
} Floor;

/*
 * Data structure for roomlist-to-folderlist conversion 
 */
struct __ofolder {
	int floor;      /* which floor is it on */
	char room[SIZ];	/* which roomname ??? */
	char name[SIZ];	/* which is its own name??? */
	int hasnewmsgs;	/* are there unread messages inside */
	int is_mailbox;	/* is it a mailbox?  */
	int selectable;	/* can we select it ??? */
	int view;       /* whats its default view? inbox/calendar.... */
	int num_rooms;	/* If this is a floor, how many rooms does it have */
};



/*
 * Data structure for roomlist-to-folderlist conversion 
 */
typedef struct _folder {
	/* Data citserver tells us about the room */
	long QRFlags;    /* roomflags */
	long QRFlags2;    /* Bitbucket NO2 */
	long RAFlags;

	int view;       /* whats its default view? inbox/calendar.... */
	long defview;
	long lastchange; /* todo... */

	/* later evaluated data from the serverdata */
	StrBuf *name;	/* the full name of the room we're talking about */
	long nRoomNameParts;
	StrBuf **RoomNameParts;

	int floorid;      /* which floor is it on */
	const Floor *Floor;   /* point to the floor we're on.. */

	int hasnewmsgs;	/* are there unread messages inside */
	int is_inbox;	/* is it a mailbox?  */

	int RoomAideLoaded;
	StrBuf *RoomAide;

/* only available if GNET contains this */
	const StrBuf* RoomAlias; /* by what mail will this room send mail? */

/* only available if GETR was run */
	int XALoaded;
	StrBuf *XAPass;
	StrBuf *Directory;
	long Order;

/* Only available from the GOTO context... */
	long nNewMessages;
	long nTotalMessages;
	long LastMessageRead;
	long HighestRead;
	int ShowInfo;
	int UsersNewMAilboxMessages; /* should we notify the user about new messages? */
	int IsTrash;
/* Only available if certain other commands ran */
	int XHaveRoomPic;
	int XHaveRoomPicLoaded;

	int XHaveInfoTextLoaded;
	StrBuf *XInfoText;

	int XHaveDownloadCount;
	int XDownloadCount;
	
	int BumpUsers; /* if SETR set to 1 to make all users who knew this room to forget about it. */

	HashList *IgnetCfgs[maxRoomNetCfg + 1];
} folder;

HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP);
void vDeleteFolder(void *vFolder);
void FlushFolder(folder *room);
void FlushIgnetCfgs(folder *room);
void ParseGoto(folder *proom, StrBuf *Line);
void FlushRoomlist(void); /* release our caches, so a deleted / zapped room disapears */
void ReloadCurrentRoom(void); /* Flush cache; reload current state */

HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP);
HashList *GetRoomListHash(StrBuf *Target, WCTemplputParams *TP);
int SortRoomsByListOrder(const void *room1, const void *room2);
void tmplput_roombanner(StrBuf *Target, WCTemplputParams *TP);


void LoadRoomAide(void);
void LoadRoomXA (void);
void LoadXRoomPic(void);
void LoadXRoomInfoText(void);
void LoadXRoomXCountFiles(void);

long gotoroom(const StrBuf *gname);

void slrp_highest(void);
void remove_march(const StrBuf *aaa);
void dotskip(void);
void smart_goto(const StrBuf *next_room);
void free_march_list(wcsession *wcf);

/*
 * wrapper around usual sort-comparator; private rooms will allways be prefered, -1 if one of them NULL
 */
int CompareRooms(const folder *room1, const folder *room2);


#define REST_TOPLEVEL 0
#define REST_IN_NAMESPACE (1<<0)
#define REST_IN_FLOOR (1<<1)
#define REST_IN_ROOM (1<<2)
#define REST_HAVE_SUB_ROOMS (1<<3)
#define REST_GOT_LOCAL_PART (1<<4)
#define REST_NONEXIST (1<<5)


extern CtxType CTX_ROOMS;
extern CtxType CTX_FLOORS;
