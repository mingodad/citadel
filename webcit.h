
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
#include "wc_gettext.h"
#include "messages.h"
#include "paramhandling.h"
#include "preferences.h"


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
	StrBuf *serv_nodename;		/* Node name of the Citadel server */
	StrBuf *serv_humannode;	        /* human readable node name of the Citadel server */
	StrBuf *serv_fqdn;		/* fully quallified Domain Name (such as uncensored.citadel.org) */
	StrBuf *serv_software;		/* What version does our connected citadel server use */
	int serv_rev_level;		/* Whats the citadel server revision */
	StrBuf *serv_bbs_city;		/* Geographic location of the Citadel server */
	StrBuf *serv_sysadm;		/* Name of system administrator */
	StrBuf *serv_moreprompt;	/* Whats the commandline textprompt */
	int serv_ok_floors;		/* nonzero == server supports floors */
	int serv_supports_ldap;		/* is the server linked against an ldap tree for adresses? */
	int serv_newuser_disabled;	/* Has the server disabled self-service new user creation? */
	StrBuf *serv_default_cal_zone;  /* Default timezone for unspecified calendar items */
	int serv_supports_sieve;	/* Does the server support Sieve mail filtering? */
	int serv_fulltext_enabled;	/* Does the server have the full text index enabled? */
	StrBuf *serv_svn_revision;	/* SVN revision of the server */
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
	StrBuf *wc_username;			/**< login name of current user */
	StrBuf *wc_fullname;			/**< Screen name of current user */
	StrBuf *wc_password;			/**< Password of current user */
	StrBuf *wc_roomname;			/**< Room we are currently in */
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
  /** Perhaps these should be within a struct instead */
	long startmsg;                          /**< message number to start at */
	long maxmsgs;                           /**< maximum messages to display */
        long num_displayed;                     /**< number of messages actually displayed */
	int is_mobile;			        /**< Client is a handheld browser */
	HashList *urlstrings;		        /**< variables passed to webcit in a URL */
	HashList *vars; 			/**< HTTP variable substitutions for this page */
	char this_page[512];			/**< URL of current page */
	char http_host[512];			/**< HTTP Host: header */
	HashList *hash_prefs;			/**< WebCit preferences for this user */
	int SavePrefsToServer;                  /**< Should we save our preferences to the server at the end of the request? */
	HashList *disp_cal_items;               /**< sorted list of calendar items; startdate is the sort criteria. */
	HashList *attachments;             	/**< list of attachments for 'enter message' */
	char last_chat_user[256];		/**< ??? todo */
	char ImportantMessage[SIZ];		/**< ??? todo */
	int ctdl_pid;				/**< Session ID on the Citadel server */
	StrBuf *httpauth_user;  		/**< only for GroupDAV sessions */
	StrBuf *httpauth_pass;  		/**< only for GroupDAV sessions */
	int gzip_ok;				/**< Nonzero if Accept-encoding: gzip */
	int is_mailbox;				/**< the current room is a private mailbox */
	struct folder *cache_fold;		/**< cache the iconbar room list */
	int cache_max_folders;			/**< ??? todo */
	int cache_num_floors;			/**< ??? todo */
	time_t cache_timestamp;			/**< ??? todo */
	HashList *IconBarSettings;             /**< which icons should be shown / not shown? */
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
  int is_ajax; /** < are we doing an ajax request? */
  int downloaded_prefs; /** Has the client download its prefs yet? */
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
	calview_brief,
	calview_summary
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
extern HashList *ZoneHash;
extern HashList *SortHash;

void InitialiseSemaphores(void);
void begin_critical_section(int which_one);
void end_critical_section(int which_one);


void stuff_to_cookie(char *cookie, size_t clen, int session,
			StrBuf *user, StrBuf *pass, StrBuf *room);
void cookie_to_stuff(StrBuf *cookie, int *session,
		     StrBuf *user,
		     StrBuf *pass,
		     StrBuf *room);
