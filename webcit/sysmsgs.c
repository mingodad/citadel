/*
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

/**
 *  save a screen which was displayed with display_edit()
 *  description the window title???
 *  enter_cmd which command to enter at the citadel server???
 *  regoto should we go to that room again after executing that command?
 */
void save_edit(char *description, char *enter_cmd, int regoto)
{
	StrBuf *Line;
	const StrBuf *templ;

	if (!havebstr("save_button")) {
		AppendImportantMessage(_("Cancelled.  %s was not saved."), -1);
		display_main_menu();
		return;
	}
	templ = sbstr("template");
	Line = NewStrBuf();
	serv_puts(enter_cmd);
	StrBuf_ServGetln(Line);
	if (GetServerStatusMsg(Line, NULL, 1, 0) != 4) {
		putlbstr("success", 0);
		FreeStrBuf(&Line);
		if (templ != NULL) {
			output_headers(1, 0, 0, 0, 0, 0);	
			DoTemplate(SKEY(templ), NULL, &NoCtx);
			end_burst();
		}
		else {
			display_main_menu();
		}
		return;
	}
	FreeStrBuf(&Line);
	text_to_server(bstr("msgtext"));
	serv_puts("000");

	AppendImportantMessage(description, -1);
	AppendImportantMessage(_(" has been saved."), -1);
	putlbstr("success", 1);
	if (templ != NULL) {
		output_headers(1, 0, 0, 0, 0, 0);	
		DoTemplate(SKEY(templ), NULL, &NoCtx);
		end_burst();
	}
	else if (regoto) {
		smart_goto(WC->CurRoom.name);
	} else {
		display_main_menu();
		return;
	}
}


void editinfo(void) {save_edit(_("Room info"), "EINF 1", 1);}
void editbio(void) { save_edit(_("Your bio"), "EBIO", 0); }

void 
InitModule_SYSMSG
(void)
{
	WebcitAddUrlHandler(HKEY("editinfo"), "", 0, editinfo, 0);
	WebcitAddUrlHandler(HKEY("editbio"), "", 0, editbio, 0);
}
