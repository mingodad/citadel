/* $Id$ */
typedef pthread_t THREAD;

/* Uncomment this if you want to track memory leaks.
 * This incurs some overhead, so don't use it unless you're debugging the code!
 */
#define DEBUG_MEMORY_LEAKS


/*
 * Generic per-session variable or data structure storage
 */
struct CtdlSessData {
	struct CtdlSessData *next;
	unsigned long sym_id;
	void *sym_data;
};

/*
 * Static user data symbol types
 */
enum {
	SYM_DESIRED_SECTION,		/* Used by the MIME parser */
	SYM_MAX
};


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
	struct CitContext *next;	/* Link to next session in the list */

	struct usersupp usersupp;	/* Database record buffers */
	struct quickroom quickroom;

	char curr_user[32];	/* name of current user */
	int logged_in;		/* logged in */
	int internal_pgm;	/* authenticated as internal program */
	char temp[32];		/* temp file name */
	int nologin;		/* not allowed to log in */

	char net_node[32];
	THREAD mythread;
	int n_crit;		/* number of critical sections open */
	int client_socket;
	int cs_pid;		/* session ID */
	char cs_room[ROOMNAMELEN];	/* current room */
	time_t cs_lastupdt;	/* time of last update */
	time_t lastcmd;		/* time of last command executed */
	time_t lastidle;	/* For computing idle time */
	char lastcmdname[5];	/* name of last command executed */
	unsigned cs_flags;	/* miscellaneous flags */

	/* feeping creaturisms... */
	int cs_clientdev;	/* client developer ID */
	int cs_clienttyp;	/* client type code */
	int cs_clientver;	/* client version number */
	char cs_clientname[32];	/* name of client software */
	char cs_host[25];	/* host logged in from */

	FILE *download_fp;	/* Fields relating to file transfer */
	FILE *upload_fp;
	char upl_file[256];
	char upl_path[256];
	char upl_comment[256];
	char upl_filedir[256];
	char chat_room[20];	/* The chat room */
	char dl_is_net;
	char upload_type;

	struct ExpressMessage *FirstExpressMessage;

	char fake_username[32];	/* Fake username <bc>                */
	char fake_postname[32];	/* Fake postname <bc>                */
	char fake_hostname[25];	/* Name of the fake hostname <bc>    */
	char fake_roomname[ROOMNAMELEN];	/* Name of the fake room <bc> */

	int FloorBeingSearched;	/* This is used by cmd_lrms() etc.   */
	struct CtdlSessData *FirstSessData;
};

typedef struct CitContext t_context;

#define CS_STEALTH	1	/* stealth mode */
#define CS_CHAT		2	/* chat mode */
#define CS_POSTING	4	/* Posting */

struct CitContext *MyContext(void);
#define CC ((struct CitContext *)MyContext())

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
	char chat_text[256];
	char chat_room[20];
	char chat_username[32];
};

/*
 * Various things we need to lock and unlock
 */
enum {
	S_USERSUPP,
	S_USER_TRANS,
	S_QUICKROOM,
	S_MSGMAIN,
	S_CALLLOG,
	S_SESSION_TABLE,
	S_FLOORTAB,
	S_CHATQUEUE,
	S_CONTROL,
	S_HOUSEKEEPING,
	S_DATABASE,
	S_NETDB,
	S_SUPPMSGMAIN,
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
 * Citadel DataBases (define one for each cdb we need to open)
 */
enum {
	CDB_MSGMAIN,		/* message base                  */
	CDB_USERSUPP,		/* user file                     */
	CDB_QUICKROOM,		/* room index                    */
	CDB_FLOORTAB,		/* floor index                   */
	CDB_MSGLISTS,		/* room message lists            */
	CDB_VISIT,		/* user/room relationships       */
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





/*
 * UserFunctionHook extensions are used for any type of hook which implements
 * an operation on a user or username (potentially) other than the one
 * operating the current session.
 */
struct UserFunctionHook {
	struct UserFunctionHook *next;
	void (*h_function_pointer) (char *username, long usernum);
	int eventtype;
};
extern struct UserFunctionHook *UserHookTable;

#define EVT_PURGEUSER	100	/* Deleting a user */
#define EVT_OUTPUTMSG	101	/* Outputting a message */




/*
 * ExpressMessageFunctionHook extensions are used for hooks which implement
 * the sending of an express message through various channels.  Any function
 * registered should return the number of recipients to whom the message was
 * successfully transmitted.
 */
struct XmsgFunctionHook {
	struct XmsgFunctionHook *next;
	int (*h_function_pointer) (char *, char *, char *);
};
extern struct XmsgFunctionHook *XmsgHookTable;






/* Defines the relationship of a user to a particular room */
struct visit {
	long v_roomnum;
	long v_roomgen;
	long v_usernum;
	long v_lastseen;
	unsigned int v_flags;
};

#define V_FORGET	1	/* User has zapped this room        */
#define V_LOCKOUT	2	/* User is locked out of this room  */
#define V_ACCESS	4	/* Access is granted to this room   */

#define UA_KNOWN                2
#define UA_GOTOALLOWED          4
#define UA_HASNEWMSGS           8
#define UA_ZAPPED		16


/* Supplementary data for a message on disk
 * (These are kept separately from the message itself because they are
 * fields whose values may change at some point after the message is saved.)
 */
struct SuppMsgInfo {
	long smi_msgnum;	/* Message number in *local* message base */
	int smi_refcount;	/* Number of rooms which point to this msg */
	char smi_content_type[64];
	/* more stuff will be added to this record in the future */
};



/* Built-in debuggable stuff for checking for memory leaks */
#ifdef DEBUG_MEMORY_LEAKS

#define mallok(howbig)		tracked_malloc(howbig, __FILE__, __LINE__)
#define phree(whichptr)			tracked_free(whichptr)
#define reallok(whichptr,howbig)	tracked_realloc(whichptr,howbig)
#define strdoop(orig)		tracked_strdup(orig, __FILE__, __LINE__)

void *tracked_malloc(size_t, char *, int);
void tracked_free(void *);
void *tracked_realloc(void *, size_t);
void dump_tracked(void);
char *tracked_strdup(const char *, char *, int);

struct TheHeap {
	struct TheHeap *next;
	char h_file[32];
	int h_line;
	void *h_ptr;
};

extern struct TheHeap *heap;

#else

#define mallok(howbig)			malloc(howbig)
#define phree(whichptr)			free(whichptr)
#define reallok(whichptr,howbig)	realloc(whichptr,howbig)
#define strdoop(orig)			strdup(orig)


#endif


/*
 * New format for a message in memory
 */
#define	CTDLMESSAGE_MAGIC		0x159d
struct CtdlMessage {
	int cm_magic;			/* Self-check */
	char cm_anon_type;		/* Anonymous or author-visible */
	char cm_format_type;		/* Format type */
	char *cm_fields[256];		/* Data fields */
};
