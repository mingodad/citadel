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



/*
 * List subscription handling
 */
void do_listsub(void)
{
	char cmd[SIZ];
	char room[SIZ];
	char token[SIZ];
	char email[SIZ];
	char subtype[SIZ];

	strcpy(WC->wc_username, "");
	strcpy(WC->wc_password, "");
	strcpy(WC->wc_roomname, "");

	output_headers(2);	/* note "2" causes cookies to be unset */

	strcpy(cmd, bstr("cmd"));
	strcpy(room, bstr("room"));
	strcpy(token, bstr("token"));
	strcpy(email, bstr("email"));
	strcpy(subtype, bstr("subtype"));

	/*
	 * Subscribe command
	 */
	if (!strcasecmp(cmd, "xx")) {
	}
	
	/*
	 * Any other (invalid) command causes the form to be displayed
	 */
	else {
		wprintf("<CENTER>"
			"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=770000><TR><TD>"
			"<FONT SIZE=+1 COLOR=\"FFFFFF\""
			"<B>List subscribe/unsubscribe</B>\n"
			"</TD></TR></TABLE><BR>\n"
		);

		wprintf("<TABLE BORDER=0>\n"
			"<FORM METHOD=\"POST\" ACTION=\"/listsub\">\n"
		);

		wprintf("<TR><TD>Name of list</TD>"
			"<TD>xx</TD></TR>\n"
		);

		wprintf("</TABLE>"
			"<INPUT TYPE=\"submit\" NAME=\"sc\""
			" VALUE=\"Submit\">\n"
			"</CENTER></FORM>\n"
		);
	}

	/*
	 * Since this isn't part of a normal Citadel session, we bail right
	 * out without maintaining any state.
	 */
	wDumpContent(2);
	end_webcit_session();
}
