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



#define CTDLEXIT_SHUTDOWN	0	/* Normal shutdown; do NOT auto-restart */

/*
 * Exit codes 101 through 109 are used for conditions in which
 * we deliberately do NOT want the service to automatically
 * restart.
 */
#define CTDLEXIT_CONFIG		101	/* Could not read citadel.config */
#define CTDLEXIT_CONTROL	102	/* Could not acquire lock */
#define CTDLEXIT_HOME		103	/* Citadel home directory not found */
#define CTDLEXIT_OOD		104	/* Out Of Date config - rerun setup */
#define CTDLEXIT_DB		105	/* Unable to initialize database */
#define CTDLEXIT_LIBCITADEL	106	/* Incorrect version of libcitadel */
#define CTDL_EXIT_UNSUP_AUTH	107	/* Unsupported auth mode configured */



/*
 * Here's the big one... the Citadel context structure.
 *
 * This structure keeps track of all information relating to a running 
 * session on the server.  We keep one of these for each session thread.
 *
 */
struct CitContext {
	struct CitContext *prev;	/* Link to previous session in list */
	struct CitContext *next;	/* Link to next session in the list */

	int state;		/* thread state (see CON_ values below) */
	int kill_me;		/* Set to nonzero to flag for termination */
	int client_socket;
	int cs_pid;		/* session ID */
	int dont_term;          /* for special activities like artv so we don't get killed */
	time_t lastcmd;		/* time of last command executed */
	time_t lastidle;	/* For computing idle time */

	char curr_user[USERNAME_SIZE];	/* name of current user */
	int logged_in;		/* logged in */
	int internal_pgm;	/* authenticated as internal program */
	int nologin;		/* not allowed to log in */
	int is_local_socket;	/* set to 1 if client is on unix domain sock */
	int curr_view;		/* The view type for the current user/room */
	int is_master;		/* Is this session logged in using the master user? */

	char net_node[32]	;/* Is the client another Citadel server? */
	time_t previous_login;	/* Date/time of previous login */
	char lastcmdname[5];	/* name of last command executed */
	unsigned cs_flags;	/* miscellaneous flags */
	void (*h_command_function) (void) ;	/* service command function */
	void (*h_async_function) (void) ;	/* do async msgs function */
	int is_async;		/* Nonzero if client accepts async msgs */
	int async_waiting;	/* Nonzero if there are async msgs waiting */
	int input_waiting;	/* Nonzero if there is client input waiting */

	/* Client information */
	int cs_clientdev;	/* client developer ID */
	int cs_clienttyp;	/* client type code */
	int cs_clientver;	/* client version number */
	char cs_clientname[32];	/* name of client software */
	char cs_host[64];	/* host logged in from */
	char cs_addr[64];	/* address logged in from */

	/* The Internet type of thing */
	char cs_inet_email[128];		/* Return address of outbound Internet mail */
	char cs_inet_other_emails[1024];	/* User's other valid Internet email addresses */
	char cs_inet_fn[128];			/* Friendly-name of outbound Internet mail */

	FILE *download_fp;	/* Fields relating to file transfer */
	char download_desired_section[128];
	FILE *upload_fp;
	char upl_file[256];
	char upl_path[PATH_MAX];
	char upl_comment[256];
	char upl_filedir[PATH_MAX];
	char upl_mimetype[64];
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

	/* A linked list of all instant messages sent to us. */
	struct ExpressMessage *FirstExpressMessage;
	int disable_exp;	/* Set to 1 to disable incoming pages */
	int newmail;		/* Other sessions increment this */

	/* Masqueraded values in the 'who is online' list */
	char fake_username[USERNAME_SIZE];
	char fake_hostname[64];
	char fake_roomname[ROOMNAMELEN];

	/* Preferred MIME formats */
	char preferred_formats[256];
	int msg4_dont_decode;

