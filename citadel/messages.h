/* $Id$ */
int ka_system(char *shc);
int entmsg(int is_reply, int c);
void readmsgs(int c, int rdir, int q);
void edit_system_message(char *which_message);
extern int lines_printed;
