/* $Id$ */

/*
 * Handle full-screen curses stuff
 */

#include "sysdep.h"
#ifdef HAVE_CURSES_H
#include <curses.h>
#endif
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <sys/types.h>
#ifdef VW_PRINTW_IN_CURSES
#define _vwprintw vw_printw
#else
/* Ancient curses implementations, this needs testing. Anybody got XENIX? */
#define _vwprintw vwprintw
#endif
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "citadel.h"
#include "commands.h"
#include "screen.h"

#ifdef HAVE_CURSES_H
static SCREEN *myscreen = NULL;
static WINDOW *mainwindow = NULL;
static WINDOW *statuswindow = NULL;

char rc_screen;
char arg_screen;

extern int screenheight;
extern int screenwidth;
extern int rc_ansi_color;
extern void check_screen_dims(void);
#endif


/*
 * status_line() is a convenience function for writing a "typical"
 * status line to the window.
 */
void status_line(const char *humannode, const char *bbs_city,
		 const char *room_name, int secure, int newmailcount)
{
#ifdef HAVE_CURSES_H
	if (statuswindow) {
		if (secure)
			sln_printf("Secure ");
		else
			sln_printf("Not secure ");
		waddch(statuswindow, ACS_VLINE);
		waddch(statuswindow, ' ');
		if (humannode && bbs_city)
			sln_printf("%s at %s ", humannode, bbs_city);
		if (room_name)
			sln_printf("in %s ", room_name);
		if (newmailcount > -1) {
			waddch(statuswindow, ACS_VLINE);
			sln_printf(" %d unread mail", newmailcount);
		}
		sln_printf("\n");
	}
#endif /* HAVE_CURSES_H */
}


/*
 * Initialize the screen.  If newterm() fails, myscreen will be NULL and
 * further handlers will assume we should be in line mode.
 */
void screen_new(void)
{
#ifdef HAVE_CURSES_H
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
		init_pair(1+DIM_BLACK, COLOR_BLACK, COLOR_BLACK);
		init_pair(1+DIM_RED, COLOR_RED, COLOR_BLACK);
		init_pair(1+DIM_GREEN, COLOR_GREEN, COLOR_BLACK);
		init_pair(1+DIM_YELLOW, COLOR_YELLOW, COLOR_BLACK);
		init_pair(1+DIM_BLUE, COLOR_BLUE, COLOR_BLACK);
		init_pair(1+DIM_MAGENTA, COLOR_MAGENTA, COLOR_BLACK);
		init_pair(1+DIM_CYAN, COLOR_CYAN, COLOR_BLACK);
		init_pair(1+DIM_WHITE, COLOR_WHITE, COLOR_BLACK);
		init_pair(17, COLOR_WHITE, COLOR_BLUE);
	} else
#endif /* HAVE_CURSES_H */
	{
		send_ansi_detect();
		look_for_ansi();
		cls(0);
		color(1+DIM_WHITE);
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
#ifdef HAVE_CURSES_H
	if (myscreen)
		delscreen(myscreen);
	myscreen = NULL;
#endif
}


/*
 * Set screen/IO parameters, e.g. at start of program or return from external
 * program run.
 */
int screen_set(void)
{
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
	if (myscreen) {
		endwin();
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
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
	static char buf[4096];
#endif

	va_start(ap, fmt);
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
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


int scr_getc(void)
{
#ifdef HAVE_CURSES_H
	if (mainwindow)
		return wgetch(mainwindow);
#endif
	return getchar();
}


/*
 * scr_putc() outputs a single character
 */
int scr_putc(int c)
{
#ifdef HAVE_CURSES_H
	if (mainwindow)
		return ((waddch(mainwindow, c) == OK) ? c : EOF);
#endif
	return putc(c, stdout);
}


int sln_putc(int c)
{
#ifdef HAVE_CURSES_H
	if (statuswindow)
		return ((waddch(statuswindow, c) == OK) ? c : EOF);
#endif
	return putc(c, stdout);
}


int sln_putc_if(int c)
{
#ifdef HAVE_CURSES_H
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
#ifdef HAVE_CURSES_H
	if (mainwindow) {
		wcolor_set(mainwindow, 1 + (colornum & 7), NULL);
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
#ifdef HAVE_CURSES_H
	if (mainwindow)
		wrefresh(mainwindow);
	else
#endif
		fflush(stdout);
}


void err_flush(void)
{
#ifdef HAVE_CURSES_H
	if (mainwindow)		/* FIXME: error status window needed */
		wrefresh(mainwindow);
	else
#endif
		fflush(stderr);
}


void sln_flush(void)
{
#ifdef HAVE_CURSES_H
	if (statuswindow)
		wrefresh(statuswindow);
	else
#endif
		fflush(stdout);
}


int scr_set_windowsize(void)
{
#ifdef HAVE_CURSES_H
	int y, x;

	if (mainwindow) {
		getmaxyx(mainwindow, y, x);
		screenheight = y;
		screenwidth = x;
		return 1;
	}
#endif /* HAVE_CURSES_H */
	return 0;
}


/*
 * scr_winch() handles window size changes from SIGWINCH
 * resizes all our windows for us
 */
RETSIGTYPE scr_winch(void)
{
#ifdef HAVE_CURSES_H
	/* FIXME: not implemented */
#endif
	check_screen_dims();
}


/*
 * Initialize the window(s) we will be using.
 */
void windows_new(void)
{
#ifdef HAVE_CURSES_H
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
		wbkgdset(statuswindow, COLOR_PAIR(17));
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
#ifdef HAVE_CURSES_H
	if (mainwindow)
		delwin(mainwindow);
	mainwindow = NULL;
	if (statuswindow)
		delwin(statuswindow);
	statuswindow = NULL;
#else /* HAVE_CURSES_H */

#endif /* HAVE_CURSES_H */
}
