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

	wprintf("cmd: %s<BR>\n", cmd);
	wprintf("room: %s<BR>\n", room);
	wprintf("token: %s<BR>\n", token);
	wprintf("email: %s<BR>\n", email);
	wprintf("subtype: %s<BR>\n", subtype);

	/*
	 * Since this isn't part of a normal Citadel session, we bail right
	 * out without maintaining any state.
	 */
	wDumpContent(2);
	end_webcit_session();
}
