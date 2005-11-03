/* $Id$ */

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

#ifdef HAVE_ICONV
#include <iconv.h>
#endif

#include "gettext.h"

#if ENABLE_NLS
#include <locale.h>
#define _(string)	gettext(string)
#else
#define _(string)	(string)
#endif

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

#ifdef HAVE_ICAL_H
#ifdef HAVE_LIBICAL
#define WEBCIT_WITH_CALENDAR_SERVICE 1
#endif
#endif

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
#include <ical.h>
#endif

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
#define SERVER			"WebCit v6.31"	/* who's in da house */
#define DEVELOPER_ID		0
#define CLIENT_ID		4
#define CLIENT_VERSION		631		/* This version of WebCit */
#define MINIMUM_CIT_VERSION	661		/* min required Citadel ver. */
#define DEFAULT_HOST		"localhost"	/* Default Citadel server */
#define DEFAULT_PORT		"504"
#define LB			(1)		/* Internal escape chars */
#define RB			(2)
#define QU			(3)
#define TARGET			"webcit01"	/* Target for inline URL's */
#define HOUSEKEEPING		15		/* Housekeeping frequency */
#define MIN_WORKER_THREADS	5
#define MAX_WORKER_THREADS	250
#define LISTEN_QUEUE_LENGTH	100		/* listen() backlog queue */

#define USERCONFIGROOM		"My Citadel Config"
#define DEFAULT_MAXMSGS		20


/*
 * Room flags (from Citadel)
 *
 * bucket one...
 */
#define QR_PERMANENT	1		/* Room does not purge	      */
#define QR_INUSE	2		/* Set if in use, clear if avail    */
#define QR_PRIVATE	4		/* Set for any type of private room */
#define QR_PASSWORDED	8		/* Set if there's a password too    */
#define QR_GUESSNAME	16		/* Set if it's a guessname room     */
#define QR_DIRECTORY	32		/* Directory room		   */
#define QR_UPLOAD	64		/* Allowed to upload		*/
#define QR_DOWNLOAD	128		/* Allowed to download	      */
#define QR_VISDIR	256		/* Visible directory		*/
#define QR_ANONONLY	512		/* Anonymous-Only room	      */
#define QR_ANONOPT	1024		/* Anonymous-Option room	    */
#define QR_NETWORK	2048		/* Shared network room	      */
#define QR_PREFONLY	4096		/* Preferred status needed to enter */
#define QR_READONLY	8192		/* Aide status required to post     */
#define QR_MAILBOX	16384		/* Set if this is a private mailbox */

/*
 * bucket two...
 */
#define QR2_SYSTEM	1		/* System room; hide by default     */
#define QR2_SELFLIST	2		/* Self-service mailing list mgmt   */


#define UA_KNOWN		2
#define UA_GOTOALLOWED	  4
#define UA_HASNEWMSGS	   8
#define UA_ZAPPED	       16


/*
 * User flags (from Citadel)
 */
#define US_NEEDVALID	1		/* User needs to be validated       */
#define US_PERM		4		/* Permanent user                   */
#define US_LASTOLD	16		/* Print last old message with new  */
#define US_EXPERT	32		/* Experienced user		    */
#define US_UNLISTED	64		/* Unlisted userlog entry           */
#define US_NOPROMPT	128		/* Don't prompt after each message  */
#define US_PROMPTCTL	256		/* <N>ext & <S>top work at prompt   */
#define US_DISAPPEAR	512		/* Use "disappearing msg prompts"   */
#define US_REGIS	1024		/* Registered user                  */
#define US_PAGINATOR	2048		/* Pause after each screen of text  */
#define US_INTERNET	4096		/* Internet mail privileges         */
#define US_FLOORS	8192		/* User wants to see floors         */
#define US_COLOR	16384		/* User wants ANSI color support    */
#define US_USER_SET	(US_LASTOLD | US_EXPERT | US_UNLISTED | \
			US_NOPROMPT | US_DISAPPEAR | US_PAGINATOR | \
			US_FLOORS | US_COLOR | US_PROMPTCTL )




struct httprequest {
	struct httprequest *next;
	char line[SIZ];
};

struct urlcontent {
	struct urlcontent *next;
	char url_key[32];
	char *url_data;
};

struct serv_info {
	int serv_pid;
	char serv_nodename[32];
	char serv_humannode[64];
	char serv_fqdn[64];
	char serv_software[64];
	int serv_rev_level;
	char serv_bbs_city[64];
	char serv_sysadm[64];
	char serv_moreprompt[SIZ];
	int serv_ok_floors;
	int serv_supports_ldap;
};



