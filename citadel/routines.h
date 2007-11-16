/* $Id$ */
void edituser(CtdlIPC *ipc, int cmd);
void interr(int errnum);
int struncmp(char *lstr, char *rstr, int len);
int pattern(char *search, char *patn);
void enter_config(CtdlIPC* ipc, int mode);
void locate_host(CtdlIPC* ipc, char *hbuf);
void misc_server_cmd(CtdlIPC *ipc, char *cmd);
int nukedir(char *dirname);
void strproc(char *string);
void back(int spaces);
void progress(CtdlIPC* ipc, unsigned long curr, unsigned long cmax);
int set_attr(CtdlIPC *ipc, unsigned int sval, char *prompt, unsigned int sbit, int backwards);
