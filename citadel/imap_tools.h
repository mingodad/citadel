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

typedef struct __citimap_command {
	StrBuf *CmdBuf;			/* our current commandline; gets chopped into: */
	ConstStr *Params;		/* Commandline tokens */
	int num_parms;			/* Number of Commandline tokens available */
	int avail_parms;		/* Number of ConstStr args is big */
} citimap_command;

/* 
 * since we work with shifted pointers to ConstStrs in some places, 
 * we can't just say we want to cut the n'th of Cmd, we have to pass it in
 * and rely on that CutMe references Cmd->CmdBuf; else peek won't work out
 * and len will differ.
 */
void TokenCutRight(citimap_command *Cmd, 
		   ConstStr *CutMe,
		   int n);
/*
 * since we just move Key around here, Cmd is just here so the syntax is identical.
 */
void TokenCutLeft(citimap_command *Cmd, 
		  ConstStr *CutMe,
		  int n);
void MakeStringOf(StrBuf *Buf, int skip);

int CmdAdjust(citimap_command *Cmd, 
	      int nArgs,
	      int Realloc);


void imap_strout(ConstStr *args);
void plain_imap_strout(char *buf);
int imap_parameterize(citimap_command *Cmd);
int old_imap_parameterize(char** args, char *n);
void imap_mailboxname(char *buf, int bufsize, struct ctdlroom *qrbuf);
void imap_ial_out(struct internet_address_list *ialist);
int imap_roomname(char *buf, int bufsize, const char *foldername);
int imap_is_message_set(const char *);
int imap_mailbox_matches_pattern(char *pattern, char *mailboxname);
int imap_datecmp(const char *datestr, time_t msgtime);

/*
 * Flags that may be returned by imap_roomname()
 * (the lower eight bits will be the floor number)
 */
#define IR_MAILBOX	0x0100		/* Mailbox                       */
#define IR_EXISTS	0x0200		/* Room exists (not implemented) */
#define IR_BABOON	0x0000		/* Just had to put this here :)  */
