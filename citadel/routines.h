/* $Id$ */
void edituser(CtdlIPC *ipc);
void interr(int errnum);
int struncmp(char *lstr, char *rstr, int len);
int pattern(char *search, char *patn);
void enter_config(CtdlIPC *ipc, int mode);
void locate_host(char *hbuf);
void misc_server_cmd(CtdlIPC *ipc, char *cmd);
int nukedir(char *dirname);
int num_parms(char *source);
void strproc(char *string);
void back(int spaces);
void progress(unsigned long curr, unsigned long cmax);
int set_attr(CtdlIPC *ipc, unsigned int sval, char *prompt, unsigned int sbit, int backwards);
