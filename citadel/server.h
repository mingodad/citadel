/* $Id$ */


#ifndef SERVER_H
#define SERVER_H

#ifdef __GNUC__
#define INLINE __inline__
#else
#define INLINE
#endif

#include "citadel.h"
#ifdef HAVE_OPENSSL
#define OPENSSL_NO_KRB5		/* work around redhat b0rken ssl headers */
#include <openssl/ssl.h>
#endif

/*
 * New format for a message in memory
 */
struct CtdlMessage {
	int cm_magic;			/* Self-check (NOT SAVED TO DISK) */
	char cm_anon_type;		/* Anonymous or author-visible */
	char cm_format_type;		/* Format type */
	char *cm_fields[256];		/* Data fields */
	unsigned int cm_flags;		/* How to handle (NOT SAVED TO DISK) */
};

#define	CTDLMESSAGE_MAGIC		0x159d
#define	CM_SKIP_HOOKS	0x01		/* Don't run server-side handlers */


/*
 * Here's the big one... the Citadel context structure.
 *
 * This structure keeps track of all information relating to a running 
 * session on the server.  We keep one of these for each session thread.
 *
 * Note that the first element is "*next" so that it may be used without
 * modification in a linked list.
 */
struct CitContext {
	struct CitContext *prev;	/* Link to previous session in list */
	struct CitContext *next;	/* Link to next session in the list */

	int state;		/* thread state (see CON_ values below) */
	int kill_me;		/* Set to nonzero to flag for termination */
	int client_socket;
	int cs_pid;		/* session ID */
	time_t lastcmd;		/* time of last command executed */
	time_t lastidle;	/* For computing idle time */

	char curr_user[USERNAME_SIZE];	/* name of current user */
	int logged_in;		/* logged in */
	int internal_pgm;	/* authenticated as internal program */
	int nologin;		/* not allowed to log in */
	int is_local_socket;	/* set to 1 if client is on unix domain sock */
	int curr_view;		/* The view type for the current user/room */

	char net_node[32]	;/* Is the client another Citadel server? */
	time_t previous_login;	/* Date/time of previous login */
	char lastcmdname[5];	/* name of last command executed */
	unsigned cs_flags;	/* miscellaneous flags */
	void (*h_command_function) (void) ;	/* service command function */
	void (*h_async_function) (void) ;	/* do async msgs function */
	int is_async;		/* Nonzero if client accepts async msgs */
	int async_waiting;	/* Nonzero if there are async msgs waiting */
	int input_waiting;	/* Nonzero if there is client input waiting */

	/* feeping creaturisms... */
	int cs_clientdev;	/* client developer ID */
	int cs_clienttyp;	/* client type code */
	int cs_clientver;	/* client version number */
	char cs_clientname[32];	/* name of client software */
	char cs_host[64];	/* host logged in from */
	char cs_addr[64];	/* address logged in from */

	/* The Internet type of thing */
	char cs_inet_email[128];/* Return address of outbound Internet mail */

	FILE *download_fp;	/* Fields relating to file transfer */
	char download_desired_section[128];
	FILE *upload_fp;
	char upl_file[256];
	char upl_path[PATH_MAX];
	char upl_comment[256];
	char upl_filedir[PATH_MAX];
	char dl_is_net;
	char upload_type;

	struct ctdluser user;	/* Database record buffers */
	struct ctdlroom room;

	/* Beginning of cryptography - session nonce */
	char cs_nonce[NONCE_SIZE];	/* The nonce for this session's next auth transaction */

	/* Redirect this session's output to a memory buffer? */
	char *redirect_buffer;		/* the buffer */
	size_t redirect_len;		/* length of data in buffer */
	size_t redirect_alloc;		/* length of allocated buffer */
#ifdef HAVE_OPENSSL
	SSL *ssl;
	int redirect_ssl;
#endif

	int buffering;
	char *output_buffer;	/* hold output for one big dump */
	int buffer_len;

	/* A linked list of all instant messages sent to us. */
	struct ExpressMessage *FirstExpressMessage;
	int disable_exp;	/* Set to 1 to disable incoming pages */
	int newmail;		/* Other sessions increment this */

	/* Masquerade... */
	char fake_username[USERNAME_SIZE];	/* Fake username <bc> */ 
	char fake_postname[USERNAME_SIZE];	/* Fake postname <bc> */
	char fake_hostname[64];			/* Fake hostname <bc> */
	char fake_roomname[ROOMNAMELEN];	/* Fake roomname <bc> */

	char preferred_formats[256];		/* Preferred MIME formats */

	/* Dynamically allocated session data */
	struct citimap *IMAP;
	struct citpop3 *POP3;
	struct citsmtp *SMTP;
	char *SMTP_RECPS;
	char *SMTP_ROOMS;
	struct cit_ical *CIT_ICAL;		/* calendaring data */
	struct ma_info *ma;			/* multipart/alternative data */
};

typedef struct CitContext t_context;

