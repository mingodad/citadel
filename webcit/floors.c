/*
 * $Id$
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
 * Display floor configuration.  If prepend_html is not NULL, its contents
 * will be displayed at the top of the screen.
 */
void display_floorconfig(StrBuf *prepend_html)
{
	char buf[SIZ];

	int floornum;
	char floorname[SIZ];
	int refcount;

        output_headers(1, 1, 2, 0, 0, 0);
        wc_printf("<div id=\"banner\">\n");
        wc_printf("<h1>");
	wc_printf(_("Add/change/delete floors"));
	wc_printf("</h1>");
        wc_printf("</div>\n");

	wc_printf("<div id=\"content\" class=\"service\">\n");
                                                                                                                             
	if (prepend_html != NULL) {
		wc_printf("<br /><b><i>");
		StrBufAppendBuf(WC->WBuf, prepend_html, 0);
		wc_printf("</i></b><br /><br />\n");
	}

	serv_printf("LFLR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wc_printf("<TABLE  class=\"floors_config\"><TR><TD>");
        	wc_printf("<SPAN CLASS=\"titlebar\">");
		wc_printf(_("Error"));
		wc_printf("</SPAN>\n");
        	wc_printf("</TD></TR></TABLE>\n");
        	wc_printf("%s<br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wc_printf("<div class=\"fix_scrollbar_bug\">"
		"<TABLE BORDER=1 WIDTH=100%% bgcolor=\"#ffffff\">\n"
		"<TR><TH>");
	wc_printf(_("Floor number"));
	wc_printf("</TH><TH>");
	wc_printf(_("Floor name"));
	wc_printf("</TH><TH>");
	wc_printf(_("Number of rooms"));
	wc_printf("</TH><TH>");
	wc_printf(_("Floor CSS"));
	wc_printf("</TH></TR>\n");

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		floornum = extract_int(buf, 0);
		extract_token(floorname, buf, 1, '|', sizeof floorname);
		refcount = extract_int(buf, 2);

		wc_printf("<TR><TD><TABLE border=0><TR><TD>%d", floornum);
		if (refcount == 0) {
			wc_printf("</TD><TD>"
				"<a href=\"delete_floor?floornum=%d\">"
				"<FONT SIZE=-1>", floornum);
			wc_printf(_("(delete floor)"));
			wc_printf("</A></FONT><br />");
		}
		wc_printf("<FONT SIZE=-1>"
			"<a href=\"display_editfloorpic?"
			"which_floor=%d\">", floornum);
		wc_printf(_("(edit graphic)"));
		wc_printf("</A></TD></TR></TABLE>");
		wc_printf("</TD>");

		wc_printf("<TD>"
			"<FORM METHOD=\"POST\" action=\"rename_floor\">"
			"<INPUT TYPE=\"hidden\" NAME=\"floornum\" "
			"VALUE=\"%d\">"
			"<INPUT TYPE=\"text\" NAME=\"floorname\" "
			"VALUE=\"%s\" MAXLENGTH=\"250\">\n",
			floornum, floorname);
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"%s\">"
			"</FORM></TD>", _("Change name"));

		wc_printf("<TD>%d</TD>\n", refcount);

		wc_printf("<TD>"
			"<FORM METHOD=\"POST\" action=\"set_floor_css\">"
			"<INPUT TYPE=\"hidden\" NAME=\"floornum\" "
			"VALUE=\"%d\">"
			"<INPUT TYPE=\"text\" NAME=\"floorcss\" "
			"VALUE=\"%s\" MAXLENGTH=\"250\">\n",
			floornum, floorname);
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wc_printf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"%s\">"
			"</FORM></TD>", _("Change CSS"));

		wc_printf("</TR>\n");
	}

	wc_printf("<TR><TD>&nbsp;</TD>"
		"<TD><FORM METHOD=\"POST\" action=\"create_floor\">");
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<INPUT TYPE=\"text\" NAME=\"floorname\" "
		"MAXLENGTH=\"250\">\n"
		"<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
		"VALUE=\"%s\">"
		"</FORM></TD>"
		"<TD>&nbsp;</TD></TR>\n", _("Create new floor"));

	wc_printf("</table></div>\n");
	wDumpContent(1);
}


/*
 * delete the actual floor
 */
void delete_floor(void) {
	int floornum;
	StrBuf *Buf;
	const char *Err;
		
	floornum = ibstr("floornum");
	Buf = NewStrBuf();
	serv_printf("KFLR %d|1", floornum);
	
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);

	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufPlain(Buf, _("Floor has been deleted."),-1);
	}
	else {
		StrBufCutLeft(Buf, 4);
	}

	FlushRoomlist();
	display_floorconfig(Buf);
	FreeStrBuf(&Buf);
}

/*
 * start creating a new floor
 */
void create_floor(void) {
	StrBuf *Buf;
	const char *Err;

	Buf = NewStrBuf();
	serv_printf("CFLR %s|1", bstr("floorname"));
	StrBufTCP_read_line(Buf, &WC->serv_sock, 0, &Err);

	if (GetServerStatus(Buf, NULL) == 2) {
		StrBufPlain(Buf, _("New floor has been created."),-1);
	}
	else {
		StrBufCutLeft(Buf, 4);
	}

	FlushRoomlist();
	display_floorconfig(Buf);
	FreeStrBuf(&Buf);
}


/*
 * rename this floor
 */
void rename_floor(void) {
	StrBuf *Buf;

	Buf = NewStrBuf();
	FlushRoomlist();

	serv_printf("EFLR %d|%s", ibstr("floornum"), bstr("floorname"));
	StrBuf_ServGetln(Buf);

	StrBufCutLeft(Buf, 4);

	display_floorconfig(Buf);
	FreeStrBuf(&Buf);
}

void _display_floorconfig(void) {display_floorconfig(NULL);}

void 
InitModule_FLOORS
(void)
{
	WebcitAddUrlHandler(HKEY("delete_floor"), "", 0, delete_floor, 0);
	WebcitAddUrlHandler(HKEY("rename_floor"), "", 0, rename_floor, 0);
	WebcitAddUrlHandler(HKEY("create_floor"), "", 0, create_floor, 0);
	WebcitAddUrlHandler(HKEY("display_floorconfig"), "", 0, _display_floorconfig, 0);
}
