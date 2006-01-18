/*
 * $Id$
 *
 * Handles HTTP upload of graphics files into the system.
 */

#include "webcit.h"

void display_graphics_upload(char *description, char *check_cmd, char *uplurl)
{
	char buf[SIZ];

	serv_puts(check_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	output_headers(1, 1, 0, 0, 0, 0);

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>"
		"<SPAN CLASS=\"titlebar\">");
	wprintf(_("Image upload"));
	wprintf("</SPAN>"
		"</TD></TR></TABLE>\n"
		"</div>\n<div id=\"content\">\n"
	);

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");

	wprintf("<CENTER>\n");

	wprintf("<FORM ENCTYPE=\"multipart/form-data\" action=\"%s\" "
		"METHOD=\"POST\" NAME=\"graphicsupload\">\n", uplurl);

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"which_room\" VALUE=\"");
	urlescputs(bstr("which_room"));
	wprintf("\">\n");

	wprintf(_("You can upload any image directly from your computer, "
		"as long as it is in GIF format (JPEG, PNG, etc. won't "
		"work)."));
	wprintf("<br /><br />\n");

	wprintf(_("Please select a file to upload:"));
	wprintf("<br /><br />\n");
	wprintf("<INPUT TYPE=\"FILE\" NAME=\"filename\" SIZE=\"35\">\n");
	wprintf("<br /><br />");
	wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"upload_button\" VALUE=\"%s\">\n", _("Upload"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"RESET\" VALUE=\"%s\">\n", _("Reset form"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"SUBMIT\" NAME=\"cancel_button\" VALUE=\"%s\">\n", _("Cancel"));
	wprintf("</FORM>\n");
	wprintf("</CENTER>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);
}

void do_graphics_upload(char *upl_cmd)
{
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;

	if (strlen(bstr("cancel_button")) > 0) {
		strcpy(WC->ImportantMessage,
			_("Graphics upload has been cancelled."));
		display_main_menu();
		return;
	}

	if (WC->upload_length == 0) {
		strcpy(WC->ImportantMessage,
			_("You didn't upload a file."));
		display_main_menu();
		return;
	}
	serv_puts(upl_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	bytes_remaining = WC->upload_length;
	while (bytes_remaining) {
		thisblock = ((bytes_remaining > 4096) ? 4096 : bytes_remaining);
		serv_printf("WRIT %d", thisblock);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '7') {
			strcpy(WC->ImportantMessage, &buf[4]);
			serv_puts("UCLS 0");
			serv_getln(buf, sizeof buf);
			display_main_menu();
			return;
		}
		thisblock = extract_int(&buf[4], 0);
		serv_write(&WC->upload[pos], thisblock);
		pos = pos + thisblock;
		bytes_remaining = bytes_remaining - thisblock;
	}

	serv_puts("UCLS 1");
	serv_getln(buf, sizeof buf);
	if (buf[0] != 'x') {
		display_success(&buf[4]);
		return;
	}
}
