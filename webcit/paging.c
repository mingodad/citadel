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
void display_page(void) {
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
	if (buf[0]=='1') {
		while(serv_gets(buf), strcmp(buf,"000")) {
			extract(user,buf,1);
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
        wprintf("</BODY></HTML>\n");
        wDumpContent();
	}

/*
 * page another user
*/
void page_user(void) {
	char recp[256];
	char msgtext[256];
	char sc[256];
	char buf[256];
	
        printf("HTTP/1.0 200 OK\n");
        output_headers(1);

	strcpy(recp,bstr("recp"));
	strcpy(msgtext,bstr("msgtext"));
	strcpy(sc,bstr("sc"));

	if (strcmp(sc,"Send message")) {
		wprintf("<EM>Message was not sent.</EM><BR>\n");
		return;
		}

	serv_printf("SEXP %s|%s",recp,msgtext);
	serv_gets(buf);	

	if (buf[0] == '2') {
		wprintf("<EM>Message has been sent to ");
		escputs(recp);
		wprintf(".</EM><BR>\n");
		}
	else {
		wprintf("<EM>%s</EM><BR>\n",&buf[4]);
		}

        wprintf("</BODY></HTML>\n");
        wDumpContent();
	}
