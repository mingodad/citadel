
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

#include "subst.h"
#include "messages.h"
#include "paramhandling.h"
#include "preferences.h"
#include "roomops.h"

#ifdef HAVE_OPENSSL
/* Work around RedHat's b0rken OpenSSL includes */
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
extern char *ssl_cipher_list;
#define	DEFAULT_SSL_CIPHER_LIST	"DEFAULT"	/* See http://openssl.org/docs/apps/ciphers.html */
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
#define CLIENT_VERSION		766		/* This version of WebCit */
#define MINIMUM_CIT_VERSION	766		/* min required Citadel ver */
#define	LIBCITADEL_MIN		766		/* min required libcitadel ver */
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
#define UA_POSTALLOWED          32
#define UA_ADMINALLOWED         64
#define UA_DELETEALLOWED        128


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


#define MAJORCODE(a) (((int)(a / 100) ) * 100)

#define LISTING_FOLLOWS	100
#define CIT_OK 		200	
#define MORE_DATA 	300
#define SEND_LISTING 	400
#define ERROR 		500
#define BINARY_FOLLOWS 	600
#define SEND_BINARY 	700
#define START_CHAT_MODE	800
#define ASYNC_MSG 	900

#define MINORCODE(a) (a % 100)
#define ASYNC_GEXP 			02	
#define INTERNAL_ERROR 			10	
#define TOO_BIG 			11	
#define ILLEGAL_VALUE 			12	
#define NOT_LOGGED_IN 			20	
#define CMD_NOT_SUPPORTED 		30	
#define SERVER_SHUTTING_DOWN 		31	
#define PASSWORD_REQUIRED 		40	
#define ALREADY_LOGGED_IN 		41	
#define USERNAME_REQUIRED 		42	
#define HIGHER_ACCESS_REQUIRED 		50	
#define MAX_SESSIONS_EXCEEDED 		51	
#define RESOURCE_BUSY 			52	
#define RESOURCE_NOT_OPEN 		53	
#define NOT_HERE 			60	
#define INVALID_FLOOR_OPERATION 	61	
#define NO_SUCH_USER 			70	
#define FILE_NOT_FOUND 			71	
#define ROOM_NOT_FOUND 			72	
#define NO_SUCH_SYSTEM 			73	
#define ALREADY_EXISTS 			74	
#define MESSAGE_NOT_FOUND 		75
/*
 * NLI is the string that shows up in a who's online listing for sessions
 * that are active, but for which no user has yet authenticated.
 */
#define NLI	"(not logged in)"


/*
 * Linked list of session variables encoded in an x-www-urlencoded content type
 */
typedef struct urlcontent urlcontent;
struct urlcontent {
	char url_key[32];		/* key */
	StrBuf *url_data;		/* value */
};

/*
 * Information about the Citadel server to which we are connected
 */ 
typedef struct _serv_info {
	int serv_pid;			/* Process ID of the Citadel server */
	StrBuf *serv_nodename;		/* Node name of the Citadel server */
	StrBuf *serv_humannode;	        /* human readable node name of the Citadel server */
	StrBuf *serv_fqdn;		/* fully quallified Domain Name (such as uncensored.citadel.org) */
	StrBuf *serv_software;		/* What version does our connected citadel server use */
	int serv_rev_level;		/* Whats the citadel server revision */
	StrBuf *serv_bbs_city;		/* Geographic location of the Citadel server */
	StrBuf *serv_sysadm;		/* Name of system administrator */
	StrBuf *serv_moreprompt;	/* Whats the commandline textprompt */
	int serv_supports_ldap;		/* is the server linked against an ldap tree for adresses? */
	int serv_newuser_disabled;	/* Has the server disabled self-service new user creation? */
	StrBuf *serv_default_cal_zone;  /* Default timezone for unspecified calendar items */
	int serv_supports_sieve;	/* Does the server support Sieve mail filtering? */
	int serv_fulltext_enabled;	/* Does the server have the full text index enabled? */
	StrBuf *serv_svn_revision;	/* SVN revision of the server */
	int serv_supports_openid;	/* Does the server support authentication via OpenID? */
} ServInfo;


