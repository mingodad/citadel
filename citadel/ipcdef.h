/* $Id$ */
#ifdef OK
#undef OK
#endif

#define LISTING_FOLLOWS		100
#define OK			200
#define MORE_DATA		300
#define SEND_LISTING		400
#define ERROR			500
#define BINARY_FOLLOWS		600
#define SEND_BINARY		700
#define START_CHAT_MODE		800

#define INTERNAL_ERROR		10
#define TOO_BIG			11
#define ILLEGAL_VALUE		12
#define NOT_LOGGED_IN		20
#define CMD_NOT_SUPPORTED	30
#define PASSWORD_REQUIRED	40
#define HIGHER_ACCESS_REQUIRED	50
#define MAX_SESSIONS_EXCEEDED	51
#define NOT_HERE		60
#define INVALID_FLOOR_OPERATION	61
#define NO_SUCH_USER		70
#define FILE_NOT_FOUND		71
#define ROOM_NOT_FOUND		72
#define NO_SUCH_SYSTEM		73
#define ALREADY_EXISTS		74

struct CtdlServInfo {
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

#define US_NEEDVALID	1		/* User needs to be validated       */
#define US_PERM		4		/* Permanent user                   */
#define US_LASTOLD	16		/* Print last old message with new  */
#define US_EXPERT	32		/* Experienced user		    */
#define US_UNLISTED	64		/* Unlisted userlog entry           */
#define US_NOPROMPT	128		/* Don't prompt after each message  */
#define US_DISAPPEAR	512		/* Use "disappearing msg prompts"   */
#define US_REGIS	1024		/* Registered user                  */
#define US_PAGINATOR	2048		/* Pause after each screen of text  */
#define US_INTERNET	4096		/* UUCP/Internet mail privileges    */
#define US_FLOORS	8192		/* User wants to see floors         */
#define US_COLOR	16384		/* User wants ANSI color support    */
#define US_USER_SET	(US_LASTOLD | US_EXPERT | US_UNLISTED | \
			US_NOPROMPT | US_DISAPPEAR | US_PAGINATOR | \
			US_FLOORS | US_COLOR )

void serv_puts(char *buf);
void serv_gets(char *buf);
