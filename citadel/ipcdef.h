#ifdef __cplusplus
extern "C" {
#endif

#define LISTING_FOLLOWS		100
#define CIT_OK			200
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
#define SERVER_SHUTTING_DOWN	31
#define PASSWORD_REQUIRED	40
#define ALREADY_LOGGED_IN	41
#define USERNAME_REQUIRED	42
#define HIGHER_ACCESS_REQUIRED	50
#define MAX_SESSIONS_EXCEEDED	51
#define RESOURCE_BUSY		52
#define RESOURCE_NOT_OPEN	53
#define NOT_HERE		60
#define INVALID_FLOOR_OPERATION	61
#define NO_SUCH_USER		70
#define FILE_NOT_FOUND		71
#define ROOM_NOT_FOUND		72
#define NO_SUCH_SYSTEM		73
#define ALREADY_EXISTS		74
#define MESSAGE_NOT_FOUND	75

#define ASYNC_MSG		900
#define ASYNC_GEXP		02

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

#define QR2_SYSTEM	1		/* System room; hide by default     */
#define QR2_SELFLIST	2		/* Self-service mailing list mgmt   */
#define QR2_COLLABDEL	4		/* Anyone who can post can delete   */
#define QR2_SUBJECTREQ	8		/* Subject strongly recommended */
#define QR2_SMTP_PUBLIC	16		/* Listservice Subscribers may post */
#define QR2_MODERATED	32		/* Listservice aide has to permit posts  */

#define US_NEEDVALID	1		/* User needs to be validated       */
#define US_EXTEDIT	2		/* Always use external editor       */
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
			US_FLOORS | US_COLOR | US_PROMPTCTL | US_EXTEDIT)

#define UA_KNOWN                2	/* Room appears in a 'known rooms' list */
#define UA_GOTOALLOWED          4	/* User may goto this room if specified by exact name */
#define UA_HASNEWMSGS           8	/* Unread messages exist in this room */
#define UA_ZAPPED               16	/* User has forgotten (zapped) this room */
#define UA_POSTALLOWED		32	/* User may post top-level messages here */
#define UA_ADMINALLOWED		64	/* Aide or Room Aide rights exist here */
#define UA_DELETEALLOWED	128	/* User is allowed to delete messages from this room */
#define UA_REPLYALLOWED		256	/* User is allowed to reply to existing messages here */

#ifdef __cplusplus
}
#endif
