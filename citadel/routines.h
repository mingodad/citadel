/* $Id$ */
void edituser(void);
void interr(int errnum);
int struncmp(char *lstr, char *rstr, int len);
int pattern(char *search, char *patn);
void enter_config(int mode);
void locate_host(char *hbuf);
void misc_server_cmd(char *cmd);
int nukedir(char *dirname);
int num_parms(char *source);
void strproc(char *string);
void back(int spaces);
