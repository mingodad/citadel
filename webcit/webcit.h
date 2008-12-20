
/* $Id$ */

#include "sysdep.h"


#include <sys/select.h>

#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#include <sys/stat.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/poll.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/utsname.h>

#include <libcitadel.h>

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#ifdef ENABLE_NLS
#ifdef HAVE_XLOCALE_H
#include <xlocale.h>
#endif
#include <libintl.h>
#include <locale.h>
#define _(string)	gettext(string)
#else
#define _(string)	(string)
#endif

#define IsEmptyStr(a) ((a)[0] == '\0')
/*
 * Uncomment to dump an HTTP trace to stderr
#define HTTP_TRACING 1
 */

#ifdef HTTP_TRACING
#undef HAVE_ZLIB_H
#undef HAVE_ZLIB
#endif

#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include <libical/ical.h>

#undef PACKAGE
#undef VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT
#include "sysdep.h"

////////#include "hash.h"

#ifdef HAVE_OPENSSL
/* Work around RedHat's b0rken OpenSSL includes */
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#endif

#define CALENDAR_ROOM_NAME	"Calendar"
#define PRODID "-//Citadel//NONSGML Citadel Calendar//EN"

#define SIZ			4096		/* generic buffer size */

#define TRACE fprintf(stderr, "Checkpoint: %s, %d\n", __FILE__, __LINE__)

#define SLEEPING		180		/* TCP connection timeout */
#define WEBCIT_TIMEOUT		900		/* WebCit session timeout */
#define PORT_NUM		2000		/* port number to listen on */
#define DEVELOPER_ID		0
#define CLIENT_ID		4
#define CLIENT_VERSION		739		/* This version of WebCit */
#define MINIMUM_CIT_VERSION	739		/* min required Citadel ver */
#define	LIBCITADEL_MIN		739		/* min required libcitadel ver */
#define DEFAULT_HOST		"localhost"	/* Default Citadel server */
#define DEFAULT_PORT		"504"
#define TARGET			"webcit01"	/* Target for inline URL's */
#define HOUSEKEEPING		15		/* Housekeeping frequency */
#define MIN_WORKER_THREADS	5
#define MAX_WORKER_THREADS	250
#define LISTEN_QUEUE_LENGTH	100		/* listen() backlog queue */

#define USERCONFIGROOM		"My Citadel Config"
#define DEFAULT_MAXMSGS		20


#ifdef LIBCITADEL_VERSION_NUMBER
#if LIBCITADEL_VERSION_NUMBER < LIBCITADEL_MIN
#error libcitadel is too old.  Please upgrade it before continuing.
#endif
#endif


/*
 * Room flags (from Citadel)
 *
 * bucket one...
 */
#define QR_PERMANENT	1		/* Room does not purge		*/
#define QR_INUSE	2		/* Set if in use, clear if avail	*/
#define QR_PRIVATE	4		/* Set for any type of private room	*/
#define QR_PASSWORDED	8		/* Set if there's a password too	*/
#define QR_GUESSNAME	16		/* Set if it's a guessname room	*/
#define QR_DIRECTORY	32		/* Directory room			*/
#define QR_UPLOAD	64	    	/* Allowed to upload			*/
#define QR_DOWNLOAD	128		/* Allowed to download		*/
#define QR_VISDIR	256		/* Visible directory			*/
#define QR_ANONONLY	512		/* Anonymous-Only room		*/
#define QR_ANONOPT	1024		/* Anonymous-Option room		*/
#define QR_NETWORK	2048		/* Shared network room		*/
#define QR_PREFONLY	4096		/* Preferred status needed to enter	*/
#define QR_READONLY	8192		/* Aide status required to post	*/
#define QR_MAILBOX	16384		/* Set if this is a private mailbox	*/

/*
 * bucket two...
 */
#define QR2_SYSTEM	1		/* System room; hide by default	*/
#define QR2_SELFLIST	2		/* Self-service mailing list mgmt	*/
#define QR2_COLLABDEL	4		/* Anyone who can post can also delete*/
#define QR2_SUBJECTREQ  8               /* Subject strongly recommended */
#define QR2_SMTP_PUBLIC 16              /* smtp public postable room */
#define QR2_MODERATED	32		/* Listservice aide has to permit posts  */

/*
 * user/room access
 */
#define UA_KNOWN	2
#define UA_GOTOALLOWED	4
#define UA_HASNEWMSGS	8
#define UA_ZAPPED	16


/*
 * User flags (from Citadel)
 */
