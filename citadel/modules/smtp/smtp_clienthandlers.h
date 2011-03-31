typedef enum _eSMTP_C_States {
	eConnect, 
	eEHLO,
	eHELO,
	eSMTPAuth,
	eFROM,
	eRCPT,
	eDATA,
	eDATABody,
	eDATATerminateBody,
	eQUIT,
	eMaxSMTPC
} eSMTP_C_States;


typedef struct _stmp_out_msg {
	MailQEntry *MyQEntry;
	OneQueItem *MyQItem;
	long n;
	AsyncIO IO;
	long CXFlags;

	eSMTP_C_States State;

	struct ares_mx_reply *AllMX;
	struct ares_mx_reply *CurrMX;
	const char *mx_port;
	const char *mx_host;
	const char *LookupHostname;
	int iMX, nMX;
	int LookupWhich;

	DNSQueryParts MxLookup;
	DNSQueryParts HostLookup;
	struct hostent *OneMX;
	char **pIP;

	ParsedURL *Relay;
	ParsedURL *pCurrRelay;
	StrBuf *msgtext;
	const char *envelope_from;
	char user[1024];
	char node[1024];
	char name[1024];
	char mailfrom[1024];
	long Flags;
} SmtpOutMsg;


typedef eNextState (*SMTPReadHandler)(SmtpOutMsg *Msg);
typedef eNextState (*SMTPSendHandler)(SmtpOutMsg *Msg);

SMTPReadHandler ReadHandlers[eMaxSMTPC];
SMTPSendHandler SendHandlers[eMaxSMTPC];
const ConstStr ReadErrors[eMaxSMTPC];
const double SMTP_C_ReadTimeouts[eMaxSMTPC];
const double SMTP_C_SendTimeouts[eMaxSMTPC];
const double SMTP_C_ConnTimeout;

#define F_RELAY          (1<<0) /* we have a Relay    host configuration */
#define F_HAVE_FALLBACK  (1<<1) /* we have a fallback host configuration */
#define F_FALLBACK       (1<<2)
#define F_HAVE_MX        (1<<3) /* we have a list of mx records to go through. */
#define F_DIRECT         (1<<4) /* no mx record found, trying direct connect. */


int smtp_resolve_recipients(SmtpOutMsg *SendMsg);