	/* Dynamically allocated session data */
	char *session_specific_data;		/* Used by individual protocol modules */
	struct cit_ical *CIT_ICAL;		/* calendaring data */
	struct ma_info *ma;			/* multipart/alternative data */
	const char *ServiceName;		/* readable purpose of this session */
	void *openid_data;			/* Data stored by the OpenID module */
	char *ldap_dn;				/* DN of user when using AUTHMODE_LDAP */
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
	int MMdbversion;		/* Version of Berkeley DB used on previous server run */
};

extern struct CitContext *ContextList;
extern int ScheduledShutdown;
extern struct CitControl CitControl;

struct ExpressMessage {
	struct ExpressMessage *next;
	time_t timestamp;	/* When this message was sent */
	unsigned flags;		/* Special instructions */
	char sender[256];	/* Name of sending user */
	char sender_email[256];	/* Email or JID of sending user */
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
	S_HOUSEKEEPING,
	S_NTTLIST,
	S_DIRECTORY,
	S_NETCONFIGS,
	S_PUBLIC_CLIENTS,
	S_FLOORCACHE,
	S_DEBUGMEMLEAKS,
	S_ATBF,
	S_JOURNAL_QUEUE,
	S_RPLIST,
	S_SIEVELIST,
	S_CHKPWD,
	S_LOG,
	S_NETSPOOL,
	S_THREAD_LIST,
	S_XMPP_QUEUE,
	S_SCHEDULE_LIST,
 	S_SINGLE_USER,
  	S_LDAP,
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
	MT_DOWNLOAD,		/* Download a component */
	MT_SPEW_SECTION		/* Download a component in a single operation */
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
	CDB_USERSBYNUMBER,	/* index of users by number      */
	CDB_OPENID,		/* associates OpenIDs with users */
	MAXCDB			/* total number of CDB's defined */
};

struct cdbdata {
	size_t len;
	char *ptr;
};


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
#define EVT_STEALTH	9	/* Entering stealth mode */
#define EVT_UNSTEALTH	10	/* Exiting stealth mode */

#define EVT_TIMER	50	/* Timer events are called once per minute
				   and are not tied to any session */
#define EVT_HOUSE	51	/* as needed houskeeping stuff */

#define EVT_PURGEUSER	100	/* Deleting a user */
#define EVT_NEWUSER	102	/* Creating a user */

#define EVT_BEFOREREAD	200
#define EVT_BEFORESAVE	201
#define EVT_AFTERSAVE	202
#define EVT_SMTPSCAN	203	/* called before submitting a msg from SMTP */
/* Priority levels for paging functions (lower is better) */
enum {
	XMSG_PRI_LOCAL,		/* Other users on -this- server */
	XMSG_PRI_REMOTE,	/* Other users on a Citadel network (future) */
	XMSG_PRI_FOREIGN,	/* Contacts on foreign instant message hosts */
	MAX_XMSG_PRI
};


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
	char mimetype[64];              /* if we were able to guess the mimetype for the data */ 
};

/* Calls to AdjRefCount() are queued and deferred, so the user doesn't
 * have to wait for various disk-intensive operations to complete synchronously.
 * This is the record format.
 */
struct arcq {
	long arcq_msgnum;		/* Message number being adjusted */
	int arcq_delta;			/* Adjustment ( usually 1 or -1 ) */
};


/* 
 * Serialization routines use this struct to return a pointer and a length
 */
struct ser_ret {
        size_t len;
        unsigned char *ser;
};


/*
 * The S_USETABLE database is used in several modules now, so we define its format here.
 */
struct UseTable {
	char ut_msgid[SIZ];
	time_t ut_timestamp;
};



/* Preferred field order 							*/
/*               **********			Important fields		*/
/*                         ***************	Semi-important fields		*/
/*                                        * 	Message text (MUST be last)	*/
#define FORDER	"IPTAFONHRDBCEWJGKLQSVXZYUM"

#endif /* SERVER_H */
