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

int Conditional_LISTSUB_EXECUTE_SUBSCRIBE(StrBuf *Target, WCTemplputParams *TP)
{
	int rc;
	StrBuf *Line;
	const char *ImpMsg;
	const StrBuf *Room, *Email, *SubType;

	if (strcmp(bstr("cmd"), "subscribe")) {
		return 0;
	}

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

	if (strcmp(bstr("cmd"), "unsubscribe")) {
		return 0;
	}

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

	if (strcmp(bstr("cmd"), "confirm")) {
		return 0;
	}

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

void do_listsub(void)
{
	if (!havebstr("cmd"))
	{
		putbstr("cmd", NewStrBufPlain(HKEY("choose")));
	}
	output_headers(1, 0, 0, 0, 1, 0);
	do_template("listsub_display");
	end_burst();
}

void 
InitModule_LISTSUB
(void)
{
	RegisterConditional("COND:LISTSUB:EXECUTE:SUBSCRIBE", 0, Conditional_LISTSUB_EXECUTE_SUBSCRIBE,  CTX_NONE);
	RegisterConditional("COND:LISTSUB:EXECUTE:UNSUBSCRIBE", 0, Conditional_LISTSUB_EXECUTE_UNSUBSCRIBE,  CTX_NONE);
	RegisterConditional("COND:LISTSUB:EXECUTE:CONFIRM:SUBSCRIBE", 0, Conditional_LISTSUB_EXECUTE_CONFIRM_SUBSCRIBE,  CTX_NONE);

	WebcitAddUrlHandler(HKEY("listsub"), "", 0, do_listsub, ANONYMOUS|COOKIEUNNEEDED|FORCE_SESSIONCLOSE);


}
