#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

/*
 * display the form for editing something (room info, bio, etc)
 */
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd)
{
	char buf[256];

	serv_puts(check_cmd);
	serv_gets(buf);

	if (buf[0] != '2') {
		display_error(&buf[4]);
		return;
	}
	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Edit ");
	escputs(description);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<CENTER>Enter %s below.  Text is formatted to\n", description);
	wprintf("the <EM>reader's</EM> screen width.  To defeat the\n");
	wprintf("formatting, indent a line at least one space.  \n");
	wprintf("<BR>");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"%s\">\n", save_cmd);
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Save\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");
	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=30 COLS=80 WIDTH=80>");
	serv_puts(read_cmd);
	serv_gets(buf);
	if (buf[0] == '1')
		server_to_text();
	wprintf("</TEXTAREA><P>\n");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);
}


/*
 * save a screen which was displayed with display_edit()
 */
void save_edit(char *description, char *enter_cmd, int regoto)
{
	char buf[256];

	if (strcmp(bstr("sc"), "Save")) {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1);
		wprintf("Cancelled.  %s was not saved.<BR>\n", description);
		wDumpContent(1);
		return;
	}
	serv_puts(enter_cmd);
	serv_gets(buf);
	if (buf[0] != '4') {
		display_error(&buf[4]);
		return;
	}
	text_to_server(bstr("msgtext"));
	serv_puts("000");

	if (regoto) {
		gotoroom(wc_roomname, 1);
	} else {
		printf("HTTP/1.0 200 OK\n");
		output_headers(1);
		wprintf("%s has been saved.\n", description);
		wDumpContent(1);
	}
}