/*
 * This struct holds a list of rooms for <G>oto operations.
 */
struct march {
	struct march *next;
	char march_name[128];
	int march_floor;
	int march_order;
};

/* 
 * This struct holds a list of rooms for client display.
 * (oooh, a tree!)
 */
struct roomlisting {
	struct roomlisting *lnext;
	struct roomlisting *rnext;
	char rlname[128];
	unsigned rlflags;
	int rlfloor;
	int rlorder;
};



/*
 * Dynamic content for variable substitution in templates
 */
struct wcsubst {
	struct wcsubst *next;
	int wcs_type;
	char wcs_key[32];
	void *wcs_value;
	void (*wcs_function)(void);
};

/*
 * Values for wcs_type
 */
enum {
	WCS_STRING,
	WCS_FUNCTION,
	WCS_SERVCMD
};


struct wc_attachment {
	struct wc_attachment *next;
	size_t length;
	char content_type[SIZ];
	char filename[SIZ];
	char *data;
};

struct message_summary {
	time_t date;
	long msgnum;
	char from[128];
	char to[128];
	char subj[128];
	int hasattachments;
	int is_new;
};

/*
 * One of these is kept for each active Citadel session.
 * HTTP transactions are bound to one at a time.
 */
struct wcsession {
	struct wcsession *next;		/* Linked list */
	int wc_session;			/* WebCit session ID */
	char wc_username[SIZ];
	char wc_password[SIZ];
	char wc_roomname[SIZ];
	int connected;
	int logged_in;
	int axlevel;
	int is_aide;
	int is_room_aide;
	int http_sock;
	int serv_sock;
	int chat_sock;
	unsigned room_flags;
	int wc_view;
	int wc_default_view;
	int wc_floor;
	char ugname[128];
	long uglsn;
	int upload_length;
	char *upload;
	char upload_filename[SIZ];
	char upload_content_type[SIZ];
	int new_mail;
	int remember_new_mail;
	int need_regi;			/* This user needs to register. */
	int need_vali;			/* New users require validation. */
	char cs_inet_email[SIZ];	/* User's preferred Internet addr. */
	pthread_mutex_t SessionMutex;	/* mutex for exclusive access */
	time_t lastreq;			/* Timestamp of most recent HTTP */
	int killthis;			/* Nonzero == purge this session */
	struct march *march;		/* march mode room list */
	char reply_to[SIZ];		/* reply-to address */

	long msgarr[10000];		/* for read operations */
	int num_summ;
	struct message_summary *summ;

	int is_wap;			/* Client is a WAP gateway */
	struct urlcontent *urlstrings;
	int HaveInstantMessages;	/* Nonzero if incoming msgs exist */
	struct wcsubst *vars;
	char this_page[SIZ];		/* address of current page */
	char http_host[SIZ];		/* HTTP Host: header */
	char *preferences;
#ifdef WEBCIT_WITH_CALENDAR_SERVICE
	struct disp_cal {
		icalcomponent *cal;		/* cal items for display */
		long cal_msgnum;		/* cal msgids for display */
	} *disp_cal;
	int num_cal;
#endif
	struct wc_attachment *first_attachment;
	char ImportantMessage[SIZ];
	char last_chat_user[SIZ];
	int ctdl_pid;			/* Session ID on the Citadel server */
	char httpauth_user[SIZ];	/* only for GroupDAV sessions */
	char httpauth_pass[SIZ];	/* only for GroupDAV sessions */

	size_t burst_len;
	char *burst;
	int gzip_ok;			/* Nonzero if Accept-encoding: gzip */
	int is_mailbox;			/* the current room is a private mailbox */
};

#define num_parms(source)		num_tokens(source, '|')

/* Per-session data */
#define WC ((struct wcsession *)pthread_getspecific(MyConKey))
extern pthread_key_t MyConKey;

/* Per-thread SSL context */
#ifdef HAVE_OPENSSL
#define THREADSSL ((SSL *)pthread_getspecific(ThreadSSL))
extern pthread_key_t ThreadSSL;
#endif

struct serv_info serv_info;
extern char floorlist[128][SIZ];
extern char *axdefs[];
extern char *ctdlhost, *ctdlport;
extern char *server_cookie;
extern int is_https;
extern int setup_wizard;
extern char wizard_filename[];
extern time_t if_modified_since;
void do_setup_wizard(void);

void stuff_to_cookie(char *cookie, int session,
			char *user, char *pass, char *room);
