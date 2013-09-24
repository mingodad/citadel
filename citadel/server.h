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
	long cm_lengths[256];		/* size of datafields */
	unsigned int cm_flags;		/* How to handle (NOT SAVED TO DISK) */
};

#define	CTDLMESSAGE_MAGIC		0x159d
#define	CM_SKIP_HOOKS	0x01		/* Don't run server-side handlers */


/* Data structure returned by validate_recipients() */
typedef struct __recptypes {
	int recptypes_magic;
        int num_local;
        int num_internet;
        int num_ignet;
	int num_room;
        int num_error;
	char *errormsg;
	char *recp_local;
	char *recp_internet;
	char *recp_ignet;
	char *recp_room;
	char *recp_orgroom;
	char *display_recp;
	char *bounce_to;
	char *envelope_from;
	char *sending_room;
} recptypes;

#define RECPTYPES_MAGIC 0xfeeb


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
 * Reasons why a session would be terminated (set CC->kill_me to these values)
 */
enum {
	KILLME_NOT,
	KILLME_UNKNOWN,
	KILLME_CLIENT_LOGGED_OUT,
	KILLME_IDLE,
	KILLME_CLIENT_DISCONNECTED,
	KILLME_AUTHFAILED,
	KILLME_SERVER_SHUTTING_DOWN,
	KILLME_MAX_SESSIONS_EXCEEDED,
	KILLME_ADMIN_TERMINATE,
	KILLME_SELECT_INTERRUPTED,
	KILLME_SELECT_FAILED,
	KILLME_WRITE_FAILED,
	KILLME_SIMULATION_WORKER,
	KILLME_NOLOGIN,
	KILLME_NO_CRYPTO,
	KILLME_READSTRING_FAILED,
	KILLME_MALLOC_FAILED,
	KILLME_QUOTA,
	KILLME_READ_FAILED,
	KILLME_ILLEGAL_MANAGESIEVE_COMMAND,
	KILLME_SPAMMER,
	KILLME_XML_PARSER
};


#define CS_STEALTH	1	/* stealth mode */
#define CS_CHAT		2	/* chat mode */
#define CS_POSTING	4	/* Posting */


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
	S_ATBF,
	S_JOURNAL_QUEUE,
	S_RPLIST,
	S_SIEVELIST,
	S_CHKPWD,
	S_LOG,
	S_NETSPOOL,
	S_XMPP_QUEUE,
	S_SCHEDULE_LIST,
	S_SINGLE_USER,
	S_LDAP,
	S_IM_LOGS,
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
#define FMT_CITADEL	0	/* Citadel vari-format (proprietary) */
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
#define EVT_SHUTDOWN	52	/* Server is shutting down */

#define EVT_PURGEUSER	100	/* Deleting a user */
#define EVT_NEWUSER	102	/* Creating a user */

#define EVT_BEFOREREAD	200
#define EVT_BEFORESAVE	201
#define EVT_AFTERSAVE	202
#define EVT_SMTPSCAN	203	/* called before submitting a msg from SMTP */
#define EVT_AFTERUSRMBOXSAVE 204 /* called afte a message was saved into a users inbox */
/* Priority levels for paging functions (lower is better) */
enum {
	XMSG_PRI_LOCAL,		/* Other users on -this- server */
	XMSG_PRI_REMOTE,	/* Other users on a Citadel network (future) */
	XMSG_PRI_FOREIGN,	/* Contacts on foreign instant message hosts */
	MAX_XMSG_PRI
};


/* Defines the relationship of a user to a particular room */
typedef struct __visit {
	long v_roomnum;
	long v_roomgen;
	long v_usernum;
	long v_lastseen;
	unsigned int v_flags;
	char v_seen[SIZ];
	char v_answered[SIZ];
	int v_view;
} visit;

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



/* Preferred field order							*/
/*               **********			Important fields		*/
/*                         ***************	Semi-important fields		*/
/*                                        **	internal only                   */
/*                                          *	Message text (MUST be last)	*/
///#define FORDER	"IPTAFONHRDBCEWJGKLQSVXZYU12M"

typedef enum _MsgField {
	eAuthor       = 'A',
	eBig_message  = 'B',
	eRemoteRoom   = 'C',
	eDestination  = 'D',
	eExclusiveID  = 'E',
	erFc822Addr   = 'F',
	eHumanNode    = 'H',
	emessageId    = 'I',
	eJournal      = 'J',
	eReplyTo      = 'K',
	eListID       = 'L',
	eMesageText   = 'M',
	eNodeName     = 'N',
	eOriginalRoom = 'O',
	eMessagePath  = 'P',
	eRecipient    = 'R',
	eSpecialField = 'S',
	eTimestamp    = 'T',
	eMsgSubject   = 'U',
	eenVelopeTo   = 'V',
	eWeferences   = 'W',
	eCarbonCopY   = 'Y',
	eErrorMsg     = '0',
	eSuppressIdx  = '1',
	eExtnotify    = '2',
	eVltMsgNum    = '3'
}eMsgField;

#endif /* SERVER_H */
