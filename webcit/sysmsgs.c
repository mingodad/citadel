/*
 * $Id$
 */
/**
 * \defgroup ShowSysMsgs Editing of various text files on the Citadel server.
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"


/**
 * \brief display the form for editing something (room info, bio, etc)
 * \param description the descriptive text for the box
 * \param check_cmd command to check????
 * \param read_cmd read answer from citadel server???
 * \param save_cmd save comand to the citadel server??
 * \param with_room_banner should we bisplay a room banner?
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

	svprintf("BOXTITLE", WCS_STRING, _("Edit %s"), description);
	do_template("beginbox");

	wprintf("<div align=\"center\">");
	wprintf(_("Enter %s below.  Text is formatted to "
		"the reader's screen width.  To defeat the "
		"formatting, indent a line at least one space."), description);
	wprintf("<br />");

	wprintf("<FORM METHOD=\"POST\" action=\"%s\">\n", save_cmd);
	wprintf("<TEXTAREA NAME=\"msgtext\" wrap=soft "
		"ROWS=10 COLS=80 WIDTH=80>\n");
	serv_puts(read_cmd);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1')
		server_to_text();
	wprintf("</TEXTAREA><br /><br />\n");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"save_button\" VALUE=\"%s\">", _("Save changes"));
	wprintf("&nbsp;");
	wprintf("<INPUT TYPE=\"submit\" NAME=\"cancel_button\" VALUE=\"%s\"><br />\n", _("Cancel"));

	wprintf("</FORM></div>\n");
	do_template("endbox");
	wDumpContent(1);
}


/**
 * \brief save a screen which was displayed with display_edit()
 * \param description the window title???
 * \param enter_cmd which command to enter at the citadel server???
 * \param regoto should we go to that room again after executing that command?
 */
void save_edit(char *description, char *enter_cmd, int regoto)
{
	char buf[SIZ];

	if (strlen(bstr("save_button")) == 0) {
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
		smart_goto(WC->wc_roomname);
	} else {
		sprintf(WC->ImportantMessage,
			_("%s has been saved."),
			description);
		display_main_menu();
		return;
	}
}


/*@}*/
