/* $Id$ */

#define aide_message(text)      quickie_message("Citadel",NULL,AIDEROOM,text)

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
	


struct ma_info {
	int is_ma;		/* Set to 1 if we are using this stuff */
	int did_print;		/* One alternative has been displayed */
};


struct repl {			/* Info for replication checking */
	char extended_id[SIZ];
	time_t highest;
};


int alias (char *name);
void get_mm (void);
void cmd_msgs (char *cmdbuf);
void help_subst (char *strbuf, char *source, char *dest);
void do_help_subst (char *buffer);
void memfmout (int width, char *mptr, char subst, char *nl);
void output_mime_parts(char *);
void cmd_msg0 (char *cmdbuf);
void cmd_msg2 (char *cmdbuf);
void cmd_msg3 (char *cmdbuf);
void cmd_msg4 (char *cmdbuf);
void cmd_opna (char *cmdbuf);
long send_message (struct CtdlMessage *, FILE *);
void loadtroom (void);
long CtdlSaveMsg(struct CtdlMessage *, char *, char *, int);
void quickie_message (char *, char *, char *, char *);
struct CtdlMessage *make_message (struct usersupp *, char *,
		   char *, int, int, int, char *);
void cmd_ent0 (char *entargs);
void cmd_ent3 (char *entargs);
void cmd_dele (char *delstr);
void cmd_move (char *args);
void GetSuppMsgInfo(struct SuppMsgInfo *, long);
void PutSuppMsgInfo(struct SuppMsgInfo *);
void AdjRefCount(long, int);
void simple_listing(long, void *);
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template);
int CtdlForEachMessage(int mode, long ref,
			int moderation_level,
			char *content_type,
			struct CtdlMessage *compare,
                        void (*CallBack) (long, void *),
			void *userdata);
int CtdlDeleteMessages(char *, long, char *);
void CtdlWriteObject(char *, char *, char *, struct usersupp *,
			int, int, unsigned int);
struct CtdlMessage *CtdlFetchMessage(long msgnum);
void CtdlFreeMessage(struct CtdlMessage *msg);
void serialize_message(struct ser_ret *, struct CtdlMessage *);
int is_valid_message(struct CtdlMessage *);
int ReplicationChecks(struct CtdlMessage *);
int CtdlSaveMsgPointerInRoom(char *roomname, long msgid, int flags);
char *CtdlReadMessageBody(char *terminator, size_t maxlen, char *exist);
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
int CtdlDoIHavePermissionToPostInThisRoom(char *errmsgbuf);
void CtdlSetSeen(long target_msgnum, int target_setting);