void cookie_to_stuff(char *cookie, int *session,
                char *user, size_t user_len,
                char *pass, size_t pass_len,
                char *room, size_t room_len);
void locate_host(char *, int);
void become_logged_in(char *, char *, char *);
void do_login(void);
void display_login(char *mesg);
void do_welcome(void);
void do_logout(void);
void display_main_menu(void);
void display_aide_menu(void);
void display_advanced_menu(void);
void slrp_highest(void);
void gotonext(void);
void ungoto(void);
void get_serv_info(char *, char *);
int uds_connectsock(char *);
int tcp_connectsock(char *, char *);
void serv_getln(char *strbuf, int bufsize);
void serv_puts(char *string);
void who(void);
void who_inner_div(void);
void fmout(char *align);
void pullquote_fmout(void);
void wDumpContent(int);
void serv_printf(const char *format,...);
char *bstr(char *key);
void urlesc(char *, char *);
void urlescputs(char *);
void jsesc(char *, char *);
void jsescputs(char *);
void output_headers(    int do_httpheaders,
			int do_htmlhead,
			int do_room_banner,
			int unset_cookies,
			int suppress_check,
			int cache);
void wprintf(const char *format,...);
void output_static(char *what);
void stresc(char *target, char *strbuf, int nbsp, int nolinebreaks);
void escputs(char *strbuf);
void url(char *buf);
void escputs1(char *strbuf, int nbsp, int nolinebreaks);
void msgesc(char *target, char *strbuf);
void msgescputs(char *strbuf);
int extract_int(const char *source, int parmnum);
long extract_long(const char *source, int parmnum);
void stripout(char *str, char leftboundary, char rightboundary);
void dump_vars(void);
void embed_main_menu(void);
void serv_read(char *buf, int bytes);
int haschar(char *, char);
void readloop(char *oper);
void embed_message(char *msgnum_as_string);
void print_message(char *msgnum_as_string);
void text_to_server(char *ptr, int convert_to_html);
void display_enter(void);
void post_message(void);
void confirm_delete_msg(void);
void delete_msg(void);
void confirm_move_msg(void);
void move_msg(void);
void userlist(void);
void showuser(void);
void display_page(void);
void page_user(void);
void do_chat(void);
void display_private(char *rname, int req_pass);
void goto_private(void);
void zapped_list(void);
void display_zap(void);
void zap(void);
void display_success(char *);
void authorization_required(const char *message);
void display_entroom(void);
void entroom(void);
void display_editroom(void);
void netedit(void);
void editroom(void);
void display_whok(void);
void do_invt_kick(void);
void server_to_text(void);
void save_edit(char *description, char *enter_cmd, int regoto);
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner);
int gotoroom(char *gname);
void confirm_delete_room(void);
void delete_room(void);
void validate(void);
void display_graphics_upload(char *, char *, char *);
void do_graphics_upload(char *upl_cmd);
void serv_read(char *buf, int bytes);
void serv_gets(char *strbuf);
void serv_write(char *buf, int nbytes);
void serv_puts(char *string);
void serv_printf(const char *format,...);
void load_floorlist(void);
void display_reg(int);
void display_changepw(void);
void changepw(void);
void display_edit_node(void);
void edit_node(void);
void display_netconf(void);
void display_confirm_delete_node(void);
void delete_node(void);
void display_add_node(void);
void add_node(void);
void terminate_session(void);
void edit_me(void);
void display_siteconfig(void);
void siteconfig(void);
void display_generic(void);
void do_generic(void);
void ajax_servcmd(void);
void display_menubar(int);
void smart_goto(char *);
void worker_entry(void);
void session_loop(struct httprequest *);
void fmt_date(char *buf, time_t thetime, int brief);
void fmt_time(char *buf, time_t thetime);
void httpdate(char *buf, time_t thetime);
time_t httpdate_to_timestamp(const char *buf);
void end_webcit_session(void);
void page_popup(void);
void chat_recv(void);
void chat_send(void);
void http_redirect(char *);
void clear_local_substs(void);
void svprintf(char *keyname, int keytype, const char *format,...);
void svcallback(char *keyname, void (*fcn_ptr)() );
void do_template(void *templatename);
int lingering_close(int fd);
char *memreadline(char *start, char *buf, int maxlen);
int num_tokens (char *source, char tok);
void extract_token(char *dest, const char *source, int parmnum, char separator, int maxlen);
void remove_token(char *source, int parmnum, char separator);
char *load_mimepart(long msgnum, char *partnum);
int pattern2(char *search, char *patn);
void do_edit_vcard(long, char *, char *);
void edit_vcard(void);
void submit_vcard(void);
void striplt(char *);
void select_user_to_edit(char *message, char *preselect);
void delete_user(char *);
void display_edituser(char *who, int is_new);
void create_user(void);
void edituser(void);
void do_change_view(int);
void change_view(void);
void folders(void);
void do_stuff_to_msgs(void);
void load_preferences(void);
void save_preferences(void);
void get_preference(char *key, char *value, size_t value_len);
void set_preference(char *key, char *value, int save_to_server);
void knrooms(void);
int is_msg_in_mset(char *mset, long msgnum);
char *safestrncpy(char *dest, const char *src, size_t n);
void display_addressbook(long msgnum, char alpha);
void offer_start_page(void);
void convenience_page(char *titlebarcolor, char *titlebarmsg, char *messagetext);
void change_start_page(void);
void output_html(char *);
void display_floorconfig(char *);
void delete_floor(void);
void create_floor(void);
void rename_floor(void);
void do_listsub(void);
void toggle_self_service(void);
void summary(void);
void summary_inner_div(void);
ssize_t write(int fd, const void *buf, size_t count);
void cal_process_attachment(char *part_source, long msgnum, char *cal_partnum);
void display_calendar(long msgnum);
void display_task(long msgnum);
void display_note(long msgnum);
void do_calendar_view(void);
void do_tasks_view(void);
void free_calendar_buffer(void);
void calendar_summary_view(void);
int load_msg_ptrs(char *servcmd, int with_headers);
void CtdlEncodeBase64(char *dest, const char *source, size_t sourcelen);
int CtdlDecodeBase64(char *dest, const char *source, size_t length);
void free_attachments(struct wcsession *sess);
void set_room_policy(void);
void display_inetconf(void);
void save_inetconf(void);
void generate_uuid(char *);
void display_preferences(void);
void set_preferences(void);
void recp_autocomplete(char *);
void begin_ajax_response(void);
void end_ajax_response(void);

