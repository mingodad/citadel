typedef pthread_t THREAD;


struct ExpressMessage {
	struct ExpressMessage *next;
	char em_text[300];
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
	
	long *msglist;
	int num_msgs;

        char curr_user[32];		/* name of current user */
        int logged_in;			/* logged in */
        int internal_pgm;		/* authenticated as internal program */
        char temp[32];			/* temp file name */
        int nologin;			/* not allowed to log in */

        char net_node[32];
	THREAD mythread;
	int client_socket;
	struct ExpressMessage *FirstExpressMessage;
	int cs_pid;			/* session ID */
	char cs_room[20];		/* current room */
	time_t cs_lastupdt;		/* time of last update */
	time_t lastcmd;			/* time of last command executed */
	time_t lastidle;		/* For computing idle time */
	char lastcmdname[5];		/* name of last command executed */
	unsigned cs_flags;		/* miscellaneous flags */

					/* feeping creaturisms... */
	int cs_clientdev;		/* client developer ID */
	int cs_clienttyp;		/* client type code */
	int cs_clientver;		/* client version number */
	char cs_clientname[32];		/* name of client software */
	char cs_host[25];		/* host logged in from */

        FILE *download_fp;		/* Fields relating to file transfer */
        FILE *upload_fp;
	char upl_file[256];
	char upl_path[256];
	char upl_comment[256];
	char upl_filedir[256];
	char chat_room[20];		/* The chat room */
	char dl_is_net;
	char upload_type;

	char ucache_name[32];		/* For a performance boost, we cache */
	long ucache_pos;		/* the position of the last user rec */
	char fake_username[32];		/* Fake username <bc>                */
	char fake_postname[32];		/* Fake postname <bc>                */
	char fake_hostname[25];		/* Name of the fake hostname <bc>    */
	char fake_roomname[20];		/* Name of the fake room <bc>        */
	char last_pager[32];		/* The username of the last pager    */

	int FloorBeingSearched;		/* This is used by cmd_lrms() etc.   */

	int CtdlErrno;			/* Error return for CitadelAPI calls */
	};

typedef struct CitContext t_context;

#define CS_STEALTH	1		/* stealth mode */
#define CS_CHAT		2		/* chat mode */
#define CS_POSTING	4		/* Posting */

struct CitContext *MyContext(void);
#define CC ((struct CitContext *)MyContext())

extern struct CitContext *ContextList;
extern int ScheduledShutdown;
extern struct CitControl CitControl;

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
#define S_USERSUPP	0
#define S_USER_TRANS	1
#define S_QUICKROOM	2
#define S_MSGMAIN	3
#define S_CALLLOG	4
#define S_SESSION_TABLE	5
#define S_FLOORTAB	6
#define S_CHATQUEUE	7
#define S_CONTROL	8
#define S_HOUSEKEEPING	9
#define MAX_SEMAPHORES	10


/*
 * Upload types
 */
#define UPL_FILE	0
#define UPL_NET		1
#define UPL_IMAGE	2



/*
 * Citadel DataBases (define one for each cdb we need to open)
 */
#define CDB_MSGMAIN	0	/* message base                  */
#define CDB_USERSUPP	1	/* user file                     */
#define CDB_QUICKROOM	2	/* room index                    */
#define CDB_FLOORTAB	3	/* floor index                   */
#define CDB_MSGLISTS	4	/* room message lists            */
#define CDB_VISIT	5	/* user/room relationships       */
#define MAXCDB		6	/* total number of CDB's defined */

struct cdbdata {
	size_t len;
	char *ptr;
	};


/* Structures and declarations for function hooks of various types */

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

#define EVT_STOP	0	/* Session is terminating */
#define EVT_START	1	/* Session is starting */
#define EVT_LOGIN	2	/* A user is logging in */
#define EVT_NEWROOM	3	/* Changing rooms */
#define EVT_LOGOUT	4	/* A user is logging out */
#define EVT_SETPASS	5	/* Setting or changing password */


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
