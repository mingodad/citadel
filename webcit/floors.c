/*
 * $Id$
 */
/**
 * \defgroup AdminFloor Administrative screens for floor maintenance
 * \ingroup CitadelConfig
 */
/*@{*/

#include "webcit.h"
#include "webserver.h"




/**
 * \brief Display floor config
 * Display floor configuration.  If prepend_html is not NULL, its contents
 * will be displayed at the top of the screen.
 * \param prepend_html pagetitle to prepend
 */
void display_floorconfig(StrBuf *prepend_html)
{
	char buf[SIZ];

	int floornum;
	char floorname[SIZ];
	int refcount;

        output_headers(1, 1, 2, 0, 0, 0);
        wprintf("<div id=\"banner\">\n");
        wprintf("<h1>");
	wprintf(_("Add/change/delete floors"));
	wprintf("</h1>");
        wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");
                                                                                                                             
	if (prepend_html != NULL) {
		wprintf("<br /><b><i>");
		StrBufAppendBuf(WC->WBuf, prepend_html, 0);
		wprintf("</i></b><br /><br />\n");
	}

	serv_printf("LFLR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE  class=\"floors_config\"><TR><TD>");
        	wprintf("<SPAN CLASS=\"titlebar\">");
		wprintf(_("Error"));
		wprintf("</SPAN>\n");
        	wprintf("</TD></TR></TABLE>\n");
        	wprintf("%s<br />\n", &buf[4]);
		wDumpContent(1);
		return;
	}

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<TABLE BORDER=1 WIDTH=100%% bgcolor=\"#ffffff\">\n"
		"<TR><TH>");
	wprintf(_("Floor number"));
	wprintf("</TH><TH>");
	wprintf(_("Floor name"));
	wprintf("</TH><TH>");
	wprintf(_("Number of rooms"));
	wprintf("</TH><TH>");
	wprintf(_("Floor CSS"));
	wprintf("</TH></TR>\n");

	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		floornum = extract_int(buf, 0);
		extract_token(floorname, buf, 1, '|', sizeof floorname);
		refcount = extract_int(buf, 2);

		wprintf("<TR><TD><TABLE border=0><TR><TD>%d", floornum);
		if (refcount == 0) {
			wprintf("</TD><TD>"
				"<a href=\"delete_floor?floornum=%d\">"
				"<FONT SIZE=-1>", floornum);
			wprintf(_("(delete floor)"));
			wprintf("</A></FONT><br />");
		}
		wprintf("<FONT SIZE=-1>"
			"<a href=\"display_editfloorpic&"
			"which_floor=%d\">", floornum);
		wprintf(_("(edit graphic)"));
		wprintf("</A></TD></TR></TABLE>");
		wprintf("</TD>");

		wprintf("<TD>"
			"<FORM METHOD=\"POST\" action=\"rename_floor\">"
			"<INPUT TYPE=\"hidden\" NAME=\"floornum\" "
			"VALUE=\"%d\">"
			"<INPUT TYPE=\"text\" NAME=\"floorname\" "
			"VALUE=\"%s\" MAXLENGTH=\"250\">\n",
			floornum, floorname);
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"%s\">"
			"</FORM></TD>", _("Change name"));

		wprintf("<TD>%d</TD>\n", refcount);

		wprintf("<TD>"
			"<FORM METHOD=\"POST\" action=\"set_floor_css\">"
			"<INPUT TYPE=\"hidden\" NAME=\"floornum\" "
			"VALUE=\"%d\">"
			"<INPUT TYPE=\"text\" NAME=\"floorcss\" "
			"VALUE=\"%s\" MAXLENGTH=\"250\">\n",
			floornum, floorname);
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
		wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"%s\">"
			"</FORM></TD>", _("Change CSS"));

		wprintf("</TR>\n");
	}

	wprintf("<TR><TD>&nbsp;</TD>"
		"<TD><FORM METHOD=\"POST\" action=\"create_floor\">");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<INPUT TYPE=\"text\" NAME=\"floorname\" "
		"MAXLENGTH=\"250\">\n"
		"<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
		"VALUE=\"%s\">"
		"</FORM></TD>"
		"<TD>&nbsp;</TD></TR>\n", _("Create new floor"));

	wprintf("</table></div>\n");
	wDumpContent(1);
}


/**
 * \brief delete the actual floor
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

	display_floorconfig(Buf);
	FreeStrBuf(&Buf);
}

/**
 * \brief tart creating a new floor
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

	display_floorconfig(Buf);
	FreeStrBuf(&Buf);
}


/**
 * \brief rename this floor
 */
void rename_floor(void) {
	StrBuf *Buf;

	Buf = NewStrBuf();

	serv_printf("EFLR %d|%s", 
		    ibstr("floornum"), 
		    bstr("floorname"));
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
	WebcitAddUrlHandler(HKEY("delete_floor"), delete_floor, 0);
	WebcitAddUrlHandler(HKEY("rename_floor"), rename_floor, 0);
	WebcitAddUrlHandler(HKEY("create_floor"), create_floor, 0);
	WebcitAddUrlHandler(HKEY("display_floorconfig"), _display_floorconfig, 0);
}
/*@}*/
