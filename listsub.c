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

	char buf[SIZ];
	int self;
	char sroom[SIZ];

	strcpy(WC->wc_username, "");
	strcpy(WC->wc_password, "");
	strcpy(WC->wc_roomname, "");

	wprintf("<HTML><HEAD><TITLE>List subscription</TITLE></HEAD><BODY>\n");

	strcpy(cmd, bstr("cmd"));
	strcpy(room, bstr("room"));
	strcpy(token, bstr("token"));
	strcpy(email, bstr("email"));
	strcpy(subtype, bstr("subtype"));

	wprintf("<CENTER>"
		"<TABLE WIDTH=100%% BORDER=0 BGCOLOR=000077><TR><TD>"
		"<FONT SIZE=+1 COLOR=\"FFFFFF\""
		"<B>List subscribe/unsubscribe</B>\n"
		"</TD></TR></TABLE><BR>\n"
	);

	/*
	 * Subscribe command
	 */
	if (!strcasecmp(cmd, "subscribe")) {
		serv_printf("SUBS subscribe|%s|%s|%s|%s/listsub",
			room,
			email,
			subtype,
			WC->http_host
		);
		serv_gets(buf);
		if (buf[0] == '2') {
			wprintf("<CENTER><H1>Confirmation request sent</H1>"
				"You are subscribing <TT>");
			escputs(email);
			wprintf("</TT> to the &quot;");
			escputs(room);
			wprintf("&quot; mailing list.  The listserver has "
				"sent you an e-mail with one additional "
				"Web link for you to click on to confirm "
				"your subscription.  This extra step is for "
				"your protection, as it prevents others from "
				"being able to subscribe you to lists "
				"without your consent.<BR><BR>"
				"Please click on the link which is being "
				"e-mailed to you and your subscription will "
				"be confirmed.<BR></CENTER>\n"
			);
		}
		else {
			wprintf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><BR><BR>\n",
				&buf[4]);
			goto FORM;
		}
	}

	/*
	 * Unsubscribe command
	 */
	else if (!strcasecmp(cmd, "unsubscribe")) {
		serv_printf("SUBS unsubscribe|%s|%s|%s/listsub",
			room,
			email,
			WC->http_host
		);
		serv_gets(buf);
		if (buf[0] == '2') {
			wprintf("<CENTER><H1>Confirmation request sent</H1>"
				"You are unsubscribing <TT>");
			escputs(email);
			wprintf("</TT> from the &quot;");
			escputs(room);
			wprintf("&quot; mailing list.  The listserver has "
				"sent you an e-mail with one additional "
				"Web link for you to click on to confirm "
				"your unsubscription.  This extra step is for "
				"your protection, as it prevents others from "
				"being able to unsubscribe you from "
				"lists without your consent.<BR><BR>"
				"Please click on the link which is being "
				"e-mailed to you and your unsubscription will "
				"be confirmed.<BR></CENTER>\n"
			);
		}
		else {
			wprintf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><BR><BR>\n",
				&buf[4]);
			goto FORM;
		}
	}

	/* 
	 * Confirm command
	 */
	else if (!strcasecmp(cmd, "confirm")) {
		serv_printf("SUBS confirm|%s|%s",
			room,
			token
		);
		serv_gets(buf);
		if (buf[0] == '2') {
			wprintf("<CENTER><H1>Confirmation successful!</H1>");
		}
		else {
			wprintf("<CENTER><H1>Confirmation failed.</H1>"
				"This could mean one of two things:<UL>\n"
				"<LI>You waited too long to confirm your "
				"subscribe/unsubscribe request (the "
				"confirmation link is only valid for three "
				"days)\n<LI>You have <i>already</i> "
				"successfully confirmed your "
				"subscribe/unsubscribe request and are "
				"attempting to do it again.</UL>\n"
				"The error returned by the server was: "
			);
		}
		wprintf("%s</CENTER><BR>\n", &buf[4]);
	}

	/*
	 * Any other (invalid) command causes the form to be displayed
	 */
	else {
FORM:		wprintf("<FORM METHOD=\"POST\" ACTION=\"/listsub\">\n"
			"<TABLE BORDER=0>\n"
		);

		wprintf("<TR><TD>Name of list</TD><TD>"
        		"<SELECT NAME=\"room\" SIZE=1>\n");

        	serv_puts("LPRM");
        	serv_gets(buf);
        	if (buf[0] == '1') {
                	while (serv_gets(buf), strcmp(buf, "000")) {
				extract(sroom, buf, 0);
				self = extract_int(buf, 4) & QR2_SELFLIST ;
				if (self) {
					wprintf("<OPTION VALUE=\"");
					escputs(sroom);
					wprintf("\">");
					escputs(sroom);
					wprintf("</OPTION>\n");
				}
                	}
		}
        	wprintf("</SELECT>"
			"</TD></TR>\n");

		wprintf("<TR><TD>Your e-mail address</TD><TD>"
			"<INPUT TYPE=\"text\" NAME=\"email\" "
			"VALUE=\""
		);
		escputs(email);
		wprintf("\" MAXLENGTH=128></TD></TR>\n");

		wprintf("</TABLE>"
			"(If subscribing) preferred format: "
			"<INPUT TYPE=\"radio\" NAME=\"subtype\""
			"VALUE=\"list\">One message at a time&nbsp; "
			"<INPUT TYPE=\"radio\" NAME=\"subtype\""
			"VALUE=\"digest\" CHECKED>Digest format&nbsp; "
			"<BR>\n"
			"<INPUT TYPE=\"submit\" NAME=\"cmd\""
			" VALUE=\"subscribe\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"cmd\""
			" VALUE=\"unsubscribe\">\n"
			"</FORM>\n"
		);

		wprintf("<BR>When you attempt to subscribe or unsubscribe to "
			"a mailing list, you will receive an e-mail containing"
			" one additional web link to click on for final "
			"confirmation.  This extra step is for your "
			"protection, as it prevents others from being able to "
			"subscribe or unsubscribe you to lists.<BR>\n"
		);

	}

	/*
	 * Since this isn't part of a normal Citadel session, we bail right
	 * out without maintaining any state.
	 */
	/* wDumpContent(2); */
	wprintf("</BODY></HTML>\n");
	end_webcit_session();
}
