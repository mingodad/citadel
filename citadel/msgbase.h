/* $Id$ */
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
long int send_message (char *message_in_memory, size_t message_length,
		       int generate_id);
void loadtroom (void);
void copy_file (char *from, char *to);
void save_message (char *, char *, char *, int, int);
void aide_message (char *text);
void make_message (char *filename, struct usersupp *author, char *recipient,
		   char *room, int type, int net_type, int format_type,
		   char *fake_name);
void cmd_ent0 (char *entargs);
void cmd_ent3 (char *entargs);
void cmd_dele (char *delstr);
void cmd_move (char *args);
void GetSuppMsgInfo(struct SuppMsgInfo *, long);
void PutSuppMsgInfo(struct SuppMsgInfo *);
void AdjRefCount(long, int);
void simple_listing(long);
void CtdlForEachMessage(int mode, long ref,
			char *content_type,
                        void (*CallBack) (long msgnum) );
int CtdlDeleteMessages(char *, long, char *);
void CtdlWriteObject(char *, char *, char *, int, int, int);
struct CtdlMessage *CtdlFetchMessage(long msgnum);
void CtdlFreeMessage(struct CtdlMessage *msg);
char *serialize_message(struct CtdlMessage *msg);




#define MSGS_ALL        0
#define MSGS_OLD        1
#define MSGS_NEW        2
#define MSGS_FIRST      3
#define MSGS_LAST       4
#define MSGS_GT         5

struct ma_info {
	char prefix[256];	/* Prefix for a multipart/alternative */
	int is_ma;		/* Set to 1 if we are using this stuff */
	int did_print;		/* One alternative has been displayed */
};
