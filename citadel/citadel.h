/*
 * $Id$
 *
 * main Citadel/UX header file
 * see copyright.txt for copyright information
 */

/* system customizations are in sysconfig.h */

#ifndef CITADEL_H
#define CITADEL_H

#include "sysdep.h"
#include <limits.h>
#include "sysconfig.h"
#include "ipcdef.h"

/*
 * Text description of this software
 */
#define CITADEL	"Citadel/UX 5.91"

/*
 * REV_LEVEL is the current version number (multiplied by 100 to avoid having
 * to fiddle with the decimal).  REV_MIN is the oldest version of Citadel
 * whose data files are compatible with the current version.  If the data files
 * are older than REV_MIN, none of the programs will work until the setup
 * program is run again to bring things up to date.
 */
#define REV_LEVEL	591		/* This version */
#define REV_MIN		591		/* Oldest compatible version */

#define SERVER_TYPE 0	/* zero for stock Citadel/UX; other developers please
			   obtain SERVER_TYPE codes for your implementations */

/*
 * This is a better implementation of tolower() than that found on some
 * systems (there are operating systems out there on which tolower() will
 * screw up if you give it a character that is already lower case).
 */
#ifdef  tolower
#undef	tolower
#endif
#define tolower(x)	( ((x>='A')&&(x<='Z')) ? (x+'a'-'A') : x )
#define NEW_CONFIG

/* 
 * The only typedef we do is an 8-bit unsigned, for screen dimensions.
 * All other defs are done using standard C types.  The code assumes that
 * 'int' 'unsigned' and 'short' are at least 16 bits, and that 'long' is at
 * least 32 bits.  There are no endian dependencies in any of the Citadel
 * programs.
 */
typedef unsigned char CIT_UBYTE;

/* Various length constants */

#define UGLISTLEN   100   /* you get a ungoto list of this size */
#define ROOMNAMELEN	128		/* The size of a roomname string */
#define NONCE_SIZE	128		/* Added by <bc> to allow for APOP auth 
					 * it is BIG becuase there is a hostname
					 * in the nonce, as per the APOP RFC.
					 */
					 
#define USERNAME_SIZE	32		/* The size of a username string */

/*
 * Message expiration policy stuff
 */
struct ExpirePolicy {
	int expire_mode;
	int expire_value;
};

#define EXPIRE_NEXTLEVEL	0	/* Inherit expiration policy    */
#define EXPIRE_MANUAL		1	/* Don't expire messages at all */
#define EXPIRE_NUMMSGS		2	/* Keep only latest n messages  */
#define EXPIRE_AGE		3	/* Expire messages after n days */


/* 
 * Global system configuration 
 */
struct config {
	char c_nodename[16];		/* Unqualified "short" nodename     */
	char c_fqdn[64];		/* Fully Qualified Domain Name      */
	char c_humannode[21];		/* Long name of system              */
	char c_phonenum[16];		/* Dialup number of system          */
	uid_t c_bbsuid;			/* UID of the bbs-only user         */
	char c_creataide;		/* room creator = room aide  flag   */
	int c_sleeping;			/* watchdog timer setting           */
	char c_initax;			/* initial access level             */
	char c_regiscall;		/* call number to register on       */
	char c_twitdetect;		/* twit detect flag                 */
	char c_twitroom[ROOMNAMELEN];	/* twit detect msg move to room     */
	char c_moreprompt[80];		/* paginator prompt                 */
	char c_restrict;		/* restrict Internet mail flag      */
	long c_msgbase;			/* size of message base (obsolete)  */
	char c_bbs_city[32];		/* physical location of server      */
	char c_sysadm[26];		/* name of system administrator     */
	char c_bucket_dir[15];		/* bit bucket for files...	    */
	int c_setup_level;		/* what rev level we've setup to    */
	int c_maxsessions;		/* maximum concurrent sessions      */
	char c_net_password[20];	/* system net password (obsolete)   */
	int c_port_number;		/* Cit listener port (usually 504)  */
	int c_ipgm_secret;		/* Internal program authentication  */
	struct ExpirePolicy c_ep;	/* System default msg expire policy */
	int c_userpurge;		/* System default user purge (days) */
	int c_roompurge;		/* System default room purge (days) */
	char c_logpages[ROOMNAMELEN];	/* Room to log pages to (or not)    */
	char c_createax;		/* Axlevel required to create rooms */
	long c_maxmsglen;		/* Maximum message length           */
	int c_min_workers;		/* Lower limit on number of threads */
	int c_max_workers;		/* Upper limit on number of threads */
	int c_pop3_port;		/* POP3 listener port (usually 110) */
	int c_smtp_port;		/* SMTP listener port (usually 25)  */
	int c_unused_1;			/* Nothin' here anymore...          */
	int c_aide_zap;			/* Are Aides allowed to zap rooms?  */
	int c_imap_port;		/* IMAP listener port (usually 143) */
	time_t c_net_freq;		/* how often to run the networker   */
	char c_disable_newu;		/* disable NEWU command             */
	char c_aide_mailboxes;		/* give Aides access to mailboxes   */
	char c_baseroom[ROOMNAMELEN];	/* Name of baseroom (Lobby)	    */
	char c_aideroom[ROOMNAMELEN];	/* Name of aideroom (Aide)	    */
};

