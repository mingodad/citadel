
#ifndef MSGBASE_H
#define MSGBASE_H

#include "event_client.h"
enum {
	MSGS_ALL,
	MSGS_OLD,
	MSGS_NEW,
	MSGS_FIRST,
	MSGS_LAST,
	MSGS_GT,
	MSGS_EQ,
	MSGS_SEARCH,
	MSGS_LT
};

enum {
	MSG_HDRS_BRIEF = 0,
	MSG_HDRS_ALL = 1,
	MSG_HDRS_EUID = 4
};

/*
 * Possible return codes from CtdlOutputMsg()
 */
enum {
	om_ok,
	om_not_logged_in,
	om_no_such_msg,
	om_mime_error,
	om_access_denied
};

/*
 * Values of "headers_only" when calling message output routines
 */
#define HEADERS_ALL	0	/* Headers and body */
#define	HEADERS_ONLY	1	/* Headers only */
#define	HEADERS_NONE	2	/* Body only */
#define HEADERS_FAST	3	/* Headers only with no MIME info */


struct ma_info {
	int is_ma;		/* Set to 1 if we are using this stuff */
	int freeze;		/* Freeze the replacement chain because we're
				 * digging through a subsection */
	int did_print;		/* One alternative has been displayed */
	char chosen_part[128];	/* Which part of a m/a did we choose? */
	int chosen_pref;	/* Chosen part preference level (lower is better) */
	int use_fo_hooks;	/* Use fixed output hooks */
	int dont_decode;        /* should we call the decoder or not? */
};


struct repl {			/* Info for replication checking */
	char exclusive_id[SIZ];
	time_t highest;
};


/* Data structure returned by validate_recipients() */
struct recptypes {
	int recptypes_magic;
        int num_local;
        int num_internet;
        int num_ignet;
	int num_room;
        int num_error;
	char *errormsg;
	char *recp_local;
	char *recp_internet;
	char *recp_ignet;
	char *recp_room;
	char *recp_orgroom;
	char *display_recp;
	char *bounce_to;
	char *envelope_from;
	char *sending_room;
};

#define RECPTYPES_MAGIC 0xfeeb

/*
 * This is a list of "harvested" email addresses that we might want to
 * stick into someone's address book.  But we defer this operaiton so
 * it can be done asynchronously.
 */
struct addresses_to_be_filed {
	struct addresses_to_be_filed *next;
	char *roomname;
	char *collected_addresses;
};

extern struct addresses_to_be_filed *atbf;

int alias (char *name);
void cmd_msgs (char *cmdbuf);
void cmd_isme (char *cmdbuf);
void help_subst (char *strbuf, char *source, char *dest);
void do_help_subst (char *buffer);
void memfmout (char *mptr, const char *nl);
void output_mime_parts(char *);
void cmd_msg0 (char *cmdbuf);
void cmd_msg2 (char *cmdbuf);
void cmd_msg3 (char *cmdbuf);
void cmd_msg4 (char *cmdbuf);
void cmd_msgp (char *cmdbuf);
void cmd_opna (char *cmdbuf);
void cmd_dlat (char *cmdbuf);
long send_message (struct CtdlMessage *);
void loadtroom (void);
long CtdlSubmitMsg(struct CtdlMessage *, struct recptypes *, const char *, int);
void quickie_message (const char *, const char *, char *, char *, const char *, int, const char *);
void cmd_ent0 (char *entargs);
void cmd_dele (char *delstr);
void cmd_move (char *args);
void GetMetaData(struct MetaData *, long);
void PutMetaData(struct MetaData *);
void AdjRefCount(long, int);
void TDAP_AdjRefCount(long, int);
int TDAP_ProcessAdjRefCountQueue(void);
void simple_listing(long, void *);
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template);
typedef void (*ForEachMsgCallback)(long MsgNumber, void *UserData);
int CtdlForEachMessage(int mode,
			long ref,
			char *searchstring,
			char *content_type,
			struct CtdlMessage *compare,
                        ForEachMsgCallback CallBack,
			void *userdata);
int CtdlDeleteMessages(char *, long *, int, char *);
void CtdlWriteObject(char *req_room,			/* Room to stuff it in */
			char *content_type,		/* MIME type of this object */
			char *raw_message,		/* Data to be written */
			off_t raw_length,		/* Size of raw_message */
			struct ctdluser *is_mailbox,	/* Mailbox room? */
			int is_binary,			/* Is encoding necessary? */
			int is_unique,			/* Del others of this type? */
			unsigned int flags		/* Internal save flags */
);
struct CtdlMessage *CtdlFetchMessage(long msgnum, int with_body);
void CtdlFreeMessage(struct CtdlMessage *msg);
void CtdlFreeMessageContents(struct CtdlMessage *msg);
void serialize_message(struct ser_ret *, struct CtdlMessage *);
int is_valid_message(struct CtdlMessage *);
void ReplicationChecks(struct CtdlMessage *);
int CtdlSaveMsgPointersInRoom(char *roomname, long newmsgidlist[], int num_newmsgs,
			int do_repl_check, struct CtdlMessage *supplied_msg, int suppress_refcount_adj);
