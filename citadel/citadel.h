/*
 * $Id$
 *
 * main Citadel/UX header file
 * see copyright.txt for copyright information
 */

/* system customizations are in sysconfig.h */
#include "sysdep.h"
#include "sysconfig.h"
#include "ipcdef.h"

#define CITADEL	"Citadel/UX 5.73"	/* Text description of this software */

/*
 * REV_LEVEL is the current version number (multiplied by 100 to avoid having
 * to fiddle with the decimal).  REV_MIN is the oldest version of Citadel
 * whose data files are compatible with the current version.  If the data files
 * are older than REV_MIN, none of the programs will work until the setup
 * program is run again to bring things up to date.
 */
#define REV_LEVEL	573		/* This version */
#define REV_MIN		570		/* Oldest compatible version */

#define SERVER_TYPE 0	/* zero for stock Citadel/UX; other developers please
			   obtain SERVER_TYPE codes for your implementations */

#undef	tolower
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

#define ROOMNAMELEN	128

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
	long c_msgbase;			/* size of message base             */
	char c_bbs_city[32];		/* city and state you are located in*/
	char c_sysadm[26];		/* name of system administrator     */
	char c_bucket_dir[15];		/* bit bucket for files...	    */
	int c_setup_level;		/* what rev level we've setup to    */
	int c_maxsessions;		/* maximum concurrent sessions      */
	char c_net_password[20];	/* system net password              */
	int c_port_number;		/* TCP port to run the server on    */
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
	int c_default_filter;		/* Default moderation filter level  */
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
	char moderation_filter;		/* Moderation filter level          */
	};


/****************************************************************************
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

/****************************************************************************
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
#define MES_ANON	66		/* "****" header                    */
#define MES_AN2		67		/* "Anonymous" header               */

#define MES_ERROR	(-1)	/* Can't send message due to bad address   */
#define MES_LOCAL	0	/* Local message, do no network processing */
#define MES_INTERNET	1	/* Convert msg and send as Internet mail   */
#define MES_BINARY	2	/* Process recipient and send via Cit net  */

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


/****************************************************************************
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
#define KA_NO		0		/* no keepalives */
#define KA_YES		1		/* full keepalives */
#define KA_CHAT		2		/* half keepalives (for chat mode) */

/* for <;G>oto and <;S>kip commands */
#define GF_GOTO		0		/* <;G>oto floor mode */
#define GF_SKIP		1		/* <;S>kip floor mode */
#define GF_ZAP		2		/* <;Z>ap floor mode */

/*
 * MIME types used in Citadel for configuration stuff
 */
#define SPOOLMIME	"application/x-citadel-delivery-list"
#define	INTERNETCFG	"application/x-citadel-internet-config"

#define TRACE	lprintf(9, "Checkpoint: %s, %d\n", __FILE__, __LINE__)