typedef struct _disp_cal {					
	icalcomponent *cal;		/* cal items for display */
	long cal_msgnum;		/* cal msgids for display */
	char *from;                     /* owner of this component */
	int unread;                     /* already seen by the user? */

	time_t event_start;
	time_t event_end;

	int multi_day_event;
	int is_repeat;
	icalcomponent *SortBy;		/* cal items for display */
	icalproperty_status Status;
} disp_cal;						

typedef struct _IcalEnumMap {
	const char *Name;
	long NameLen;
	icalproperty_kind map;
} IcalEnumMap;

/*
 * Address book entry (keep it short and sweet, it's just a quickie lookup
 * which we can use to get to the real meat and bones later)
 */
typedef struct _addrbookent {
	char ab_name[64];	/* name string */
	long ab_msgnum;		/* message number of address book entry */
} addrbookent;


#define AJAX (1<<0)
#define ANONYMOUS (1<<1)
#define NEED_URL (1<<2)
#define XHTTP_COMMANDS (1<<3)
#define BOGUS (1<<4)
#define URLNAMESPACE (1<<4)
#define LOGCHATTY (1<<5)
#define COOKIEUNNEEDED (1<<6)
#define ISSTATIC (1<<7)
#define FORCE_SESSIONCLOSE (1<<8)
#define PARSE_REST_URL (1<<9)

typedef void (*WebcitHandlerFunc)(void);
typedef struct  _WebcitHandler{
	WebcitHandlerFunc F;
	long Flags;
	StrBuf *Name;
	StrBuf *DisplayName;
} WebcitHandler;


void WebcitAddUrlHandler(const char * UrlString, long UrlSLen, const char *DisplayName, long dslen, WebcitHandlerFunc F, long Flags);

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

enum {
	eGET,
	ePOST,
	eOPTIONS,
	ePROPFIND,
	ePUT,
	eDELETE,
	eHEAD,
	eMOVE,
	eCOPY,
	eNONE
};
const char *ReqStrs[eNONE];

#define NO_AUTH 0
#define AUTH_COOKIE 1
#define AUTH_BASIC 2



typedef struct _HdrRefs {
	long eReqType;				/* HTTP method */
	int desired_session;
	int SessionKey;

	int got_auth;
	int DontNeedAuth;
	long ContentLength;
	time_t if_modified_since;
	int gzip_ok;				/* Nonzero if Accept-encoding: gzip */
	int prohibit_caching;
	int dav_depth;

	/* these are references into Hdr->HTTPHeaders, so we don't need to free them. */
	StrBuf *ContentType;
	StrBuf *RawCookie;
	StrBuf *ReqLine;
	StrBuf *http_host;			/* HTTP Host: header */
	StrBuf *browser_host;
	StrBuf *user_agent;
	StrBuf *plainauth;
	StrBuf *dav_ifmatch;

	const WebcitHandler *Handler;
} HdrRefs;

typedef struct _ParsedHttpHdrs {
	int http_sock;				/* HTTP server socket */
	const char *Pos;
	StrBuf *ReadBuf;

	StrBuf *c_username;
	StrBuf *c_password;
	StrBuf *c_roomname;
	StrBuf *c_language;
	StrBuf *this_page;			/* URL of current page */
	StrBuf *PlainArgs; 

	HashList *urlstrings;		        /* variables passed to webcit in a URL */
	HashList *HTTPHeaders;                  /* the headers the client sent us */
	int nWildfireHeaders;                   /* how many wildfire headers did we already send? */

	HdrRefs HR;
} ParsedHttpHdrs;


/*
 * One of these is kept for each active Citadel session.
 * HTTP transactions are bound to one at a time.
 */
typedef struct wcsession wcsession;
struct wcsession {
/* infrastructural members */
	wcsession *next;			/* Linked list */
	pthread_mutex_t SessionMutex;		/* mutex for exclusive access */
	int wc_session;				/* WebCit session ID */
	int killthis;				/* Nonzero == purge this session */
	int is_mobile;			        /* Client is a handheld browser */
	int ctdl_pid;				/* Session ID on the Citadel server */
	int nonce;				/* session nonce (to prevent session riding) */
	int SessionKey;

/* Session local Members */
	int serv_sock;				/* Client socket to Citadel server */
	StrBuf *ReadBuf;                        /* here we keep our stuff while reading linebuffered from the server. */
	StrBuf *MigrateReadLineBuf;             /* here we buffer legacy server read stuff */
	const char *ReadPos;                    /* whats our read position in ReadBuf? */
	int chat_sock;				/* Client socket to Citadel server - for chat */
	time_t lastreq;				/* Timestamp of most recent HTTP */
	time_t last_pager_check;		/* last time we polled for instant msgs */
	ServInfo *serv_info;			/* Information about the citserver we're connected to */
	int is_ajax;                            /* are we doing an ajax request? */

/* Request local Members */
	StrBuf *CLineBuf;                       /* linebuffering client stuff */
	ParsedHttpHdrs *Hdr;
	StrBuf *WBuf;                           /* Our output buffer */
	StrBuf *HBuf;                           /* Our HeaderBuffer */

