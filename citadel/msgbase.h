/* $Id$ */

#define aide_message(text)      quickie_message("Citadel",NULL,AIDEROOM,text)

#define MSGS_ALL        0
#define MSGS_OLD        1
#define MSGS_NEW        2
#define MSGS_FIRST      3
#define MSGS_LAST       4
#define MSGS_GT         5

/*
 * Flags which may be passed to CtdlSaveMsgPointerInRoom()
 */
#define SM_VERIFY_GOODNESS	1	/* Verify this is a real msg number */
#define SM_DO_REPL_CHECK	2	/* Perform replication checks */


struct ma_info {
	char prefix[256];	/* Prefix for a multipart/alternative */
	int is_ma;		/* Set to 1 if we are using this stuff */
	int did_print;		/* One alternative has been displayed */
};


struct repl {			/* Info for replication checking */
	char extended_id[256];
	time_t highest;
};


int alias (char *name);
void get_mm (void);
void cmd_msgs (char *cmdbuf);
void help_subst (char *strbuf, char *source, char *dest);
void do_help_subst (char *buffer);
void memfmout (int width, char *mptr, char subst);
void output_mime_parts(char *);
void output_message (char *, int, int);
void cmd_msg0 (char *cmdbuf);
void cmd_msg2 (char *cmdbuf);
void cmd_msg3 (char *cmdbuf);
void cmd_msg4 (char *cmdbuf);
void cmd_opna (char *cmdbuf);
long send_message (struct CtdlMessage *, int, FILE *);
void loadtroom (void);
void CtdlSaveMsg(struct CtdlMessage *, char *, char *, int, int);
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
void simple_listing(long);
int CtdlMsgCmp(struct CtdlMessage *msg, struct CtdlMessage *template);
void CtdlForEachMessage(int mode, long ref,
			char *content_type,
			struct CtdlMessage *compare,
                        void (*CallBack) (long msgnum) );
int CtdlDeleteMessages(char *, long, char *);
void CtdlWriteObject(char *, char *, char *, struct usersupp *,
			int, int, unsigned int);
struct CtdlMessage *CtdlFetchMessage(long msgnum);
void CtdlFreeMessage(struct CtdlMessage *msg);
void serialize_message(struct ser_ret *, struct CtdlMessage *);
int is_valid_message(struct CtdlMessage *);
int ReplicationChecks(struct CtdlMessage *);
