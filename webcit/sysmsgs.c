/*
 * $Id$
 */
#include "webcit.h"


/**
 *  display the form for editing something (room info, bio, etc)
 *  description the descriptive text for the box
 *  check_cmd command to check????
 *  read_cmd read answer from citadel server???
 *  save_cmd save comand to the citadel server??
 *  with_room_banner should we bisplay a room banner?
 */
void display_edit(char *description, char *check_cmd,
		  char *read_cmd, char *save_cmd, int with_room_banner)
{
	char buf[SIZ];

	serv_puts(check_cmd);
	serv_getln(buf, sizeof buf);

	if (buf[0] != '2') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}
	if (with_room_banner) {
		output_headers(1, 1, 1, 0, 0, 0);
	}
	else {
		output_headers(1, 1, 0, 0, 0, 0);
	}

	do_template("beginbox_1", NULL);
	StrBufAppendPrintf (WC->WBuf, _("Edit %s"), description);
	do_template("beginbox_2", NULL);

	wc_printf(_("Enter %s below. Text is formatted to the reader's browser."
		" A newline is forced by preceding the next line by a blank."), description);

	wc_printf("<form method=\"post\" action=\"%s\">\n", save_cmd);
	wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WC->nonce);
	wc_printf("<textarea name=\"msgtext\" wrap=soft "
		"rows=10 cols=80 width=80>\n");
	serv_puts(read_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1')
		server_to_text();
	wc_printf("</textarea><div class=\"buttons\" >\n");
	wc_printf("<input type=\"submit\" name=\"save_button\" value=\"%s\">", _("Save changes"));
	wc_printf("&nbsp;");
	wc_printf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\"><br />\n", _("Cancel"));
	wc_printf("</div></form>\n");

	do_template("endbox", NULL);
	wDumpContent(1);
}


/**
 *  save a screen which was displayed with display_edit()
 *  description the window title???
 *  enter_cmd which command to enter at the citadel server???
 *  regoto should we go to that room again after executing that command?
 */
void save_edit(char *description, char *enter_cmd, int regoto)
{
	char buf[SIZ];

	if (!havebstr("save_button")) {
		sprintf(WC->ImportantMessage,
			_("Cancelled.  %s was not saved."),
			description);
		display_main_menu();
		return;
	}
	serv_puts(enter_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '4') {
		safestrncpy(WC->ImportantMessage, &buf[4], sizeof WC->ImportantMessage);
		display_main_menu();
		return;
	}
	text_to_server(bstr("msgtext"));
	serv_puts("000");

	if (regoto) {
		smart_goto(WC->CurRoom.name);
	} else {
		sprintf(WC->ImportantMessage,
			_("%s has been saved."),
			description);
		display_main_menu();
		return;
	}
}


void display_editinfo(void){ display_edit(_("Room info"), "EINF 0", "RINF", "editinfo", 1);}
void editinfo(void) {save_edit(_("Room info"), "EINF 1", 1);}
void display_editbio(void) {
	char buf[SIZ];

	snprintf(buf, SIZ, "RBIO %s", ChrPtr(WC->wc_fullname));
	display_edit(_("Your bio"), "NOOP", buf, "editbio", 3);
}
void editbio(void) { save_edit(_("Your bio"), "EBIO", 0); }

void 
InitModule_SYSMSG
(void)
{
	WebcitAddUrlHandler(HKEY("display_editinfo"), "", 0, display_editinfo, 0);
	WebcitAddUrlHandler(HKEY("editinfo"), "", 0, editinfo, 0);
	WebcitAddUrlHandler(HKEY("display_editbio"), "", 0, display_editbio, 0);
	WebcitAddUrlHandler(HKEY("editbio"), "", 0, editbio, 0);
}