/*
 * Values for CitContext.state
 * 
 * A session that is doing nothing is in CON_IDLE state.  When activity
 * is detected on the socket, it goes to CON_READY, indicating that it
 * needs to have a worker thread bound to it.  When a thread binds to
 * the session, it goes to CON_EXECUTING and does its thing.  When the
 * transaction is finished, the thread sets it back to CON_IDLE and lets
 * it go.
 */
enum {
	CON_IDLE,		/* This context is doing nothing */
	CON_READY,		/* This context needs attention */
	CON_EXECUTING		/* This context is bound to a thread */
};


#define CS_STEALTH	1	/* stealth mode */
#define CS_CHAT		2	/* chat mode */
#define CS_POSTING	4	/* Posting */

struct CitContext *MyContext(void);
#define CC MyContext()

/*
 * This is the control record for the message base... 
 */
struct CitControl {
	long MMhighest;			/* highest message number in file   */
	unsigned MMflags;		/* Global system flags              */
	long MMnextuser;		/* highest user number on system    */
	long MMnextroom;		/* highest room number on system    */
	int version;			/* Server-hosted upgrade level      */
	int fulltext_wordbreaker;	/* ID of wordbreaker in use         */
	long MMfulltext;		/* highest message number indexed   */
};

extern struct CitContext *ContextList;
extern int ScheduledShutdown;
extern struct CitControl CitControl;


struct ExpressMessage {
	struct ExpressMessage *next;
	time_t timestamp;	/* When this message was sent */
	unsigned flags;		/* Special instructions */
	char sender[64];	/* Name of sending user */
	char *text;		/* Message text (if applicable) */
};

#define EM_BROADCAST	1	/* Broadcast message */
#define EM_GO_AWAY	2	/* Server requests client log off */
#define EM_CHAT		4	/* Server requests client enter chat */

struct ChatLine {
	struct ChatLine *next;
	int chat_seq;
	time_t chat_time;
	char chat_text[SIZ];
	char chat_username[USERNAME_SIZE];
	char chat_room[ROOMNAMELEN];
};

/*
 * Various things we need to lock and unlock
 */
enum {
	S_USERS,
	S_ROOMS,
	S_SESSION_TABLE,
	S_FLOORTAB,
	S_CHATQUEUE,
	S_CONTROL,
	S_NETDB,
	S_SUPPMSGMAIN,
	S_CONFIG,
	S_WORKER_LIST,
	S_HOUSEKEEPING,
	S_NTTLIST,
	S_DIRECTORY,
	S_NETCONFIGS,
	S_PUBLIC_CLIENTS,
	S_LDAP,
	S_FLOORCACHE,
	S_DEBUGMEMLEAKS,
	S_ATBF,
	S_JOURNAL_QUEUE,
	MAX_SEMAPHORES
};


/*
 * Upload types
 */
#define UPL_FILE	0
#define UPL_NET		1
#define UPL_IMAGE	2


/*
 * message transfer formats
 */
enum {
	MT_CITADEL,		/* Citadel proprietary */
	MT_RFC822,		/* RFC822 */
	MT_MIME,		/* MIME-formatted message */
	MT_DOWNLOAD		/* Download a component */
};

/*
 * Message format types in the database
 */
#define	FMT_CITADEL	0	/* Citadel vari-format (proprietary) */
#define FMT_FIXED	1	/* Fixed format (proprietary)        */
#define FMT_RFC822	4	/* Standard (headers are in M field) */


/*
 * Citadel DataBases (define one for each cdb we need to open)
 */
enum {
	CDB_MSGMAIN,		/* message base                  */
	CDB_USERS,		/* user file                     */
	CDB_ROOMS,		/* room index                    */
	CDB_FLOORTAB,		/* floor index                   */
	CDB_MSGLISTS,		/* room message lists            */
	CDB_VISIT,		/* user/room relationships       */
	CDB_DIRECTORY,		/* address book directory        */
	CDB_USETABLE,		/* network use table             */
	CDB_BIGMSGS,		/* larger message bodies         */
	CDB_FULLTEXT,		/* full text search index        */
	CDB_EUIDINDEX,		/* locate msgs by EUID           */
	MAXCDB			/* total number of CDB's defined */
};

struct cdbdata {
	size_t len;
	char *ptr;
};



/* Structures and declarations for function hooks of various types */

struct LogFunctionHook {
	struct LogFunctionHook *next;
	int loglevel;
	void (*h_function_pointer) (char *);
};
extern struct LogFunctionHook *LogHookTable;

struct CleanupFunctionHook {
	struct CleanupFunctionHook *next;
	void (*h_function_pointer) (void);
};
extern struct CleanupFunctionHook *CleanupHookTable;

struct FixedOutputHook {
	struct FixedOutputHook *next;
	char content_type[64];
	void (*h_function_pointer) (char *, int);
};
extern struct FixedOutputHook *FixedOutputTable;



/*
 * SessionFunctionHook extensions are used for any type of hook for which
 * the context in which it's being called (which is determined by the event
 * type) will make it obvious for the hook function to know where to look for
 * pertinent data.
 */
struct SessionFunctionHook {
	struct SessionFunctionHook *next;
	void (*h_function_pointer) (void);
	int eventtype;
};
extern struct SessionFunctionHook *SessionHookTable;