#define US_NEEDVALID	1		/* User needs to be validated		*/
#define US_PERM		4		/* Permanent user			*/
#define US_LASTOLD	16		/* Print last old message with new	*/
#define US_EXPERT	32		/* Experienced user			*/
#define US_UNLISTED	64		/* Unlisted userlog entry		*/
#define US_NOPROMPT	128		/* Don't prompt after each message	*/
#define US_PROMPTCTL	256		/* <N>ext & <S>top work at prompt	*/
#define US_DISAPPEAR	512		/* Use "disappearing msg prompts"	*/
#define US_REGIS	1024		/* Registered user			*/
#define US_PAGINATOR	2048		/* Pause after each screen of text	*/
#define US_INTERNET	4096		/* Internet mail privileges		*/
#define US_FLOORS	8192		/* User wants to see floors		*/
#define US_COLOR	16384		/* User wants ANSI color support	*/
#define US_USER_SET	(US_LASTOLD | US_EXPERT | US_UNLISTED | \
			US_NOPROMPT | US_DISAPPEAR | US_PAGINATOR | \
			US_FLOORS | US_COLOR | US_PROMPTCTL )

/*
 * NLI is the string that shows up in a who's online listing for sessions
 * that are active, but for which no user has yet authenticated.
 */
#define NLI	"(not logged in)"


/*
 * \brief	Linked list of session variables encoded in an x-www-urlencoded content type
 */
typedef struct urlcontent urlcontent;
struct urlcontent {
	char url_key[32];          /* the variable name */
	StrBuf *url_data;            /* its value */
};

/*
 * \brief information about us ???
 */ 
struct serv_info {
	int serv_pid;			/* Process ID of the Citadel server */
	char serv_nodename[32];		/* Node name of the Citadel server */
	char serv_humannode[64];	/* human readable node name of the Citadel server */
	char serv_fqdn[64];		/* fully quallified Domain Name (such as uncensored.citadel.org) */
	char serv_software[64];		/* What version does our connected citadel server use */
	int serv_rev_level;		/* Whats the citadel server revision */
	char serv_bbs_city[64];		/* Geographic location of the Citadel server */
	char serv_sysadm[64];		/* Name of system administrator */
	char serv_moreprompt[256];	/* Whats the commandline textprompt */
	int serv_ok_floors;		/* nonzero == server supports floors */
	int serv_supports_ldap;		/* is the server linked against an ldap tree for adresses? */
	int serv_newuser_disabled;	/* Has the server disabled self-service new user creation? */
	char serv_default_cal_zone[128];/* Default timezone for unspecified calendar items */
	int serv_supports_sieve;	/* Does the server support Sieve mail filtering? */
	int serv_fulltext_enabled;	/* Does the server have the full text index enabled? */
	char serv_svn_revision[256];	/* SVN revision of the server */
	int serv_supports_openid;	/* Does the server support authentication via OpenID? */
};



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


#define TYPE_STR   1
#define TYPE_LONG  2
#define TYPE_PREFSTR 3
#define TYPE_PREFINT 4
#define TYPE_GETTEXT 5
#define TYPE_BSTR 6
#define MAXPARAM  20


typedef struct _TemplateParam {
	const char *Start;
	int Type;
	long len;
	long lvalue;
} TemplateParam;

typedef struct _TemplateToken {
	const StrBuf *FileName; /* Reference to print error messages; not to be freed */
	StrBuf *FlatToken;
	long Line;
	const char *pTokenStart;
	size_t TokenStart;
	size_t TokenEnd;
	const char *pTokenEnd;
	int Flags;
	void *PreEval;

	const char *pName;
	size_t NameEnd;

	int HaveParameters;
	int nParameters;
	TemplateParam *Params[MAXPARAM];
} WCTemplateToken;

typedef void (*WCHandlerFunc)();


/*
 * \brief Dynamic content for variable substitution in templates
 */
typedef struct _wcsubst {
	int wcs_type;			    /* which type of Substitution are we */
	char wcs_key[32];		    /* copy of our hashkey for debugging */
	StrBuf *wcs_value;		    /* if we're a string, keep it here */
	long lvalue;                        /* type long? keep data here */
	int ContextRequired;                /* do we require a context type? */
	WCHandlerFunc wcs_function; /* funcion hook ???*/
} wcsubst;

#define CTX_NONE 0
#define CTX_SITECFG 1
#define CTX_SESSION 2
#define CTX_INETCFG 3
#define CTX_VNOTE 4
#define CTX_WHO 5
#define CTX_PREF 6
#define CTX_NODECONF 7
#define CTX_USERLIST 8
#define CTX_MAILSUM 9
#define CTX_MIME_ATACH 10
#define CTX_STRBUF 12
#define CTX_LONGVECTOR 13


void RegisterNS(const char *NSName, long len, 
		int nMinArgs, 
		int nMaxArgs, 
		WCHandlerFunc HandlerFunc,
		int ContextRequired);
#define RegisterNamespace(a, b, c, d, e) RegisterNS(a, sizeof(a)-1, b, c, d, e)

