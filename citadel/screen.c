/* $Id$ */

/*
 * Handle full-screen curses stuff
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <sys/types.h>
#include "sysdep.h"
#ifdef HAVE_VW_PRINTW
#define _vwprintw vw_printw
#else
/* SYSV style curses (Solaris, etc.) */
#define _vwprintw vwprintw
#endif
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "citadel.h"
#include "citadel_ipc.h"
#include "citadel_decls.h"
#include "commands.h"
#include "screen.h"

#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
static SCREEN *myscreen = NULL;
static WINDOW *mainwindow = NULL;
static WINDOW *statuswindow = NULL;

char rc_screen;
#endif
char arg_screen;

extern int screenheight;
extern int screenwidth;
extern int rc_ansi_color;
extern void check_screen_dims(void);

void do_keepalive(void);


int is_curses_enabled(void) {
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	return mainwindow != NULL;
#else
	return 0;
#endif
}

/*
 * status_line() is a convenience function for writing a "typical"
 * status line to the window.
 */
void status_line(const char *humannode, const char *site_location,
		 const char *room_name, int secure, int newmailcount)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (statuswindow) {
		if (secure) {
			sln_printf("Encrypted ");
			waddch(statuswindow, ACS_VLINE);
			waddch(statuswindow, ' ');
		}
		if (room_name)
			sln_printf("%s on ", room_name);
		if (humannode)
			sln_printf("%s ", humannode);
		if (newmailcount > 0) {
			waddch(statuswindow, ACS_VLINE);
			sln_printf(" %d new mail ", newmailcount);
		}
		sln_printf("\n");
	}
#endif /* HAVE_CURSES_H */
}


/*
 * Display a 3270-style "wait" indicator at the bottom of the screen
 */
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
void wait_indicator(int state) {

	if (!isendwin() && statuswindow) {

		mvwinch(statuswindow, 0, screenwidth - 2);
		switch (state) {
		default:
		case 0:		/* Idle */
			waddch(statuswindow, ' ');
			break;
		case 1:		/* Waiting */
			waddch(statuswindow, 'X');
			break;
		case 2:		/* Receiving */
			waddch(statuswindow, '<');
			break;
		case 3:		/* Sending */
			waddch(statuswindow, '>');
			break;
		}
		waddch(statuswindow, '\r');
		wrefresh(statuswindow);
		wrefresh(mainwindow);	/* this puts the cursor back */
	}
}
#else
void wait_indicator(int state) {}
#endif


/*
 * Initialize the screen.  If newterm() fails, myscreen will be NULL and
 * further handlers will assume we should be in line mode.
 */
void screen_new(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (arg_screen != RC_NO && rc_screen != RC_NO)
		myscreen = newterm(NULL, stdout, stdin);
	if (myscreen) {
		cbreak();
		noecho();
		nonl();
		intrflush(stdscr, FALSE);
		keypad(stdscr, TRUE);
		/* Setup all our colors */
		start_color();
		if (rc_ansi_color)
			enable_color = 1;
		/*init_pair(DIM_BLACK, COLOR_BLACK, COLOR_BLACK);*/
		init_pair(DIM_RED, COLOR_RED, COLOR_BLACK);
		init_pair(DIM_GREEN, COLOR_GREEN, COLOR_BLACK);
		init_pair(DIM_YELLOW, COLOR_YELLOW, COLOR_BLACK);
		init_pair(DIM_BLUE, COLOR_BLUE, COLOR_BLACK);
		init_pair(DIM_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(DIM_CYAN, COLOR_CYAN, COLOR_BLACK);
		init_pair(DIM_WHITE, COLOR_WHITE, COLOR_BLACK);

		if (COLOR_PAIRS > 8)
			init_pair(8, COLOR_WHITE, COLOR_BLUE);
	} else
#endif /* HAVE_CURSES_H */
	{
		send_ansi_detect();
		look_for_ansi();
		cls(0);
		color(DIM_WHITE);
	}
	screen_set();
	windows_new();
}


/*
 * Kill the screen completely (used at exit).  It is safe to call this
 * function more than once.
 */
void screen_delete(void)
{
	windows_delete();
	screen_reset();
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (myscreen) {
		delscreen(myscreen);
	}
	myscreen = NULL;
#endif
}

/*
 * Beep.
 */
void ctdl_beep(void) {
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (myscreen)
		beep();
	else
#endif
	putc(7, stdout);
}
	


/*
 * Set screen/IO parameters, e.g. at start of program or return from external
 * program run.
 */
int screen_set(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (myscreen) {
		set_term(myscreen);
		wrefresh(curscr);
		return 1;
	}
#endif /* HAVE_CURSES_H */
	return 0;
}


/*
 * Reset screen/IO parameters, e.g. at exit or fork of external program.
 */
int screen_reset(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (myscreen) {
		if (!isendwin()) endwin();
		return 1;
	}
#endif /* HAVE_CURSES_H */
	return 0;
}


/*
 * scr_printf() outputs to the main window (or screen if not in curses)
 */
int scr_printf(char *fmt, ...)
{
	va_list ap;
	register int retval;

	va_start(ap, fmt);
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow) {
		retval = _vwprintw(mainwindow, fmt, ap);
	} else
#endif
		retval = vprintf(fmt, ap);
	va_end(ap);
	return retval;
}