struct march {
	struct march *next;
	char march_name[ROOMNAMELEN];
	char march_floor;
	char march_order;
};

#define NODENAME		config.c_nodename
#define FQDN			config.c_fqdn
#define HUMANNODE		config.c_humannode
#define PHONENUM		config.c_phonenum
#define BBSUID			config.c_bbsuid
#define CREATAIDE		config.c_creataide
#define REGISCALL		config.c_regiscall
#define TWITDETECT		config.c_twitdetect
#define TWITROOM		config.c_twitroom
#define RESTRICT_INTERNET	config.c_restrict

/* Defines the actual user record */
 
struct usersupp {			/* User record                      */
	int version;			/* Cit vers. which created this rec */
	uid_t uid;			/* Associate with a unix account?   */
	char password[32];		/* password (for BBS-only users)    */
	unsigned flags;			/* See US_ flags below              */
	long timescalled;		/* Total number of logins           */
	long posted;			/* Number of messages posted (ever) */
	CIT_UBYTE axlevel;		/* Access level                     */
	long usernum;			/* User number (never recycled)     */
	time_t lastcall;		/* Last time the user called        */
	int USuserpurge;		/* Purge time (in days) for user    */
	char fullname[64];		/* Name for Citadel messages & mail */
	CIT_UBYTE USscreenwidth;	/* Screen width (for textmode users)*/
	CIT_UBYTE USscreenheight;	/* Screen height(for textmode users)*/
};


/*
 * This is the control record for the message base... 
 */
struct CitControl {
	long MMhighest;			/* highest message number in file   */
	unsigned MMflags;		/* Global system flags              */
	long MMnextuser;		/* highest user number on system    */
	long MMnextroom;		/* highest room number on system    */
	int version;			/* Server-hosted upgrade level      */
};

/* Bits which may appear in CitControl.MMflags.  Note that these don't
 * necessarily pertain to the message base -- it's just a good place to
 * store any global flags.
 */
#define MM_VALID	4		/* New users need validating        */

/*
 * Room records
 */
struct quickroom {
	char QRname[ROOMNAMELEN];	/* Name of room                     */
	char QRpasswd[10];		/* Only valid if it's a private rm  */
	long QRroomaide;		/* User number of room aide         */
	long QRhighest;			/* Highest message NUMBER in room   */
	time_t QRgen;			/* Generation number of room        */
	unsigned QRflags;		/* See flag values below            */
	char QRdirname[15];		/* Directory name, if applicable    */
	long QRinfo;			/* Info file update relative to msgs*/
	char QRfloor;			/* Which floor this room is on      */
	time_t QRmtime;			/* Date/time of last post           */
	struct ExpirePolicy QRep;	/* Message expiration policy        */
	long QRnumber;			/* Globally unique room number      */
	char QRorder;			/* Sort key for room listing order  */
	unsigned QRflags2;		/* Additional flags                 */
	int QRdefaultview;		/* How to display the contents      */
};

