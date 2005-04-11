/*
 * $Id$
 *
 * Editing of various text files on the Citadel server.
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
 * display the form for editing something (room info, bio, etc)
 */
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner)
{
	char buf[SIZ];

	serv_puts(check_cmd);
	serv_getln(buf, sizeof buf);

	if (buf[0] != '2') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}
	if (with_room_banner) {
		output_headers(1, 1, 1, 0, 0, 0, 0);
	}
	else {
		output_headers(1, 1, 0, 0, 0, 0, 0);
	}

	svprintf("BOXTITLE", WCS_STRING, "Edit %s", description);
	do_template("beginbox");

	wprintf("<CENTER>Enter %s below.  Text is formatted to\n", description);
	wprintf("the <EM>reader's</EM> screen width.  To defeat the\n");
	wprintf("formatting, indent a line at least one space.  \n");
	wprintf("<br />");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"%s\">\n", save_cmd);
	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n");
	serv_puts(read_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1')
		server_to_text();
	wprintf("</TEXTAREA><br /><br />\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save\">");
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><br />\n");

	wprintf("</FORM></CENTER>\n");
	do_template("endbox");
	wDumpContent(1);
}


/*
 * save a screen which was displayed with display_edit()
 */
void save_edit(char *description, char *enter_cmd, int regoto)
{
	char buf[SIZ];

	if (strcmp(bstr("sc"), "Save")) {
		sprintf(WC->ImportantMessage,
			"Cancelled.  %s was not saved.\n", description);
		display_main_menu();
		return;
	}
	serv_puts(enter_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}
	text_to_server(bstr("msgtext"), 0);
	serv_puts("000");

	if (regoto) {
		smart_goto(WC->wc_roomname);
	} else {
		sprintf(WC->ImportantMessage,
			"%s has been saved.\n", description);
		display_main_menu();
		return;
	}
}
