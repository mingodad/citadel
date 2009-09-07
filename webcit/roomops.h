
#define VIRTUAL_MY_FLOOR -1

/*
 * \brief This struct holds a list of rooms for \\\<G\\\>oto operations.
 */
struct march {
	struct march *next;       /* pointer to next in linked list */
	char march_name[128];     /* name of room */
	int march_floor;          /* floor number of room */
	int march_order;          /* sequence in which we are to visit this room */
};

/* *
 * \brief	This struct holds a list of rooms for client display.
 *		It is a binary tree.
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
	long ID;
	StrBuf *Name;
	long NRooms;
	long AlphaN;
} floor;

/**
 * \brief  Data structure for roomlist-to-folderlist conversion 
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



/**
 * \brief  Data structure for roomlist-to-folderlist conversion 
 */
typedef struct _folder {
	/* Data citserver tells us about the room */
	StrBuf *name;	/* the full name of the room we're talking about */
	int QRFlags;    /* roomflags */
	int floorid;      /* which floor is it on */

	int listorder; /* todo */
	int QRFlags2;    /* Bitbucket NO2 */

	long RAFlags;
///	StrBuf *ACL;    /* access Control list... */

	int view;       /* whats its default view? inbox/calendar.... */
	int defview;
	int lastchange; /* todo... */

	/* later evaluated data from the serverdata */
	long nRoomNameParts;
	StrBuf **RoomNameParts;

	const floor *Floor;   /* pint to the floor we're on.. */
	StrBuf *room;	/* which roomname ??? */


	int hasnewmsgs;	/* are there unread messages inside */
	int is_mailbox;	/* is it a mailbox?  */
	int selectable;	/* can we select it ??? */
	int num_rooms;	/* If this is a floor, how many rooms does it have */

}folder;

