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





void select_user_to_edit(char *message)
{
	char buf[SIZ];
	char username[SIZ];

	output_headers(3);	/* No room banner on this screen */

	if (message != NULL) wprintf(message);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>"
		"Add/change/delete user accounts"
		"</B></FONT></TD></TR></TABLE>\n");

	wprintf("<TABLE border=0 CELLSPACING=10><TR VALIGN=TOP>"
		"<TD>To edit an existing user account, select the user "
		"name from the list and click 'Edit'.<BR><BR>");
	
        wprintf("<CENTER><FORM METHOD=\"POST\" ACTION=\"/display_edituser\">\n");
        wprintf("<SELECT NAME=\"username\" SIZE=10>\n");
        serv_puts("LIST");
        serv_gets(buf);
        if (buf[0] == '1') {
                while (serv_gets(buf), strcmp(buf, "000")) {
                        extract(username, buf, 0);
                        wprintf("<OPTION>");
                        escputs(username);
                        wprintf("\n");
                }
        }
        wprintf("</SELECT><BR>\n");

        wprintf("<input type=submit name=sc value=\"Edit\">");
        wprintf("</FORM></CENTER>\n");

	wprintf("</TD><TD>"
		"To create a new user account, enter the desired "
		"user name in the box below and click 'Create'.<BR><BR>");

        wprintf("<CENTER><FORM METHOD=\"POST\" ACTION=\"/create_user\">\n");
        wprintf("New user: ");
        wprintf("<input type=text name=username><BR>\n"
        	"<input type=submit value=\"Create\">"
		"</FORM></CENTER>\n");

	wprintf("</TD></TR></TABLE>\n");

	wDumpContent(1);
}



/*
 * Edit a user.  If supplied_username is null, look in the "username"
 * web variable for the name of the user to edit.
 */
void display_edituser(char *supplied_username) {
	char buf[SIZ];
	char error_message[SIZ];

	char username[SIZ];
	char password[SIZ];
	unsigned int flags;
	int timescalled;
	int msgsposted;
	int axlevel;
	long usernum;
	time_t lastcall;
	int purgedays;

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
			"<IMG SRC=\"static/error.gif\" VALIGN=CENTER>"
			"%s<BR><BR>\n", &buf[4]);
		select_user_to_edit(error_message);
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

	output_headers(3);	/* No room banner on this screen */
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>"
		"Edit user account: ");
	escputs(username);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wprintf("<FORM METHOD=\"POST\" ACTION=\"/edituser\">\n");

	wprintf("<CENTER><TABLE>");

	wprintf("<TR><TD>Password</TD><TD>"
		"<INPUT TYPE=\"password\" NAME=\"password\" VALUE=\"");
	escputs(password);
	wprintf("\" MAXLENGTH=\"20\"></TD></TR>\n");

	wprintf("<TR><TD>Flags (FIXME)</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"flags\" VALUE=\"");
	wprintf("%d", flags);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>Times logged in</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"timescalled\" VALUE=\"");
	wprintf("%d", timescalled);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>Messages posted</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"msgsposted\" VALUE=\"");
	wprintf("%d", msgsposted);
	wprintf("\" MAXLENGTH=\"6\"></TD></TR>\n");

	wprintf("<TR><TD>Access level (FIXME) </TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"axlevel\" VALUE=\"");
	wprintf("%d", axlevel);
	wprintf("\" MAXLENGTH=\"1\"></TD></TR>\n");

	wprintf("<TR><TD>User ID number</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"usernum\" VALUE=\"");
	wprintf("%ld", usernum);
	wprintf("\" MAXLENGTH=\"7\"></TD></TR>\n");

	wprintf("<TR><TD>Date/time of last login</TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"lastcall\" VALUE=\"");
	escputs(asctime(localtime(&lastcall)));
	wprintf("\" MAXLENGTH=\"30\"></TD></TR>\n");

	wprintf("<TR><TD>Purge days (FIXME) </TD><TD>"
		"<INPUT TYPE=\"text\" NAME=\"purgedays\" VALUE=\"");
	wprintf("%d", purgedays);
	wprintf("\" MAXLENGTH=\"5\"></TD></TR>\n");

	wprintf("</TABLE>\n");

	wprintf("<INPUT type=\"submit\" NAME=\"action\" VALUE=\"OK\">\n"
		"<INPUT type=\"submit\" NAME=\"action\" VALUE=\"Cancel\">\n"
		"</CENTER>\n");

	wprintf("</FORM>\n");

	wDumpContent(1);

}



void create_user(void) {
	char buf[SIZ];
	char error_message[SIZ];
	char username[SIZ];

	strcpy(username, bstr("username"));

	serv_printf("CREU %s", username);
	serv_gets(buf);

	if (buf[0] == '2') {
		display_edituser(username);
	}
	else {
		sprintf(error_message,
			"<IMG SRC=\"static/error.gif\" VALIGN=CENTER>"
			"%s<BR><BR>\n", &buf[4]);
		select_user_to_edit(error_message);
	}

}
