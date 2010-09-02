/*
 * Screen output handling
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include "sysdep.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include <libcitadel.h>
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "commands.h"
#include "screen.h"

char arg_screen;

extern int screenheight;
extern int screenwidth;
extern int rc_ansi_color;
extern void check_screen_dims(void);

void do_keepalive(void);


/*
 * Initialize the screen
 */
void screen_new(void)
{
	send_ansi_detect();
	look_for_ansi();
	cls(0);
	color(DIM_WHITE);
}


/*
 * Kill the screen completely (used at exit).  It is safe to call this
 * function more than once.
 */
void screen_delete(void)
{
	screen_reset();
}



/*
 * Beep.
 */
void ctdl_beep(void) {
	putc(7, stdout);
}
	


/*
 * Set screen/IO parameters, e.g. at start of program or return from external
 * program run.
 */
int screen_set(void)
{
	return 0;
}


/*
 * Reset screen/IO parameters, e.g. at exit or fork of external program.
 */
int screen_reset(void)
{
	return 0;
}


/*
 * scr_printf() outputs to the terminal
 */
int scr_printf(char *fmt, ...)
{
	va_list ap;
	register int retval;
	va_start(ap, fmt);
	retval = vprintf(fmt, ap);
	va_end(ap);
	return retval;
}


/*
 * Read one character from the terminal
 */
int scr_getc(int delay)
{
	unsigned char buf;
	buf = '\0';
	if (!read (0, &buf, 1))
		logoff(NULL, 3);
	return buf;
}

/*
 * Output one character to the terminal
 */
int scr_putc(int c)
{
	if (putc(c, stdout) == EOF)
		logoff(NULL, 3);
	return c;
}


/*
 * scr_color() sets the window color for mainwindow
 */
int scr_color(int colornum)
{
	return 0;
}


void scr_flush(void)
{
	fflush(stdout);
}


static volatile int caught_sigwinch = 0;

/*
 * this is not supposed to be called from a signal handler.
 */
int scr_set_windowsize(CtdlIPC* ipc)
{
	return 0;
}

/*
 * scr_winch() handles window size changes from SIGWINCH
 * resizes all our windows for us
 */
RETSIGTYPE scr_winch(int signum)
{
	/* if we receive this signal, we must be running
	 * in a terminal that supports resizing.
	 */
	have_xterm = 1;
	caught_sigwinch = 1;
	check_screen_dims();
	signal(SIGWINCH, scr_winch);
}
