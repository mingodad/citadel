/* $Id$ */

#define SLEEPING	180			/* TCP connection timeout */
#define WEBCIT_TIMEOUT	900			/* WebCit session timeout */
#define PORT_NUM	2000			/* port number to listen on */
#define SERVER		"WebCit v2.0 (Velma)"	/* who's in da house */
#define DEVELOPER_ID	0
#define CLIENT_ID	4
#define CLIENT_VERSION	200
#define DEFAULT_HOST	"localhost"
#define DEFAULT_PORT	"citadel"
#define LB		(1)
#define RB		(2)
#define QU		(3)
#define TARGET		"webcit01"
#define HOUSEKEEPING	60			/* Housekeeping frequency */


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



struct webcontent {
	struct webcontent *next;
	char w_data[256];
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
	char serv_moreprompt[256];
	int serv_ok_floors;
	};

extern char wc_username[256];
extern char wc_password[256];
extern char wc_roomname[256];
extern int connected;
extern int logged_in;
extern int axlevel;
extern int is_aide;
extern int is_room_aide;
extern int serv_sock;
extern struct serv_info serv_info;
extern unsigned room_flags;
extern char ugname[128];
extern long uglsn;
extern char *axdefs[];
extern int upload_length;
extern char *upload;
extern char floorlist[128][256];

void stuff_to_cookie(char *, int, char *, char *, char *);
void cookie_to_stuff(char *, int *, char *, char *, char *);