	HashList *vars; 			/* HTTP variable substitutions for this page */
	StrBuf *trailing_javascript;		/* extra javascript to be appended to page */
	char ImportantMessage[SIZ];
	StrBuf *ImportantMsg;
	HashList *Directory;			/* Parts of the directory URL in snippets */
	const floor *CurrentFloor;              /**< when Parsing REST, which floor are we on? */

/* accounting */
	StrBuf *wc_username;			/* login name of current user */
	StrBuf *wc_fullname;			/* Screen name of current user */
	StrBuf *wc_password;			/* Password of current user */
	StrBuf *httpauth_pass;  		/* only for GroupDAV sessions */
	int axlevel;				/* this user's access level */
	int is_aide;				/* nonzero == this user is an Aide */
	int is_room_aide;			/* nonzero == this user is a Room Aide in this room */
	int connected;				/* nonzero == we are connected to Citadel */
	int logged_in;				/* nonzero == we are logged in  */
	int need_regi;				/* This user needs to register. */
	int need_vali;				/* New users require validation. */

/* Preferences */
	StrBuf *cs_inet_email;  		/* User's preferred Internet addr. */
	char reply_to[512];			/* reply-to address */
	HashList *hash_prefs;			/* WebCit preferences for this user */
	StrBuf *DefaultCharset;                 /* Charset the user preferes */
	int downloaded_prefs;                   /* Has the client download its prefs yet? */
	int SavePrefsToServer;                  /* Should we save our preferences to the server at the end of the request? */
	int selected_language;			/* Language selected by user */
	int time_format_cache;                  /* which timeformat does our user like? */

/* current room related */
	StrBuf *wc_roomname;			/* Room we are currently in */
	unsigned room_flags;			/* flags associated with the current room */
	unsigned room_flags2;			/* flags associated with the current room */
	int wc_view;				/* view for the current room */
	int wc_default_view;			/* default view for the current room */
	int wc_is_trash;			/* nonzero == current room is a Trash folder */
	int wc_floor;				/* floor number of current room */
	int is_mailbox;				/* the current room is a private mailbox */

/* next/previous room thingabob */
	struct march *march;			/* march mode room list */
	char ugname[128];			/* where does 'ungoto' take us */
	long uglsn;				/* last seen message number for ungoto */

/* Uploading; mime attachments for composing messages */
	HashList *attachments;             	/* list of attachments for 'enter message' */
	int upload_length;			/* content length of http-uploaded data */
	StrBuf *upload;				/* pointer to http-uploaded data */
	char upload_filename[PATH_MAX];		/* filename of http-uploaded data */
	char upload_content_type[256];		/* content type of http-uploaded data */

	int new_mail;				/* user has new mail waiting */
	int remember_new_mail;			/* last count of new mail messages */

/* Roomiew control */
	HashList *Floors;                       /* floors our citserver has hashed numeric for quicker access*/
	HashList *FloorsByName;                 /* same but hashed by its name */
	HashList *summ;                         /* list of messages for mailbox summary view */
  /** Perhaps these should be within a struct instead */
	long startmsg;                          /* message number to start at */
	long maxmsgs;                           /* maximum messages to display */
        long num_displayed;                     /* number of messages actually displayed */
	HashList *disp_cal_items;               /* sorted list of calendar items; startdate is the sort criteria. */


