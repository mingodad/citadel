/* $Id$ */

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
 * display the form for paging (x-messaging) another user
 */
void display_page(void)
{
	char recp[SIZ];

	strcpy(recp, bstr("recp"));

	output_headers(3);

	svprintf("BOXTITLE", WCS_STRING, "Page: %s", recp);
	do_template("beginbox");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/page_user\">\n");

	wprintf("<TABLE border=0 width=100%%><TR><TD>\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"");
	escputs(recp);
	wprintf("\">\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"closewin\" VALUE=\"");
	escputs(bstr("closewin"));
	wprintf("\">\n");

	wprintf("Enter message text:<BR>");

	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=5 COLS=40 "
		"WIDTH=40></TEXTAREA>\n");

	wprintf("</TD></TR></TABLE><BR>\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"sc\" VALUE=\"Send message\">");
	wprintf("<BR><A HREF=\"javascript:window.close();\"Cancel</A>\n");

	wprintf("</FORM></CENTER>\n");
	do_template("endbox");
	wDumpContent(1);
}

/*
 * page another user
 */
void page_user(void)
{
	char recp[SIZ];
	char sc[SIZ];
	char buf[SIZ];
	char closewin[SIZ];

	output_headers(3);

	strcpy(recp, bstr("recp"));
	strcpy(sc, bstr("sc"));
	strcpy(closewin, bstr("closewin"));

	if (strcmp(sc, "Send message")) {
		wprintf("<EM>Message was not sent.</EM><BR>\n");
	} else {
		serv_printf("SEXP %s|-", recp);
		serv_gets(buf);

		if (buf[0] == '4') {
			text_to_server(bstr("msgtext"), 0);
			serv_puts("000");
			wprintf("<EM>Message has been sent to ");
			escputs(recp);
			wprintf(".</EM><BR>\n");
		}
		else {
			wprintf("<EM>%s</EM><BR>\n", &buf[4]);
		}
	}
	
	if (!strcasecmp(closewin, "yes")) {
		wprintf("<CENTER><A HREF=\"javascript:window.close();\">"
			"[ close window ]</A></CENTER>\n");
	}

	wDumpContent(1);
}



/*
 * multiuser chat
 */
void do_chat(void)
{

	output_headers(1);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#000077\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Real-time chat</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");

	if (!strcasecmp(ctdlhost, "uds")) {
		wprintf("<I>Sorry ... chat is not available here.</i></BR>\n");
	}
	else {
		wprintf("A chat window should be appearing on your screen ");
		wprintf("momentarily.  When you're ");
		wprintf("done, type <TT>/quit</TT> to exit.  You can also ");
		wprintf("type <TT>/help</TT> for more commands.\n");
	
		wprintf("<applet codebase=\"/static\" ");
		wprintf("code=\"wcchat\" width=2 height=2>\n");
		wprintf("<PARAM NAME=username VALUE=\"%s\">\n", WC->wc_username);
		wprintf("<PARAM NAME=password VALUE=\"%s\">\n", WC->wc_password);
		wprintf("<PARAM NAME=roomname VALUE=\"%s\">\n", WC->wc_roomname);
		wprintf("<H2>Oops!</H2>Looks like your browser doesn't support Java, ");
		wprintf("so you won't be able to access Chat.  Sorry.\n");
		wprintf("</applet>\n");
	}
	wDumpContent(1);
}


/*
 *
 */
void page_popup(void)
{
	char buf[SIZ];
	char pagefrom[SIZ];

	/* suppress express message check, do headers but no fake frames */
	output_headers(0x08 | 0x03);

	while (serv_puts("GEXP"), serv_gets(buf), buf[0]=='1') {

		extract(pagefrom, &buf[4], 3);

		wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>");
		wprintf("<SPAN CLASS=\"titlebar\">Express message from ");
		escputs(pagefrom);
		wprintf("</SPAN></TD></TR></TABLE>\n");
		
		fmout(NULL);
	}

	wprintf("<CENTER>");
	wprintf("<A HREF=\"/display_page&closewin=yes&recp=");
	urlescputs(pagefrom);
        wprintf("\">[ reply ]</A>&nbsp;&nbsp;&nbsp;\n");

	wprintf("<A HREF=\"javascript:window.close();\">"
		"[ close window ]</A></B>\n"
		"</CENTER>");

	wDumpContent(1);
	WC->HaveExpressMessages = 0;
}


