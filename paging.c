/* $Id$ */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

/*
 * display the form for paging (x-messaging) another user
 */
void display_page(void)
{
	char buf[256];
	char user[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Page another user</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("This command sends a near-real-time message to any currently\n");
	wprintf("logged in user.<BR><BR>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/page_user\">\n");

	wprintf("Select a user to send a message to: <BR>");

	wprintf("<SELECT NAME=\"recp\" SIZE=10>\n");
	serv_puts("RWHO");
	serv_gets(buf);
	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			extract(user, buf, 1);
			wprintf("<OPTION>");
			escputs(user);
			wprintf("\n");
		}
	}
	wprintf("</SELECT>\n");
	wprintf("<BR>\n");

	wprintf("Enter message text:<BR>");
	wprintf("<INPUT TYPE=\"text\" NAME=\"msgtext\" MAXLENGTH=80 SIZE=80>\n");
	wprintf("<BR><BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Send message\">");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Cancel\"><BR>\n");

	wprintf("</FORM></CENTER>\n");
	wDumpContent(1);
}

/*
 * page another user
 */
void page_user(void)
{
	char recp[256];
	char msgtext[256];
	char sc[256];
	char buf[256];

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	strcpy(recp, bstr("recp"));
	strcpy(msgtext, bstr("msgtext"));
	strcpy(sc, bstr("sc"));

	if (strcmp(sc, "Send message")) {
		wprintf("<EM>Message was not sent.</EM><BR>\n");
	} else {
		serv_printf("SEXP %s|%s", recp, msgtext);
		serv_gets(buf);

		if (buf[0] == '2') {
			wprintf("<EM>Message has been sent to ");
			escputs(recp);
			wprintf(".</EM><BR>\n");
		} else {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		}
	}
	wDumpContent(1);
}



/*
 * multiuser chat
 */
void do_chat(void)
{

	printf("HTTP/1.0 200 OK\n");
	output_headers(1);

	wprintf("<TABLE WIDTH=100% BORDER=0 BGCOLOR=000077><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"");
	wprintf("<B>Real-time chat</B>\n");
	wprintf("</FONT></TD></TR></TABLE>\n");

	wprintf("A chat window should be appearing on your screen ");
	wprintf("momentarily.  When you're ");
	wprintf("done, type <TT>/quit</TT> to exit.  You can also ");
	wprintf("type <TT>/help</TT> for more commands.\n");

	wprintf("<applet codebase=\"/static\" ");
	wprintf("code=\"wcchat\" width=2 height=2>\n");
	wprintf("<PARAM NAME=username VALUE=\"%s\">\n", wc_username);
	wprintf("<PARAM NAME=password VALUE=\"%s\">\n", wc_password);
	wprintf("<H2>Oops!</H2>Looks like your browser doesn't support Java, ");
	wprintf("so you won't be able to access Chat.  Sorry.\n");
	wprintf("</applet>\n");
	wDumpContent(1);
}
