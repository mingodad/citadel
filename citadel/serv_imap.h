/* $Id$ 
 */

extern long SYM_IMAP;


void imap_cleanup_function(void);
void imap_greeting(void);
void imap_command_loop(void);


struct citimap {
	int selected;		/* set to 1 if in the SELECTED state */
	int readonly;		/* mailbox is open read only */
	int num_msgs;		/* Number of messages being mapped */
	long *msgids;
	unsigned int *flags;
};

/* Flags for the above struct.  Note that some of these are for internal use,
 * and are not to be reported to IMAP clients.
 */
#define IMAP_ANSWERED	1
#define IMAP_FLAGGED	2
#define IMAP_DELETED	4
#define IMAP_DRAFT	8
#define IMAP_SEEN	16
#define IMAP_FETCHED	32	/* internal */


#define IMAP ((struct citimap *)CtdlGetUserData(SYM_IMAP))
