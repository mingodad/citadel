/*
 * $Id$
 *
 * Main Citadel header file
 * See copyright.txt for copyright information.
 */

/* system customizations are in sysconfig.h */

#ifndef CITADEL_H
#define CITADEL_H
/* #include <dmalloc.h> uncomment if using dmalloc */

/* Build Citadel with the calendar service only if the header *and*
 * library for libical are both present.
 */
#ifdef HAVE_LIBICAL
#ifdef HAVE_ICAL_H
#define CITADEL_WITH_CALENDAR_SERVICE 1
#endif
#endif

#include "sysdep.h"
#include <limits.h>
#include "sysconfig.h"
#include "typesize.h"
#include "ipcdef.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Text description of this software
 */
#define CITADEL	"Citadel 6.84"

/*
 * REV_LEVEL is the current version number (multiplied by 100 to avoid having
 * to fiddle with the decimal).  REV_MIN is the oldest version of Citadel
 * whose data files are compatible with the current version.  If the data files
 * are older than REV_MIN, none of the programs will work until the setup
 * program is run again to bring things up to date.  EXPORT_REV_MIN is the
 * oldest version of Citadel whose export files we can read.  The latter is
 * usually more strict because you're not really supposed to dump/load and
 * upgrade at the same time.
 */
#define REV_LEVEL	684		/* This version */
#define REV_MIN		591		/* Oldest compatible database */
#define EXPORT_REV_MIN	684		/* Oldest compatible export files */

#define SERVER_TYPE 0	/* zero for stock Citadel; other developers please
			   obtain SERVER_TYPE codes for your implementations */

/* Various length constants */

#define UGLISTLEN	100	/* you get a ungoto list of this size */
#define ROOMNAMELEN	128	/* The size of a roomname string */
#define NONCE_SIZE	128	/* Added by <bc> to allow for APOP auth 
				 * it is BIG becuase there is a hostname
				 * in the nonce, as per the APOP RFC.
				 */
					 
#define USERNAME_SIZE	64	/* The size of a username string */
#define MAX_EDITORS	5	/* # of external editors supported */
				/* MUST be at least 1 */

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
 * This struct stores a list of rooms with new messages which the client
 * fetches from the server.  This allows the client to "march" through
 * relevant rooms without having to ask the server each time where to go next.
 */
struct march {
	struct march *next;
	char march_name[ROOMNAMELEN];
	unsigned int march_flags;
	char march_floor;
	char march_order;
	unsigned int march_flags2;
	int march_access;
};

#define NODENAME		config.c_nodename
#define FQDN			config.c_fqdn
#define HUMANNODE		config.c_humannode
#define PHONENUM		config.c_phonenum
#define CTDLUID			config.c_ctdluid
#define CREATAIDE		config.c_creataide
#define REGISCALL		config.c_regiscall
#define TWITDETECT		config.c_twitdetect
#define TWITROOM		config.c_twitroom
#define RESTRICT_INTERNET	config.c_restrict

/*
 * User records.
 */
struct ctdluser {			/* User record                      */
	int version;			/* Cit vers. which created this rec */
	uid_t uid;			/* Associate with a unix account?   */
	char password[32];		/* password (for Citadel-only users)*/
	unsigned flags;			/* See US_ flags below              */
	long timescalled;		/* Total number of logins           */
	long posted;			/* Number of messages posted (ever) */
	cit_uint8_t axlevel;		/* Access level                     */
	long usernum;			/* User number (never recycled)     */
	time_t lastcall;		/* Last time the user called        */
	int USuserpurge;		/* Purge time (in days) for user    */
	char fullname[64];		/* Name for Citadel messages & mail */
	cit_uint8_t USscreenwidth;	/* Screen width (for textmode users)*/
	cit_uint8_t USscreenheight;	/* Screen height(for textmode users)*/
};


/* Bits which may appear in CitControl.MMflags.  Note that these don't
 * necessarily pertain to the message base -- it's just a good place to
 * store any global flags.
 */
#define MM_VALID	4		/* New users need validating        */

/*
 * Room records.
 */
struct ctdlroom {
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
 * Miscellaneous
 */
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

/* commands we can send to the stty_ctdl() routine */
#define SB_NO_INTR	0		/* set to Citadel client mode, i/q disabled */
#define SB_YES_INTR	1		/* set to Citadel client mode, i/q enabled */
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

#define TRACE	lprintf(CTDL_DEBUG, "Checkpoint: %s, %d\n", __FILE__, __LINE__)

#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif


/*
 * Views
 */
#define	VIEW_BBS		0	/* Bulletin board view */
#define VIEW_MAILBOX		1	/* Mailbox summary */
#define VIEW_ADDRESSBOOK	2	/* Address book view */
#define VIEW_CALENDAR		3	/* Calendar view */
#define VIEW_TASKS		4	/* Tasks view */
#define VIEW_NOTES		5	/* Notes view */
#define	VIEW_WIKI		6	/* Wiki view */
#define VIEW_CALBRIEF		7	/* Brief Calendar view */


#ifdef __cplusplus
}
#endif

#endif /* CITADEL_H */