typedef int (*WCConditionalFunc)(WCTemplateToken *Token, void *Context, int ContextType);
typedef struct _ConditionalStruct {
	const char *PlainName;
	int nParams;
	int ContextRequired;
	WCConditionalFunc CondF;
} ConditionalStruct;
void RegisterConditional(const char *Name, long len, 
			 int nParams,
			 WCConditionalFunc CondF, 
			 int ContextRequired);



typedef void (*SubTemplFunc)(StrBuf *TemplBuffer, void *Context, WCTemplateToken *Token);
typedef HashList *(*RetrieveHashlistFunc)(StrBuf *Target, int nArgs, WCTemplateToken *Tokens, void *Context, int ContextType);
typedef void (*HashDestructorFunc) (HashList **KillMe);
void RegisterITERATOR(const char *Name, long len, /* Our identifier */
		      int AdditionalParams,       /* doe we use more parameters? */
		      HashList *StaticList,       /* pointer to webcit lifetime hashlists */
		      RetrieveHashlistFunc GetHash, /* else retrieve the hashlist by calling this function */
		      SubTemplFunc DoSubTempl,       /* call this function on each iteration for svput & friends */
		      HashDestructorFunc Destructor, /* use this function to shut down the hash; NULL if its a reference */
		      int ContextType,               /* which context do we provide to the subtemplate? */
		      int XPectContextType);         /* which context do we expct to be called in? */
#define RegisterIterator(a, b, c, d, e, f, g, h) RegisterITERATOR(a, sizeof(a)-1, b, c, d, e, f, g, h)

void GetTemplateTokenString(WCTemplateToken *Tokens,
			    int N, 
			    const char **Value, 
			    long *len);


void SVPut(char *keyname, size_t keylen, int keytype, char *Data);
#define svput(a, b, c) SVPut(a, sizeof(a) - 1, b, c)
void SVPutLong(char *keyname, size_t keylen, long Data);
#define svputlong(a, b) SVPutLong(a, sizeof(a) - 1, b)
void svprintf(char *keyname, size_t keylen, int keytype, const char *format,...) __attribute__((__format__(__printf__,4,5)));
void SVPRINTF(char *keyname, int keytype, const char *format,...) __attribute__((__format__(__printf__,3,4)));
void SVCALLBACK(char *keyname, WCHandlerFunc fcn_ptr);
void SVCallback(char *keyname, size_t keylen,  WCHandlerFunc fcn_ptr);
#define svcallback(a, b) SVCallback(a, sizeof(a) - 1, b)

void SVPUTBuf(const char *keyname, int keylen, const StrBuf *Buf, int ref);
#define SVPutBuf(a, b, c); SVPUTBuf(a, sizeof(a) - 1, b, c)

void DoTemplate(const char *templatename, long len, StrBuf *Target, void *Context, int ContextType);
#define do_template(a, b) DoTemplate(a, sizeof(a) -1, NULL, b, 0);
void url_do_template(void);

int CompareSubstToToken(TemplateParam *ParamToCompare, TemplateParam *ParamToLookup);
int CompareSubstToStrBuf(StrBuf *Compare, TemplateParam *ParamToLookup);

void StrBufAppendTemplate(StrBuf *Target, 
			  int nArgs, 
			  WCTemplateToken *Tokens,
			  void *Context, int ContextType,
			  const StrBuf *Source, int FormatTypeIndex);
CompareFunc RetrieveSort(long ContextType, const char *OtherPrefix, 
			 const char *Default, long ldefault, long DefaultDirection);
void RegisterSortFunc(const char *name, long len, 
		      const char *prepend, long preplen,
		      CompareFunc Forward, 
		      CompareFunc Reverse, 
		      long ContextType);


/*
 * \brief Values for wcs_type
 */
enum {
	WCS_STRING,       /* its a string */
	WCS_FUNCTION,     /* its a function callback */
	WCS_SERVCMD,      /* its a command to send to the citadel server */
	WCS_STRBUF,       /* its a strbuf we own */
	WCS_STRBUF_REF,   /* its a strbuf we mustn't free */
	WCS_LONG          /* its an integer */
};



typedef struct wc_mime_attachment wc_mime_attachment;
typedef void (*RenderMimeFunc)(wc_mime_attachment *Mime, StrBuf *RawData, StrBuf *FoundCharset);
struct wc_mime_attachment {
	int level;
	StrBuf *Name;
	StrBuf *FileName;
	StrBuf *PartNum;
	StrBuf *Disposition;
	StrBuf *ContentType;
	StrBuf *Charset;
	StrBuf *Data;
	size_t length;			   /* length of the mimeatachment */
	long size_known;
	long lvalue;               /* if we put a long... */
	long msgnum;		/**< the message number on the citadel server derived from message_summary */
	RenderMimeFunc Renderer;
};
void DestroyMime(void *vMime);


/*
 * \brief message summary structure. ???
 */
