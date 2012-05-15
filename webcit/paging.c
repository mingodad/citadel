/*
 * This module handles instant message related functions.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"

/*
 * display the form for paging (x-messaging) another user
 */
void display_page(void)
{
	char recp[SIZ];

	strcpy(recp, bstr("recp"));

        output_headers(1, 1, 1, 0, 0, 0);
        wc_printf("<div id=\"room_banner_override\">\n");
        wc_printf("<h1>");
	wc_printf(_("Send instant message"));
	wc_printf("</h1>");
        wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");

	wc_printf("<table class=\"paging_background\"><tr><td>\n");

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
	wc_printf("<br>");

	wc_printf("<TEXTAREA NAME=\"msgtext\" wrap=soft ROWS=5 COLS=40 "
		"WIDTH=40></TEXTAREA>\n");

	wc_printf("</TD></TR></TABLE><br>\n");

	wc_printf("<INPUT TYPE=\"submit\" NAME=\"send_button\" VALUE=\"%s\">", _("Send message"));
	wc_printf("<br><a href=\"javascript:window.close();\"%s</A>\n", _("Cancel"));

	wc_printf("</FORM></CENTER>\n");
	wc_printf("</td></tr></table>\n");
	wDumpContent(1);
}

/*
 * page another user
 */
void page_user(void)
{
	char recp[256];
	StrBuf *Line;

	safestrncpy(recp, bstr("recp"), sizeof recp);

	if (!havebstr("send_button")) {
		AppendImportantMessage(_("Message was not sent."), -1);
	} else {
		Line = NewStrBuf();
		serv_printf("SEXP %s|-", recp);
		StrBuf_ServGetln(Line);
		if (GetServerStatusMsg(Line, NULL, 0, 0) == 4) {
			char buf[256];
			text_to_server(bstr("msgtext"));
			serv_puts("000");
			stresc(buf, 256, recp, 0, 0);
			AppendImportantMessage(buf, -1);
			AppendImportantMessage(_("Message has been sent to "), -1);
		}
	}

	url_do_template();
}



/*
 * display page popup
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
	/* Then schedule it to happen again a minute from now if the user is idle. */
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
	WebcitAddUrlHandler(HKEY("ajax_send_instant_message"), "", 0, ajax_send_instant_message, AJAX);
	RegisterConditional(HKEY("COND:PAGE:WAITING"), 0, Conditional_PAGE_WAITING, CTX_NONE);
}


void 
SessionDestroyModule_PAGING
(wcsession *sess)
{
	/* nothing here anymore */
}
