/* $Id$ */

/* client code may need the ERR define: */

#ifdef HAVE_NCURSES_H
#include <ncurses.h>
#elif defined(HAVE_CURSES_H)
#include <curses.h>
#endif

void status_line(const char *humannode, const char *bbs_city,
		 const char *room_name, int secure, int newmailcount);
void screen_new(void);
void screen_delete(void);
int screen_set(void);
int screen_reset(void);
int scr_printf(char *fmt, ...);
int err_printf(char *fmt, ...);
int sln_printf(char *fmt, ...);
int sln_printf_if(char *fmt, ...);

#define SCR_NOBLOCK 0
#define SCR_BLOCK -1
int scr_getc(int delay);

int scr_putc(int c);
int sln_putc(int c);
int scr_color(int colornum);
void scr_flush(void);
void err_flush(void);
void sln_flush(void);
int scr_set_windowsize(void);
void windows_new(void);
void windows_delete(void);
int scr_blockread(void);
int is_curses_enabled(void);
RETSIGTYPE scr_winch(int signum);
