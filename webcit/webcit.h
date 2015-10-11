/*
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

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
#include <sys/stat.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/un.h>
#include <sys/poll.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <syslog.h>
#include <sys/utsname.h>
#include <expat.h>
#include <libcitadel.h>

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

#define DO_DBG_QR 0
#define DBG_QR(x) if(DO_DBG_QR) _DBG_QR(x)
#define DBG_QR2(x) if(DO_DBG_QR) _DBG_QR2(x)

#include <zlib.h>

#include <libical/ical.h>

#undef PACKAGE
#undef VERSION
#undef PACKAGE_NAME
#undef PACKAGE_STRING
#undef PACKAGE_TARNAME
#undef PACKAGE_VERSION
#undef PACKAGE_BUGREPORT

typedef struct wcsession wcsession;

#include "sysdep.h"

#include "subst.h"
#include "messages.h"
#include "paramhandling.h"
#include "roomops.h"
#include "preferences.h"

#include "tcp_sockets.h"
#include "utils.h"
#ifdef HAVE_OPENSSL
/* Work around RedHat's b0rken OpenSSL includes */
#define OPENSSL_NO_KRB5
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
extern char *ssl_cipher_list;
#define	DEFAULT_SSL_CIPHER_LIST	"DEFAULT"	/* See http://openssl.org/docs/apps/ciphers.html */
#endif

#if SIZEOF_SIZE_T == SIZEOF_INT 
#define SIZE_T_FMT "%d"
#else
#define SIZE_T_FMT "%ld"
#endif

#if SIZEOF_LONG_UNSIGNED_INT == SIZEOF_INT
#define ULONG_FMT "%d"
#else
#define ULONG_FMT "%ld"
#endif

#define CALENDAR_ROOM_NAME	"Calendar"
#define PRODID "-//Citadel//NONSGML Citadel Calendar//EN"

#define SIZ			4096		/* generic buffer size */

#define TRACE syslog(LOG_DEBUG, "\033[3%dmCHECKPOINT: %s:%d\033[0m", ((__LINE__%6)+1), __FILE__, __LINE__)

#ifdef UNDEF_MEMCPY
#undef memcpy
#endif

#define SLEEPING		180		/* TCP connection timeout */
#define WEBCIT_TIMEOUT		900		/* WebCit session timeout */
#define PORT_NUM		2000		/* port number to listen on */
#define DEVELOPER_ID		0
#define CLIENT_ID		4
#define CLIENT_VERSION		901		/* This version of WebCit */
#define MINIMUM_CIT_VERSION	901		/* Minimum required version of Citadel server */
#define	LIBCITADEL_MIN		901		/* Minimum required version of libcitadel */
#define DEFAULT_HOST		"localhost"	/* Default Citadel server */
#define DEFAULT_PORT		"504"
#define TARGET			"webcit01"	/* Window target for inline URL's */
#define HOUSEKEEPING		15		/* Housekeeping frequency */
#define MAX_WORKER_THREADS	250
#define LISTEN_QUEUE_LENGTH	100		/* listen() backlog queue */

#define USERCONFIGROOM		"My Citadel Config"
#define DEFAULT_MAXMSGS		20


#ifdef LIBCITADEL_VERSION_NUMBER
#if LIBCITADEL_VERSION_NUMBER < LIBCITADEL_MIN
#error libcitadel is too old.  Please upgrade it before continuing.
#endif
#endif




#define SRV_STATUS_MSG(ServerLineBuf) (ChrPtr(ServerLineBuf) + 4), (StrLength(ServerLineBuf) - 4)
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
 * that are active but do not (yet) have a user logged in.
 */
#define NLI	"(not logged in)"

/*
 * Expiry policy for the autopurger
 */
#define EXPIRE_NEXTLEVEL        0       /* Inherit expiration policy    */
#define EXPIRE_MANUAL           1       /* Don't expire messages at all */
#define EXPIRE_NUMMSGS          2       /* Keep only latest n messages  */
#define EXPIRE_AGE              3       /* Expire messages after n days */