#ifdef WEBCIT_WITH_CALENDAR_SERVICE
void display_edit_task(void);
void save_task(void);
void display_edit_event(void);
void save_event(void);
void display_icaltimetype_as_webform(struct icaltimetype *, char *);
void icaltime_from_webform(struct icaltimetype *result, char *prefix);
void icaltime_from_webform_dateonly(struct icaltimetype *result, char *prefix);
void display_edit_individual_event(icalcomponent *supplied_vtodo, long msgnum);
void save_individual_event(icalcomponent *supplied_vtodo, long msgnum);
void respond_to_request(void);
void handle_rsvp(void);
void ical_dezonify(icalcomponent *cal);
void partstat_as_string(char *buf, icalproperty *attendee);
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp);
void check_attendee_availability(icalcomponent *supplied_vevent);
void do_freebusy(char *req);
#endif

extern char *months[];
extern char *days[];
void read_server_binary(char *buffer, size_t total_len);
char *read_server_text(void);
int goto_config_room(void);
long locate_user_vcard(char *username, long usernum);
void sleeeeeeeeeep(int);
void http_transmit_thing(char *thing, size_t length, char *content_type,
			 int is_static);
void unescape_input(char *buf);
void do_iconbar(void);
void display_customize_iconbar(void);
void commit_iconbar(void);
int CtdlDecodeQuotedPrintable(char *decoded, char *encoded, int sourcelen);
void spawn_another_worker_thread(void);
void display_rss(char *roomname, char *request_method);

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
int client_read_ssl(char *buf, int bytes, int timeout);
void client_write_ssl(char *buf, int nbytes);
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
int ZEXPORT compress_gzip(Bytef * dest, uLongf * destLen,
                          const Bytef * source, uLong sourceLen, int level);
#endif

#ifdef HAVE_ICONV
void utf8ify_rfc822_string(char *buf);
#endif

void begin_burst(void);
void end_burst(void);

extern char *ascmonths[];
extern char *hourname[];
void http_datestring(char *buf, size_t n, time_t xtime);

/* Views (from citadel.h) */
#define	VIEW_BBS		0	/* Traditional Citadel BBS view */
#define VIEW_MAILBOX		1	/* Mailbox summary */
#define VIEW_ADDRESSBOOK	2	/* Address book view */
#define VIEW_CALENDAR		3	/* Calendar view */
#define VIEW_TASKS		4	/* Tasks view */
#define VIEW_NOTES		5	/* Notes view */


/* These should be empty, but we have them for testing */
#define DEFAULT_HTTPAUTH_USER	""
#define DEFAULT_HTTPAUTH_PASS	""

