/*
 * $Id$
 *
 * First-time setup wizard
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"



/*
 */
void do_setup_wizard(void)
{

	output_headers(1, 1, 2, 0, 1, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<IMG SRC=\"/static/users-icon.gif\" ALT=\" \" ALIGN=MIDDLE>");
	wprintf("<SPAN CLASS=\"titlebar\">&nbsp;First time setup");
	wprintf("</SPAN></TD><TD ALIGN=RIGHT>");
	offer_start_page();
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n"
		"<div id=\"content\">\n");

	wprintf("<div id=\"fix_scrollbar_bug\">");

	wprintf("wow");

	wprintf("</div>\n"
		"<div align=center>"
		"Click on a name to read user info.  Click on "
		"<IMG ALIGN=MIDDLE SRC=\"/static/page.gif\" ALT=\"(p)\" "
		"BORDER=0> to send "
		"a page (instant message) to that user.</div>\n");
	wDumpContent(1);
}


