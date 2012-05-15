/*
 * Web forms for handling mailing list subscribe/unsubscribe requests.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"

/*
 * List subscription handling
 */
#ifndef EXPERIMENTAL_LISTSUB
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
	FlushStrBuf(WC->CurRoom.name);

	output_headers(1, 0, 0, 1, 1, 0);
	begin_burst();

	wc_printf("<HTML><HEAD>\n"
		"<meta name=\"MSSmartTagsPreventParsing\" content=\"TRUE\" />\n"
		"<link href=\"static/styles/webcit.css\" rel=\"stylesheet\" type=\"text/css\">\n"
		"<TITLE>\n"
	);
	wc_printf(_("List subscription"));
	wc_printf("</TITLE></HEAD><BODY>\n");

	strcpy(cmd, bstr("cmd"));
	strcpy(room, bstr("room"));
	strcpy(token, bstr("token"));
	strcpy(email, bstr("email"));
	strcpy(subtype, bstr("subtype"));

	wc_printf("<div align=center>");
	wc_printf("<table border=0 width=75%%><tr><td>");

	do_template("box_begin_1");
	StrBufAppendBufPlain(WC->WBuf, _("List subscribe/unsubscribe"), -1, 0);
	do_template("box_begin_2");
	wc_printf("<div align=center><br>");

	/*
	 * Subscribe command
	 */
	if (!strcasecmp(cmd, "subscribe")) {
		serv_printf("SUBS subscribe|%s|%s|%s|%s/listsub",
			room,
			email,
			subtype,
			ChrPtr(site_prefix)
		);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			stresc(escaped_email, 256, email, 0, 0);
			stresc(escaped_room, 256, room, 0, 0);

			wc_printf("<CENTER><H1>");
			wc_printf(_("Confirmation request sent"));
			wc_printf("</H1>");
			wc_printf(_("You are subscribing <TT>%s"
				"</TT> to the <b>%s</b> mailing list.  "
				"The listserver has "
				"sent you an e-mail with one additional "
				"Web link for you to click on to confirm "
				"your subscription.  This extra step is for "
				"your protection, as it prevents others from "
				"being able to subscribe you to lists "
				"without your consent.<br><br>"
				"Please click on the link which is being "
				"e-mailed to you and your subscription will "
				"be confirmed.<br>\n"),
				escaped_email, escaped_room);
			wc_printf("<a href=\"listsub\">%s</A></CENTER>\n", _("Go back..."));
		}
		else {
			wc_printf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><br><br>\n",
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
			ChrPtr(site_prefix)
		);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			wc_printf("<CENTER><H1>Confirmation request sent</H1>"
				"You are unsubscribing <TT>");
			escputs(email);
			wc_printf("</TT> from the &quot;");
			escputs(room);
			wc_printf("&quot; mailing list.  The listserver has "
				"sent you an e-mail with one additional "
				"Web link for you to click on to confirm "
				"your unsubscription.  This extra step is for "
				"your protection, as it prevents others from "
				"being able to unsubscribe you from "
				"lists without your consent.<br><br>"
				"Please click on the link which is being "
				"e-mailed to you and your unsubscription will "
				"be confirmed.<br>\n"
				"<a href=\"listsub\">Back...</A></CENTER>\n"
			);
		}
		else {
			wc_printf("<FONT SIZE=+1><B>ERROR: %s</B>"
				"</FONT><br><br>\n",
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
			wc_printf("<CENTER><H1>Confirmation successful!</H1>");
		}
		else {
			wc_printf("<CENTER><H1>Confirmation failed.</H1>"
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
		wc_printf("%s</CENTER><br>\n", &buf[4]);
	}

	/*
	 * Any other (invalid) command causes the form to be displayed
	 */
	else {
FORM:		wc_printf("<form method=\"POST\" action=\"listsub\">\n");

		wc_printf("Name of list: "
        		"<select name=\"room\" size=1>\n");

        	serv_puts("LPRM");
        	serv_getln(buf, sizeof buf);
        	if (buf[0] == '1') {
                	while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
				extract_token(sroom, buf, 0, '|', sizeof sroom);
				self = extract_int(buf, 4) & QR2_SELFLIST ;
				if (self) {
					wc_printf("<option value=\"");
					escputs(sroom);
					wc_printf("\">");
					escputs(sroom);
					wc_printf("</option>\n");
				}
                	}
		}
        	wc_printf("</select><br><br>\n");

		wc_printf("Your e-mail address: "
			"<INPUT TYPE=\"text\" NAME=\"email\" "
			"VALUE=\""
		);
		escputs(email);
		wc_printf("\" maxlength=128 size=60><br><br>\n");

		wc_printf("(If subscribing) preferred format: "
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

		wc_printf("<hr>When you attempt to subscribe or unsubscribe to "
			"a mailing list, you will receive an e-mail containing"
			" one additional web link to click on for final "
			"confirmation.  This extra step is for your "
			"protection, as it prevents others from being able to "
			"subscribe or unsubscribe you to lists.<br>\n"
		);

	}

	wc_printf("</div>");
	do_template("box_end");
	wc_printf("</td></tr></table></div>");

	wc_printf("</BODY></HTML>\n");
	wDumpContent(0);
	end_webcit_session();
}
#endif

int Conditional_LISTSUB_EXECUTE_SUBSCRIBE(StrBuf *Target, WCTemplputParams *TP)
{
	int rc;
	StrBuf *Line;
	const char *ImpMsg;
	const StrBuf *Room, *Email, *SubType;
	
	Room = sbstr("room");
	if (Room == NULL)
	{
		ImpMsg = _("You need to specify the mailinglist to subscribe to.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}
	Email = sbstr("email");
	if (Email == NULL)
	{
		ImpMsg = _("You need to specify the email address you'd like to subscribe with.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}
	SubType = sbstr("subtype");

	Line = NewStrBuf();
	serv_printf("SUBS subscribe|%s|%s|%s|%s/listsub",
		    ChrPtr(Room),
		    ChrPtr(Email),
		    ChrPtr(SubType),
		    ChrPtr(site_prefix)
		);
	StrBuf_ServGetln(Line);
	rc = GetServerStatusMsg(Line, NULL, 1, 2);
	FreeStrBuf(&Line);
	if (rc == 2)
		putbstr("__FAIL", NewStrBufPlain(HKEY("1")));
	return rc == 2;
}

int Conditional_LISTSUB_EXECUTE_UNSUBSCRIBE(StrBuf *Target, WCTemplputParams *TP)
{
	int rc;
	StrBuf *Line;
	const char *ImpMsg;
	const StrBuf *Room, *Email;
	
	Room = sbstr("room");
	if (Room == NULL)
	{
		ImpMsg = _("You need to specify the mailinglist to subscribe to.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}
	Email = sbstr("email");
	if (Email == NULL)
	{
		ImpMsg = _("You need to specify the email address you'd like to subscribe with.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}

	serv_printf("SUBS unsubscribe|%s|%s|%s/listsub",
		    ChrPtr(Room),
		    ChrPtr(Email),
		    ChrPtr(site_prefix)
		);
	Line = NewStrBuf();
	StrBuf_ServGetln(Line);
	rc = GetServerStatusMsg(Line, NULL, 1, 2);
	FreeStrBuf(&Line);
	if (rc == 2)
		putbstr("__FAIL", NewStrBufPlain(HKEY("1")));
	return rc == 2;
}

int Conditional_LISTSUB_EXECUTE_CONFIRM_SUBSCRIBE(StrBuf *Target, WCTemplputParams *TP)
{
	int rc;
	StrBuf *Line;
	const char *ImpMsg;
	const StrBuf *Room, *Token;
	
	Room = sbstr("room");
	if (Room == NULL)
	{
		ImpMsg = _("You need to specify the mailinglist to subscribe to.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}
	Token = sbstr("token");
	if (Room == NULL)
	{
		ImpMsg = _("You need to specify the mailinglist to subscribe to.");
		AppendImportantMessage(ImpMsg, -1);
		return 0;
	}

	Line = NewStrBuf();
	serv_printf("SUBS confirm|%s|%s",
		    ChrPtr(Room),
		    ChrPtr(Token)
		);
	StrBuf_ServGetln(Line);
	rc = GetServerStatusMsg(Line, NULL, 1, 2);
	FreeStrBuf(&Line);
	if (rc == 2)
		putbstr("__FAIL", NewStrBufPlain(HKEY("1")));
	return rc == 2;
}

#ifdef EXPERIMENTAL_LISTSUB
void do_listsub(void)
{
	if (!havebstr("cmd"))
	{
		putbstr("cmd", NewStrBufPlain(HKEY("")));
	}
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("listsub_display");
	end_burst();
}
#endif

void 
InitModule_LISTSUB
(void)
{
	RegisterConditional(HKEY("COND:LISTSUB:EXECUTE:SUBSCRIBE"), 0, Conditional_LISTSUB_EXECUTE_SUBSCRIBE,  CTX_NONE);
	RegisterConditional(HKEY("COND:LISTSUB:EXECUTE:UNSUBSCRIBE"), 0, Conditional_LISTSUB_EXECUTE_UNSUBSCRIBE,  CTX_NONE);
	RegisterConditional(HKEY("COND:LISTSUB:EXECUTE:CONFIRM:SUBSCRIBE"), 0, Conditional_LISTSUB_EXECUTE_CONFIRM_SUBSCRIBE,  CTX_NONE);

	WebcitAddUrlHandler(HKEY("listsub"), "", 0, do_listsub, ANONYMOUS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);


}