int CtdlSaveMsgPointerInRoom(char *roomname, long msgid, int do_repl_check, struct CtdlMessage *msg);
char *CtdlReadMessageBody(char *terminator, long tlen, size_t maxlen, char *exist, int crlf, int *sock);
StrBuf *CtdlReadMessageBodyBuf(char *terminator,	/* token signalling EOT */
			       long tlen,
			       size_t maxlen,		/* maximum message length */
			       char *exist,		/* if non-null, append to it;
							   exist is ALWAYS freed  */
			       int crlf,		/* CRLF newlines instead of LF */
			       int *sock		/* socket handle or 0 for this session's client socket */
	);

int CtdlOutputMsg(long msg_num,		/* message number (local) to fetch */
		  int mode,		/* how would you like that message? */
		  int headers_only,	/* eschew the message body? */
		  int do_proto,		/* do Citadel protocol responses? */
		  int crlf,		/* 0=LF, 1=CRLF */
		  char *section,		/* output a message/rfc822 section */
		  int flags,		/* should the bessage be exported clean? */
		  char **Author,        /* if you want to know the author of the message... */
		  char **Address        /* if you want to know the sender address of the message... */
);

/* Flags which may be passed to CtdlOutputMsg() and CtdlOutputPreLoadedMsg() */
#define QP_EADDR	(1<<0)		/* quoted-printable encode email addresses */
#define CRLF		(1<<1)
#define ESC_DOT		(1<<2)		/* output a line containing only "." as ".." instead */
#define SUPPRESS_ENV_TO	(1<<3)		/* suppress Envelope-to: header (warning: destructive!) */

int CtdlOutputPreLoadedMsg(struct CtdlMessage *,
			   int mode,		/* how would you like that message? */
			   int headers_only,	/* eschew the message body? */
			   int do_proto,	/* do Citadel protocol responses? */
			   int crlf,		/* 0=LF, 1=CRLF */
			   int flags		/* should the bessage be exported clean? */
);
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void);
int CtdlDoIHavePermissionToReadMessagesInThisRoom(void);

enum {
	POST_LOGGED_IN,
	POST_EXTERNAL,
	CHECK_EXISTANCE,
	POST_LMTP
};

int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf, 
	size_t n, 
	const char* RemoteIdentifier,
	int PostPublic,
	int is_reply
);


/* values for which_set */
enum {
	ctdlsetseen_seen,
	ctdlsetseen_answered
};
void CtdlSetSeen(long *target_msgnums, int num_target_msgnums,
		 int target_setting, int which_set,
		struct ctdluser *which_user, struct ctdlroom *which_room);
void CtdlGetSeen(char *buf, int which_set);

struct recptypes *validate_recipients(const char *recipients,
 				      const char *RemoteIdentifier, 
				      int Flags);

void free_recipients(struct recptypes *);

struct CtdlMessage *CtdlMakeMessage(
        struct ctdluser *author,        /* author's user structure */
        char *recipient,                /* NULL if it's not mail */
        char *recp_cc,	                /* NULL if it's not mail */
        char *room,                     /* room where it's going */
        int type,                       /* see MES_ types in header file */
        int format_type,                /* variformat, plain text, MIME... */
        char *fake_name,                /* who we're masquerading as */
	char *my_email,			/* which of my email addresses to use (empty is ok) */
        char *subject,                  /* Subject (optional) */
	char *supplied_euid,		/* ...or NULL if this is irrelevant */
        char *preformatted_text,        /* ...or NULL to read text from client */
	char *references		/* Thread references */
);
int CtdlCheckInternetMailPermission(struct ctdluser *who);
int CtdlIsMe(char *addr, int addr_buf_len);

/* 
 * loading messages async via an FD: 
 * add IO->ReadMsg = NewAsyncMsg(...)
 * and then call CtdlReadMessageBodyAsync() from your linreader handler.
 */

ReadAsyncMsg *NewAsyncMsg(const char *terminator,	/* token signalling EOT */
			  long tlen,
			  size_t expectlen,             /* if we expect a message, how long should it be? */
			  size_t maxlen,		/* maximum message length */
			  char *exist,			/* if non-null, append to it;
						   	   exist is ALWAYS freed  */
			  long eLen,            	/* length of exist */
			  int crlf			/* CRLF newlines instead of LF */
	);

eReadState CtdlReadMessageBodyAsync(AsyncIO *IO);
void DeleteAsyncMsg(ReadAsyncMsg **Msg);

extern int MessageDebugEnabled;

#define MSGDBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (MessageDebugEnabled != 0))
#define CCCID CCC->cs_pid
#define MSG_syslog(LEVEL, FORMAT, ...)			\
	MSGDBGLOG(LEVEL) syslog(LEVEL,			\
				"CC[%d]" FORMAT,	\
				CCCID, __VA_ARGS__)

#define MSGM_syslog(LEVEL, FORMAT)			\
	MSGDBGLOG(LEVEL) syslog(LEVEL,			\
				"CC[%d]" FORMAT,	\
				CCCID)



#endif /* MSGBASE_H */
