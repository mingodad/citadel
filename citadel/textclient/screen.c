/*
 * Screen output handling
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/ioctl.h>
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

int enable_status_line = 0;	/* FIXME the status line works, but not on Mac.  Make this configurable. */
char status_line[1024] = "     ";

/* the default paginator prompt will be replaced by the server's prompt when we learn it */
char *moreprompt = " -- more -- ";

int screenheight = 24;
int screenwidth = 80;
int lines_printed = 0;
int cols_printed = 0;

extern int rc_ansi_color;
extern int rc_prompt_control;
void do_keepalive(void);

/*
 * Attempt to discover the screen dimensions. 
 * WARNING: This is sometimes called from a signal handler.
 */
void check_screen_dims(void)
{
#ifdef TIOCGWINSZ
	struct {
		unsigned short height;	/* rows */
		unsigned short width;	/* columns */
		unsigned short xpixels;
		unsigned short ypixels;	/* pixels */
	} xwinsz;

	if (ioctl(0, TIOCGWINSZ, &xwinsz) == 0) {
		if (xwinsz.height)
			screenheight = (int) xwinsz.height;
		if (xwinsz.width)
			screenwidth = (int) xwinsz.width;
	}
#endif
}


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
 * Beep.
 */
void ctdl_beep(void) {
	putc(7, stdout);
}
	



/*
 * scr_printf() outputs to the terminal
 */
int scr_printf(char *fmt, ...)
{
	static char outbuf[4096];	/* static for performance -- not re-entrant -- change if needed */
	va_list ap;
	register int retval;
	int i, len;

	va_start(ap, fmt);
	retval = vsnprintf(outbuf, sizeof outbuf, fmt, ap);
	va_end(ap);

	len = strlen(outbuf);
	for (i=0; i<len; ++i) {
		scr_putc(outbuf[i]);
	}
	return retval;
}


/*
 * Read one character from the terminal
 */
int scr_getc(int delay)
{
	unsigned char buf;

	scr_flush();

	buf = '\0';
	if (!read (0, &buf, 1))
		logoff(NULL, 3);

	lines_printed = 0;
	return buf;
}

/*
 * Issue the paginator prompt (more / hit any key to continue)
 */
void hit_any_key(void) {
	int a, b;

	color(COLOR_PUSH);
	color(DIM_RED);
	scr_printf("%s\r", moreprompt);
	color(COLOR_POP);
	b=inkey();
	for (a=0; a<screenwidth; ++a) {
		scr_putc(' ');
	}
	scr_printf("\r");

	if ( (rc_prompt_control == 1) || ((rc_prompt_control == 3) && (userflags & US_PROMPTCTL)) ) {
		if (b == 'q' || b == 'Q' || b == 's' || b == 'S') {
			b = STOP_KEY;
		}
		if (b == 'n' || b == 'N') {
			b = NEXT_KEY;
		}
	}

	if (b==NEXT_KEY) sigcaught = SIGINT;
	if (b==STOP_KEY) sigcaught = SIGQUIT;
}


/*
 * Output one character to the terminal
 */
int scr_putc(int c)
{
	/* handle tabs normally */
	if (c == '\t') {
		do {
			scr_putc(' ');
		} while ((cols_printed % 8) != 0);
		return(c);
	}

	/* Output the character... */
	if (putc(c, stdout) == EOF) {
		logoff(NULL, 3);
	}

	if (c == '\n') {
		++lines_printed;
		cols_printed = 0;
	}
	else if (c == '\r') {
		cols_printed = 0;
	}
	else if (isprint(c)) {
		++cols_printed;
		if ((screenwidth > 0) && (cols_printed > screenwidth)) {
			++lines_printed;
			cols_printed = 0;
		}
	}

	/* How many lines output before stopping for the paginator?
	 * Depends on whether we are displaying a status line.
	 */
	int height_offset = ( ((enable_color) && (screenwidth > 0) && (enable_status_line)) ? (3) : (2) ) ;

	/* Ok, go check it.  Stop and display the paginator prompt if necessary. */
	if ((screenheight > 0) && (lines_printed > (screenheight-height_offset))) {
		lines_printed = 0;
		hit_any_key();
		lines_printed = 0;
		cols_printed = 0;
	}

	return c;
}

void scr_flush(void)
{
	if ((enable_color) && (screenwidth > 0) && (enable_status_line)) {
		if (strlen(status_line) < screenwidth) {
			memset(&status_line[strlen(status_line)], 32, screenwidth - strlen(status_line));
		}
		printf("\033[s\033[1;1H\033[K\033[7m");
		fwrite(status_line, screenwidth, 1, stdout);
		printf("\033[27m\033[u");
	}
	fflush(stdout);
}


static volatile int caught_sigwinch = 0;


/*
 * scr_winch() handles window size changes from SIGWINCH
 * resizes all our windows for us
 */
RETSIGTYPE scr_winch(int signum)
{
	/* if we receive this signal, we must be running
	 * in a terminal that supports resizing.
	 */
	caught_sigwinch = 1;
	check_screen_dims();
	signal(SIGWINCH, scr_winch);
}



/*
 * Display a 3270-style "wait" indicator at the bottom of the screen
 */
void scr_wait_indicator(int state) {
	int sp = (screenwidth - 2);

	if (!enable_status_line) return;

	if (screenwidth > 0) {
		switch (state) {
			default:
			case 0:	 /* Idle */
				status_line[sp] = ' ';
				break;
			case 1:	 /* Waiting */
				status_line[sp] = 'X';
				break;
			case 2:	 /* Receiving */
				status_line[sp] = '<';
				break;
			case 3:	 /* Sending */
				status_line[sp] = '>';
				break;
		}
		scr_flush();
	}
}

