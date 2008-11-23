/*
 * $Id$
 *
 * Handles HTTP upload of graphics files into the system.
 * \ingroup WebcitHttpServer
 */

#include "webcit.h"

void display_graphics_upload(char *description, char *filename, char *uplurl)
{
	char buf[SIZ];


	snprintf(buf, SIZ, "UIMG 0||%s", filename);
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	output_headers(1, 1, 0, 0, 0, 0);

	StrBuf *Buf;
	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Image upload"), -1);
	DoTemplate(HKEY("beginbox"), NULL, Buf, CTX_STRBUF);

	FreeStrBuf(&Buf);

	wprintf("<form enctype=\"multipart/form-data\" action=\"%s\" "
		"method=\"post\" name=\"graphicsupload\">\n", uplurl);

	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wprintf("<input type=\"hidden\" name=\"which_room\" value=\"");
	urlescputs(bstr("which_room"));
	wprintf("\">\n");

	wprintf(_("You can upload an image directly from your computer"));
	wprintf("<br /><br />\n");

	wprintf(_("Please select a file to upload:"));
	wprintf("<input type=\"file\" name=\"filename\" size=\"35\">\n");

	wprintf("<div class=\"uploadpic\"><img src=\"image&name=%s\"></div>\n", filename);

	wprintf("<div class=\"buttons\">");
	wprintf("<input type=\"submit\" name=\"upload_button\" value=\"%s\">\n", _("Upload"));
	wprintf("&nbsp;");
	wprintf("<input type=\"reset\" value=\"%s\">\n", _("Reset form"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wprintf("</div>\n");
	wprintf("</form>\n");

	do_template("endbox", NULL);

	wDumpContent(1);
}

void do_graphics_upload(char *filename)
{
	const char *MimeType;
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;
	bytes_remaining = WC->upload_length;

	if (havebstr("cancel_button")) {
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
	
	MimeType = GuessMimeType(&WC->upload[0], bytes_remaining);
	snprintf(buf, SIZ, "UIMG 1|%s|%s", MimeType, filename);
	serv_puts(buf);

	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
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


void edithellopic(void)    { do_graphics_upload("hello"); }
void editpic(void)         { do_graphics_upload("_userpic_"); }
void editgoodbuyepic(void) { do_graphics_upload("UIMG 1|%s|goodbuye"); }

/* The users photo display / upload facility */
void display_editpic(void) {
	display_graphics_upload(_("your photo"),
				"_userpic_",
				"editpic");
}
/* room picture dispay / upload facility */
void display_editroompic(void) {
	display_graphics_upload(_("the icon for this room"),
				"_roompic_",
				"editroompic");
}

/* the greetingpage hello pic */
void display_edithello(void) {
	display_graphics_upload(_("the Greetingpicture for the login prompt"),
				"hello",
				"edithellopic");
}

/* the logoff banner */
void display_editgoodbyepic(void) {
	display_graphics_upload(_("the Logoff banner picture"),
				"UIMG 0|%s|goodbuye",
				"editgoodbuyepic");
}

void display_editfloorpic(void) {
	char buf[SIZ];
	snprintf(buf, SIZ, "UIMG 0|_floorpic_|%s",
		 bstr("which_floor"));
	display_graphics_upload(_("the icon for this floor"),
				buf,
				"editfloorpic");
}

void editfloorpic(void){
	char buf[SIZ];
	snprintf(buf, SIZ, "UIMG 1|_floorpic_|%s",
		 bstr("which_floor"));
	do_graphics_upload(buf);
}

void 
InitModule_GRAPHICS
(void)
{
	WebcitAddUrlHandler(HKEY("display_editpic"), display_editpic, 0);
	WebcitAddUrlHandler(HKEY("editpic"), editpic, 0);
	WebcitAddUrlHandler(HKEY("display_editroompic"), display_editroompic, 0);
	WebcitAddUrlHandler(HKEY("display_edithello"), display_edithello, 0);
	WebcitAddUrlHandler(HKEY("edithellopic"), edithellopic, 0);
	WebcitAddUrlHandler(HKEY("display_editgoodbyepic"), display_editgoodbyepic, 0);
	WebcitAddUrlHandler(HKEY("editgoodbuyepic"), editgoodbuyepic, 0);
	WebcitAddUrlHandler(HKEY("display_editfloorpic"), display_editfloorpic, 0);
	WebcitAddUrlHandler(HKEY("editfloorpic"), editfloorpic, 0);
}
