/* $Id$ */
int ka_system(char *shc);
int entmsg(int is_reply, int c);
void readmsgs(int c, int rdir, int q);
void edit_system_message(char *which_message);
pid_t ka_wait(int *kstatus);
void list_urls(void);
void check_message_base(void);
int make_message(char *filename,	/* temporary file name */
		char *recipient,	/* NULL if it's not mail */
		int anon_type,		/* see MES_ types in header file */
		int format_type,
		int mode);
void citedit(FILE *);
