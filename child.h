/*
 * child.h: prototypes for the `webcit' child process
 *
 * $Id$
 */

void become_logged_in(char *user, char *pass, char *serv_response);
void do_login(void);
void display_login(void);
void do_welcome(void);
void do_logout(void);
void display_main_menu(void);
void display_advanced_menu(void);
void list_all_rooms_by_floor(void);
void slrp_highest(void);
void gotonext(void);
void ungoto(void);
void dotgoto(void);
void get_serv_info(void);
int connectsock(char *host, char *service, char *protocol);
void serv_gets(char *strbuf);
void serv_puts(char *string);
void whobbs(void);
void fmout(FILE *fp);
void wDumpContent(void);
void serv_printf(const char *format, ...);
char *bstr(char *key);
char *urlesc(char *);
void urlescputs(char *);
void output_headers(void);
void wprintf(const char *format, ...);
void extract(char *dest, char *source, int parmnum);
int extract_int(char *source, int parmnum);
void output_static(char *what);
void escputs(char *strbuf);
void url(char *buf);
void escputs1(char *strbuf, int nbsp);
long extract_long(char *source, long int parmnum);
void dump_vars(void);