typedef struct __ExpirePolicy {
        int expire_mode;
        int expire_value;
} ExpirePolicy;
void LoadExpirePolicy(GPEXWhichPolicy which);
void SaveExpirePolicyFromHTTP(GPEXWhichPolicy which);

/*
 * Linked list of session variables encoded in an x-www-urlencoded content type
 */
typedef struct urlcontent urlcontent;
struct urlcontent {
	char url_key[32];		/* key */
	long klen;
	StrBuf *url_data;		/* value */
	HashList *sub;
};

/*
 * Information about the Citadel server to which we are connected
 */ 
typedef struct _serv_info {
	int serv_pid;			/* Process ID of the Citadel server */
	StrBuf *serv_nodename;		/* Node name of the Citadel server */
	StrBuf *serv_humannode;	        /* Juman readable node name of the Citadel server */
	StrBuf *serv_fqdn;		/* Fully qualified Domain Name (such as uncensored.citadel.org) */
	StrBuf *serv_software;		/* Free form text description of the server software in use */
	int serv_rev_level;		/* Server version number (times 100) */
	StrBuf *serv_bbs_city;		/* Geographic location of the Citadel server */
	StrBuf *serv_sysadm;		/* Name of system administrator */
	int serv_supports_ldap;		/* is the server linked against an ldap tree for adresses? */
	int serv_newuser_disabled;	/* Has the server disabled self-service new user creation? */
	StrBuf *serv_default_cal_zone;  /* Default timezone for unspecified calendar items */
	int serv_supports_sieve;	/* Server supports Sieve mail filtering */
	int serv_fulltext_enabled;	/* Full text index is enabled */
	StrBuf *serv_svn_revision;	/* svn or git revision of the server */
	int serv_supports_openid;	/* Server supports authentication via OpenID */
	int serv_supports_guest;	/* Server supports unauthenticated guest logins */
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

typedef struct _IcalKindEnumMap {
	const char *Name;
	long NameLen;
	icalproperty_kind map;
} IcalKindEnumMap;
typedef struct _IcalMethodEnumMap {
	const char *Name;
	long NameLen;
        icalproperty_method map;
} IcalMethodEnumMap;

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
#define PROHIBIT_STARTPAGE (1<<10)


#define DATEFMT_FULL 0
#define DATEFMT_BRIEF 1
#define DATEFMT_RAWDATE 2
#define DATEFMT_LOCALEDATE 3
long webcit_fmt_date(char *buf, size_t siz, time_t thetime, int Format);


typedef enum _RESTDispatchID {
	ExistsID,
	PutID,
	DeleteID
} RESTDispatchID;

typedef int (*WebcitRESTDispatchID)(RESTDispatchID WhichAction, int IgnoreFloor);
typedef void (*WebcitHandlerFunc)(void);
typedef struct  _WebcitHandler{
	WebcitHandlerFunc F;
	WebcitRESTDispatchID RID;
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
	eREPORT,
	eNONE
};
extern const char *ReqStrs[eNONE];

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
	int Static;

	/* these are references into Hdr->HTTPHeaders, so we don't need to free them. */
	StrBuf *ContentType;
	StrBuf *RawCookie;
	StrBuf *ReqLine;
	StrBuf *browser_host;
	StrBuf *browser_language;
	StrBuf *user_agent;
	StrBuf *plainauth;
	StrBuf *dav_ifmatch;

	const WebcitHandler *Handler;
} HdrRefs;

typedef struct _ParsedHttpHdrs {
	int http_sock;				/* HTTP server socket */
	long HaveRange;
	long RangeStart;
	long RangeTil;
	long TotalBytes;
	const char *Pos;
	StrBuf *ReadBuf;

	StrBuf *c_username;
	StrBuf *c_password;
	StrBuf *c_roomname;
	StrBuf *c_language;
	StrBuf *this_page;			/* URL of current page */
	StrBuf *PlainArgs; 
	StrBuf *HostHeader;

	HashList *urlstrings;		        /* variables passed to webcit in a URL */
	HashList *HTTPHeaders;                  /* the headers the client sent us */
	int nWildfireHeaders;                   /* how many wildfire headers did we already send? */

	HdrRefs HR;
} ParsedHttpHdrs;