void locate_host(char *, int);
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
void wprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void hprintf(const char *format,...)__attribute__((__format__(__printf__,1,2)));
void output_static(char *what);

void print_menu_box(char* Title, char *Class, int nLines, ...);
long stresc(char *target, long tSize, char *strbuf, int nbsp, int nolinebreaks);
void escputs(const char *strbuf);
void url(char *buf, size_t bufsize);
void UrlizeText(StrBuf* Target, StrBuf *Source, StrBuf *WrkBuf);
void escputs1(const char *strbuf, int nbsp, int nolinebreaks);
void msgesc(char *target, size_t tlen, char *strbuf);
void msgescputs(char *strbuf);
void msgescputs1(char *strbuf);
void stripout(char *str, char leftboundary, char rightboundary);
void dump_vars(void);
void embed_main_menu(void);
void serv_read(char *buf, int bytes);

void SetAccessCommand(long Oper);
void do_addrbook_view(addrbookent *addrbook, int num_ab);
void fetch_ab_name(message_summary *Msg, char *namebuf);
void display_vcard(StrBuf *Target, const char *vcard_source, char alpha, int full, char *storename, long msgnum);
void jsonMessageList(void);
void new_summary_view(void);
void getseen(void);
void text_to_server(char *ptr);
void text_to_server_qp(char *ptr);
void confirm_delete_msg(void);
void display_success(char *);
void authorization_required(const char *message);
void server_to_text(void);
void save_edit(char *description, char *enter_cmd, int regoto);
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner);
long gotoroom(const StrBuf *gname);
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
void smart_goto(const StrBuf *);
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
void do_edit_vcard(long, char *, char *, const char *);
void striplt(char *);
void stripltlen(char *, int *);
void select_user_to_edit(char *message, char *preselect);
void delete_user(char *);
void do_change_view(int);
void folders(void);



void display_addressbook(long msgnum, char alpha);
void offer_start_page(StrBuf *Target, WCTemplputParams *TP);
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


extern char *months[];
extern char *days[];
int read_server_binary(StrBuf *Ret, size_t total_len);
int StrBuf_ServGetBLOB(StrBuf *buf, long BlobSize);
int read_server_text(StrBuf *Buf, long *nLines);
int goto_config_room(void);
long locate_user_vcard_in_this_room(void);
void sleeeeeeeeeep(int);
void http_transmit_thing(const char *content_type, int is_static);
long unescape_input(char *buf);
void do_selected_iconbar(void);
void spawn_another_worker_thread(void);
void display_rss(const StrBuf *roomname, StrBuf *request_method);
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
void webcit_fmt_date(char *buf, time_t thetime, int brief);
int fetch_http(char *url, char *target_buf, int maxbytes);
void free_attachments(wcsession *sess);
void summary(void);

int is_mobile_ua(char *user_agent);

void embed_room_banner(char *, int);
#define FLOOR_PARAM_LEN 3
extern const char FLOOR_PARAM_NAMES[(FLOOR_PARAM_LEN + 1)][15];
extern const int FLOOR_PARAM_NAMELEN[(FLOOR_PARAM_LEN + 1)];
#define FPKEY(a) FLOOR_PARAM_NAMES[a], FLOOR_PARAM_NAMELEN[a]
#define ROOM_PARAM_LEN 8
extern const char ROOM_PARAM_NAMES[(ROOM_PARAM_LEN + 1)][20];
extern const int ROOM_PARAM_NAMELEN[(ROOM_PARAM_LEN +1)];
#define RPKEY(a) ROOM_PARAM_NAMES[a], ROOM_PARAM_NAMELEN[a]
HashList *GetFloorListHash(StrBuf *Target, WCTemplputParams *TP);
HashList *GetRoomListHash(StrBuf *Target, WCTemplputParams *TP);
int SortRoomsByListOrder(const void *room1, const void *room2);
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


void LoadIconSettings(void);
