/*
 * $Id$
 *
 * This module handles multiuser chat.
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"
#include "webserver.h"

/*
 * Display the screen containing multiuser chat for a room.
 */
void do_chat(void)
{
	char buf[256];

	WC->last_chat_seq = 0;
	WC->last_chat_user[0] = 0;

	output_headers(1, 1, 1, 0, 0, 0);
	do_template("roomchat", NULL);

	serv_puts("RCHT enter");
	serv_getln(buf, sizeof buf);

	wDumpContent(1);
}


/*
 * Receiving side of the chat window.  
 * This does JavaScript writes to
 * other divs whenever it refreshes and finds new data.
 */
void chat_recv(void) {
	char buf[SIZ];
	char cl_user[SIZ];

	serv_printf("RCHT poll|%d", WC->last_chat_seq);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		WC->last_chat_seq = extract_int(&buf[4], 0);
		extract_token(cl_user, &buf[4], 2, '|', sizeof cl_user);

		/* who is speaking ... */
		if (strcasecmp(cl_user, WC->last_chat_user)) {
			wc_printf("<br>\n");
			if (!strcasecmp(cl_user, ChrPtr(WC->wc_fullname))) {
				wc_printf("<span class=\"chat_myname\">");
			}
			else {
				wc_printf("<span class=\"chat_notmyname\">");
			}
			escputs(cl_user);
			strcpy(WC->last_chat_user, cl_user);

			wc_printf(": </span>");
		}
		else {
			wc_printf("&nbsp;&nbsp;&nbsp;");
		}

		/* what did they say ... */
		wc_printf("<span class=\"chat_text\">");
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			escputs(buf);
		}
		wc_printf("<br></span>\n");
	}
}


/*
 * This is the sending side of the chat window.  The form is designed to transmit asynchronously.
 */
void chat_send(void) {
	char send_this[SIZ];
	char buf[SIZ];

	begin_ajax_response();

	if (havebstr("send_this")) {
		strcpy(send_this, bstr("send_this"));
	}
	else {
		strcpy(send_this, "");
	}

	if (havebstr("exit_button")) {
		strcpy(send_this, "/quit");
	}

	if (!IsEmptyStr(send_this)) {
		serv_puts("RCHT send");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			text_to_server(send_this);
			serv_puts("000");
		}
	}
	end_ajax_response();
}


/*
 * wholist for chat
 */
void chat_rwho(void) {
	char buf[1024];

	serv_puts("RCHT rwho");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			if (!strcasecmp(buf, ChrPtr(WC->wc_fullname))) {
				wc_printf("<span class=\"chat_myname\">");
			}
			else {
				wc_printf("<span class=\"chat_notmyname\">");
			}
			wc_printf("<img src=\"static/citadelchat_16x.gif\">");
			escputs(buf);
			wc_printf("</span><br>\n");
		}
	}
}


void 
InitModule_ROOMCHAT
(void)
{
	WebcitAddUrlHandler(HKEY("chat"), "", 0, do_chat, 0);
	WebcitAddUrlHandler(HKEY("chat_recv"), "", 0, chat_recv, AJAX);
	WebcitAddUrlHandler(HKEY("chat_rwho"), "", 0, chat_rwho, AJAX);
	WebcitAddUrlHandler(HKEY("chat_send"), "", 0, chat_send, 0);
}


void 
SessionDestroyModule_ROOMCHAT
(wcsession *sess)
{
	/* nothing here anymore */
}