/*
 * One of these is kept for each active Citadel session.
 * HTTP transactions are bound to one at a time.
 */
struct wcsession {
/* infrastructural members */
	wcsession *next;			/* Linked list */
	pthread_mutex_t SessionMutex;		/* mutex for exclusive access */
	int wc_session;				/* WebCit session ID */
	int killthis;				/* Nonzero == purge this session */
	int ctdl_pid;				/* Session ID on the Citadel server */
	int nonce;				/* session nonce (to prevent session riding) */
	int inuse;				/* set to nonzero if bound to a running thread */
	int isFailure;                          /* Http 2xx or 5xx? */

/* Session local Members */
	int serv_sock;				/* Client socket to Citadel server */
	StrBuf *ReadBuf;                        /* linebuffered reads from the server */
	StrBuf *MigrateReadLineBuf;             /* here we buffer legacy server read stuff */
	const char *ReadPos;                    /* whats our read position in ReadBuf? */
	int last_chat_seq;			/* When in chat - last message seq# we saw */
	time_t lastreq;				/* Timestamp of most recent HTTP */
	time_t last_pager_check;		/* last time we polled for instant msgs */
	ServInfo *serv_info;			/* Information about the citserver we're connected to */
	StrBuf *PushedDestination;		/* Where to go after login, registration, etc. */

/* Request local Members */
	StrBuf *CLineBuf;                       /* linebuffering client stuff */
	ParsedHttpHdrs *Hdr;
	StrBuf *WBuf;                           /* Our output buffer */
	StrBuf *HBuf;                           /* Our HeaderBuffer */
	StrBuf *WFBuf;                          /* Wildfire error logging buffer */
	StrBuf *trailing_javascript;		/* extra javascript to be appended to page */
	StrBuf *ImportantMsg;
	HashList *Directory;			/* Parts of the directory URL in snippets */
	const Floor *CurrentFloor;              /* when Parsing REST, which floor are we on? */

/* accounting */
	StrBuf *wc_username;			/* login name of current user */
	StrBuf *wc_fullname;			/* Screen name of current user */
	StrBuf *wc_password;			/* Password of current user */
	StrBuf *httpauth_pass;  		/* only for GroupDAV sessions */
	int axlevel;				/* this user's access level */
	int is_aide;				/* nonzero == this user is an Admin */
	int connected;				/* nonzero == we are connected to Citadel */
	int logged_in;				/* nonzero == we are logged in  */
	int need_regi;				/* This user needs to register. */
	int need_vali;				/* New users require validation. */

/* Preferences */
	StrBuf *cs_inet_email;  		/* User's preferred Internet addr. */
	HashList *hash_prefs;			/* WebCit preferences for this user */
	StrBuf *DefaultCharset;                 /* Charset the user preferes */
	int downloaded_prefs;                   /* Has the client download its prefs yet? */
	int SavePrefsToServer;                  /* Should we save our preferences to the server at the end of the request? */
	int selected_language;			/* Language selected by user */
	int time_format_cache;                  /* which timeformat does our user like? */

	folder CurRoom;                         /* information about our current room */
	const folder *ThisRoom;                 /* if REST found a room, remember it here. */
/* next/previous room thingabob */
	struct march *march;			/* march mode room list */
	char ugname[128];			/* where does 'ungoto' take us */
	long uglsn;				/* last seen message number for ungoto */

/* Uploading; mime attachments for composing messages */
	HashList *attachments;             	/* list of attachments for 'enter message' */
	int upload_length;			/* content length of http-uploaded data */
	StrBuf *upload;				/* pointer to http-uploaded data */
	StrBuf *upload_filename;		/* filename of http-uploaded data */
	char upload_content_type[256];		/* content type of http-uploaded data */