/*
 * err_printf() outputs to error status window (or stderr if not in curses)
 */
int err_printf(char *fmt, ...)
{
	va_list ap;
	register int retval;

	va_start(ap, fmt);
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow)	{		/* FIXME: direct to error window */
		retval = _vwprintw(mainwindow, fmt, ap);
		if (fmt[strlen(fmt) - 1] == '\n')
			wrefresh(mainwindow);
	} else
#endif
		retval = vfprintf(stderr, fmt, ap);
	va_end(ap);
	return retval;
}


/*
 * sln_printf() outputs to error status window (or stderr if not in curses)
 */
int sln_printf(char *fmt, ...)
{
	va_list ap;
	register int retval;
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	static char buf[4096];
#endif

	va_start(ap, fmt);
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (statuswindow) {
		register char *i;
		
		retval = vsnprintf(buf, 4096, fmt, ap);
		for (i = buf; *i; i++) {
			if (*i == '\r' || *i == '\n')
				wclrtoeol(statuswindow);
			sln_putc(*i);
			if (*i == '\r' || *i == '\n') {
				wrefresh(statuswindow);
				mvwinch(statuswindow, 0, 0);
			}
		}
	} else
#endif
		retval = vprintf(fmt, ap);
	va_end(ap);
	return retval;
}


/*
 * sln_printf_if() outputs to status window, no output if not in curses
 */
int sln_printf_if(char *fmt, ...)
{
	register int retval = 1;
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	static char buf[4096];
	va_list ap;

	va_start(ap, fmt);
	if (statuswindow) {
		register char *i;
		
		retval = vsnprintf(buf, 4096, fmt, ap);
		for (i = buf; *i; i++) {
			if (*i == '\r' || *i == '\n')
				wclrtoeol(statuswindow);
			sln_putc(*i);
			if (*i == '\r' || *i == '\n') {
				wrefresh(statuswindow);
				mvwinch(statuswindow, 0, 0);
			}
		}
	}
	va_end(ap);
#endif
	return retval;
}


int scr_getc(int delay)
{
  unsigned char buf;

#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow) {
		wtimeout(mainwindow, delay);
		return wgetch(mainwindow);
	}
#endif

	buf = '\0';
	if (!read (0, &buf, 1))
		logoff(NULL, 3);
	return buf;
}

/* the following is unused and looks broken, but there may
   be some input problems still lurking in curses mode, so
   i'll leave it blocked out for now for informational
   purposes. */
#if 0
int scr_blockread(void)
  {
    int a = 0;
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
    wtimeout(mainwindow, S_KEEPALIVE); 
    while (1)
      {
        do_keepalive();
        a = wgetch(mainwindow); /* will block for food */
        if (a != ERR)
          break;
        /* a = scr_getc(); */
      }
#endif
    return a;
  }
#endif /* 0 */

/*
 * scr_putc() outputs a single character
 */
