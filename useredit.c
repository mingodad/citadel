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
	char username[SIZ];
	char buf[SIZ];
	char error_message[SIZ];

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

	output_headers(3);	/* No room banner on this screen */

	wprintf("this is %s", username);

	wDumpContent(1);

}



void create_user(void) {
	char buf[SIZ];
	char username[SIZ];
	char error_message[SIZ];

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
		return;
	}

	output_headers(3);

	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=007700><TR><TD>");
	wprintf("<FONT SIZE=+1 COLOR=\"FFFFFF\"<B>"
		"Edit user account: ");
	escputs(username);
	wprintf("</B></FONT></TD></TR></TABLE>\n");

	wDumpContent(1);

}
