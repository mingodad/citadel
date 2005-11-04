/*
 * $Id$
 *
 * Administrative screen to add/change/delete user accounts
 *
 */


#include "webcit.h"
#include "webserver.h"


void select_user_to_edit(char *message, char *preselect)
{
	char buf[SIZ];
	char username[SIZ];

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<table width=100%% border=0 bgcolor=#444455><tr>"
		"<td>"
		"<span class=\"titlebar\">"
		"<img src=\"/static/usermanag_48x.gif\">");
	wprintf(_("Edit or delete users"));
	wprintf("</span></td></tr></table>\n"
		"</div>\n<div id=\"content\">\n"
	);

	if (message != NULL) wprintf(message);

	wprintf("<TABLE border=0 CELLSPACING=10><TR VALIGN=TOP><TD>\n");

	svprintf("BOXTITLE", WCS_STRING, _("Add users"));
	do_template("beginbox");

	wprintf(_("To create a new user account, enter the desired "
		"user name in the box below and click 'Create'."));
	wprintf("<br /><br />");

        wprintf("<CENTER><FORM METHOD=\"POST\" action=\"/create_user\">\n");
        wprintf(_("New user: "));
        wprintf("<input type=\"text\" name=\"username\"><br />\n"
        	"<input type=\"submit\" name=\"create_button\" value=\"%s\">"
		"</FORM></CENTER>\n", _("Create"));

	do_template("endbox");

	wprintf("</TD><TD>");

	svprintf("BOXTITLE", WCS_STRING, _("Edit or Delete users"));
	do_template("beginbox");

	wprintf(_("To edit an existing user account, select the user "
		"name from the list and click 'Edit'."));
	wprintf("<br /><br />");
	
        wprintf("<CENTER>"
		"<FORM METHOD=\"POST\" action=\"/display_edituser\">\n");
        wprintf("<SELECT NAME=\"username\" SIZE=10 STYLE=\"width:100%%\">\n");
        serv_puts("LIST");
        serv_getln(buf, sizeof buf);
        if (buf[0] == '1') {
                while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
                        extract_token(username, buf, 0, '|', sizeof username);
                        wprintf("<OPTION");
			if (preselect != NULL)
			   if (!strcasecmp(username, preselect))
			      wprintf(" SELECTED");
			wprintf(">");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</SELECT><br />\n");

        wprintf("<input type=\"submit\" name=\"edit_config_button\" value=\"%s\">", _("Edit configuration"));
        wprintf("<input type=\"submit\" name=\"edit_abe_button\" value=\"%s\">", _("Edit address book entry"));
        wprintf("<input type=\"submit\" name=\"delete_button\" value=\"%s\" "
		"onClick=\"return confirm('%s');\">", _("Delete user"), _("Delete this user?"));
        wprintf("</FORM></CENTER>\n");
	do_template("endbox");

	wprintf("</TD></TR></TABLE>\n");

	wDumpContent(1);
}



/* 
 * Locate the message number of a user's vCard in the current room
 */
long locate_user_vcard(char *username, long usernum) {
	char buf[SIZ];
	long vcard_msgnum = (-1L);
	char content_type[SIZ];
	char partnum[SIZ];
	int already_tried_creating_one = 0;

	struct stuff_t {
		struct stuff_t *next;
		long msgnum;
	};

	struct stuff_t *stuff = NULL;
	struct stuff_t *ptr;

TRYAGAIN:
	/* Search for the user's vCard */
	serv_puts("MSGS ALL");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
		ptr = malloc(sizeof(struct stuff_t));
		ptr->msgnum = atol(buf);
		ptr->next = stuff;
		stuff = ptr;
	}

	/* Iterate through the message list looking for vCards */
	while (stuff != NULL) {
		serv_printf("MSG0 %ld|2", stuff->msgnum);
		serv_getln(buf, sizeof buf);
		if (buf[0]=='1') {
			while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				if (!strncasecmp(buf, "part=", 5)) {
					extract_token(partnum, &buf[5], 2, '|', sizeof partnum);
					extract_token(content_type, &buf[5], 4, '|', sizeof content_type);
					if (!strcasecmp(content_type,
					   "text/x-vcard")) {
						vcard_msgnum = stuff->msgnum;
					}
				}
			}
		}

		ptr = stuff->next;
		free(stuff);
		stuff = ptr;
	}

	/* If there's no vcard, create one */
	if (vcard_msgnum < 0) if (already_tried_creating_one == 0) {
		already_tried_creating_one = 1;
		serv_puts("ENT0 1|||4");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '4') {
			serv_puts("Content-type: text/x-vcard");
			serv_puts("");
			serv_puts("begin:vcard");
			serv_puts("end:vcard");
			serv_puts("000");
		}
		goto TRYAGAIN;
	}

	return(vcard_msgnum);
}