typedef struct _message_summary {
	time_t date;        /**< its creation date */
	long msgnum;		/**< the message number on the citadel server */
	int nhdr;
	int format_type;
	StrBuf *from;		/**< the author */
	StrBuf *to;		/**< the recipient */
	StrBuf *subj;		/**< the title / subject */
	StrBuf *reply_inreplyto;
	StrBuf *reply_references;
	StrBuf *reply_to;
	StrBuf *cccc;
	StrBuf *hnod;
	StrBuf *AllRcpt;
	StrBuf *Room;
	StrBuf *Rfca;
	StrBuf *OtherNode;
	const StrBuf *PartNum;

	HashList *Attachments;  /**< list of Accachments */
	HashList *Submessages;
	HashList *AttachLinks;

	HashList *AllAttach;

	int is_new;         /**< is it yet read? */
	int hasattachments;	/* does it have atachments? */


	/** The mime part of the message */
	wc_mime_attachment *MsgBody;
} message_summary;
void DestroyMessageSummary(void *vMsg);
inline message_summary* GetMessagePtrAt(int n, HashList *Summ);

typedef void (*ExamineMsgHeaderFunc)(message_summary *Msg, StrBuf *HdrLine, StrBuf *FoundCharset);

void evaluate_mime_part(message_summary *Msg, wc_mime_attachment *Mime);



/**
 * \brief  Data structure for roomlist-to-folderlist conversion 
 */
struct folder {
	int floor;      /* which floor is it on */
	char room[SIZ];	/* which roomname ??? */
	char name[SIZ];	/* which is its own name??? */
	int hasnewmsgs;	/* are there unread messages inside */
	int is_mailbox;	/* is it a mailbox?  */
	int selectable;	/* can we select it ??? */
	int view;       /* whats its default view? inbox/calendar.... */
	int num_rooms;	/* If this is a floor, how many rooms does it have */
};

typedef struct _disp_cal {					
	icalcomponent *cal;		/* cal items for display */
	long cal_msgnum;		/* cal msgids for display */
	char *from;                     /* owner of this component */
	int unread;                     /* already seen by the user? */

	time_t event_start;
	time_t event_end;

	int multi_day_event;
	int is_repeat;
} disp_cal;						



/*
 * Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
typedef struct _addrbookent {
	char ab_name[64]; /**< name string */
	long ab_msgnum;   /**< message number of address book entry */
} addrbookent;


typedef struct _headereval {
	ExamineMsgHeaderFunc evaluator;
	int Type;
} headereval;


struct attach_link {
	char partnum[32];
	char html[1024];
};


enum {
	eUp,
	eDown,
	eNone
};

/*
 * One of these is kept for each active Citadel session.
 * HTTP transactions are bound to one at a time.
 */