	char last_chat_user[256];

/* Iconbar controls */
	struct __ofolder *cache_fold;		/* cache the iconbar room list */
	int cache_max_folders;
	int cache_num_floors;
	time_t cache_timestamp;
	HashList *IconBarSettings;		/* which icons should be shown / not shown? */
	const StrBuf *floordiv_expanded;	/* which floordiv currently expanded */



/* cache stuff for templates. TODO: find a smartrer way */
	HashList *ServCfg;                      /* cache our server config for editing */
	HashList *InetCfg;                      /* Our inet server config for editing */

};


typedef void (*Header_Evaluator)(StrBuf *Line, ParsedHttpHdrs *hdr);

typedef struct _HttpHeader {
	Header_Evaluator H;
	StrBuf *Val;
	int HaveEvaluator;
} OneHttpHeader;

void RegisterHeaderHandler(const char *Name, long Len, Header_Evaluator F);


enum {
	S_SELECT,
	S_SHUTDOWN,
	MAX_SEMAPHORES
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

extern char floorlist[128][SIZ];
extern char *axdefs[];
extern char *ctdlhost, *ctdlport;
extern int http_port;
extern char *server_cookie;
extern int is_https;
extern int setup_wizard;
extern char wizard_filename[];
extern int follow_xff;
extern int num_threads;

void InitialiseSemaphores(void);
void begin_critical_section(int which_one);
void end_critical_section(int which_one);

void stuff_to_cookie(char *cookie, size_t clen,
		int session,
		StrBuf *user,
		StrBuf *pass,
		StrBuf *room,
		const char *language
);
void cookie_to_stuff(StrBuf *cookie,
		int *session,
		StrBuf *user,
		StrBuf *pass,
		StrBuf *room,
		StrBuf *language
);
void locate_host(StrBuf *TBuf, int);
void become_logged_in(const StrBuf *user, const StrBuf *pass, StrBuf *serv_response);
void openid_manual_create(void);
void display_login();
void display_openids(void);
void do_welcome(void);
void do_logout(void);
void display_main_menu(void);
void display_aide_menu(void);
void display_advanced_menu(void);
void slrp_highest(void);
ServInfo *get_serv_info(StrBuf *, StrBuf *);
void RegisterEmbeddableMimeType(const char *MimeType, long MTLen, int Priority);
void CreateMimeStr(void);
int GetConnected(void);
void DeleteServInfo(ServInfo **FreeMe);
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
void output_custom_content_header(const char *ctype);
void wc_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void hprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void output_static(const char* What);

long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks);
void escputs(const char *strbuf);
void url(char *buf, size_t bufsize);
void UrlizeText(StrBuf* Target, StrBuf *Source, StrBuf *WrkBuf);
void escputs1(const char *strbuf, int nbsp, int nolinebreaks);
void msgesc(char *target, size_t tlen, char *strbuf);
void msgescputs(char *strbuf);
void msgescputs1(char *strbuf);
void dump_vars(void);
void embed_main_menu(void);

void do_addrbook_view(addrbookent *addrbook, int num_ab);
void fetch_ab_name(message_summary *Msg, char **namebuf);
void display_vcard(StrBuf *Target, StrBuf *vcard_source, char alpha, int full, char **storename, long msgnum);
void jsonMessageList(void);
void new_summary_view(void);
void getseen(void);
void text_to_server(char *ptr);
void text_to_server_qp(char *ptr);
void confirm_delete_msg(void);
void display_success(char *);
void CheckAuthBasic(ParsedHttpHdrs *hdr);
void GetAuthBasic(ParsedHttpHdrs *hdr);
void server_to_text(void);
void save_edit(char *description, char *enter_cmd, int regoto);
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner);
long gotoroom(const StrBuf *gname);
void remove_march(const StrBuf *aaa);
void dotskip(void);
void confirm_delete_room(void);
void validate(void);
void display_graphics_upload(char *, char *, char *);
void do_graphics_upload(char *upl_cmd);
void serv_gets(char *strbuf);
void serv_write(const char *buf, int nbytes);
void serv_putbuf(const StrBuf *string);
void serv_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void load_floorlist(StrBuf *Buf);
void shutdown_sessions(void);
void do_housekeeping(void);
void smart_goto(const StrBuf *);
void worker_entry(void);
void session_loop(void);
size_t wc_strftime(char *s, size_t max, const char *format, const struct tm *tm);
void fmt_time(char *buf, size_t siz, time_t thetime);
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
StrBuf *load_mimepart(long msgnum, char *partnum);
void MimeLoadData(wc_mime_attachment *Mime);
int pattern2(char *search, char *patn);
void do_edit_vcard(long msgnum, char *partnum, 
		   message_summary *VCMsg,
		   wc_mime_attachment *VCAtt,
		   const char *return_to, 
		   const char *force_room);