	int remember_new_mail;			/* last count of new mail messages */

/* Roomiew control */
	HashList *Floors;                       /* floors our citserver has hashed numeric for quicker access*/
	HashList *FloorsByName;                 /* same but hashed by its name */
	HashList *Rooms;                        /* our directory structure as loaded by LKRA */
	HashList *summ;                         /* list of messages for mailbox summary view */
  /** Perhaps these should be within a struct instead */
	long startmsg;                          /* message number to start at */
	long maxmsgs;                           /* maximum messages to display */
        long num_displayed;                     /* number of messages actually displayed */
	HashList *disp_cal_items;               /* sorted list of calendar items; startdate is the sort criteria. */


	char last_chat_user[256];

	StrBuf *IconTheme;                      /* Icontheme setting */

/* Iconbar controls */
	int cache_max_folders;
	int cache_num_floors;
	long *IBSettingsVec;                    /* which icons should be shown / not shown? */
	const StrBuf *floordiv_expanded;	/* which floordiv currently expanded */
	int ib_wholist_expanded;
	int ib_roomlist_expanded;

/* our known Sieve scripts; loaded by SIEVE:SCRIPTS iterator. */
	HashList *KnownSieveScripts;

/* Transcoding cache buffers; used to avoid to frequent realloc */
	StrBuf *ConvertBuf1;
	StrBuf *ConvertBuf2;

/* cache stuff for templates. TODO: find a smarter way */
	HashList *ServCfg;                      /* cache our server config for editing */
	HashList *InetCfg;                      /* Our inet server config for editing */
	ExpirePolicy Policy[maxpolicy];

/* used by the blog viewer */
	int bptlid;				/* hash of thread currently being rendered */
};


typedef void (*Header_Evaluator)(StrBuf *Line, ParsedHttpHdrs *hdr);

typedef struct _HttpHeader {
	Header_Evaluator H;
	StrBuf *Val;
	int HaveEvaluator;
} OneHttpHeader;

void RegisterHeaderHandler(const char *Name, long Len, Header_Evaluator F);


enum {
	S_SHUTDOWN,
	S_SPAWNER,
	MAX_SEMAPHORES
};

#ifndef num_parms
#define num_parms(source)		num_tokens(source, '|') 
#endif

#define site_prefix	(WC ? (WC->Hdr->HostHeader) : NULL)

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

void init_ssl(void);
void endtls(void);
void ssl_lock(int mode, int n, const char *file, int line);
int starttls(int sock);
extern SSL_CTX *ssl_ctx;  
int client_read_sslbuffer(StrBuf *buf, int timeout);
int client_write_ssl(const StrBuf *Buf);
#endif

extern int is_https;
extern int follow_xff;
extern char *server_cookie;
extern char *ctdlhost, *ctdlport;
extern char *axdefs[];
extern int num_threads_existing;
extern int num_threads_executing;
extern int setup_wizard;
extern char wizard_filename[];

void InitialiseSemaphores(void);
void begin_critical_section(int which_one);
void end_critical_section(int which_one);

void CheckGZipCompressionAllowed(const char *MimeType, long MLen);

extern void do_404(void);
void http_redirect(const char *);


#ifdef UBER_VERBOSE_DEBUGGING
#define wc_printf(...) wcc_printf(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)
void wcc_printf(const char *FILE, const char *FUNCTION, long LINE, const char *format, ...);
#else 
void wc_printf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
#endif

void hprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));

void CheckAuthBasic(ParsedHttpHdrs *hdr);
void GetAuthBasic(ParsedHttpHdrs *hdr);

void sleeeeeeeeeep(int);

size_t wc_strftime(char *s, size_t max, const char *format, const struct tm *tm);
void fmt_time(char *buf, size_t siz, time_t thetime);
void httpdate(char *buf, time_t thetime);
time_t httpdate_to_timestamp(StrBuf *buf);




void end_webcit_session(void);




void cookie_to_stuff(StrBuf *cookie,
		int *session,
		StrBuf *user,
		StrBuf *pass,
		StrBuf *room,
		StrBuf *language
);
void locate_host(StrBuf *TBuf, int);
void become_logged_in(const StrBuf *user, const StrBuf *pass, StrBuf *serv_response);

