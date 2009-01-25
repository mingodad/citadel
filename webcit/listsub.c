/*
 * $Id$
 *
 * Web forms for handling mailing list subscribe/unsubscribe requests.
 */

#include "webcit.h"

/*
 * List subscription handling
 */
void do_listsub(void)
{
	char cmd[256];
	char room[256];
	char token[256];
	char email[256];
	char subtype[256];
	char escaped_email[256];
	char escaped_room[256];

	char buf[SIZ];
	int self;
	char sroom[SIZ];

	FlushStrBuf(WC->wc_fullname);
	FlushStrBuf(WC->wc_username);
	FlushStrBuf(WC->wc_password);
	FlushStrBuf(WC->wc_roomname);

	output_headers(1, 0, 0, 1, 1, 0);
	begin_burst();

	wprintf("<HTML><HEAD>\n"
		"<meta name=\"MSSmartTagsPreventParsing\" content=\"TRUE\" />\n"
		"<link href=\"static/webcit.css\" rel=\"stylesheet\" type=\"text/css\">\n"
		"<TITLE>\n"
	);
	wprintf(_("List subscription"));
	wprintf("</TITLE></HEAD><BODY>\n");

	strcpy(cmd, bstr("cmd"));
	strcpy(room, bstr("room"));
	strcpy(token, bstr("token"));
	strcpy(email, bstr("email"));
	strcpy(subtype, bstr("subtype"));

	wprintf("<div align=center>");
	wprintf("<table border=0 width=75%%><tr><td>");

	svput("BOXTITLE", WCS_STRING, _("List subscribe/unsubscribe"));
	do_template("beginboxx", NULL);
	wprintf("<div align=center><br>");

	/*
	 * Subscribe command
	 */
	if (!strcasecmp(cmd, "subscribe")) {
		serv_printf("SUBS subscribe|%s|%s|%s|%s://%s/listsub",
			room,
			email,
			subtype,
			(is_https ? "https" : "http"),
			    ChrPtr(WC->http_host)
		);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			stresc(escaped_email, 256, email, 0, 0);
			stresc(escaped_room, 256, room, 0, 0);

			wprintf("<CENTER><H1>");
			wprintf(_("Confirmation request sent"));
			wprintf("</H1>");
			wprintf(_("You are subscribing <TT>%s"
				"</TT> to the <b>%s</b> mailing list.  "
				"The listserver has "
				"sent you an e-mail with one additional "
				"Web link for you to click on to confirm "
				"your subscription.  This extra step is for "
				"your protection, as it prevents others from "
				"being able to subscribe you to lists "
				"without your consent.<br /><br />"
				"Please click on the link which is being "
				"e-mailed to you and your subscription will "
				"be confirmed.<br />\n"),
				escaped_email, escaped_room);
			wprintf("<a href=\"listsub\">%s</A></CENTER>\n", _("Go back..."));
		}
		else {
			wprintf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><br /><br />\n",
				&buf[4]);
			goto FORM;
		}
	}

	/*
	 * Unsubscribe command
	 */
	else if (!strcasecmp(cmd, "unsubscribe")) {
		serv_printf("SUBS unsubscribe|%s|%s|%s://%s/listsub",
			    room,
			    email,
			    (is_https ? "https" : "http"),
			    ChrPtr(WC->http_host)
		);
		serv_getln(buf, sizeof buf);
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
				"lists without your consent.<br /><br />"
				"Please click on the link which is being "
				"e-mailed to you and your unsubscription will "
				"be confirmed.<br />\n"
				"<a href=\"listsub\">Back...</A></CENTER>\n"
			);
		}
		else {
			wprintf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><br /><br />\n",
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
		serv_getln(buf, sizeof buf);
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
		wprintf("%s</CENTER><br />\n", &buf[4]);
	}

	/*
	 * Any other (invalid) command causes the form to be displayed
	 */
	else {
FORM:		wprintf("<form method=\"POST\" action=\"listsub\">\n");

		wprintf("Name of list: "
        		"<select name=\"room\" size=1>\n");

        	serv_puts("LPRM");
        	serv_getln(buf, sizeof buf);
        	if (buf[0] == '1') {
                	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(sroom, buf, 0, '|', sizeof sroom);
				self = extract_int(buf, 4) & QR2_SELFLIST ;
				if (self) {
					wprintf("<option value=\"");
					escputs(sroom);
					wprintf("\">");
					escputs(sroom);
					wprintf("</option>\n");
				}
                	}
		}
        	wprintf("</select><br><br>\n");

		wprintf("Your e-mail address: "
			"<INPUT TYPE=\"text\" NAME=\"email\" "
			"VALUE=\""
		);
		escputs(email);
		wprintf("\" maxlength=128 size=60><br><br>\n");

		wprintf("(If subscribing) preferred format: "
			"<INPUT TYPE=\"radio\" NAME=\"subtype\" "
			"VALUE=\"list\" CHECKED>One message at a time&nbsp; "
			"<INPUT TYPE=\"radio\" NAME=\"subtype\" "
			"VALUE=\"digest\">Digest format&nbsp; "
			"<br><br>\n"
			"<INPUT TYPE=\"submit\" NAME=\"cmd\""
			" VALUE=\"subscribe\">\n"
			"<INPUT TYPE=\"submit\" NAME=\"cmd\""
			" VALUE=\"unsubscribe\"><br><br>\n"
			"</FORM>\n"
		);

		wprintf("<hr>When you attempt to subscribe or unsubscribe to "
			"a mailing list, you will receive an e-mail containing"
			" one additional web link to click on for final "
			"confirmation.  This extra step is for your "
			"protection, as it prevents others from being able to "
			"subscribe or unsubscribe you to lists.<br />\n"
		);

	}

	wprintf("</div>");
	do_template("endbox", NULL);
	wprintf("</td></tr></table></div>");

	wprintf("</BODY></HTML>\n");
	wDumpContent(0);
	end_webcit_session();
}
