/*
 * Administrative screen to add/change/delete user accounts
 *
 */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "webserver.h"





void select_user_to_edit(char *message, char *preselect)
{
	char buf[SIZ];
	char username[SIZ];

	output_headers(3);	/* No room banner on this screen */

	if (message != NULL) wprintf(message);

	wprintf("<TABLE border=0 CELLSPACING=10><TR VALIGN=TOP><TD>\n");

	svprintf("BOXTITLE", WCS_STRING, "Edit or Delete users");
	do_template("beginbox");

	wprintf("To edit an existing user account, select the user "
		"name from the list and click 'Edit'.<BR><BR>");
	
        wprintf("<CENTER>"
		"<FORM METHOD=\"POST\" ACTION=\"/display_edituser\">\n");
        wprintf("<SELECT NAME=\"username\" SIZE=10>\n");
        serv_puts("LIST");
        serv_gets(buf);
        if (buf[0] == '1') {
                while (serv_gets(buf), strcmp(buf, "000")) {
                        extract(username, buf, 0);
                        wprintf("<OPTION");
			if (preselect != NULL)
			   if (!strcasecmp(username, preselect))
			      wprintf(" SELECTED");
			wprintf(">");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</SELECT><BR>\n");

        wprintf("<input type=submit name=sc value=\"Edit configuration\">");
        wprintf("<input type=submit name=sc value=\"Edit address book entry\">");
        wprintf("</FORM></CENTER>\n");
	do_template("endbox");

	wprintf("</TD><TD>");

	svprintf("BOXTITLE", WCS_STRING, "Add users");
	do_template("beginbox");

	wprintf("To create a new user account, enter the desired "
		"user name in the box below and click 'Create'.<BR><BR>");

        wprintf("<CENTER><FORM METHOD=\"POST\" ACTION=\"/create_user\">\n");
        wprintf("New user: ");
        wprintf("<input type=text name=username><BR>\n"
        	"<input type=submit value=\"Create\">"
		"</FORM></CENTER>\n");

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
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		ptr = malloc(sizeof(struct stuff_t));
		ptr->msgnum = atol(buf);
		ptr->next = stuff;
		stuff = ptr;
	}

	/* Iterate through the message list looking for vCards */
	while (stuff != NULL) {
		serv_printf("MSG0 %ld|2", stuff->msgnum);
		serv_gets(buf);
		if (buf[0]=='1') {
			while(serv_gets(buf), strcmp(buf, "000")) {
				if (!strncasecmp(buf, "part=", 5)) {
					extract(partnum, &buf[5], 2);
					extract(content_type, &buf[5], 4);
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
		serv_gets(buf);
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
	serv_gets(buf);
	if (buf[0] != '2') {
		serv_printf("CRE8 1|%s|5|||1|", roomname);
		serv_gets(buf);
		serv_printf("GOTO %s||1", roomname);
		serv_gets(buf);
		if (buf[0] != '2') {
			sprintf(error_message,
				"<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
				"%s<BR><BR>\n", &buf[4]);
			select_user_to_edit(error_message, username);
			return;
		}
	}

	vcard_msgnum = locate_user_vcard(username, usernum);

	if (vcard_msgnum < 0) {
		sprintf(error_message,
			"<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
			"Could not create/edit vCard"
			"<BR><BR>\n"
		);
		select_user_to_edit(error_message, username);
		return;
	}

	do_edit_vcard(vcard_msgnum, "1", "/select_user_to_edit");
}




/*
 * Edit a user.  If supplied_username is null, look in the "username"
 * web variable for the name of the user to edit.
 */
void display_edituser(char *supplied_username) {
	char buf[SIZ];
	char error_message[SIZ];
	time_t now;

	char username[SIZ];
	char password[SIZ];
	unsigned int flags;
	int timescalled;
	int msgsposted;
	int axlevel;
	long usernum;
	time_t lastcall;
	int purgedays;
	int i;

	if (supplied_username != NULL) {
		strcpy(username, supplied_username);
	}
	else {
		strcpy(username, bstr("username") );
	}

	serv_printf("AGUP %s", username);
	serv_gets(buf);
	if (buf[0] != '2') {
		sprintf(error_message,
			"<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
			"%s<BR><BR>\n", &buf[4]);
		select_user_to_edit(error_message, username);
		return;
	}

	extract(username, &buf[4], 0);
	extract(password, &buf[4], 1);
	flags = extract_int(&buf[4], 2);
	timescalled = extract_int(&buf[4], 3);
	msgsposted = extract_int(&buf[4], 4);
	axlevel = extract_int(&buf[4], 5);
	usernum = extract_long(&buf[4], 6);
	lastcall = extract_long(&buf[4], 7);
	purgedays = extract_long(&buf[4], 8);

	if (!strcmp(bstr("sc"), "Edit address book entry")) {
		display_edit_address_book_entry(username, usernum);
		return;
	}

	output_headers(3);	/* No room banner on this screen */
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#007700\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">"
		"Edit user account: ");
	escputs(username);
	wprintf("</SPAN></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/edituser\">\n"
		"<INPUT TYPE=\"hidden\" NAME=\"username\" VALUE=\"");
	escputs(username);
	wprintf("\">\n");

	wprintf("<INPUT TYPE=\"hidden\" NAME=\"flags\" VALUE=\"%d\">\n", flags);

	wprintf("<CENTER><TABLE>");

	wprintf("<TR><TD>Password</TD><TD>"
		"<INPUT TYPE=\"password\" NAME=\"password\" VALUE=\"");
	escputs(password);
	wprintf("\" MAXLENGTH=\"20\"></TD></TR>\n");

	wprintf("<TR><TD>Times logged in</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"timescalled\" VALUE=\"");
	wprintf("%d", timescalled);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>Messages posted</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"msgsposted\" VALUE=\"");
	wprintf("%d", msgsposted);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>Access level</TD><TD>"
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

	wprintf("<TR><TD>User ID number</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"usernum\" VALUE=\"");
	wprintf("%ld", usernum);
	wprintf("\" MAXLENGTH=\"7\"></TD></TR>\n");

	now = time(NULL);
	wprintf("<TR><TD>Date/time of last login</TD><TD>"
		"<SELECT NAME=\"lastcall\">\n");

	wprintf("<OPTION SELECTED VALUE=\"%ld\">", lastcall);
	escputs(asctime(localtime(&lastcall)));
	wprintf("</OPTION>\n");

	wprintf("<OPTION VALUE=\"%ld\">", now);
	escputs(asctime(localtime(&now)));
	wprintf("</OPTION>\n");

	wprintf("</SELECT></TD></TR>");

	wprintf("<TR><TD>Auto-purge after days</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"purgedays\" VALUE=\"");
	wprintf("%d", purgedays);
	wprintf("\" MAXLENGTH=\"5\"></TD></TR>\n");

	wprintf("</TABLE>\n");

	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"OK\">\n"
		"<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Cancel\">\n"
		"<BR><BR></FORM>\n");

	wprintf("</CENTER>\n");

	wDumpContent(1);

}



void edituser(void) {
	char message[SIZ];
	char buf[SIZ];

	if (strcasecmp(bstr("action"), "OK")) {
		strcpy(message, "Edit user cancelled.");
	}

	else {

		serv_printf("ASUP %s|%s|%s|%s|%s|%s|%s|%s|%s|",
			bstr("username"),
			bstr("password"),
			bstr("flags"),
			bstr("timescalled"),
			bstr("msgsposted"),
			bstr("axlevel"),
			bstr("usernum"),
			bstr("lastcall"),
			bstr("purgedays")
		);
		serv_gets(buf);
		if (buf[0] != '2') {
			sprintf(message,
				"<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
				"%s<BR><BR>\n", &buf[4]);
		}
		else {
			strcpy(message, "");
		}
	}

	select_user_to_edit(message, bstr("username"));
}




void create_user(void) {
	char buf[SIZ];
	char error_message[SIZ];
	char username[SIZ];

	strcpy(username, bstr("username"));

	serv_printf("CREU %s", username);
	serv_gets(buf);

	if (buf[0] == '2') {
		sprintf(error_message, "<b>User has been created.</b>");
		select_user_to_edit(error_message, username);
	}
	else {
		sprintf(error_message,
			"<IMG SRC=\"static/error.gif\" ALIGN=CENTER>"
			"%s<BR><BR>\n", &buf[4]);
		select_user_to_edit(error_message, NULL);
	}

}

