/*
 * $Id$
 *
 */

/*
 * FDELIM defines which character we want to use as a folder delimiter
 * in room names.  Originally we used a forward slash, but that caused
 * rooms with names like "Sent/Received Pages" to get delimited, so we
 * changed it to a backslash.  This is completely irrelevant to how Citadel
 * speaks to IMAP clients -- the delimiter used in the IMAP protocol is
 * a vertical bar, which is illegal in Citadel room names anyway.
 */
#define FDELIM '\\'

void imap_strout(char *buf);
int imap_parameterize(char **args, char *buf);
void imap_mailboxname(char *buf, int bufsize, struct quickroom *qrbuf);
void imap_ial_out(struct internet_address_list *ialist);
int imap_roomname(char *buf, int bufsize, char *foldername);
int imap_is_message_set(char *);
int imap_mailbox_matches_pattern(char *pattern, char *mailboxname);

/*
 * Flags that may be returned by imap_roomname()
 * (the lower eight bits will be the floor number)
 */
#define IR_MAILBOX	0x0100		/* Mailbox                       */
#define IR_EXISTS	0x0200		/* Room exists (not implemented) */
#define IR_BABOON	0x0000		/* Just had to put this here :)  */
