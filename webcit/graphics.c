/*
 * Handles HTTP upload of graphics files into the system.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"

extern void output_static(const char* What);


// upload your photo
void editpic(void)
{
	if (havebstr("cancel_button")) {
		AppendImportantMessage(_("Graphics upload has been cancelled."), -1);
		display_main_menu();
		return;
	}

	if (WC->upload_length == 0) {
		AppendImportantMessage(_("You didn't upload a file."), -1);
		display_main_menu();
		return;
	}
	
	serv_printf("ULUI %ld|%s", (long)WC->upload_length, GuessMimeType(ChrPtr(WC->upload), WC->upload_length));
	StrBuf *Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 0, 0) == 7) {
		serv_write(ChrPtr(WC->upload), WC->upload_length);
		display_success(ChrPtr(Line) + 4);
	}
	else {
		AppendImportantMessage((ChrPtr(Line) + 4), -1);
		display_main_menu();
	}
	FreeStrBuf(&Line);
}


void display_graphics_upload(char *filename)
{
	StrBuf *Line;

	Line = NewStrBuf();
	serv_printf("UIMG 0||%s", filename);
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 2) != 2) {
		display_main_menu();
		return;
	}
	else
	{
		output_headers(1, 0, 0, 0, 1, 0);
		do_template("files_graphicsupload");
		end_burst();
	}
	FreeStrBuf(&Line);
}

void do_graphics_upload(char *filename)
{
	StrBuf *Line;
	const char *MimeType;
	wcsession *WCC = WC;
	int bytes_remaining;
	int pos = 0;
	int thisblock;
	bytes_remaining = WCC->upload_length;

	if (havebstr("cancel_button")) {
		AppendImportantMessage(_("Graphics upload has been cancelled."), -1);
		display_main_menu();
		return;
	}

	if (WCC->upload_length == 0) {
		AppendImportantMessage(_("You didn't upload a file."), -1);
		display_main_menu();
		return;
	}
	
	MimeType = GuessMimeType(ChrPtr(WCC->upload), bytes_remaining);
	serv_printf("UIMG 1|%s|%s", MimeType, filename);

	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 2) != 2) {
		display_main_menu();
		FreeStrBuf(&Line);
		return;
	}
	while (bytes_remaining) {
		thisblock = ((bytes_remaining > 4096) ? 4096 : bytes_remaining);
		serv_printf("WRIT %d", thisblock);
		StrBuf_ServGetln(Line);
		if (GetServerStatusMsg(Line, NULL, 1, 7) != 7) {
			serv_puts("UCLS 0");
			StrBuf_ServGetln(Line);
			display_main_menu();
			FreeStrBuf(&Line);
			return;
		}
		thisblock = extract_int(ChrPtr(Line) +4, 0);
		serv_write(&ChrPtr(WCC->upload)[pos], thisblock);
		pos += thisblock;
		bytes_remaining -= thisblock;
	}

	serv_puts("UCLS 1");
	StrBuf_ServGetln(Line);
	if (*ChrPtr(Line) != 'x') {
		display_success(ChrPtr(Line) + 4);
	
	}
	FreeStrBuf(&Line);

}


void edithellopic(void)    { do_graphics_upload("hello"); }
void editgoodbuyepic(void) { do_graphics_upload("UIMG 1|%s|goodbuye"); }

/* The users photo display / upload facility */
void display_editpic(void) {
	putbstr("__WHICHPIC", NewStrBufPlain(HKEY("_userpic_")));
	putbstr("__PICDESC", NewStrBufPlain(_("your photo"), -1));
	putbstr("__UPLURL", NewStrBufPlain(HKEY("editpic")));
	display_graphics_upload("editpic");
}
/* room picture dispay / upload facility */
void display_editroompic(void) {
	putbstr("__WHICHPIC", NewStrBufPlain(HKEY("_roompic_")));
	putbstr("__PICDESC", NewStrBufPlain(_("the icon for this room"), -1));
	putbstr("__UPLURL", NewStrBufPlain(HKEY("editroompic")));
	display_graphics_upload("editroompic");
}

/* the greetingpage hello pic */
void display_edithello(void) {
	putbstr("__WHICHPIC", NewStrBufPlain(HKEY("hello")));
	putbstr("__PICDESC", NewStrBufPlain(_("the Greetingpicture for the login prompt"), -1));
	putbstr("__UPLURL", NewStrBufPlain(HKEY("edithellopic")));
	display_graphics_upload("edithellopic");
}

/* the logoff banner */
void display_editgoodbyepic(void) {
	putbstr("__WHICHPIC", NewStrBufPlain(HKEY("UIMG 0|%s|goodbuye")));
	putbstr("__PICDESC", NewStrBufPlain(_("the Logoff banner picture"), -1));
	putbstr("__UPLURL", NewStrBufPlain(HKEY("editgoodbuyepic")));
	display_graphics_upload("editgoodbuyepic");
}

void editroompic(void) {
	char buf[SIZ];
	snprintf(buf, SIZ, "_roompic_|%s",
		 bstr("which_room"));
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
}
