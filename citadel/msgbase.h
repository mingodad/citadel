/* $Id$ */

#define aide_message(text)      quickie_message("Citadel",NULL,AIDEROOM,text,0,NULL)

#define MSGS_ALL        0
#define MSGS_OLD        1
#define MSGS_NEW        2
#define MSGS_FIRST      3
#define MSGS_LAST       4
#define MSGS_GT         5
#define MSGS_EQ		6

/*
 * Flags which may be passed to CtdlSaveMsgPointerInRoom()
 */
#define SM_VERIFY_GOODNESS	1	/* Verify this is a real msg number */
#define SM_DO_REPL_CHECK	2	/* Perform replication checks */
#define SM_DONT_BUMP_REF	4	/* Don't bump reference count
					   (use with extreme care!!!!!!) */


/*
 * Possible return codes from CtdlOutputMsg()
 */
enum {
	om_ok,
	om_not_logged_in,
	om_no_such_msg,
	om_mime_error
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
	int did_print;		/* One alternative has been displayed */
	char chosen_part[SIZ];	/* Which part of a m/a did we choose? */
};


struct repl {			/* Info for replication checking */
	char exclusive_id[SIZ];
	time_t highest;
};


/* Data structure returned by validate_recipients() */
struct recptypes {
        int num_local;
        int num_internet;
        int num_ignet;
	int num_room;
        int num_error;
	char errormsg[SIZ];
	char recp_local[SIZ];
	char recp_internet[SIZ];
	char recp_ignet[SIZ];
	char recp_room[SIZ];
	char display_recp[SIZ];
};



int alias (char *name);
void get_mm (void);
void cmd_msgs (char *cmdbuf);
void cmd_isme (char *cmdbuf);
void help_subst (char *strbuf, char *source, char *dest);
void do_help_subst (char *buffer);
void memfmout (int width, char *mptr, char subst, char *nl);
void output_mime_parts(char *);
void cmd_msg0 (char *cmdbuf);
void cmd_msg2 (char *cmdbuf);
void cmd_msg3 (char *cmdbuf);
void cmd_msg4 (char *cmdbuf);
void cmd_msgp (char *cmdbuf);
void cmd_opna (char *cmdbuf);
long send_message (struct CtdlMessage *);
void loadtroom (void);
long CtdlSubmitMsg(struct CtdlMessage *, struct recptypes *, char *);
void quickie_message (char *, char *, char *, char *, int, char *);
void cmd_ent0 (char *entargs);
void cmd_dele (char *delstr);
void cmd_move (char *args);
void GetMetaData(struct MetaData *, long);
void PutMetaData(struct MetaData *);
void AdjRefCount(long, int);
void simple_listing(long, void *);
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template);
int CtdlForEachMessage(int mode, long ref,
			char *content_type,
			struct CtdlMessage *compare,
                        void (*CallBack) (long, void *),
			void *userdata);
int CtdlDeleteMessages(char *, long, char *);
void CtdlWriteObject(char *, char *, char *, struct ctdluser *,
			int, int, unsigned int);
struct CtdlMessage *CtdlFetchMessage(long msgnum, int with_body);
void CtdlFreeMessage(struct CtdlMessage *msg);
void serialize_message(struct ser_ret *, struct CtdlMessage *);
int is_valid_message(struct CtdlMessage *);
int ReplicationChecks(struct CtdlMessage *);
int CtdlSaveMsgPointerInRoom(char *roomname, long msgid, int flags);
char *CtdlReadMessageBody(char *terminator, size_t maxlen, char *exist, int crlf);
char *CtdlGetSysConfig(char *sysconfname);
void CtdlPutSysConfig(char *sysconfname, char *sysconfdata);
int CtdlOutputMsg(long msg_num,		/* message number (local) to fetch */
		int mode,		/* how would you like that message? */
		int headers_only,	/* eschew the message body? */
		int do_proto,		/* do Citadel protocol responses? */
		int crlf);
int CtdlOutputPreLoadedMsg(struct CtdlMessage *,
		long,
		int mode,		/* how would you like that message? */
		int headers_only,	/* eschew the message body? */
		int do_proto,		/* do Citadel protocol responses? */
		int crlf);
int CtdlCopyMsgToRoom(long msgnum, char *dest);
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void);
int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf, size_t n);


/* values for which_set */
enum {
	ctdlsetseen_seen,
	ctdlsetseen_answered
};
void CtdlSetSeen(long target_msgnum, int target_setting, int which_set,
		struct ctdluser *which_user, struct ctdlroom *which_room);
void CtdlGetSeen(char *buf, int which_set);

struct recptypes *validate_recipients(char *recipients);
struct CtdlMessage *CtdlMakeMessage(
        struct ctdluser *author,        /* author's user structure */
        char *recipient,                /* NULL if it's not mail */
        char *room,                     /* room where it's going */
        int type,                       /* see MES_ types in header file */
        int format_type,                /* variformat, plain text, MIME... */
        char *fake_name,                /* who we're masquerading as */
        char *subject,                  /* Subject (optional) */
        char *preformatted_text         /* ...or NULL to read text from client */
);
int CtdlCheckInternetMailPermission(struct ctdluser *who);
int CtdlIsMe(char *addr, int addr_buf_len);
