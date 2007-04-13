/*
 * $Id:  $
 *//**
 * \defgroup AjaxAutoCompletion ajax-powered autocompletion...
 * \ingroup ClientPower
 */

/*@{*/
#include "webcit.h"


/**
 * \brief Address book popup window
 */
void display_address_book_middle_div(void) {
	char buf[256];
	char ebuf[256];

	begin_ajax_response();

	wprintf("<table border=0 width=100%%><tr valign=middle>");
	wprintf("<td align=left><img src=\"static/viewcontacts_32x.gif\"></td>");
	wprintf("<td align=center>");

	wprintf("<form><select class=\"address_book_popup_title\" size=1>");
	serv_puts("LKRA");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		if (extract_int(buf, 6) == VIEW_ADDRESSBOOK) {
			extract_token(ebuf, buf, 0, '|', sizeof ebuf);
			wprintf("<option value=\"");
			urlescputs(ebuf);
			wprintf("\">");
			escputs(ebuf);
			wprintf("</option>\n");
		}
	}
	wprintf("</select></form>");

	wprintf("</td>");
	wprintf("<td align=right "
		"onclick=\"javascript:$('address_book_popup').style.display='none';\" "
		"><img src=\"static/closewindow.gif\">");
	wprintf("</td></tr></table>");

	end_ajax_response();
}



/**
 * \brief Address book popup results
 */
void display_address_book_inner_div(void) {
	int i;

	begin_ajax_response();

	wprintf("<div align=center><form>"
		"<select name=\"whichaddr\" size=\"15\">\n");

	for (i=0; i<100; ++i) {
		wprintf("<option>Contact %d &lt;contact%d@example.com&gt;</option>\n", i, i);
	}

	wprintf("</select></form></div>\n");

	end_ajax_response();
}


/** @} */
