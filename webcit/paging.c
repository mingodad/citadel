/*
 * $Id$
 *
 * Functions which implement the chat and paging facilities.
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
#include <sys/poll.h>
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

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#000077\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">Real-time chat</SPAN>\n"
		"</TD></TR></TABLE>\n"
		"<IFRAME WIDTH=100%% HEIGHT=200 SRC=\"/chat_recv\" "
		"NAME=\"chat_recv\">\n"
		"<!-- Alternate content for non-supporting browsers -->\n"
		"If you are seeing this message, your browser does not contain\n"
		"the IFRAME support required for the chat window.  Please upgrade\n"
		"to a supported browser, such as\n"
		"<A HREF=\"http://www.mozilla.org\">Mozilla</A>.\n"
		"</IFRAME>\n"
		"<HR width=100%%>\n"
		"<IFRAME WIDTH=100%% HEIGHT=50 SRC=\"/chat_send\" "
		"NAME=\"chat_send\">\n"
		"</IFRAME>\n"
	);
	wDumpContent(1);
}


/*
 *
 */
void page_popup(void)
{
	char buf[SIZ];
	char pagefrom[SIZ];

	/* suppress express message check, do headers but no frames */
	output_headers(0x08 | 0x03);

	while (serv_puts("GEXP"), serv_gets(buf), buf[0]=='1') {

		extract(pagefrom, &buf[4], 3);

		wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>");
		wprintf("<SPAN CLASS=\"titlebar\">Instant message from ");
		escputs(pagefrom);
		wprintf("</SPAN></TD></TR></TABLE>\n");
		
		fmout(NULL, "LEFT");
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



/*
 * Support function for chat -- make sure the chat socket is connected
 * and in chat mode.
 */
int setup_chat_socket(void) {
	char buf[SIZ];
	int i;
	int good_chatmode = 0;

	if (WC->chat_sock < 0) {

		for (i=0; i<CHATLINES; ++i) {
			strcpy(WC->chatlines[i], "");
		}

		if (!strcasecmp(ctdlhost, "uds")) {
			/* unix domain socket */
			sprintf(buf, "%s/citadel.socket", ctdlport);
			WC->chat_sock = uds_connectsock(buf);
		}
		else {
			/* tcp socket */
			WC->chat_sock = tcp_connectsock(ctdlhost, ctdlport);
		}

		if (WC->chat_sock < 0) {
			return(errno);
		}

		/* Temporarily swap the serv and chat sockets during chat talk */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

		serv_gets(buf);
		if (buf[0] == '2') {
			serv_printf("USER %s", WC->wc_username);
			serv_gets(buf);
			if (buf[0] == '3') {
				serv_printf("PASS %s", WC->wc_password);
				serv_gets(buf);
				if (buf[0] == '2') {
					serv_printf("GOTO %s", WC->wc_roomname);
					serv_gets(buf);
					if (buf[0] == '2') {
						serv_puts("CHAT");
						serv_gets(buf);
						if (buf[0] == '8') {
							good_chatmode = 1;
						}
					}
				}
			}
		}

		/* Unswap the sockets. */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

		if (!good_chatmode) close(WC->serv_sock);

	}
	return(0);
}



/*
 * receiving side of the chat window
 */
void chat_recv(void) {
	int i;
	char name[SIZ];
	char text[SIZ];
	struct pollfd pf;
	int got_data = 0;
	int end_chat_now = 0;

	output_headers(0);

	wprintf("Content-type: text/html\n");
	wprintf("\n");
	wprintf("<HTML>\n"
		"<HEAD>\n"
		"<META HTTP-EQUIV=\"refresh\" CONTENT=\"3\">\n"
		"</HEAD>\n"
		"<BODY BGCOLOR=\"#FFFFFF\">"
	);

	if (setup_chat_socket() != 0) {
		wprintf("Error setting up chat socket</BODY></HTML>\n");
		wDumpContent(0);
		return;
	}

	/*
	 * See if there is any chat data waiting.
	 */
	do {
		got_data = 0;
		pf.fd = WC->chat_sock;
		pf.events = POLLIN;
		pf.revents = 0;
		if (poll(&pf, 1, 1) > 0) if (pf.revents & POLLIN) {
			++got_data;

			for (i=0; i<CHATLINES-1; ++i) {
				strcpy(WC->chatlines[i], WC->chatlines[i+1]);
			}
	
			/* Temporarily swap the serv and chat sockets during chat talk */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;
	
			serv_gets(WC->chatlines[CHATLINES-1]);
			if (!strcmp(WC->chatlines[CHATLINES-1], "000")) {
				end_chat_now = 1;
				strcpy(WC->chatlines[CHATLINES-1], ":|exiting chat mode");
			}
			
			/* Unswap the sockets. */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;
		}
	} while ( (got_data) && (!end_chat_now) );

	/*
	 * Display appropriately.
	 */
	for (i=0; i<CHATLINES; ++i) {
		if (strlen(WC->chatlines[i]) > 0) {
			extract(name, WC->chatlines[i], 0);
			extract(text, WC->chatlines[i], 1);
			if (!strcasecmp(name, WC->wc_username)) {
				wprintf("<FONT COLOR=\"#004400\">");
			}
			else if (!strcmp(name, ":")) {
				wprintf("<FONT COLOR=\"#440000\">");
			}
			else {
				wprintf("<FONT COLOR=\"#000044\">");
			}
			escputs(name);
			wprintf(": </FONT>");
			escputs(text);
			wprintf("<BR>\n");
		}
	}

	if (end_chat_now) {
		close(WC->chat_sock);
		WC->chat_sock = (-1);
		wprintf("<IMG SRC=\"/static/blank.gif\" onLoad=\"top.location.replace('/do_welcome');\">\n");
	}

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
}


/*
 * sending side of the chat window
 */
void chat_send(void) {
	int i;
	char send_this[SIZ];

	output_headers(0);
	wprintf("Content-type: text/html\n");
	wprintf("\n");
	wprintf("<HTML>"
		"<BODY onLoad=\"document.chatsendform.send_this.focus();\" >"
	);

	if (bstr("send_this") != NULL) {
		strcpy(send_this, bstr("send_this"));
	}
	else {
		strcpy(send_this, "");
	}

	if (bstr("sendbutton") != NULL) {

		if (!strcasecmp(bstr("sendbutton"), "Exit")) {
			strcpy(send_this, "/quit");
		}

		if (setup_chat_socket() != 0) {
			wprintf("Error setting up chat socket</BODY></HTML>\n");
			wDumpContent(0);
			return;
		}

		/* Temporarily swap the serv and chat sockets during chat talk */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

		serv_puts(send_this);

		/* Unswap the sockets. */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

	}

	wprintf("Send: ");
	wprintf("<FORM METHOD=\"POST\" ACTION=\"/chat_send\" NAME=\"chatsendform\">\n");
	wprintf("<INPUT TYPE=\"text\" SIZE=\"80\" MAXLENGTH=\"80\" NAME=\"send_this\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"Send\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"Exit\">\n");
	wprintf("</FORM>\n");

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
}


