
#define MAXURLS		50	/* Max embedded URL's per message */
extern int num_urls;
extern char urls[MAXURLS][SIZ];

int ka_system(char *shc);
int entmsg(CtdlIPC *ipc, int is_reply, int c, int masquerade);
void readmsgs(CtdlIPC *ipc, enum MessageList c, enum MessageDirection rdir, int q);
void edit_system_message(CtdlIPC *ipc, char *which_message);
pid_t ka_wait(int *kstatus);
void list_urls(CtdlIPC *ipc);
int client_make_message(CtdlIPC *ipc,
			char *filename,		/* temporary file name */
			char *recipient,	/* NULL if it's not mail */
			int anon_type,		/* see MES_ types in header file */
			int format_type,
			int mode,
			char *subject,
			int subject_required
);
void citedit(FILE *);
char *load_message_from_file(FILE *src);
int file_checksum(char *filename);