typedef struct wcsession wcsession;
struct wcsession {
	wcsession *next;			/**< Linked list */
	int wc_session;				/**< WebCit session ID */
	char wc_username[128];			/**< login name of current user */
	char wc_fullname[128];			/**< Screen name of current user */
	char wc_password[128];			/**< Password of current user */
	char wc_roomname[256];			/**< Room we are currently in */
	int connected;				/**< nonzero == we are connected to Citadel */
	int logged_in;				/**< nonzero == we are logged in  */
	int axlevel;				/**< this user's access level */
	int is_aide;				/**< nonzero == this user is an Aide */
	int is_room_aide;			/**< nonzero == this user is a Room Aide in this room */
	int http_sock;				/**< HTTP server socket */
	int serv_sock;				/**< Client socket to Citadel server */
	int chat_sock;				/**< Client socket to Citadel server - for chat */
	unsigned room_flags;			/**< flags associated with the current room */
	unsigned room_flags2;			/**< flags associated with the current room */
	int wc_view;				/**< view for the current room */
	int wc_default_view;			/**< default view for the current room */
	int wc_is_trash;			/**< nonzero == current room is a Trash folder */
	int wc_floor;				/**< floor number of current room */
	char ugname[128];			/**< where does 'ungoto' take us */
	long uglsn;				/**< last seen message number for ungoto */
	int upload_length;			/**< content length of http-uploaded data */
	char *upload;				/**< pointer to http-uploaded data */
	char upload_filename[PATH_MAX];		/**< filename of http-uploaded data */
	char upload_content_type[256];		/**< content type of http-uploaded data */
	int new_mail;				/**< user has new mail waiting */
	int remember_new_mail;			/**< last count of new mail messages */
	int need_regi;				/**< This user needs to register. */
	int need_vali;				/**< New users require validation. */
	char cs_inet_email[256];		/**< User's preferred Internet addr. */
	pthread_mutex_t SessionMutex;		/**< mutex for exclusive access */
	time_t lastreq;				/**< Timestamp of most recent HTTP */
	int killthis;				/**< Nonzero == purge this session */
	struct march *march;			/**< march mode room list */
	char reply_to[512];			/**< reply-to address */
	HashList *summ;                         /**< list of messages for mailbox summary view */
	int is_mobile;			/**< Client is a handheld browser */
	HashList *urlstrings;		        /**< variables passed to webcit in a URL */
	HashList *vars; 			/**< HTTP variable substitutions for this page */
	char this_page[512];			/**< URL of current page */
	char http_host[512];			/**< HTTP Host: header */
	HashList *hash_prefs;			/**< WebCit preferences for this user */
	HashList *disp_cal_items;               /**< sorted list of calendar items; startdate is the sort criteria. */
	HashList *attachments;             	/**< list of attachments for 'enter message' */
	char last_chat_user[256];		/**< ??? todo */
	char ImportantMessage[SIZ];		/**< ??? todo */
	int ctdl_pid;				/**< Session ID on the Citadel server */
	char httpauth_user[256];		/**< only for GroupDAV sessions */
	char httpauth_pass[256];		/**< only for GroupDAV sessions */
	int gzip_ok;				/**< Nonzero if Accept-encoding: gzip */
	int is_mailbox;				/**< the current room is a private mailbox */
	struct folder *cache_fold;		/**< cache the iconbar room list */
	int cache_max_folders;			/**< ??? todo */
	int cache_num_floors;			/**< ??? todo */
	time_t cache_timestamp;			/**< ??? todo */
	HashList *IconBarSetttings;             /**< which icons should be shown / not shown? */
	long current_iconbar;			/**< What is currently in the iconbar? */
	const StrBuf *floordiv_expanded;	/**< which floordiv currently expanded */
	int selected_language;			/**< Language selected by user */
	time_t last_pager_check;		/**< last time we polled for instant msgs */
	int nonce;				/**< session nonce (to prevent session riding) */
	int time_format_cache;                  /**< which timeformat does our user like? */
	StrBuf *UrlFragment1;                   /**< first urlfragment, if NEED_URL is specified by the handler*/
	StrBuf *UrlFragment2;                   /**< second urlfragment, if NEED_URL is specified by the handler*/
	StrBuf *UrlFragment3;                   /**< third urlfragment, if NEED_URL is specified by the handler*/
	StrBuf *WBuf;                           /**< Our output buffer */
	StrBuf *HBuf;                           /**< Our HeaderBuffer */
	StrBuf *CLineBuf;                       /**< linebuffering client stuff */
	StrBuf *DefaultCharset;                 /**< Charset the user preferes */
	HashList *ServCfg;                      /**< cache our server config for editing */
	HashList *InetCfg;                      /**< Our inet server config for editing */

	StrBuf *trailing_javascript;		/**< extra javascript to be appended to page */
};

/* values for WC->current_iconbar */
enum {
	current_iconbar_menu,     /* view the icon menue */
	current_iconbar_roomlist  /* view the roomtree */
};
enum {
	S_SELECT,
	S_SHUTDOWN,
	MAX_SEMAPHORES
};


/*
 * calview contains data passed back and forth between the message fetching loop
 * and the calendar view renderer.
 */
enum {
	calview_month,
	calview_day,
	calview_week,
	calview_brief
};

struct calview {
	int view;
	int year;
	int month;
	int day;
	time_t lower_bound;
	time_t upper_bound;
};

#ifndef num_parms
#define num_parms(source)		num_tokens(source, '|') 
#endif

/* Per-session data */
#define WC ((struct wcsession *)pthread_getspecific(MyConKey))
extern pthread_key_t MyConKey;

/* Per-thread SSL context */
#ifdef HAVE_OPENSSL
#define THREADSSL ((SSL *)pthread_getspecific(ThreadSSL))
extern pthread_key_t ThreadSSL;
extern char ctdl_key_dir[PATH_MAX];
extern char file_crpt_file_key[PATH_MAX];
extern char file_crpt_file_csr[PATH_MAX];
extern char file_crpt_file_cer[PATH_MAX];
#endif

struct serv_info serv_info;
extern char floorlist[128][SIZ];
extern char *axdefs[];
extern char *ctdlhost, *ctdlport;
extern int http_port;
extern char *server_cookie;
extern int is_https;
extern int setup_wizard;
extern char wizard_filename[];
extern time_t if_modified_since;
extern int follow_xff;
extern HashList *HandlerHash;
extern HashList *PreferenceHooks;
extern HashList *WirelessTemplateCache;
extern HashList *WirelessLocalTemplateCache;
extern HashList *TemplateCache;
extern HashList *LocalTemplateCache;
extern HashList *GlobalNS;
extern HashList *Iterators;
extern HashList *ZoneHash;
extern HashList *Conditionals;
extern HashList *MsgHeaderHandler;
extern HashList *MimeRenderHandler;
extern HashList *SortHash;

