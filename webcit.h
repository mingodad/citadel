/* $Id$ */

#define SIZ			4096		/* generic buffer size */

#define TRACE fprintf(stderr, "Checkpoint: %s, %d\n", __FILE__, __LINE__)

#define SLEEPING		180		/* TCP connection timeout */
#define WEBCIT_TIMEOUT		900		/* WebCit session timeout */
#define PORT_NUM		2000		/* port number to listen on */
#define SERVER			"WebCit v3.23"	/* who's in da house */
#define DEVELOPER_ID		0
#define CLIENT_ID		4
#define CLIENT_VERSION		323
#define DEFAULT_HOST		"localhost"	/* Default Citadel server */
#define DEFAULT_PORT		"504"
#define LB			(1)		/* Internal escape chars */
#define RB			(2)
#define QU			(3)
#define TARGET			"webcit01"	/* Target for inline URL's */
#define HOUSEKEEPING		15		/* Housekeeping frequency */
#define INITIAL_WORKER_THREADS	5
#define LISTEN_QUEUE_LENGTH	100		/* listen() backlog queue */

#define USERCONFIGROOM		"My Citadel Config"


/*
 * Room flags (from Citadel)
 *
 * bucket one...
 */
#define QR_PERMANENT	1		/* Room does not purge              */
#define QR_INUSE	2		/* Set if in use, clear if avail    */
#define QR_PRIVATE	4		/* Set for any type of private room */
#define QR_PASSWORDED	8		/* Set if there's a password too    */
#define QR_GUESSNAME	16		/* Set if it's a guessname room     */
#define QR_DIRECTORY	32		/* Directory room                   */
#define QR_UPLOAD	64		/* Allowed to upload                */
#define QR_DOWNLOAD	128		/* Allowed to download              */
#define QR_VISDIR	256		/* Visible directory                */
#define QR_ANONONLY	512		/* Anonymous-Only room              */
#define QR_ANONOPT	1024		/* Anonymous-Option room            */
#define QR_NETWORK	2048		/* Shared network room              */
#define QR_PREFONLY	4096		/* Preferred status needed to enter */
#define QR_READONLY	8192		/* Aide status required to post     */
#define QR_MAILBOX	16384		/* Set if this is a private mailbox */

/*
 * bucket two...
 */
#define QR2_SYSTEM	1		/* System room; hide by default     */
#define QR2_SELFLIST	2		/* Self-service mailing list mgmt   */


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
};



/*
 * This struct holds a list of rooms for <G>oto operations.
 */
struct march {
	struct march *next;
	char march_name[32];
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
	char rlname[64];
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
	unsigned room_flags;
	int wc_view;
	char ugname[128];
	long uglsn;
	int upload_length;
	char *upload;
	int new_mail;
	int remember_new_mail;
	int need_vali;
        pthread_mutex_t SessionMutex;	/* mutex for exclusive access */
        time_t lastreq;			/* Timestamp of most recent HTTP */
	int killthis;			/* Nonzero == purge this session */
	struct march *march;		/* march mode room list */
	char reply_to[SIZ];		/* reply-to address */
	long msgarr[1024];		/* for read operations */
	int fake_frames;
	int is_wap;			/* Client is a WAP gateway */
	struct urlcontent *urlstrings;
	int HaveExpressMessages;	/* Nonzero if incoming msgs exist */
	struct wcsubst *vars;
	char this_page[SIZ];		/* address of current page */
	char http_host[SIZ];		/* HTTP Host: header */
	char *preferences;
};

#define extract(dest,source,parmnum)	extract_token(dest,source,parmnum,'|')
#define num_parms(source)		num_tokens(source, '|')

#define WC ((struct wcsession *)pthread_getspecific(MyConKey))
extern pthread_key_t MyConKey;

struct serv_info serv_info;
extern char floorlist[128][SIZ];
extern char *axdefs[];
extern char *defaulthost, *defaultport;
extern char *server_cookie;

extern struct wcsubst *global_subst;


void stuff_to_cookie(char *cookie, int session,
			char *user, char *pass, char *room);
void cookie_to_stuff(char *cookie, int *session,
			char *user, char *pass, char *room);
void locate_host(char *, int);
void become_logged_in(char *, char *, char *);
void do_login(void);
void display_login(char *mesg);
void do_welcome(void);
void do_logout(void);
void display_main_menu(void);
void display_advanced_menu(void);
void list_all_rooms_by_floor(void);
void slrp_highest(void);
void gotonext(void);
void ungoto(void);
void get_serv_info(char *, char *);
int uds_connectsock(char *);
int tcp_connectsock(char *, char *);
void serv_gets(char *strbuf);
void serv_puts(char *string);
void whobbs(void);
void fmout(FILE * fp);
void wDumpContent(int);
void serv_printf(const char *format,...);
char *bstr(char *key);
void urlesc(char *, char *);
void urlescputs(char *);
void output_headers(int);
void wprintf(const char *format,...);
void output_static(char *what);
void stresc(char *target, char *strbuf, int nbsp);
void escputs(char *strbuf);
void url(char *buf);
void escputs1(char *strbuf, int nbsp);
long extract_long(char *source, long int parmnum);
int extract_int(char *source, int parmnum);
void dump_vars(void);
void embed_main_menu(void);
void serv_read(char *buf, int bytes);
int haschar(char *, char);
void readloop(char *oper);
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
void display_error(char *);
void display_success(char *);
void display_entroom(void);
void entroom(void);
void display_editroom(void);
void netedit(void);
void editroom(void);
void display_whok(void);
void server_to_text(void);
void save_edit(char *description, char *enter_cmd, int regoto);
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd);
void gotoroom(char *gname, int display_name);
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
void register_user(void);
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
void display_menubar(int);
void embed_room_banner(char *);
void smart_goto(char *);
void worker_entry(void);
void session_loop(struct httprequest *);
void fmt_date(char *buf, time_t thetime);
void httpdate(char *buf, time_t thetime);
void end_webcit_session(void);
void page_popup(void);
void http_redirect(char *);
void clear_local_substs(void);
void svprintf(char *keyname, int keytype, const char *format,...);
void svcallback(char *keyname, void (*fcn_ptr)() );
void do_template(void *templatename);
int lingering_close(int fd);
char *memreadline(char *start, char *buf, int maxlen);
int num_tokens (char *source, char tok);
void extract_token(char *dest, char *source, int parmnum, char separator);
void remove_token(char *source, int parmnum, char separator);
int decode_base64(char *dest, char *source, size_t length);
char *load_mimepart(long msgnum, char *partnum);
int pattern2(char *search, char *patn);
void do_edit_vcard(long, char *, char *);
void edit_vcard(void);
void submit_vcard(void);
void striplt(char *);
void select_user_to_edit(char *message, char *preselect);
void display_edituser(char *who);
void create_user(void);
void edituser(void);
void change_view(void);
void folders(void);
void do_stuff_to_msgs(void);
void load_preferences(void);
void save_preferences(void);
void get_preference(char *key, char *value);
void set_preference(char *key, char *value);
void knrooms(void);
int is_msg_in_mset(char *mset, long msgnum);
char *safestrncpy(char *dest, const char *src, size_t n);
void display_addressbook(long msgnum, char alpha);
void offer_start_page(void);
void change_start_page(void);
void output_html(void);
void display_floorconfig(char *);
void delete_floor(void);
void create_floor(void);
void rename_floor(void);
void do_listsub(void);
void toggle_self_service(void);
