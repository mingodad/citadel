/*
 * $Id$
 *
 */



void imap_strout(char *buf);
int imap_parameterize(char **args, char *buf);
void imap_mailboxname(char *buf, int bufsize, struct quickroom *qrbuf);
void imap_ial_out(struct internet_address_list *ialist);
int imap_roomname(char *buf, int bufsize, char *foldername);
