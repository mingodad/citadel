/*
 * $Id$
 *
 * Handles HTTP upload of graphics files into the system.
 * \ingroup WebcitHttpServer
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

	output_headers(1, 1, 1, 0, 0, 0);

	svprintf("BOXTITLE", WCS_STRING, _("Image upload"));
	do_template("beginbox");

	wprintf("<form enctype=\"multipart/form-data\" action=\"%s\" "
		"method=\"post\" name=\"graphicsupload\">\n", uplurl);

	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
	wprintf("<input type=\"hidden\" name=\"which_room\" value=\"");
	urlescputs(bstr("which_room"));
	wprintf("\">\n");

	wprintf(_("You can upload any image directly from your computer, "
		"as long as it is in GIF format (JPEG, PNG, etc. won't "
		"work)."));
	wprintf("<br /><br />\n");

	wprintf(_("Please select a file to upload:"));
	wprintf("<input type=\"file\" name=\"filename\" size=\"35\">\n");
	wprintf("<div class=\"buttons\">");
	wprintf("<input type=\"submit\" name=\"upload_button\" value=\"%s\">\n", _("Upload"));
	wprintf("&nbsp;");
	wprintf("<input type=\"reset\" value=\"%s\">\n", _("Reset form"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wprintf("</div>\n");
	wprintf("</form>\n");

	do_template("endbox");

	wDumpContent(1);
}

void do_graphics_upload(char *upl_cmd)
{
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;

	if (!IsEmptyStr(bstr("cancel_button"))) {
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
