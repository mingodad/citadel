/**
 ** CxClient - Improved Citadel/UX Client
 ** Copyright (c) 2000, SCCG/Flaming Sword Productions
 ** All Rights Reserved
 **
 ** Module: CxClient.h
 ** Date: 2000-12-14
 ** Last Revision: 2000-12-14
 ** Description: Global CxClient library header.
 ** CVS: $Id$
 **/
#ifndef		__CXCLIENT_H__
#define		__CXCLIENT_H__

#define		CXREVISION		"$Id$"

#define		RC_LISTING		100	// LISTING_FOLLOWS
#define		RC_OK			200	// OK
#define		RC_MOREDATA		300	// MORE_DATA
#define		RC_SENDLIST		400	// SEND_LISTING
#define		RC_ERROR		500	// ERROR
#define		RC_RECVBIN		600	// BINARY_FOLLOWS
#define		RC_SENDBIN		700	// SEND_BINARY
#define		RC_STARTCHAT		800	// START_CHAT_MODE
#define		RC_ASYNCMSG		900	// Asynchronous Message
#define		RC_AEXPMSG		901	// Express Message Follows
#define		RC_MESGWAIT		1000	// Message Waiting

/**
 ** The CHECKRC() macro is used to compare a Result Code 'x' with
 ** the expected result 'y'.
 **/
#define		CHECKRC(x,y)		((x - (x % y)) == y)

#ifdef		DEBUG

#define		DFA		__FILE__,__LINE__,__FUNCTION__
#define		DPF(a)		CxDebug a

#else

#define		DFA		NULL
#define		DPF(a)	

#endif

typedef
struct          _file_info {

 char		name[255];
 long unsigned
 int		size;
 char		descr[255];

 /**
  ** Reserved for future use.  Cit/UX may not
  ** support this yet....
  **/
 long unsigned
 int		owner,
 		group;
 short int	mode;

} FILEINFO;

/**
 ** Linked-List Structure
 **/
typedef struct	_linked_list {
	char	*data;
	struct
	_linked_list *next;
} CLIST;
typedef CLIST*	CXLIST;

typedef
struct          _message_info {
 long unsigned
 int		message_id;
 char           author[255],
                rcpt[255],
                subject[255],
                room[255],
                path[255],
                node[255],
                date[255];
 char           *body;
} MESGINFO;

/**
 ** struct ROOMINFO: This structure contains all information related
 ** to a room.
 **/
typedef
struct		_room_info {
 char		name[255];
 long unsigned
 int		msgs_unread,
		msgs_total;
 short int	info_flag;
 long unsigned
 int		flags,
		msgs_highest,
		msgs_highest_u;
 short int	mailroom,
		aide,
		mode;
 long unsigned
 int		msgs_newmail,
		floor_id;
} ROOMINFO;

/**
 ** struct USERINFO: This structure contains all information related
 ** to a user account.
 **/
typedef
struct		_user_info {
 char		username[255],
 		fullname[255],
		password[255];

 /**
  ** USERINFO.addr: Address information.
  **/
 struct _a {
  char		street[255],
		city[255],
		st[255],
		zip[255];
 } addr;

 /**
  ** USERINFO.contact: Contact information.
  **/
 struct _c {
  char		telephone[255],
		emailaddr[255];
 } contact;

 /**
  ** USERINFO.system: System information.  Should not be modified.
  **/
 struct _sys {
  int		access_level;
  long unsigned
  int		times_called,
		messages_posted,
		user_flags,
		user_number;
 } system;

} USERINFO;

/**
 ** struct _Cmd_Callback: This record contains information regarding Server->Client
 ** message callbacks.  The general rule is such: IF the client wishes to handle
 ** certain types of Server-to-Client traffic [currently unimplemented], the author
 ** should register a Callback function to handle the command sent by the server.
 ** The client can register a callback function for a single message type, or for
 ** a specific data session (like, to handle a file download). Similarly, if a
 ** client wishes to stop processing a certain type of message, the client can
 ** deregister the command's callback.
 **
 ** Any messages received which do not have an attached callback function will be
 ** ignored.
 **/
typedef
struct          _Cmd_Callback {

 int            cmd;            // Command sent from server.  [9xx]
 char           session[10];    // Optional session id.
 void           (*Function)(void *); // Function to call upon success.

 struct _Cmd_Callback *next;

} CXCSCALLBACK;
typedef CXCSCALLBACK* CXCBHNDL;

#ifdef		__cplusplus
extern "C" {
#endif

float		CxRevision();
void		CxSerialize(const char *, char **);

/**
 ** Client/Server Communications
 **/
void		CxClRegClient(const char *);
int		CxClConnect(const char *);
void		CxClDisconnect();
int		CxClStat();
void		CxClSend(const char *s);
int		CxClRecv(char *s);
int		CxClChatInit();
void		CxClChatShutdown();
int		CxClCbRegister(int, void *);
void		CxClCbShutdown();
void		CxClCbRemove(int);
CXCBHNDL	CxClCbExists(int);

/**
 ** File Input/Output
 **/
CXLIST		CxFiIndex();
int		CxFiPut(FILEINFO, int);
char		*CxFiGet(const char *);

/**
 ** Message Input/Output
 **/
CXLIST		CxMsInfo(CXLIST);
CXLIST		CxMsList();
int		CxMsLoad(const char *, int, MESGINFO *);
int		CxMsSaveOk(const char *);
int		CxMsSave(MESGINFO);

/**
 ** Room/Floor Commands
 **/
ROOMINFO	*CxRmGoto(const char *, int);
CXLIST		CxRmList();
CXLIST		CxFlList();
int		CxRmCreate(ROOMINFO);

/**
 ** Miscellaneous Commands
 **/
int		CxMiExpSend(const char *, const char *);
char		*CxMiExpRecv();
int		CxMiExpCheck();
void		CxMiExpHook(void (*)(const char *, const char*));
char		*CxMiMessage(const char *);
char		*CxMiImage(const char *);

/**
 ** Linked-List Handlers
 **/
CXLIST		CxLlInsert(CXLIST, char *);
CXLIST		CxLlRemove(CXLIST, unsigned int);
CXLIST		CxLlFlush(CXLIST);

/**
 ** User Info Commands
 **/
CXLIST          CxUsOnline(int);
CXLIST          CxUsList();
int		CxUsCreate(USERINFO);
USERINFO	*CxUsAuth(const char *, const char *);

#ifdef		__cplusplus
} // extern "C"
#endif

#endif
