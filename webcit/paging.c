/*
 * $Id$
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
        wc_printf("<div id=\"banner\">\n");
        wc_printf("<h1>");
	wc_printf(_("Send instant message"));
	wc_printf("</h1>");
        wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

        wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<table class=\"paging_background\"><tr><td>\n");

	wc_printf(_("Send an instant message to: "));
	escputs(recp);
	wc_printf("<br>\n");

	wc_printf("<FORM METHOD=\"POST\" action=\"page_user\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<input type=\"hidden\" name=\"template\" value=\"who\">\n");

	wc_printf("<TABLE border=0 width=100%%><TR><TD>\n");

	wc_printf("<INPUT TYPE=\"hidden\" NAME=\"recp\" VALUE=\"");
	escputs(recp);
	wc_printf("\">\n");

	wc_printf(_("Enter message text:"));
	wc_printf("<br />");

	wc_printf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=5 COLS=40 "
		"WIDTH=40></TEXTAREA>\n");

	wc_printf("</TD></TR></TABLE><br />\n");

	wc_printf("<INPUT TYPE=\"submit\" NAME=\"send_button\" VALUE=\"%s\">", _("Send message"));
	wc_printf("<br /><a href=\"javascript:window.close();\"%s</A>\n", _("Cancel"));

	wc_printf("</FORM></CENTER>\n");
	wc_printf("</td></tr></table></div>\n");
	wDumpContent(1);
}

/**
 * \brief page another user
 */
void page_user(void)
{
	char recp[256];
	char buf[256];

	safestrncpy(recp, bstr("recp"), sizeof recp);

	if (!havebstr("send_button")) {
		safestrncpy(WC->ImportantMessage,
			_("Message was not sent."),
			sizeof WC->ImportantMessage
		);
	} else {
		serv_printf("SEXP %s|-", recp);
		serv_getln(buf, sizeof buf);

		if (buf[0] == '4') {
			text_to_server(bstr("msgtext"));
			serv_puts("000");
			stresc(buf, 256, recp, 0, 0);
			snprintf(WC->ImportantMessage,
				sizeof WC->ImportantMessage,
				"%s%s.",
				_("Message has been sent to "),
				buf
			);
		}
		else {
			safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		}
	}

	url_do_template();
}



/**
 * \brief multiuser chat
 */
void do_chat(void)
{
	char buf[SIZ];

	/** First, check to make sure we're still allowed in this room. */
	serv_printf("GOTO %s", ChrPtr(WC->wc_roomname));
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		StrBuf *Buf;
		Buf = NewStrBufPlain(HKEY("_BASEROOM_"));
		smart_goto(Buf);
		FreeStrBuf(&Buf);
		return;
	}

	/**
	 * If the chat socket is still open from a previous chat,
	 * close it -- because it might be stale or in the wrong room.
	 */
	if (WC->chat_sock < 0) {
		close(WC->chat_sock);
		WC->chat_sock = (-1);
	}

	/**
	 * WebCit Chat works by having transmit, receive, and refresh
	 * frames.  Load the frameset.  (This isn't AJAX but the headers
	 * output by begin_ajax_response() happen to be the ones we need.)
	 */
	begin_ajax_response();
	do_template("chatframeset", NULL);
	end_ajax_response();
	return;
}


/**
 * \brief display page popup
 * If there are instant messages waiting, and we notice that we haven't checked them in
 * a while, it probably means that we need to open the instant messenger window.
 */
int Conditional_PAGE_WAITING(StrBuf *Target, WCTemplputParams *TP)
{
	int len;
	char buf[SIZ];

	/** JavaScript function to alert the user that popups are probably blocked */
	/** First, do the check as part of our page load. */
	serv_puts("NOOP");
	len = serv_getln(buf, sizeof buf);
	if ((len >= 3) && (buf[3] == '*')) {
		if ((time(NULL) - WC->last_pager_check) > 60) {
			return 1;
		}
	}
	return 0;
	/** Then schedule it to happen again a minute from now if the user is idle. */
}



/**
 * \brief Support function for chat
 * make sure the chat socket is connected
 * and in chat mode.
 */
