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
};

#define IMAP ((struct citimap *)CtdlGetUserData(SYM_IMAP))
