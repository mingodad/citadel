/*
 * $Id$
 *
 * Functions which implement the chat and paging facilities.
 */

#include "webcit.h"

/*
 * display the form for paging (x-messaging) another user
 */
void display_page(void)
{
	char recp[SIZ];

	strcpy(recp, bstr("recp"));

        output_headers(1, 1, 2, 0, 0, 0);
        wprintf("<div id=\"banner\">\n"
                "<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
                "<SPAN CLASS=\"titlebar\">");
	wprintf(_("Send instant message"));
	wprintf("</SPAN>"
                "</TD></TR></TABLE>\n"
                "</div>\n<div id=\"content\">\n"
        );
                                                                                                                             
        wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf(_("Send an instant message to: "));
	escputs(recp);
	wprintf("<br>\n");

	wprintf("<FORM METHOD=\"POST\" action=\"page_user\">\n");

	wprintf("<TABLE border=0 width=100%%><TR><TD>\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"");
	escputs(recp);
	wprintf("\">\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"closewin\" VALUE=\"");
	escputs(bstr("closewin"));
	wprintf("\">\n");

	wprintf(_("Enter message text:"));
	wprintf("<br />");

	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=5 COLS=40 "
		"WIDTH=40></TEXTAREA>\n");

	wprintf("</TD></TR></TABLE><br />\n");

	wprintf("<INPUT TYPE=\"submit\" NAME=\"send_button\" VALUE=\"%s\">", _("Send message"));
	wprintf("<br /><a href=\"javascript:window.close();\"%s</A>\n", _("Cancel"));

	wprintf("</FORM></CENTER>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/*
 * page another user
 */
void page_user(void)
{
	char recp[SIZ];
	char buf[SIZ];
	char closewin[SIZ];

        output_headers(1, 1, 2, 0, 0, 0);
        wprintf("<div id=\"banner\">\n"
                "<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
                "<SPAN CLASS=\"titlebar\">");
	wprintf(_("Add or edit an event"));
	wprintf("</SPAN>"
                "</TD></TR></TABLE>\n"
                "</div>\n<div id=\"content\">\n"
        );
                                                                                                                             
	strcpy(recp, bstr("recp"));
	strcpy(closewin, bstr("closewin"));

	if (strlen(bstr("send_button")) == 0) {
		wprintf("<EM>");
		wprintf(_("Message was not sent."));
		wprintf("</EM><br />\n");
	} else {
		serv_printf("SEXP %s|-", recp);
		serv_getln(buf, sizeof buf);

		if (buf[0] == '4') {
			text_to_server(bstr("msgtext"), 0);
			serv_puts("000");
			wprintf("<EM>");
			wprintf(_("Message has been sent to "));
			escputs(recp);
			wprintf(".</EM><br />\n");
		}
		else {
			wprintf("<EM>%s</EM><br />\n", &buf[4]);
		}
	}
	
	if (!strcasecmp(closewin, "yes")) {
		wprintf("<CENTER><a href=\"javascript:window.close();\">");
		wprintf(_("[ close window ]"));
		wprintf("</A></CENTER>\n");
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
	serv_getln(buf, sizeof buf);
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

	while (serv_puts("GEXP"), serv_getln(buf, sizeof buf), buf[0]=='1') {

		extract_token(pagefrom, &buf[4], 3, '|', sizeof pagefrom);

		wprintf("<table border=1 bgcolor=\"#880000\"><tr><td>");
		wprintf("<span class=\"titlebar\">");
		wprintf(_("Instant message from "));
		escputs(pagefrom);
		wprintf("</span></td></tr><tr><td><font color=\"#FFFFFF\">");
		fmout("LEFT");
		wprintf("</font></td></tr>"
			"<tr><td><div align=center><font color=\"#FFFFFF\">"
			"<a href=\"javascript:hide_page_popup()\">");
		wprintf(_("[ close window ]"));
		wprintf("</a>"
			"</font></div>"
			"</td></tr>"
			"</table>\n");
	}

	WC->HaveInstantMessages = 0;
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

		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			serv_printf("USER %s", WC->wc_username);
			serv_getln(buf, sizeof buf);
			if (buf[0] == '3') {
				serv_printf("PASS %s", WC->wc_password);
				serv_getln(buf, sizeof buf);
				if (buf[0] == '2') {
					serv_printf("GOTO %s", WC->wc_roomname);
					serv_getln(buf, sizeof buf);
					if (buf[0] == '2') {
						serv_puts("CHAT");
						serv_getln(buf, sizeof buf);
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

	output_headers(0, 0, 0, 0, 0, 0);

	wprintf("Content-type: text/html; charset=utf-8\n");
	wprintf("\n");
	wprintf("<html>\n"
		"<head>\n"
		"<meta http-equiv=\"refresh\" content=\"3\" />\n"
		"</head>\n"

		"<body bgcolor=\"#FFFFFF\">\n"
	);

	if (setup_chat_socket() != 0) {
		wprintf(_("An error occurred while setting up the chat socket."));
		wprintf("</BODY></HTML>\n");
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
	
			serv_getln(buf, sizeof buf);

			if (!strcmp(buf, "000")) {
				strcpy(buf, ":|");
				strcat(buf, _("Now exiting chat mode."));
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
		wprintf("<img src=\"static/blank.gif\" onLoad=\"parent.window.close();\">\n");
	}

	if (strlen(output_data) > 0) {

		if (output_data[strlen(output_data)-1] == '\n') {
			output_data[strlen(output_data)-1] = 0;
		}

		/* Output our fun to the other frame. */
		wprintf("<img src=\"static/blank.gif\" WIDTH=1 HEIGHT=1\n"
			"onLoad=\" \n"
		);

		for (i=0; i<num_tokens(output_data, '\n'); ++i) {
			extract_token(buf, output_data, i, '\n', sizeof buf);
			extract_token(cl_user, buf, 0, '|', sizeof cl_user);
			extract_token(cl_text, buf, 1, '|', sizeof cl_text);

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

	output_headers(0, 0, 0, 0, 0, 0);
	wprintf("Content-type: text/html; charset=utf-8\n");
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

	if (strlen(bstr("help_button")) > 0) {
		strcpy(send_this, "/help");
	}

	if (strlen(bstr("list_button")) > 0) {
		strcpy(send_this, "/who");
	}

	if (strlen(bstr("exit_button")) > 0) {
		strcpy(send_this, "/quit");
	}

	if (setup_chat_socket() != 0) {
		wprintf(_("An error occurred while setting up the chat socket."));
		wprintf("</BODY></HTML>\n");
		wDumpContent(0);
		return;
	}

	/* Temporarily swap the serv and chat sockets during chat talk */
	i = WC->serv_sock;
	WC->serv_sock = WC->chat_sock;
	WC->chat_sock = i;

	while (strlen(send_this) > 0) {
		if (strlen(send_this) < 67) {
			serv_puts(send_this);
			strcpy(send_this, "");
		}
		else {
			for (i=55; i<67; ++i) {
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

	wprintf("<FORM METHOD=\"POST\" action=\"chat_send\" NAME=\"chatsendform\">\n");
	wprintf("<INPUT TYPE=\"text\" SIZE=\"80\" MAXLENGTH=\"%d\" "
		"NAME=\"send_this\">\n", SIZ-10);
	wprintf("<br />");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"send_button\" VALUE=\"%s\">\n", _("Send"));
	wprintf("<INPUT TYPE=\"submit\" NAME=\"help_button\" VALUE=\"%s\">\n", _("Help"));
	wprintf("<INPUT TYPE=\"submit\" NAME=\"list_button\" VALUE=\"%s\">\n", _("List users"));
	wprintf("<INPUT TYPE=\"submit\" NAME=\"exit_button\" VALUE=\"%s\">\n", _("Exit"));
	wprintf("</FORM>\n");

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
}


