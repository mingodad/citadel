/*
 * $Id$
 */
/**
 * \defgroup AdminFloor Administrative screens for floor maintenance
 *
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
void display_floorconfig(char *prepend_html)
{
	char buf[SIZ];

	int floornum;
	char floorname[SIZ];
	int refcount;

        output_headers(1, 1, 2, 0, 0, 0);
        wprintf("<div id=\"banner\">\n"
                "<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
                "<SPAN CLASS=\"titlebar\">");
	wprintf(_("Add/change/delete floors"));
	wprintf("</SPAN>"
                "</TD></TR></TABLE>\n"
                "</div>\n<div id=\"content\">\n"
        );
                                                                                                                             
	if (prepend_html != NULL) {
		wprintf("<br /><b><i>");
		client_write(prepend_html, strlen(prepend_html));
		wprintf("</i></b><br /><br />\n");
	}

	serv_printf("LFLR");
	serv_getln(buf, sizeof buf);
	if (buf[0] != '1') {
        	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#770000\"><TR><TD>");
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
		wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"sc\" "
			"VALUE=\"%s\">"
			"</FORM></TD>", _("Change name"));

		wprintf("<TD>%d</TD></TR>\n", refcount);
	}

	wprintf("<TR><TD>&nbsp;</TD>"
		"<TD><FORM METHOD=\"POST\" action=\"create_floor\">"
		"<INPUT TYPE=\"text\" NAME=\"floorname\" "
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
	char buf[SIZ];
	char message[SIZ];

	floornum = atoi(bstr("floornum"));

	serv_printf("KFLR %d|1", floornum);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		sprintf(message, _("Floor has been deleted."));
	}
	else {
		sprintf(message, "%s", &buf[4]);
	}

	display_floorconfig(message);
}

/**
 * \brief tart creating a new floor
 */
void create_floor(void) {
	char buf[SIZ];
	char message[SIZ];
	char floorname[SIZ];

	strcpy(floorname, bstr("floorname"));

	serv_printf("CFLR %s|1", floorname);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		sprintf(message, _("New floor has been created."));
	} else {
		sprintf(message, "%s", &buf[4]);
	}

	display_floorconfig(message);
}

/**
 * \brief rename this floor
 */
void rename_floor(void) {
	int floornum;
	char buf[SIZ];
	char message[SIZ];
	char floorname[SIZ];

	floornum = atoi(bstr("floornum"));
	strcpy(floorname, bstr("floorname"));

	serv_printf("EFLR %d|%s", floornum, floorname);
	serv_getln(buf, sizeof buf);

	sprintf(message, "%s", &buf[4]);

	display_floorconfig(message);
}


/*@}*/