void InitialiseSemaphores(void);
void begin_critical_section(int which_one);
void end_critical_section(int which_one);


void stuff_to_cookie(char *cookie, size_t clen, int session,
			char *user, char *pass, char *room);
void cookie_to_stuff(StrBuf *cookie, int *session,
                char *user, size_t user_len,
                char *pass, size_t pass_len,
                char *room, size_t room_len);
void locate_host(char *, int);
void become_logged_in(char *, char *, char *);
void openid_manual_create(void);
void display_login();
void display_openids(void);
void do_welcome(void);
void do_logout(void);
void display_main_menu(void);
void display_aide_menu(void);
void display_advanced_menu(void);
void slrp_highest(void);
void get_serv_info(char *, char *);
int uds_connectsock(char *);
int tcp_connectsock(char *, char *);
int serv_getln(char *strbuf, int bufsize);
int StrBuf_ServGetln(StrBuf *buf);
int GetServerStatus(StrBuf *Line, long* FullState);
void serv_puts(const char *string);
void who(void);
void who_inner_div(void);
void ajax_mini_calendar(void);
void fmout(char *align);
void _fmout(StrBuf *Targt, char *align);
void FmOut(StrBuf *Target, char *align, StrBuf *Source);
void pullquote_fmout(void);
void wDumpContent(int);

int Flathash(const char *str, long len);



/* URL / Mime Post parsing -> paramhandling.c */
void upload_handler(char *name, char *filename, char *partnum, char *disp,
		    void *content, char *cbtype, char *cbcharset,
		    size_t length, char *encoding, char *cbid, void *userdata);

void ParseURLParams(StrBuf *url);


/* These may return NULL if not foud */
#define sbstr(a) SBstr(a, sizeof(a) - 1)
const StrBuf *SBSTR(const char *key);
const StrBuf *SBstr(const char *key, size_t keylen);

#define xbstr(a, b) (char*) XBstr(a, sizeof(a) - 1, b)
const char *XBstr(const char *key, size_t keylen, size_t *len);
const char *XBSTR(const char *key, size_t *len);

#define lbstr(a) LBstr(a, sizeof(a) - 1)
long LBstr(const char *key, size_t keylen);
long LBSTR(const char *key);

#define ibstr(a) IBstr(a, sizeof(a) - 1)
int IBstr(const char *key, size_t keylen);
int IBSTR(const char *key);

#define havebstr(a) HaveBstr(a, sizeof(a) - 1)
int HaveBstr(const char *key, size_t keylen);
int HAVEBSTR(const char *key);

#define yesbstr(a) YesBstr(a, sizeof(a) - 1)
int YesBstr(const char *key, size_t keylen);
int YESBSTR(const char *key);

/* TODO: get rid of the non-const-typecast */
#define bstr(a) (char*) Bstr(a, sizeof(a) - 1)
const char *BSTR(const char *key);
const char *Bstr(const char *key, size_t keylen);



void UrlescPutStrBuf(const StrBuf *strbuf);
void StrEscPuts(const StrBuf *strbuf);
void StrEscputs1(const StrBuf *strbuf, int nbsp, int nolinebreaks);

void urlescputs(const char *);
void hurlescputs(const char *);
void jsesc(char *, size_t, char *);
void jsescputs(char *);
void output_headers(    int do_httpheaders,
			int do_htmlhead,
			int do_room_banner,
			int unset_cookies,
			int suppress_check,
			int cache);
void wprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void hprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void output_static(char *what);

void print_menu_box(char* Title, char *Class, int nLines, ...);
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks);
void escputs(char *strbuf);
void url(char *buf, size_t bufsize);
void UrlizeText(StrBuf* Target, StrBuf *Source, StrBuf *WrkBuf);
void escputs1(char *strbuf, int nbsp, int nolinebreaks);
void msgesc(char *target, size_t tlen, char *strbuf);
void msgescputs(char *strbuf);
void msgescputs1(char *strbuf);
void stripout(char *str, char leftboundary, char rightboundary);
void dump_vars(void);
void embed_main_menu(void);
void serv_read(char *buf, int bytes);

enum {
	do_search,
	headers,
	readfwd,
	readnew,
	readold
};

typedef void (*readloop_servcmd)(char *buf, long bufsize);

