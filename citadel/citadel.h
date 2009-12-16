/*
 * $Id$
 *
 * Main Citadel header file
 * See COPYING for copyright information.
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* system customizations are in sysconfig.h */

#ifndef CITADEL_H
#define CITADEL_H
/* #include <dmalloc.h> uncomment if using dmalloc */

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
 * (We used to define this ourselves, but why bother when
 * the GNU build tools do it for us?)
 */
#define CITADEL	PACKAGE_STRING

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
#define REV_LEVEL	766		/* This version */
#define REV_MIN		591		/* Oldest compatible database */
#define EXPORT_REV_MIN	760		/* Oldest compatible export files */
#define LIBCITADEL_MIN	766		/* Minimum required version of libcitadel */

#define SERVER_TYPE 0			/* zero for stock Citadel; other developers please
					   obtain SERVER_TYPE codes for your implementations */

#ifdef LIBCITADEL_VERSION_NUMBER
#if LIBCITADEL_VERSION_NUMBER < LIBCITADEL_MIN
#error libcitadel is too old.  Please upgrade it before continuing.
#endif
#endif

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
typedef struct ExpirePolicy ExpirePolicy;
struct ExpirePolicy {
/*
#include "datadefinitions.h"
#include "dtds/expirepolicy-defs.h"
#include "undef_data.h"
*/
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
typedef struct march march;
struct march {
/*
#include "datadefinitions.h"
#include "dtds/march-defs.h"
#include "undef_data.h"
*/
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
typedef struct ctdluser ctdluser;
struct ctdluser {			/* User record                      */
/*
#include "datadefinitions.h"
#include "dtds/user-defs.h"
#include "undef_data.h"
*/
	int version;			/* Cit vers. which created this rec  */
	uid_t uid;			/* Associate with a unix account?    */
	char password[32];		/* password                          */
	unsigned flags;			/* See US_ flags below               */
	long timescalled;		/* Total number of logins            */
	long posted;			/* Number of messages ever submitted */
	cit_uint8_t axlevel;		/* Access level                      */
	long usernum;			/* User number (never recycled)      */
	time_t lastcall;		/* Date/time of most recent login    */
	int USuserpurge;		/* Purge time (in days) for user     */
	char fullname[64];		/* Display name (primary identifier) */
	cit_uint8_t USscreenwidth;	/* Screen width (for textmode users) */
	cit_uint8_t USscreenheight;	/* Screen height(for textmode users) */
};


/* Bits which may appear in CitControl.MMflags.  Note that these don't
 * necessarily pertain to the message base -- it's just a good place to
 * store any global flags.
 */
#define MM_VALID	4		/* New users need validating        */

/*
 * Room records.
 */
typedef struct ctdlroom ctdlroom;
struct ctdlroom {
/*
#include "datadefinitions.h"
#include "dtds/room-defs.h"
#include "undef_data.h"
*/
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
typedef struct floor floor;
struct floor {
/*
#include "datadefinitions.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
*/
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
#define SIEVECONFIG	"application/x-citadel-sieve-config"

#define TRACE	CtdlLogPrintf(CTDL_DEBUG, "Checkpoint: %s, %d\n", __FILE__, __LINE__)

#ifndef LONG_MAX
#define LONG_MAX 2147483647L
#endif

/*
 * Authentication modes
 */
#define AUTHMODE_NATIVE		0	/* Native (self-contained or "black box") */
#define AUTHMODE_HOST		1	/* Authenticate against the host OS user database */
#define AUTHMODE_LDAP		2	/* Authenticate against an LDAP server with RFC 2307 schema */
#define AUTHMODE_LDAP_AD	3	/* Authenticate against non-standard MS Active Directory LDAP */

#ifdef __cplusplus
}
#endif

#endif /* CITADEL_H */
