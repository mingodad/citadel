/*
 * $Id$
 *
 * Handles HTTP upload of graphics files into the system.
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

void display_graphics_upload(char *description, char *filename, char *uplurl)
{
	WCTemplputParams SubTP;
	StrBuf *Buf;
	char buf[SIZ];


	snprintf(buf, SIZ, "UIMG 0||%s", filename);
	serv_puts(buf);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	/*output_headers(1, 1, 0, 0, 0, 0); */

	output_headers(1, 1, 1, 0, 0, 0);

	Buf = NewStrBufPlain(_("Image upload"), -1);
	memset(&SubTP, 0, sizeof(WCTemplputParams));
	SubTP.Filter.ContextType = CTX_STRBUF;
	SubTP.Context = Buf;
	DoTemplate(HKEY("beginbox"), NULL, &SubTP);

	FreeStrBuf(&Buf);

	wc_printf("<form enctype=\"multipart/form-data\" action=\"%s\" "
		"method=\"post\" name=\"graphicsupload\">\n", uplurl);

	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<input type=\"hidden\" name=\"which_room\" value=\"");
	urlescputs(bstr("which_room"));
	wc_printf("\">\n");

	wc_printf(_("You can upload an image directly from your computer"));
	wc_printf("<br /><br />\n");

	wc_printf(_("Please select a file to upload:"));
	wc_printf("<input type=\"file\" name=\"filename\" size=\"35\">\n");

	wc_printf("<div class=\"uploadpic\"><img src=\"image?name=%s\"></div>\n", filename);

	wc_printf("<div class=\"buttons\">");
	wc_printf("<input type=\"submit\" name=\"upload_button\" value=\"%s\">\n", _("Upload"));
	wc_printf("&nbsp;");
	wc_printf("<input type=\"reset\" value=\"%s\">\n", _("Reset form"));
	wc_printf("&nbsp;");
	wc_printf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wc_printf("</div>\n");
	wc_printf("</form>\n");

	do_template("endbox", NULL);

	wDumpContent(1);
}

void do_graphics_upload(char *filename)
{
	const char *MimeType;
	wcsession *WCC = WC;
	char buf[SIZ];
	int bytes_remaining;
	int pos = 0;
	int thisblock;
	bytes_remaining = WCC->upload_length;

	if (havebstr("cancel_button")) {
		strcpy(WC->ImportantMessage,
			_("Graphics upload has been cancelled."));
		display_main_menu();
		return;
	}

	if (WCC->upload_length == 0) {
		strcpy(WC->ImportantMessage,
			_("You didn't upload a file."));
		display_main_menu();
		return;
	}
	
	MimeType = GuessMimeType(ChrPtr(WCC->upload), bytes_remaining);
	snprintf(buf, SIZ, "UIMG 1|%s|%s", MimeType, filename);
	serv_puts(buf);

	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		strcpy(WCC->ImportantMessage, &buf[4]);
		display_main_menu();
		return;
	}
	while (bytes_remaining) {
		thisblock = ((bytes_remaining > 4096) ? 4096 : bytes_remaining);
		serv_printf("WRIT %d", thisblock);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '7') {
			strcpy(WCC->ImportantMessage, &buf[4]);
			serv_puts("UCLS 0");
			serv_getln(buf, sizeof buf);
			display_main_menu();
			return;
		}
		thisblock = extract_int(&buf[4], 0);
		serv_write(&ChrPtr(WCC->upload)[pos], thisblock);
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
	snprintf(buf, SIZ, "_floorpic_|%s",
		 bstr("which_floor"));
	display_graphics_upload(_("the icon for this floor"),
				buf,
				"editfloorpic");
}

void editroompic(void) {
	char buf[SIZ];
	snprintf(buf, SIZ, "_roompic_|%s",
		 bstr("which_room"));
	do_graphics_upload(buf);
}

void editfloorpic(void){
	char buf[SIZ];
	snprintf(buf, SIZ, "_floorpic_|%s",
		 bstr("which_floor"));
	do_graphics_upload(buf);
}

void 
InitModule_GRAPHICS
(void)
{
	WebcitAddUrlHandler(HKEY("display_editpic"), "", 0, display_editpic, 0);
	WebcitAddUrlHandler(HKEY("editpic"), "", 0, editpic, 0);
	WebcitAddUrlHandler(HKEY("display_editroompic"), "", 0, display_editroompic, 0);
	WebcitAddUrlHandler(HKEY("editroompic"), "", 0, editroompic, 0);
	WebcitAddUrlHandler(HKEY("display_edithello"), "", 0, display_edithello, 0);
	WebcitAddUrlHandler(HKEY("edithellopic"), "", 0, edithellopic, 0);
	WebcitAddUrlHandler(HKEY("display_editgoodbuye"), "", 0, display_editgoodbyepic, 0);
	WebcitAddUrlHandler(HKEY("editgoodbuyepic"), "", 0, editgoodbuyepic, 0);
	WebcitAddUrlHandler(HKEY("display_editfloorpic"), "", 0, display_editfloorpic, 0);
	WebcitAddUrlHandler(HKEY("editfloorpic"), "", 0, editfloorpic, 0);
}
