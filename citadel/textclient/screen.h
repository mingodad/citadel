
/* client code may need the ERR define: */

void screen_new(void);
void screen_delete(void);
int screen_set(void);
int screen_reset(void);
int scr_printf(char *fmt, ...);

#define SCR_NOBLOCK 0
#define SCR_BLOCK -1
int scr_getc(int delay);

int scr_putc(int c);
int scr_color(int colornum);
void scr_flush(void);
int scr_set_windowsize(CtdlIPC* ipc);
int scr_blockread(void);
RETSIGTYPE scr_winch(int signum);
void wait_indicator(int state);
void ctdl_beep(void);