/* Private rooms are always flagged with QR_PRIVATE.  If neither QR_PASSWORDED
 * or QR_GUESSNAME is set, then it is invitation-only.  Passworded rooms are
 * flagged with both QR_PRIVATE and QR_PASSWORDED while guess-name rooms are
 * flagged with both QR_PRIVATE and QR_GUESSNAME.  NEVER set all three flags.
 */

/*
 * Events which might show up in the Citadel Log
 */
#define CL_CONNECT	8		/* Connect to server                */
#define CL_LOGIN	16		/* CLfullname logged in		    */
#define CL_NEWUSER	32		/* CLfullname is a new user	    */
#define CL_BADPW	64		/* Bad attempt at CLfullname's pw   */
#define CL_TERMINATE	128		/* Logout - proper termination	    */
#define CL_DROPCARR	256		/* Logout - dropped carrier	    */
#define CL_SLEEPING	512		/* Logout - sleeping		    */
#define CL_PWCHANGE	1024		/* CLfullname changed passwords     */

/* Miscellaneous                                                            */

#define MES_NORMAL	65		/* Normal message                   */
#define MES_ANONONLY	66		/* "****" header                    */
#define MES_ANONOPT	67		/* "Anonymous" header               */

#define MES_ERROR	(-1)	/* Can't send message due to bad address   */
#define MES_LOCAL	0	/* Local message, do no network processing */
#define MES_INTERNET	1	/* Convert msg and send as Internet mail   */
#define MES_IGNET	2	/* Process recipient and send via Cit net  */

/****************************************************************************/

/*
 * Floor record.  The floor number is implicit in its location in the file.
 */
struct floor {
	unsigned short f_flags;		/* flags */
	char f_name[256];		/* name of floor */
	int f_ref_count;		/* reference count */
	struct ExpirePolicy f_ep;	/* default expiration policy */
	};

#define F_INUSE		1		/* floor is in use */


/*
 * Values used internally for function call returns, etc.
 */

#define NEWREGISTER	0		/* new user to register */
#define REREGISTER	1		/* existing user reregistering */

#define READ_HEADER	2
#define READ_MSGBODY	3

/* commands we can send to the sttybbs() routine */
#define SB_NO_INTR	0		/* set to bbs mode, i/q disabled */
#define SB_YES_INTR	1		/* set to bbs mode, i/q enabled */
#define SB_SAVE		2		/* save settings */
#define SB_RESTORE	3		/* restore settings */
#define SB_LAST		4		/* redo the last command sent */

#define	NEXT_KEY	15
#define STOP_KEY	3

/* server exit codes */
#define EXIT_NORMAL	0		/* server terminated normally */
					/* 1 through 63 reserved for signals */
#define EXIT_NULL	64		/* EOF on server command input */

/* citadel.rc stuff */
#define RC_NO		0		/* always no */
#define RC_YES		1		/* always yes */
#define RC_DEFAULT	2		/* setting depends on user config */

/* keepalives */
enum {
	KA_NO,				/* no keepalives */
	KA_YES,				/* full keepalives */
	KA_HALF				/* half keepalives */
};

/* for <;G>oto and <;S>kip commands */
#define GF_GOTO		0		/* <;G>oto floor mode */
#define GF_SKIP		1		/* <;S>kip floor mode */
#define GF_ZAP		2		/* <;Z>ap floor mode */

/*
 * MIME types used in Citadel for configuration stuff
 */
#define SPOOLMIME	"application/x-citadel-delivery-list"
#define	INTERNETCFG	"application/x-citadel-internet-config"
#define IGNETCFG	"application/x-citadel-ignet-config"
#define IGNETMAP	"application/x-citadel-ignet-map"
#define FILTERLIST	"application/x-citadel-filter-list"
#define SPAMSTRINGS	"application/x-citadel-spam-strings"

#define TRACE	lprintf(9, "Checkpoint: %s, %d\n", __FILE__, __LINE__)

#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif

#endif /* CITADEL_H */


/*
 * Views
 */
#define	VIEW_BBS		0	/* Traditional Citadel BBS view */
#define VIEW_MAILBOX		1	/* Mailbox summary */
#define VIEW_ADDRESSBOOK	2	/* Address book view */
