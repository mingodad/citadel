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
	char buf[SIZ];

	/* First, check to make sure we're still allowed in this room. */
	serv_printf("GOTO %s", WC->wc_roomname);
	serv_gets(buf);
	if (buf[0] != '2') {
		smart_goto("_BASEROOM_");
		return;
	}

	/* If the chat socket is still open from a previous chat,
	 * close it -- because it might be stale or in the wrong room.
	 */
	if (WC->chat_sock < 0) {
		close(WC->chat_sock);
		WC->chat_sock = (-1);
	}

	/* WebCit Chat works by having transmit, receive, and refresh
	 * frames.  Load the frameset.
	 */
	do_template("chatframeset");
	return;
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

		wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
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
 * Receiving side of the chat window.  This is implemented in a
 * tiny hidden IFRAME that just does JavaScript writes to
 * other frames whenever it refreshes and finds new data.
 */
void chat_recv(void) {
	int i;
	struct pollfd pf;
	int got_data = 0;
	int end_chat_now = 0;
	char buf[SIZ];
	char cl_user[SIZ];
	char cl_text[SIZ];
	char *output_data = NULL;

	output_headers(0);

	wprintf("Content-type: text/html\n");
	wprintf("\n");
	wprintf("<HTML>\n"
		"<HEAD>\n"
		"<META HTTP-EQUIV=\"refresh\" CONTENT=\"3\">\n"
		"</HEAD>\n"

		"<BODY BGCOLOR=\"#FFFFFF\">\n"
	);

	if (setup_chat_socket() != 0) {
		wprintf("Error setting up chat socket</BODY></HTML>\n");
		wDumpContent(0);
		return;
	}

	/*
	 * See if there is any chat data waiting.
	 */
	output_data = strdup("");
	do {
		got_data = 0;
		pf.fd = WC->chat_sock;
		pf.events = POLLIN;
		pf.revents = 0;
		if (poll(&pf, 1, 1) > 0) if (pf.revents & POLLIN) {
			++got_data;

			/* Temporarily swap the serv and chat sockets during chat talk */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;
	
			serv_gets(buf);

			if (!strcmp(buf, "000")) {
				strcpy(buf, ":|exiting chat mode");
				end_chat_now = 1;
			}
			
			/* Unswap the sockets. */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;

			/* Append our output data */
			output_data = realloc(output_data, strlen(output_data) + strlen(buf) + 4);
			strcat(output_data, buf);
			strcat(output_data, "\n");
		}

	} while ( (got_data) && (!end_chat_now) );

	if (end_chat_now) {
		close(WC->chat_sock);
		WC->chat_sock = (-1);
		wprintf("<IMG SRC=\"/static/blank.gif\" onLoad=\"parent.window.close();\">\n");
	}

	if (strlen(output_data) > 0) {

		if (output_data[strlen(output_data)-1] == '\n') {
			output_data[strlen(output_data)-1] = 0;
		}

		/* Output our fun to the other frame. */
		wprintf("<IMG SRC=\"/static/blank.gif\" WIDTH=1 HEIGHT=1\n"
			"onLoad=\" \n"
		);

		for (i=0; i<num_tokens(output_data, '\n'); ++i) {
			extract_token(buf, output_data, i, '\n');
			extract_token(cl_user, buf, 0, '|');
			extract_token(cl_text, buf, 1, '|');

			if (strcasecmp(cl_text, "NOOP")) {

				wprintf("parent.chat_transcript.document.write('");
	
				if (strcasecmp(cl_user, WC->last_chat_user)) {
					wprintf("<TABLE border=0 WIDTH=100%% "
						"CELLSPACING=1 CELLPADDING=0 "
						"BGCOLOR=&quot;#FFFFFF&quot;>"
						"<TR><TD></TR></TD></TABLE>"
					);
	
				}

				wprintf("<TABLE border=0 WIDTH=100%% "
					"CELLSPACING=0 CELLPADDING=0 "
					"BGCOLOR=&quot;#EEEEEE&quot;>");
	
				wprintf("<TR><TD>");
	
				if (!strcasecmp(cl_user, ":")) {
					wprintf("<I>");
				}

				if (strcasecmp(cl_user, WC->last_chat_user)) {
					wprintf("<B>");
	
					if (!strcasecmp(cl_user, WC->wc_username)) {
						wprintf("<FONT COLOR=&quot;#FF0000&quot;>");
					}
					else {
						wprintf("<FONT COLOR=&quot;#0000FF&quot;>");
					}
					jsescputs(cl_user);
	
					wprintf("</FONT>: </B>");
				}
				else {
					wprintf("&nbsp;&nbsp;&nbsp;");
				}
				jsescputs(cl_text);
				if (!strcasecmp(cl_user, ":")) {
					wprintf("</I>");
				}

				wprintf("</TD></TR></TABLE>");
				wprintf("'); \n");

				strcpy(WC->last_chat_user, cl_user);
			}
		}

		wprintf("parent.chat_transcript.scrollTo(999999,999999);\">\n");
	}

	free(output_data);

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
}


/*
 * sending side of the chat window
 */
void chat_send(void) {
	int i;
	char send_this[SIZ];
	char buf[SIZ];

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

		if (!strcasecmp(bstr("sendbutton"), "Help")) {
			strcpy(send_this, "/help");
		}

		if (!strcasecmp(bstr("sendbutton"), "List Users")) {
			strcpy(send_this, "/who");
		}

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

		while (strlen(send_this) > 0) {
			if (strlen(send_this) < 72) {
				serv_puts(send_this);
				strcpy(send_this, "");
			}
			else {
				for (i=60; i<72; ++i) {
					if (send_this[i] == ' ') break;
				}
				strncpy(buf, send_this, i);
				buf[i] = 0;
				strcpy(send_this, &send_this[i]);
				serv_puts(buf);
			}
		}

		/* Unswap the sockets. */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

	}

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/chat_send\" NAME=\"chatsendform\">\n");
	wprintf("<INPUT TYPE=\"text\" SIZE=\"80\" MAXLENGTH=\"%d\" "
		"NAME=\"send_this\">\n", SIZ-10);
	wprintf("<BR>");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"Send\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"Help\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"List Users\">\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"sendbutton\" VALUE=\"Exit\">\n");
	wprintf("</FORM>\n");

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
}


