/* $Id$ */
int ka_system(char *shc);
int entmsg(CtdlIPC *ipc, int is_reply, int c);
void readmsgs(CtdlIPC *ipc, int c, int rdir, int q);
void edit_system_message(char *which_message);
pid_t ka_wait(int *kstatus);
void list_urls(CtdlIPC *ipc);
void check_message_base(CtdlIPC *ipc);
int client_make_message(CtdlIPC *ipc,
		char *filename,		/* temporary file name */
		char *recipient,	/* NULL if it's not mail */
		int anon_type,		/* see MES_ types in header file */
		int format_type,
		int mode,
		char *subject);
void citedit(CtdlIPC *ipc, FILE *);
int file_checksum(char *filename);
