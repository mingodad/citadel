/*
 * $Id$
 *
 */



void imap_strout(char *buf);
int imap_parameterize(char **args, char *buf);
void imap_mailboxname(char *buf, int bufsize, struct quickroom *qrbuf);
void imap_ial_out(struct internet_address_list *ialist);
int imap_roomname(char *buf, int bufsize, char *foldername);


/*
 * Flags that may be returned by imap_roomname()
 * (the lower eight bits will be the floor number)
 */
#define IR_MAILBOX	0x0100		/* Mailbox                       */
#define IR_EXISTS	0x0200		/* Room exists (not implemented) */
#define IR_BABOON	0x0000		/* Just had to put this here :)  */