/* 
 * Event types can't be enum'ed, because they must remain consistent between
 * builds (to allow for binary modules built somewhere else)
 */
#define EVT_STOP	0	/* Session is terminating */
#define EVT_START	1	/* Session is starting */
#define EVT_LOGIN	2	/* A user is logging in */
#define EVT_NEWROOM	3	/* Changing rooms */
#define EVT_LOGOUT	4	/* A user is logging out */
#define EVT_SETPASS	5	/* Setting or changing password */
#define EVT_CMD		6	/* Called after each server command */
#define EVT_RWHO	7	/* An RWHO command is being executed */
#define EVT_ASYNC	8	/* Doing asynchronous messages */

#define EVT_TIMER	50	/* Timer events are called once per minute
				   and are not tied to any session */

/*
 * UserFunctionHook extensions are used for any type of hook which implements
 * an operation on a user or username (potentially) other than the one
 * operating the current session.
 */
struct UserFunctionHook {
	struct UserFunctionHook *next;
	void (*h_function_pointer) (struct ctdluser *usbuf);
	int eventtype;
};
extern struct UserFunctionHook *UserHookTable;

#define EVT_PURGEUSER	100	/* Deleting a user */
#define EVT_NEWUSER	102	/* Creating a user */

/*
 * MessageFunctionHook extensions are used for hooks which implement handlers
 * for various types of message operations (save, read, etc.)
 */
struct MessageFunctionHook {
	struct MessageFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg);
	int eventtype;
};
extern struct MessageFunctionHook *MessageHookTable;

#define EVT_BEFOREREAD	200
#define EVT_BEFORESAVE	201
#define EVT_AFTERSAVE	202
#define EVT_SMTPSCAN	203	/* called before submitting a msg from SMTP */



/*
 * NetprocFunctionHook extensions are used for hooks which implement handlers
 * for incoming network messages.
 */
struct NetprocFunctionHook {
	struct NetprocFunctionHook *next;
	int (*h_function_pointer) (struct CtdlMessage *msg, char *target_room);
};
extern struct NetprocFunctionHook *NetprocHookTable;


/*
 * DeleteFunctionHook extensions are used for hooks which get called when a
 * message is about to be deleted.
 */
struct DeleteFunctionHook {
	struct DeleteFunctionHook *next;
	void (*h_function_pointer) (char *target_room, long msgnum);
};
extern struct DeleteFunctionHook *DeleteHookTable;


/*
 * ExpressMessageFunctionHook extensions are used for hooks which implement
 * the sending of an instant message through various channels.  Any function
 * registered should return the number of recipients to whom the message was
 * successfully transmitted.
 */
struct XmsgFunctionHook {
	struct XmsgFunctionHook *next;
	int (*h_function_pointer) (char *, char *, char *);
	int order;
};
extern struct XmsgFunctionHook *XmsgHookTable;

/* Priority levels for paging functions (lower is better) */
enum {
	XMSG_PRI_LOCAL,		/* Other users on -this- server */
	XMSG_PRI_REMOTE,	/* Other users on a Citadel network (future) */
	XMSG_PRI_FOREIGN,	/* Contacts on foreign instant message hosts */
	MAX_XMSG_PRI
};



/*
 * ServiceFunctionHook extensions are used for hooks which implement various
 * non-Citadel services (on TCP protocols) directly in the Citadel server.
 */
struct ServiceFunctionHook {
	struct ServiceFunctionHook *next;
	int tcp_port;
	char *sockpath;
	void (*h_greeting_function) (void) ;
	void (*h_command_function) (void) ;
	void (*h_async_function) (void) ;
	int msock;
};
extern struct ServiceFunctionHook *ServiceHookTable;



/* Defines the relationship of a user to a particular room */
struct visit {
	long v_roomnum;
	long v_roomgen;
	long v_usernum;
	long v_lastseen;
	unsigned int v_flags;
	char v_seen[SIZ];
	char v_answered[SIZ];
	int v_view;
};

#define V_FORGET	1	/* User has zapped this room        */
#define V_LOCKOUT	2	/* User is locked out of this room  */
#define V_ACCESS	4	/* Access is granted to this room   */


/* Supplementary data for a message on disk
 * These are kept separate from the message itself for one of two reasons:
 * 1. Either their values may change at some point after initial save, or
 * 2. They are merely caches of data which exist somewhere else, for speed.
 */
struct MetaData {
	long meta_msgnum;		/* Message number in *local* message base */
	int meta_refcount;		/* Number of rooms pointing to this msg */
	char meta_content_type[64];	/* Cached MIME content-type */
	long meta_rfc822_length;	/* Cache of RFC822-translated msg length */
};


/* 
 * Serialization routines use this struct to return a pointer and a length
 */
struct ser_ret {
        size_t len;
        unsigned char *ser;
};


/* Preferred field order */
/*               **********			Important fields */
/*                         ***************	Semi-important fields */
/*                                        * 	Message text (MUST be last) */
#define FORDER	"IPTAFONHRDBCEJGKLQSVWXZYUM"

#endif /* SERVER_H */