/* 
 * Display the form for editing a user's address book entry
 */
void display_edit_address_book_entry(char *username, long usernum) {
	char roomname[SIZ];
	char buf[SIZ];
	char error_message[SIZ];
	long vcard_msgnum = (-1L);

	/* Locate the user's config room, creating it if necessary */
	sprintf(roomname, "%010ld.%s", usernum, USERCONFIGROOM);
	serv_printf("GOTO %s||1", roomname);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		serv_printf("CRE8 1|%s|5|||1|", roomname);
		serv_getln(buf, sizeof buf);
		serv_printf("GOTO %s||1", roomname);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			sprintf(error_message,
				"<img src=\"static/error.gif\" ALIGN=CENTER>"
				"%s<br /><br />\n", &buf[4]);
			select_user_to_edit(error_message, username);
			return;
		}
	}

	vcard_msgnum = locate_user_vcard(username, usernum);

	if (vcard_msgnum < 0) {
		sprintf(error_message,
			"<img src=\"static/error.gif\" ALIGN=CENTER>%s<br /><br />\n",
			_("An error occurred while trying to create or edit this address book entry.")
		);
		select_user_to_edit(error_message, username);
		return;
	}

	do_edit_vcard(vcard_msgnum, "1", "/select_user_to_edit");
}




/*
 * Edit a user.  If supplied_username is null, look in the "username"
 * web variable for the name of the user to edit.
 * 
 * If "is_new" is set to nonzero, this screen will set the web variables
 * to send the user to the vCard editor next.
 */