int setup_chat_socket(void) {
	char buf[SIZ];
	int i;
	int good_chatmode = 0;

	if (WC->chat_sock < 0) {

		if (!strcasecmp(ctdlhost, "uds")) {
			/** unix domain socket */
			sprintf(buf, "%s/citadel.socket", ctdlport);
			WC->chat_sock = uds_connectsock(buf);
		}
		else {
			/** tcp socket */
			WC->chat_sock = tcp_connectsock(ctdlhost, ctdlport);
		}

		if (WC->chat_sock < 0) {
			return(errno);
		}

		/** Temporarily swap the serv and chat sockets during chat talk */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			serv_printf("USER %s", ChrPtr(WC->wc_username));
			serv_getln(buf, sizeof buf);
			if (buf[0] == '3') {
				serv_printf("PASS %s", ChrPtr(WC->wc_password));
				serv_getln(buf, sizeof buf);
				if (buf[0] == '2') {
					serv_printf("GOTO %s", ChrPtr(WC->wc_roomname));
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

		/** Unswap the sockets. */
		i = WC->serv_sock;
		WC->serv_sock = WC->chat_sock;
		WC->chat_sock = i;

		if (!good_chatmode) close(WC->serv_sock);

	}
	return(0);
}



/**
 * \brief Receiving side of the chat window.  
 * This is implemented in a
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

	hprintf("Content-type: text/html; charset=utf-8\r\n");
	begin_burst();
	wc_printf("<html>\n"
		"<head>\n"
		"<meta http-equiv=\"refresh\" content=\"3\" />\n"
		"</head>\n"

		"<body bgcolor=\"#FFFFFF\">\n"
	);

	if (setup_chat_socket() != 0) {
		wc_printf(_("An error occurred while setting up the chat socket."));
		wc_printf("</BODY></HTML>\n");
		wDumpContent(0);
		return;
	}

	/**
	 * See if there is any chat data waiting.
	 */
	output_data = strdup("");
	do {
		got_data = 0;
		pf.fd = WC->chat_sock;
		pf.events = POLLIN;
		pf.revents = 0;
		if ((poll(&pf, 1, 1) > 0) && (pf.revents & POLLIN)) {
			++got_data;

			/** Temporarily swap the serv and chat sockets during chat talk */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;
	
			serv_getln(buf, sizeof buf);

			if (!strcmp(buf, "000")) {
				strcpy(buf, ":|");
				strcat(buf, _("Now exiting chat mode."));
				end_chat_now = 1;
			}
			
			/** Unswap the sockets. */
			i = WC->serv_sock;
			WC->serv_sock = WC->chat_sock;
			WC->chat_sock = i;

			/** Append our output data */
			output_data = realloc(output_data, strlen(output_data) + strlen(buf) + 4);
			strcat(output_data, buf);
			strcat(output_data, "\n");
		}

	} while ( (got_data) && (!end_chat_now) );

	if (end_chat_now) {
		close(WC->chat_sock);
		WC->chat_sock = (-1);
		wc_printf("<img src=\"static/blank.gif\" onLoad=\"parent.window.close();\">\n");
	}

	if (!IsEmptyStr(output_data)) {
		int len;
		len = strlen(output_data);
		if (output_data[len-1] == '\n') {
			output_data[len-1] = 0;
		}

		/** Output our fun to the other frame. */
		wc_printf("<img src=\"static/blank.gif\" WIDTH=1 HEIGHT=1\n"
			"onLoad=\" \n"
		);

		for (i=0; i<num_tokens(output_data, '\n'); ++i) {
			extract_token(buf, output_data, i, '\n', sizeof buf);
			extract_token(cl_user, buf, 0, '|', sizeof cl_user);
			extract_token(cl_text, buf, 1, '|', sizeof cl_text);

			if (strcasecmp(cl_text, "NOOP")) {

				wc_printf("parent.chat_transcript.document.write('");
	
				if (strcasecmp(cl_user, WC->last_chat_user)) {
					wc_printf("<TABLE border=0 WIDTH=100%% "
						"CELLSPACING=1 CELLPADDING=0 "
						"BGCOLOR=&quot;#FFFFFF&quot;>"
						"<TR><TD></TR></TD></TABLE>"
					);
	
				}

				wc_printf("<TABLE border=0 WIDTH=100%% "
					"CELLSPACING=0 CELLPADDING=0 "
					"BGCOLOR=&quot;#EEEEEE&quot;>");
	
				wc_printf("<TR><TD>");
	
				if (!strcasecmp(cl_user, ":")) {
					wc_printf("<I>");
				}

				if (strcasecmp(cl_user, WC->last_chat_user)) {
					wc_printf("<B>");
	
					if (!strcasecmp(cl_user, ChrPtr(WC->wc_fullname))) {
						wc_printf("<FONT COLOR=&quot;#FF0000&quot;>");
					}
					else {
						wc_printf("<FONT COLOR=&quot;#0000FF&quot;>");
					}
					jsescputs(cl_user);
	
					wc_printf("</FONT>: </B>");
				}
				else {
					wc_printf("&nbsp;&nbsp;&nbsp;");
				}
				jsescputs(cl_text);
				if (!strcasecmp(cl_user, ":")) {
					wc_printf("</I>");
				}

				wc_printf("</TD></TR></TABLE>");
				wc_printf("'); \n");

				strcpy(WC->last_chat_user, cl_user);
			}
		}

		wc_printf("parent.chat_transcript.scrollTo(999999,999999);\">\n");
	}

	free(output_data);

	wc_printf("</BODY></HTML>\n");
	wDumpContent(0);
}


/**
 * \brief sending side of the chat window
 */
void chat_send(void) {
	int i;
	char send_this[SIZ];
	char buf[SIZ];

	output_headers(0, 0, 0, 0, 0, 0);
	hprintf("Content-type: text/html; charset=utf-8\r\n");
	begin_burst();
	wc_printf("<HTML>"
		"<BODY onLoad=\"document.chatsendform.send_this.focus();\" >"
	);

	if (havebstr("send_this")) {
		strcpy(send_this, bstr("send_this"));
	}
	else {
		strcpy(send_this, "");
	}

	if (havebstr("help_button")) {
		strcpy(send_this, "/help");
	}

	if (havebstr("list_button")) {
		strcpy(send_this, "/who");
	}

	if (havebstr("exit_button")) {
		strcpy(send_this, "/quit");
	}

	if (setup_chat_socket() != 0) {
		wc_printf(_("An error occurred while setting up the chat socket."));
		wc_printf("</BODY></HTML>\n");
		wDumpContent(0);
		return;
	}

	/** Temporarily swap the serv and chat sockets during chat talk */
	i = WC->serv_sock;
	WC->serv_sock = WC->chat_sock;
	WC->chat_sock = i;

	while (!IsEmptyStr(send_this)) {
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

	/** Unswap the sockets. */
	i = WC->serv_sock;
	WC->serv_sock = WC->chat_sock;
	WC->chat_sock = i;

	wc_printf("<FORM METHOD=\"POST\" action=\"chat_send\" NAME=\"chatsendform\">\n");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<INPUT TYPE=\"text\" SIZE=\"80\" MAXLENGTH=\"%d\" "
		"NAME=\"send_this\">\n", SIZ-10);
	wc_printf("<br />");
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"send_button\" VALUE=\"%s\">\n", _("Send"));
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"help_button\" VALUE=\"%s\">\n", _("Help"));
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"list_button\" VALUE=\"%s\">\n", _("List users"));
	wc_printf("<INPUT TYPE=\"submit\" NAME=\"exit_button\" VALUE=\"%s\">\n", _("Exit"));
	wc_printf("</FORM>\n");

	wc_printf("</BODY></HTML>\n");
	wDumpContent(0);
}