void display_login(void);
void display_openids(void);
void display_default_landing_page(void);
void do_welcome(void);

void display_reg(int during_login);
void display_main_menu(void);
void display_aide_menu(void);

void RegisterEmbeddableMimeType(const char *MimeType, long MTLen, int Priority);
void CreateMimeStr(void);


void pop_destination(void);

void FmOut(StrBuf *Target, const char *align, const StrBuf *Source);
void wDumpContent(int);


void PutRequestLocalMem(void *Data, DeleteHashDataFunc DeleteIt);

void output_headers(    int do_httpheaders,
			int do_htmlhead,
			int do_room_banner,
			int unset_cookies,
			int suppress_check,
			int cache);
void output_custom_content_header(const char *ctype);
void cdataout(char *rawdata);


void url(char *buf, size_t bufsize);
void UrlizeText(StrBuf* Target, StrBuf *Source, StrBuf *WrkBuf);


void display_vcard(StrBuf *Target, wc_mime_attachment *Mime, char alpha, int full, char **storename, long msgnum);

void display_success(const char *successmessage);

void shutdown_sessions(void);



StrBuf *load_mimepart(long msgnum, char *partnum);
void MimeLoadData(wc_mime_attachment *Mime);
void do_edit_vcard(long msgnum, char *partnum, 
		   message_summary *VCMsg,
		   wc_mime_attachment *VCAtt,
		   const char *return_to, 
		   const char *force_room);

void select_user_to_edit(const char *preselect);

void convenience_page(const char *titlebarcolor, const char *titlebarmsg, const char *messagetext);
void output_html(const char *, int, int, StrBuf *, StrBuf *);

ssize_t write(int fd, const void *buf, size_t count);
void cal_process_attachment(wc_mime_attachment *Mime);

void begin_ajax_response(void);
void end_ajax_response(void);

extern char *months[];
extern char *days[];
long locate_user_vcard_in_this_room(message_summary **VCMsg,
				    wc_mime_attachment **VCAtt);
void http_transmit_thing(const char *content_type, int is_static);
void http_transmit_headers(const char *content_type, int is_static, long is_chunked, int is_gzip);
long unescape_input(char *buf);
void check_thread_pool_size(void);
void StrEndTab(StrBuf *Target, int tabnum, int num_tabs);
void StrBeginTab(StrBuf *Target, int tabnum, int num_tabs, StrBuf **Names);
void StrTabbedDialog(StrBuf *Target, int num_tabs, StrBuf *tabnames[]);
void tabbed_dialog(int num_tabs, char *tabnames[]);
void begin_tab(int tabnum, int num_tabs);
void end_tab(int tabnum, int num_tabs);

int get_time_format_cached (void);
void display_wiki_pagelist(void);
void str_wiki_index(char *);

HashList *GetRoomListHashLKRA(StrBuf *Target, WCTemplputParams *TP);

/* actual supported locales */
void TmplGettext(StrBuf *Target, WCTemplputParams *TP);

void set_selected_language(const char *);
void go_selected_language(void);
void stop_selected_language(void);
const char *get_selected_language(void);

void utf8ify_rfc822_string(char **buf);

void begin_burst(void);
long end_burst(void);

void AppendImportantMessage(const char *pch, long len);

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

extern int time_to_die;			/* Nonzero if server is shutting down */
extern int DisableGzip;

/* 
 * Array type for a blog post.  The first message is the post; the rest are comments
 */
struct blogpost {
	int top_level_id;
	long *msgs;		/* Array of msgnums for messages we are displaying */
	int num_msgs;		/* Number of msgnums stored in 'msgs' */
	int alloc_msgs;		/* Currently allocated size of array */
	int unread_oments;
};


/*
 * Data which gets returned from a call to blogview_learn_thread_references()
 */
struct bltr {
	int id;
	int refs;
};


struct bltr blogview_learn_thread_references(long msgnum);
void tmplput_blog_permalink(StrBuf *Target, WCTemplputParams *TP);
void display_summary_page(void);

HashList *GetValidDomainNames(StrBuf *Target, WCTemplputParams *TP);
