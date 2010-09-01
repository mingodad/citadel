#include "ctdl_module.h"

void ChatUnloadingTest(void);
void allwrite (char *cmdbuf, int flag, char *username);
CitContext *find_context (char **unstr);
void cmd_pexp (char *argbuf); /* arg unused */
void cmd_sexp (char *argbuf);
void delete_instant_messages(void);
void cmd_gexp(char *);
int send_instant_message(char *, char *, char *, char *);