int scr_putc(int c)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow) {
		if (c == 7) beep();
		if (waddch(mainwindow, c) != OK)
			logoff(NULL, 3);
		return c;
	}
#endif
	if (putc(c, stdout) == EOF)
		logoff(NULL, 3);
	return c;
}


int sln_putc(int c)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (statuswindow)
		return ((waddch(statuswindow, c) == OK) ? c : EOF);
#endif
	return putc(c, stdout);
}


int sln_putc_if(int c)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (statuswindow)
		return ((waddch(statuswindow, c) == OK) ? c : EOF);
#endif
	return 1;
}


/*
 * scr_color() sets the window color for mainwindow
 */
int scr_color(int colornum)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow) {
#ifdef HAVE_WCOLOR_SET
		wcolor_set(mainwindow, (colornum & 7), NULL);
#else
		wattron(mainwindow, COLOR_PAIR((colornum & 7)));
#endif
		if (colornum & 8) {
			wattron(mainwindow, A_BOLD);
		} else {
			wattroff(mainwindow, A_BOLD);
		}
		return 1;
	}
#endif
	return 0;
}


void scr_flush(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow)
		wrefresh(mainwindow);
	else
#endif
		fflush(stdout);
}


void err_flush(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow)		/* FIXME: error status window needed */
		wrefresh(mainwindow);
	else
#endif
		fflush(stderr);
}


void sln_flush(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (statuswindow)
		wrefresh(statuswindow);
	else
#endif
		fflush(stdout);
}

static volatile int caught_sigwinch = 0;

/*
 * this is not supposed to be called from a signal handler.
 */
int scr_set_windowsize(CtdlIPC* ipc)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow && caught_sigwinch) {
		caught_sigwinch = 0;
#ifdef HAVE_RESIZETERM
		resizeterm(screenheight + 1, screenwidth);
#endif
#ifdef HAVE_WRESIZE
		wresize(mainwindow, screenheight, screenwidth);
		wresize(statuswindow, 1, screenwidth);
#endif
		mvwin(statuswindow, screenheight, 0);
		status_line(ipc->ServInfo.humannode, ipc->ServInfo.site_location,
				room_name, secure, -1);
		wnoutrefresh(mainwindow);
		wnoutrefresh(statuswindow);
		doupdate();
		return 1;
	}
#endif /* HAVE_CURSES_H */
	return 0;
}

/*
 * scr_winch() handles window size changes from SIGWINCH
 * resizes all our windows for us
 */
RETSIGTYPE scr_winch(int signum)
{
	/* if we receive this signal, we must be running
	   in a terminal that supports resizing. */
	have_xterm = 1;
	caught_sigwinch = 1;
	check_screen_dims();
	signal(SIGWINCH, scr_winch);
}


/*
 * Initialize the window(s) we will be using.
 */
void windows_new(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	register int x, y;

	if (myscreen) {
		getmaxyx(stdscr, y, x);
		mainwindow = newwin(y - 1, x, 0, 0);
		screenwidth = x;
		screenheight = y - 1;
		immedok(mainwindow, FALSE);
		leaveok(mainwindow, FALSE);
		scrollok(mainwindow, TRUE);
		statuswindow = newwin(1, x, y - 1, 0);

		if (COLOR_PAIRS > 8)
			wbkgdset(statuswindow, ' ' | COLOR_PAIR(8));
		else
			wbkgdset(statuswindow, ' ' | COLOR_PAIR(DIM_WHITE));

		werase(statuswindow);
		immedok(statuswindow, FALSE);
		leaveok(statuswindow, FALSE);
		scrollok(statuswindow, FALSE);
		wrefresh(statuswindow);
	}
#else /* HAVE_CURSES_H */

#endif /* HAVE_CURSES_H */
}


/*
 * Deinitialize the window(s) we were using (at exit).
 */
void windows_delete(void)
{
#if defined(HAVE_CURSES_H) && !defined(DISABLE_CURSES)
	if (mainwindow)
		delwin(mainwindow);
	mainwindow = NULL;
	if (statuswindow)
		delwin(statuswindow);
	statuswindow = NULL;
#else /* HAVE_CURSES_H */

#endif /* HAVE_CURSES_H */
}
