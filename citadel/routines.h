/* $Id$ */
void edituser(void);
void extract(char *dest, char *source, int parmnum);
long extract_long(char *source, int parmnum);
int extract_int(char *source, int parmnum);
void interr(int errnum);
int struncmp(char *lstr, char *rstr, int len);
int yesno(void);
int checkpagin(int lp, int pagin, int height);
int pattern(char *search, char *patn);
int num_parms(char *source);
void enter_config(int mode);
void locate_host(char *hbuf);
void misc_server_cmd(char *cmd);
int nukedir(char *dirname);
int num_parms(char *source);
void strproc(char *string);
void back(int spaces);
