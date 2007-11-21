/* $Id$ 
 */


#define GLOBAL_UIDVALIDITY_VALUE	1L


void imap_cleanup_function(void);
void imap_greeting(void);
void imap_command_loop(void);
int imap_grabroom(char *returned_roomname, char *foldername, int zapped_ok);
void imap_free_transmitted_message(void);
int imap_do_expunge(void);
void imap_rescan_msgids(void);


struct citimap {
	int authstate;
	char authseq[SIZ];
	int selected;			/* set to 1 if in the SELECTED state */
	int readonly;			/* mailbox is open read only */
	int num_msgs;			/* Number of messages being mapped */
	int num_alloc;			/* Number of messages for which we've allocated space */
	time_t last_mtime;		/* For checking whether the room was modified... */
	long *msgids;
	unsigned int *flags;
	char *transmitted_message;	/* for APPEND command... */
	size_t transmitted_length;

	/* Cache most recent RFC822 FETCH because client might load in pieces */
	char *cached_rfc822_data;
	long cached_rfc822_msgnum;
	size_t cached_rfc822_len;
	char cached_rfc822_withbody;	/* 1 = body cached; 0 = only headers cached */

	/* Cache most recent BODY FETCH because client might load in pieces */
	char *cached_body;
	size_t cached_body_len;
	char cached_bodypart[SIZ];
	long cached_bodymsgnum;
	char cached_body_withbody;	/* 1 = body cached; 0 = only headers cached */
};

/*
 * values of 'authstate'
 */
enum {
	imap_as_normal,
	imap_as_expecting_username,
	imap_as_expecting_password,
	imap_as_expecting_plainauth
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


#define IMAP ((struct citimap *)CC->session_specific_data)

/*
 * When loading arrays of message ID's into memory, increase the buffer to
 * hold this many additional messages instead of calling realloc() each time.
 */
#define REALLOC_INCREMENT 100