void ajax_send_instant_message(void) {
	char recp[256];
	char buf[256];

	safestrncpy(recp, bstr("recp"), sizeof recp);

	serv_printf("SEXP %s|-", recp);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '4') {
		text_to_server(bstr("msg"));
		serv_puts("000");
	}

	escputs(buf);	/* doesn't really matter what we return - the client ignores it */
}


void 
InitModule_PAGING
(void)
{
	WebcitAddUrlHandler(HKEY("display_page"), "", 0, display_page, 0);
	WebcitAddUrlHandler(HKEY("page_user"), "", 0, page_user, 0);
	WebcitAddUrlHandler(HKEY("chat"), "", 0, do_chat, 0);
	WebcitAddUrlHandler(HKEY("chat_recv"), "", 0, chat_recv, 0);
	WebcitAddUrlHandler(HKEY("chat_send"), "", 0, chat_send, 0);
	WebcitAddUrlHandler(HKEY("ajax_send_instant_message"), "", 0, ajax_send_instant_message, AJAX);
	RegisterConditional(HKEY("COND:PAGE:WAITING"), 0, Conditional_PAGE_WAITING, CTX_NONE);
}


void 
SessionDestroyModule_CHAT
(wcsession *sess)
{
	if (sess->chat_sock > 0)
		close(sess->chat_sock);
}
