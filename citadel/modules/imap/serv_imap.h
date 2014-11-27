#define GLOBAL_UIDVALIDITY_VALUE	1L


void imap_cleanup_function(void);
void imap_greeting(void);
void imap_command_loop(void);
int imap_grabroom(char *returned_roomname, const char *foldername, int zapped_ok);
void imap_free_transmitted_message(void);
int imap_do_expunge(void);
void imap_rescan_msgids(void);

/*
 * FDELIM defines which character we want to use as a folder delimiter
 * in room names.  Originally we used a forward slash, but that caused
 * rooms with names like "Sent/Received Pages" to get delimited, so we
 * changed it to a backslash.  This is completely irrelevant to how Citadel
 * speaks to IMAP clients -- the delimiter used in the IMAP protocol is
 * a vertical bar, which is illegal in Citadel room names anyway.
 */

typedef void (*imap_handler)(int num_parms, ConstStr *Params);

typedef struct _imap_handler_hook {
	imap_handler h;
	int Flags;
} imap_handler_hook;

typedef struct __citimap_command {
	StrBuf *CmdBuf;			/* our current commandline; gets chopped into: */
	ConstStr *Params;		/* Commandline tokens */
	int num_parms;			/* Number of Commandline tokens available */
	int avail_parms;		/* Number of ConstStr args is big */
	const imap_handler_hook *hh;
} citimap_command;


typedef struct __citimap {
	StrBuf *Reply;
	int authstate;
	char authseq[SIZ];
	int selected;			/* set to 1 if in the SELECTED state */
	int readonly;			/* mailbox is open read only */
	int num_msgs;			/* Number of messages being mapped */
	int num_alloc;			/* Number of messages for which we've allocated space */
	time_t last_mtime;		/* For checking whether the room was modified... */
	long *msgids;
	unsigned int *flags;

	StrBuf *TransmittedMessage;	/* for APPEND command... */

	citimap_command Cmd;            /* our current commandline */

	/* Cache most recent RFC822 FETCH because client might load in pieces */
	StrBuf *cached_rfc822;
	long cached_rfc822_msgnum;
	char cached_rfc822_withbody;	/* 1 = body cached; 0 = only headers cached */

	/* Cache most recent BODY FETCH because client might load in pieces */
	char *cached_body;
	size_t cached_body_len;
	char cached_bodypart[SIZ];
	long cached_bodymsgnum;
	char cached_body_withbody;	/* 1 = body cached; 0 = only headers cached */
} citimap;

/*
 * values of 'authstate'
 */
enum {
	imap_as_normal,
	imap_as_expecting_username,
	imap_as_expecting_password,
	imap_as_expecting_plainauth,
	imap_as_expecting_multilineusername,
	imap_as_expecting_multilinepassword
};

/* Flags for the above struct.  Note that some of these are for internal use,
 * and are not to be reported to IMAP clients.
 */
#define IMAP_ANSWERED		1	/* reportable and setable */
#define IMAP_FLAGGED		2	/* reportable and setable */
#define IMAP_DELETED		4	/* reportable and setable */
#define IMAP_DRAFT		8	/* reportable and setable */
#define IMAP_SEEN		16	/* reportable and setable */

#define IMAP_MASK_SETABLE	0x1f
#define IMAP_MASK_SYSTEM	0xe0

#define IMAP_SELECTED		32	/* neither reportable nor setable */
#define IMAP_RECENT		64	/* reportable but not setable */


/*
 * Flags that may be returned by imap_roomname()
 * (the lower eight bits will be the floor number)
 */
#define IR_MAILBOX	0x0100		/* Mailbox                       */
#define IR_EXISTS	0x0200		/* Room exists (not implemented) */
#define IR_BABOON	0x0000		/* Just had to put this here :)  */

#define FDELIM '\\'

extern int IMAPDebugEnabled;

#define IMAP ((citimap *)CC->session_specific_data)
#define CCCIMAP ((citimap *)CCC->session_specific_data)

#define IMAPDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (IMAPDebugEnabled != 0))
#define CCCID CCC->cs_pid
#define IMAP_syslog(LEVEL, FORMAT, ...)				\
	IMAPDBGLOG(LEVEL) syslog(LEVEL,				\
				 "IMAP %s CC[%d] " FORMAT,	\
				 IOSTR, CCCID, __VA_ARGS__)

#define IMAPM_syslog(LEVEL, FORMAT)				\
	IMAPDBGLOG(LEVEL) syslog(LEVEL,				\
				 "IMAP %s CC[%d] " FORMAT,	\
				 IOSTR, CCCID)

#define I_FLAG_NONE          (0)
#define I_FLAG_LOGGED_IN  (1<<0)
#define I_FLAG_SELECT     (1<<1)
/* RFC3501 says that we cannot output untagged data during these commands */
#define I_FLAG_UNTAGGED   (1<<2)

/*
 * When loading arrays of message ID's into memory, increase the buffer to
 * hold this many additional messages instead of calling realloc() each time.
 */
#define REALLOC_INCREMENT 100


void registerImapCMD(const char *First, long FLen, 
		     const char *Second, long SLen,
		     imap_handler H,
		     int Flags);

#define RegisterImapCMD(First, Second, H, Flags) \
	registerImapCMD(HKEY(First), HKEY(Second), H, Flags)
