void load_command_set(void);
void sttybbs(int cmd);
void newprompt(char *prompt, char *str, int len);
void strprompt(char *prompt, char *str, int len);
int fmout(int width, FILE *fp, char pagin, int height, int starting_lp,
	  char subst);
int getcmd(char *argbuf);
void display_help(char *name);
void color(int colornum);
void cls(int colornum);
void send_ansi_detect(void);
void look_for_ansi(void);
int inkey(void);
void set_keepalives(int s);