typedef struct _readloopstruct {
	ConstStr name;
	readloop_servcmd cmd;
} readloop_struct;
void readloop(long oper);
int  read_message(StrBuf *Target, const char *tmpl, long tmpllen, long msgnum, int printable_view, const StrBuf *section);
void do_addrbook_view(addrbookent *addrbook, int num_ab);
void fetch_ab_name(message_summary *Msg, char *namebuf);
void display_vcard(StrBuf *Target, const char *vcard_source, char alpha, int full, char *storename, long msgnum);
void text_to_server(char *ptr);
void text_to_server_qp(char *ptr);
void confirm_delete_msg(void);
void display_success(char *);
void authorization_required(const char *message);
void server_to_text(void);
void save_edit(char *description, char *enter_cmd, int regoto);
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner);
int gotoroom(char *gname);
void confirm_delete_room(void);
void validate(void);
void display_graphics_upload(char *, char *, char *);
void do_graphics_upload(char *upl_cmd);
void serv_read(char *buf, int bytes);
void serv_gets(char *strbuf);
void serv_write(const char *buf, int nbytes);
void serv_puts(const char *string);
void serv_putbuf(const StrBuf *string);
void serv_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void load_floorlist(void);
void shutdown_sessions(void);
void do_housekeeping(void);
void smart_goto(char *);
void worker_entry(void);
void session_loop(HashList *HTTPHeaders, StrBuf *ReqLine, StrBuf *ReqType, StrBuf *ReadBuf);
size_t wc_strftime(char *s, size_t max, const char *format, const struct tm *tm);
void fmt_time(char *buf, time_t thetime);
void httpdate(char *buf, time_t thetime);
time_t httpdate_to_timestamp(StrBuf *buf);
void end_webcit_session(void);
void page_popup(void);
void http_redirect(const char *);
void clear_substs(struct wcsession *wc);
void clear_local_substs(void);



int lingering_close(int fd);
char *memreadline(char *start, char *buf, int maxlen);
char *memreadlinelen(char *start, char *buf, int maxlen, int *retlen);
long extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen);
void remove_token(char *source, int parmnum, char separator);
char *load_mimepart(long msgnum, char *partnum);
void MimeLoadData(wc_mime_attachment *Mime);
int pattern2(char *search, char *patn);
void do_edit_vcard(long, char *, char *, char *);
void striplt(char *);
void stripltlen(char *, int *);
void select_user_to_edit(char *message, char *preselect);
void delete_user(char *);
void do_change_view(int);
void folders(void);


void load_preferences(void);
void save_preferences(void);
#define get_preference(a, b) get_PREFERENCE(a, sizeof(a) - 1, b)
#define get_pref(a, b)       get_PREFERENCE(ChrPtr(a), StrLength(a), b)
int get_PREFERENCE(const char *key, size_t keylen, StrBuf **value);
#define set_preference(a, b, c) set_PREFERENCE(a, sizeof(a) - 1, b, c)
#define set_pref(a, b, c)       set_PREFERENCE(ChrPtr(a), StrLength(a), b, c)
void set_PREFERENCE(const char *key, size_t keylen, StrBuf *value, int save_to_server);

#define get_pref_long(a, b, c) get_PREF_LONG(a, sizeof(a) - 1, b, c)
int get_PREF_LONG(const char *key, size_t keylen, long *value, long Default);
#define set_pref_long(a, b, c) set_PREF_LONG(a, sizeof(a) - 1, b, c)
void set_PREF_LONG(const char *key, size_t keylen, long value, int save_to_server);

#define get_pref_yesno(a, b, c) get_PREF_YESNO(a, sizeof(a) - 1, b, c)
int get_PREF_YESNO(const char *key, size_t keylen, int *value, int Default);
#define set_pref_yesno(a, b, c) set_PREF_YESNO(a, sizeof(a) - 1, b, c)
void set_PREF_YESNO(const char *key, size_t keylen, int value, int save_to_server);

#define get_room_pref(a) get_ROOM_PREFS(a, sizeof(a) - 1)
StrBuf *get_ROOM_PREFS(const char *key, size_t keylen);

#define set_room_pref(a, b, c) set_ROOM_PREFS(a, sizeof(a) - 1, b, c)
void set_ROOM_PREFS(const char *key, size_t keylen, StrBuf *value, int save_to_server);

void display_addressbook(long msgnum, char alpha);
void offer_start_page(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType);
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext);
void output_html(const char *, int, int, StrBuf *, StrBuf *);
void do_listsub(void);
ssize_t write(int fd, const void *buf, size_t count);
void cal_process_attachment(wc_mime_attachment *Mime);
void load_calendar_item(message_summary *Msg, int unread, struct calview *c);
void display_calendar(message_summary *Msg, int unread);
void display_task(message_summary *Msg, int unread);
void display_note(message_summary *Msg, int unread);
void updatenote(void);
void parse_calendar_view_request(struct calview *c);
void render_calendar_view(struct calview *c);
void do_tasks_view(void);
void calendar_summary_view(void);
int load_msg_ptrs(char *servcmd, int with_headers);
void free_attachments(wcsession *sess);
void free_march_list(wcsession *wcf);
void display_rules_editor_inner_div(void);
void generate_uuid(char *);
void CtdlMakeTempFileName(char *, int);
void address_book_popup(void);
void begin_ajax_response(void);
void end_ajax_response(void);
void initialize_viewdefs(void);
void initialize_axdefs(void);
void burn_folder_cache(time_t age);
void list_all_rooms_by_floor(const char *viewpref);
void display_pictureview(void);

