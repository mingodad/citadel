/* $Id$ */

void screen_new(void);
void screen_delete(void);
int screen_set(void);
int screen_reset(void);
int scr_printf(char *fmt, ...);
int err_printf(char *fmt, ...);
int sln_printf(char *fmt, ...);
int scr_getc(void);
int scr_putc(int c);
int scr_color(int colornum);
void scr_flush(void);
void err_flush(void);
void sln_flush(void);
int scr_set_windowsize(void);
void windows_new(void);
void windows_delete(void);
