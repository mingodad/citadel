/* $Id$ 
 */


void imap_cleanup_function(void);
void imap_greeting(void);
void imap_command_loop(void);
int imap_grabroom(char *returned_roomname, char *foldername);
void imap_free_transmitted_message(void);
int imap_do_expunge(void);


struct citimap {
	int authstate;
	char authseq[SIZ];
	int selected;		/* set to 1 if in the SELECTED state */
	int readonly;		/* mailbox is open read only */
	int num_msgs;		/* Number of messages being mapped */
	long *msgids;
	unsigned int *flags;
	char *transmitted_message;	/* for APPEND command... */
	size_t transmitted_length;

	FILE *cached_fetch;		/* cache our most recent RFC822 FETCH */
	long cached_msgnum;		/* because the client might ask for it in pieces */

	FILE *cached_body;		/* cache our most recent BODY FETCH */
	char cached_bodypart[SIZ];	/* because the client might ask for it in pieces */
	long cached_bodymsgnum;
};

/*
 * values of 'authstate'
 */
enum {
	imap_as_normal,
	imap_as_expecting_username,
	imap_as_expecting_password
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


#define IMAP ((struct citimap *)CtdlGetUserData(SYM_IMAP))

/*
 * When loading arrays of message ID's into memory, increase the buffer to
 * hold this many additional messages instead of calling realloc() each time.
 */
#define REALLOC_INCREMENT 100