void display_edit_task(void);
void display_edit_event(void);
icaltimezone *get_default_icaltimezone(void);
void display_icaltimetype_as_webform(struct icaltimetype *, char *, int);
void icaltime_from_webform(struct icaltimetype *result, char *prefix);
void icaltime_from_webform_dateonly(struct icaltimetype *result, char *prefix);
void display_edit_individual_event(icalcomponent *supplied_vtodo, long msgnum, char *from,
	int unread, struct calview *calv);
void save_individual_event(icalcomponent *supplied_vtodo, long msgnum, char *from,
	int unread, struct calview *calv);
void ical_dezonify(icalcomponent *cal);
void partstat_as_string(char *buf, icalproperty *attendee);
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp);
void check_attendee_availability(icalcomponent *supplied_vevent);
void do_freebusy(const char *req);
int ical_ctdl_is_overlap(
                        struct icaltimetype t1start,
                        struct icaltimetype t1end,
                        struct icaltimetype t2start,
                        struct icaltimetype t2end
);

#ifdef ENABLE_NLS
void initialize_locales(void);
void ShutdownLocale(void);
#endif
void TmplGettext(StrBuf *Target, int nTokens, WCTemplateToken *Token);

extern char *months[];
extern char *days[];
int read_server_binary(StrBuf *Ret, size_t total_len);
int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize);
int read_server_text(StrBuf *Buf, long *nLines);;
int goto_config_room(void);
long locate_user_vcard(char *username, long usernum);
void sleeeeeeeeeep(int);
void http_transmit_thing(const char *content_type, int is_static);
long unescape_input(char *buf);
void do_selected_iconbar(void);
void spawn_another_worker_thread(void);
void display_rss(char *roomname, StrBuf *request_method);
void offer_languages(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType);
void set_selected_language(const char *);
void go_selected_language(void);
void stop_selected_language(void);
void preset_locale(void);
void httplang_to_locale(StrBuf *LocaleString);
void StrEndTab(StrBuf *Target, int tabnum, int num_tabs);
void StrBeginTab(StrBuf *Target, int tabnum, int num_tabs);
void StrTabbedDialog(StrBuf *Target, int num_tabs, StrBuf *tabnames[]);
void tabbed_dialog(int num_tabs, char *tabnames[]);
void begin_tab(int tabnum, int num_tabs);
void end_tab(int tabnum, int num_tabs);
void str_wiki_index(char *s);
int get_time_format_cached (void);
int xtoi(const char *in, size_t len);
const char *get_selected_language(void);
void webcit_fmt_date(char *buf, time_t thetime, int brief);
int fetch_http(char *url, char *target_buf, int maxbytes);

int is_mobile_ua(char *user_agent);

void embed_room_banner(char *, int);

/* navbar types that can be passed to embed_room_banner */
enum {
	navbar_none,
	navbar_default
};


#ifdef HAVE_OPENSSL
void init_ssl(void);
void endtls(void);
void ssl_lock(int mode, int n, const char *file, int line);
int starttls(int sock);
extern SSL_CTX *ssl_ctx;  
int client_read_sslbuffer(StrBuf *buf, int timeout);
void client_write_ssl(const StrBuf *Buf);
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
int ZEXPORT compress_gzip(Bytef * dest, size_t * destLen,
                          const Bytef * source, uLong sourceLen, int level);
#endif

void utf8ify_rfc822_string(char *buf);

void begin_burst(void);
long end_burst(void);

extern char *hourname[];	/* Names of hours (12am, 1am, etc.) */

void http_datestring(char *buf, size_t n, time_t xtime);


typedef void (*WebcitHandlerFunc)(void);
typedef struct  _WebcitHandler{
	WebcitHandlerFunc F;
	long Flags;
} WebcitHandler;
void WebcitAddUrlHandler(const char * UrlString, long UrlSLen, WebcitHandlerFunc F, long Flags);

#define AJAX (1<<0)
#define ANONYMOUS (1<<1)
#define NEED_URL (1<<2)


/* These should be empty, but we have them for testing */
#define DEFAULT_HTTPAUTH_USER	""
#define DEFAULT_HTTPAUTH_PASS	""


/* Exit codes 101 through 109 are initialization failures so we don't want to
 * just keep respawning indefinitely.
 */
#define WC_EXIT_BIND		101	/* Can't bind to the port */
#define WC_EXIT_SSL		102	/* Can't initialize SSL */


#define WC_TIMEFORMAT_NONE 0
#define WC_TIMEFORMAT_AMPM 1
#define WC_TIMEFORMAT_24 2