void select_user_to_edit(const char *preselect);
void delete_user(char *);
void do_change_view(int);
void folders(void);



void display_addressbook(long msgnum, char alpha);
void offer_start_page(StrBuf *Target, WCTemplputParams *TP);
void convenience_page(const char *titlebarcolor, const char *titlebarmsg, const char *messagetext);
void output_html(const char *, int, int, StrBuf *, StrBuf *);
void do_listsub(void);
ssize_t write(int fd, const void *buf, size_t count);
void cal_process_attachment(wc_mime_attachment *Mime);
void display_calendar(message_summary *Msg, int unread);
void display_note(message_summary *Msg, int unread);
void updatenote(void);
void do_tasks_view(void);
int calendar_summary_view(void);
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
void partstat_as_string(char *buf, icalproperty *attendee);
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp);
void check_attendee_availability(icalcomponent *supplied_vevent);
int ical_ctdl_is_overlap(
                        struct icaltimetype t1start,
                        struct icaltimetype t1end,
                        struct icaltimetype t2start,
                        struct icaltimetype t2end
);


extern char *months[];
extern char *days[];
int read_server_binary(StrBuf *Ret, size_t total_len, StrBuf *Buf);
int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize);
int StrBuf_ServGetBLOBBuffered(StrBuf *buf, long BlobSize);
int read_server_text(StrBuf *Buf, long *nLines);
int goto_config_room(StrBuf *Buf);
long locate_user_vcard_in_this_room(message_summary **VCMsg,
				    wc_mime_attachment **VCAtt);
void sleeeeeeeeeep(int);
void http_transmit_thing(const char *content_type, int is_static);
long unescape_input(char *buf);
void do_selected_iconbar(void);
void spawn_another_worker_thread(void);
void StrEndTab(StrBuf *Target, int tabnum, int num_tabs);
void StrBeginTab(StrBuf *Target, int tabnum, int num_tabs);
void StrTabbedDialog(StrBuf *Target, int num_tabs, StrBuf *tabnames[]);
void tabbed_dialog(int num_tabs, char *tabnames[]);
void begin_tab(int tabnum, int num_tabs);
void end_tab(int tabnum, int num_tabs);
void str_wiki_index(char *s);
long guess_calhourformat(void);
int get_time_format_cached (void);
int xtoi(const char *in, size_t len);
const char *get_selected_language(void);

#define DATEFMT_FULL 0
#define DATEFMT_BRIEF 1
#define DATEFMT_RAWDATE 2
#define DATEFMT_LOCALEDATE 3
void webcit_fmt_date(char *buf, size_t siz, time_t thetime, int Format);
int fetch_http(char *url, char *target_buf, int maxbytes);
void free_attachments(wcsession *sess);
void summary(void);

int is_mobile_ua(char *user_agent);

void embed_room_banner(char *, int);
HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP);
HashList *GetRoomListHash(StrBuf *Target, WCTemplputParams *TP);
int SortRoomsByListOrder(const void *room1, const void *room2);
/* navbar types that can be passed to embed_room_banner */
enum {
	navbar_none,
	navbar_default
};

/* actual supported locales */
void TmplGettext(StrBuf *Target, WCTemplputParams *TP);
void offer_languages(StrBuf *Target, int nArgs, WCTemplateToken *Token, void *Context, int ContextType);
void set_selected_language(const char *);
void go_selected_language(void);
void stop_selected_language(void);

#ifdef HAVE_OPENSSL
void init_ssl(void);
void endtls(void);
void ssl_lock(int mode, int n, const char *file, int line);
int starttls(int sock);
extern SSL_CTX *ssl_ctx;  
int client_read_sslbuffer(StrBuf *buf, int timeout);
void client_write_ssl(const StrBuf *Buf);
#endif

void utf8ify_rfc822_string(char **buf);

void begin_burst(void);
long end_burst(void);

extern char *hourname[];	/* Names of hours (12am, 1am, etc.) */

void http_datestring(char *buf, size_t n, time_t xtime);


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

