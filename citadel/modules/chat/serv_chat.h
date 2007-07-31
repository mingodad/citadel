/* $Id$ */
void ChatUnloadingTest(void);
void allwrite (char *cmdbuf, int flag, char *username);
t_context *find_context (char **unstr);
void do_chat_listing (int allflag);
void cmd_chat (char *argbuf);
void cmd_pexp (char *argbuf); /* arg unused */
void cmd_sexp (char *argbuf);
void delete_instant_messages(void);
void cmd_gexp(char *);
int send_instant_message(char *, char *, char *);

struct savelist {
	struct savelist *next;
	char roomname[ROOMNAMELEN];
};