void display_edituser(char *supplied_username, int is_new) {
	char buf[1024];
	char error_message[1024];
	time_t now;

	char username[256];
	char password[256];
	unsigned int flags;
	int timescalled;
	int msgsposted;
	int axlevel;
	long usernum;
	time_t lastcall;
	int purgedays;
	int i;

	if (supplied_username != NULL) {
		safestrncpy(username, supplied_username, sizeof username);
	}
	else {
		safestrncpy(username, bstr("username"), sizeof username);
	}

	serv_printf("AGUP %s", username);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		sprintf(error_message,
			"<img src=\"static/error.gif\" ALIGN=CENTER>"
			"%s<br /><br />\n", &buf[4]);
		select_user_to_edit(error_message, username);
		return;
	}

	extract_token(username, &buf[4], 0, '|', sizeof username);
	extract_token(password, &buf[4], 1, '|', sizeof password);
	flags = extract_int(&buf[4], 2);
	timescalled = extract_int(&buf[4], 3);
	msgsposted = extract_int(&buf[4], 4);
	axlevel = extract_int(&buf[4], 5);
	usernum = extract_long(&buf[4], 6);
	lastcall = extract_long(&buf[4], 7);
	purgedays = extract_long(&buf[4], 8);

	if (strlen(bstr("edit_abe_button")) > 0) {
		display_edit_address_book_entry(username, usernum);
		return;
	}

	if (strlen(bstr("delete_button")) > 0) {
		delete_user(username);
		return;
	}

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("Edit user account: "));
	escputs(username);
	wprintf("</SPAN></TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<div id=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#ffffff\"><tr><td>\n");
	wprintf("<FORM METHOD=\"POST\" action=\"/edituser\">\n"
		"<INPUT TYPE=\"hidden\" NAME=\"username\" VALUE=\"");
	escputs(username);
	wprintf("\">\n");
	wprintf("<INPUT TYPE=\"hidden\" NAME=\"is_new\" VALUE=\"%d\">\n"
		"<INPUT TYPE=\"hidden\" NAME=\"usernum\" VALUE=\"%ld\">\n",
		is_new, usernum);

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"flags\" VALUE=\"%d\">\n", flags);

	wprintf("<CENTER><TABLE>");

	wprintf("<TR><TD>");
	wprintf(_("Password"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"password\" NAME=\"password\" VALUE=\"");
	escputs(password);
	wprintf("\" MAXLENGTH=\"20\"></TD></TR>\n");

	wprintf("<tr><td>");
	wprintf(_("Permission to send Internet mail"));
	wprintf("</td><td>");
	wprintf("<input type=\"checkbox\" name=\"inetmail\" value=\"yes\" ");
	if (flags & US_INTERNET) {
		wprintf("CHECKED ");
	}
	wprintf("></td></tr>\n");

	wprintf("<TR><TD>");
	wprintf(_("Number of logins"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"timescalled\" VALUE=\"");
	wprintf("%d", timescalled);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("Messages submitted"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"msgsposted\" VALUE=\"");
	wprintf("%d", msgsposted);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("Access level"));
	wprintf("</TD><TD>"
		"<SELECT NAME=\"axlevel\">\n");
	for (i=0; i<7; ++i) {
		wprintf("<OPTION ");
		if (axlevel == i) {
			wprintf("SELECTED ");
		}
		wprintf("VALUE=\"%d\">%d - %s</OPTION>\n",
			i, i, axdefs[i]);
	}
	wprintf("</SELECT></TD></TR>\n");

	wprintf("<TR><TD>");
	wprintf(_("User ID number"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"usernum\" VALUE=\"");
	wprintf("%ld", usernum);
	wprintf("\" MAXLENGTH=\"7\"></TD></TR>\n");

	now = time(NULL);
	wprintf("<TR><TD>");
	wprintf(_("Date and time of last login"));
	wprintf("</TD><TD>"
		"<SELECT NAME=\"lastcall\">\n");

	wprintf("<OPTION SELECTED VALUE=\"%ld\">", lastcall);
	escputs(asctime(localtime(&lastcall)));
	wprintf("</OPTION>\n");

	wprintf("<OPTION VALUE=\"%ld\">", now);
	escputs(asctime(localtime(&now)));
	wprintf("</OPTION>\n");

	wprintf("</SELECT></TD></TR>");

	wprintf("<TR><TD>");
	wprintf(_("Auto-purge after this many days"));
	wprintf("</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"purgedays\" VALUE=\"");
	wprintf("%d", purgedays);
	wprintf("\" MAXLENGTH=\"5\"></TD></TR>\n");

	wprintf("</TABLE>\n");

	wprintf("<INPUT type=\"submit\" NAME=\"ok_button\" VALUE=\"%s\">\n"
		"&nbsp;"
		"<INPUT type=\"submit\" NAME=\"cancel\" VALUE=\"%s\">\n"
		"<br /><br /></FORM>\n", _("Save changes"), _("Cancel"));

	wprintf("</CENTER>\n");
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

}



void edituser(void) {
	char message[SIZ];
	char buf[SIZ];
	int is_new = 0;
	unsigned int flags = 0;

	is_new = atoi(bstr("is_new"));

	if (strlen(bstr("ok_button")) == 0) {
		safestrncpy(message, _("Changes were not saved."), sizeof message);
	}
	else {
		flags = atoi(bstr("flags"));
		if (!strcasecmp(bstr("inetmail"), "yes")) {
			flags |= US_INTERNET;
		}
		else {
			flags &= ~US_INTERNET ;
		}

		serv_printf("ASUP %s|%s|%d|%s|%s|%s|%s|%s|%s|",
			bstr("username"),
			bstr("password"),
			flags,
			bstr("timescalled"),
			bstr("msgsposted"),
			bstr("axlevel"),
			bstr("usernum"),
			bstr("lastcall"),
			bstr("purgedays")
		);
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			sprintf(message,
				"<img src=\"static/error.gif\" ALIGN=CENTER>"
				"%s<br /><br />\n", &buf[4]);
		}
		else {
			safestrncpy(message, "", sizeof message);
		}
	}

	/* If we are in the middle of creating a new user, move on to
	 * the vCard edit screen.
	 */
	if (is_new) {
		display_edit_address_book_entry( bstr("username"), atol(bstr("usernum")) );
	}
	else {
		select_user_to_edit(message, bstr("username"));
	}
}


void delete_user(char *username) {
	char buf[SIZ];
	char message[SIZ];

	serv_printf("ASUP %s|0|0|0|0|0|", username);
	serv_getln(buf, sizeof buf);
	if (buf[0] != '2') {
		sprintf(message,
			"<img src=\"static/error.gif\" ALIGN=CENTER>"
			"%s<br /><br />\n", &buf[4]);
	}
	else {
		safestrncpy(message, "", sizeof message);
	}
	select_user_to_edit(message, bstr("username"));
}
		



void create_user(void) {
	char buf[SIZ];
	char error_message[SIZ];
	char username[SIZ];

	safestrncpy(username, bstr("username"), sizeof username);

	serv_printf("CREU %s", username);
	serv_getln(buf, sizeof buf);

	if (buf[0] == '2') {
		sprintf(WC->ImportantMessage,
			_("A new user has been created."));
		display_edituser(username, 1);
	}
	else {
		sprintf(error_message,
			"<img src=\"static/error.gif\" ALIGN=CENTER>"
			"%s<br /><br />\n", &buf[4]);
		select_user_to_edit(error_message, NULL);
	}

}

