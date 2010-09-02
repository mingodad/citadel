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
int lines_printed = 0;
int cols_printed = 0;

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
	int b;

	color(COLOR_PUSH);
	color(DIM_RED);
	/* scr_printf("%s\r", ipc->ServInfo.moreprompt); */
	scr_printf("<<more>>\r");	// FIXME use the prompt given by the server
	color(COLOR_POP);
	stty_ctdl(0);
	b=inkey();
	/*
	for (a=0; !IsEmptyStr(&ipc->ServInfo.moreprompt[a]); ++a)
		scr_putc(' ');
	*/
	scr_printf("        ");
	scr_putc(13);
	stty_ctdl(1);
/*
	if ( (rc_prompt_control == 1)
	   || ((rc_prompt_control == 3) && (userflags & US_PROMPTCTL)) ) {
		if (b == 'q' || b == 'Q' || b == 's' || b == 'S')
			b = STOP_KEY;
		if (b == 'n' || b == 'N')
			b = NEXT_KEY;
	}
*/
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

	//	if we want to do a top status line, this is a reasonable way to do it
	//	printf("\033[s\033[0;70H");
	//	printf("\033[K   %d/%d  %d/%d", cols_printed, screenwidth, lines_printed, screenheight);
	//	printf("\033[u");

	if ((screenheight > 0) && (lines_printed > (screenheight-2))) {
		lines_printed = 0;
		hit_any_key();
		lines_printed = 0;
		cols_printed = 0;
	}

	return c;
}


void scr_flush(void)
{
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
	have_xterm = 1;
	caught_sigwinch = 1;
	check_screen_dims();
	signal(SIGWINCH, scr_winch);
}
