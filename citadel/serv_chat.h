/* $Id$ */
void ChatUnloadingTest(void);
void allwrite (char *cmdbuf, int flag, char *roomname, char *username);
t_context *find_context (char **unstr);
void do_chat_listing (int allflag);
void cmd_chat (char *argbuf);
void cmd_pexp (char *argbuf); /* arg unused */
char check_express (void);
void cmd_sexp (char *argbuf);
