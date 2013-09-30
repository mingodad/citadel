
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
void imap_strbuffer(StrBuf *Reply, ConstStr *args);
void plain_imap_strbuffer(StrBuf *Reply, char *buf);
int imap_parameterize(citimap_command *Cmd);
long imap_mailboxname(char *buf, int bufsize, struct ctdlroom *qrbuf);
int imap_roomname(char *buf, int bufsize, const char *foldername);
int imap_is_message_set(const char *);
int imap_mailbox_matches_pattern(const char *pattern, char *mailboxname);
int imap_datecmp(const char *datestr, time_t msgtime);


/* Imap Append Printf, send to the outbuffer */
void IAPrintf(const char *Format, ...) __attribute__((__format__(__printf__,1,2)));

void iaputs(const char *Str, long Len);
#define IAPuts(Msg) iaputs(HKEY(Msg))
/* give it a naughty name since its ugly. */
#define _iaputs(Msg) iaputs(Msg, strlen(Msg))

/* outputs a static message prepended by the sequence no */
void ireply(const char *Msg, long len);
#define IReply(msg) ireply(HKEY(msg))
/* outputs a dynamic message prepended by the sequence no */
void IReplyPrintf(const char *Format, ...);


/* output a string like that {%ld}%s */
void IPutStr(const char *Msg, long Len);
#define IPutCStr(_ConstStr) IPutStr(CKEY(_ConstStr))
#define IPutCParamStr(n) IPutStr(CKEY(Params[n]))
#define IPutMsgField(Which) IPutStr(CM_KEY(msg, Which))
void IUnbuffer (void);
