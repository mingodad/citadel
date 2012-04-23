/*
 * Functions pertaining to rooms with a wiki view
 *
 * Copyright (c) 2009-2012 by the citadel.org team
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
#include "dav.h"

/* 
 * Convert a string to something suitable as a wiki index
 */
void str_wiki_index(char *s)
{
	int i;

	if (s == NULL) return;

	/* First remove all non-alphanumeric characters */
	for (i=0; i<strlen(s); ++i) {
		if (!isalnum(s[i])) {
			strcpy(&s[i], &s[i+1]);
		}
	}

	/* Then make everything lower case */
	for (i=0; i<strlen(s); ++i) {
		s[i] = tolower(s[i]);
	}
}

/*
 * Display a specific page from a wiki room
 *
 * "rev" may be set to an empty string to display the current version.
 * "do_revert" may be set to nonzero to perform a reversion to the specified version.
 */
void display_wiki_page_backend(char *pagename, char *rev, int do_revert)
{
	const StrBuf *Mime;
	long msgnum = (-1L);
	char buf[256];

	str_wiki_index(pagename);

	if (WC->CurRoom.view != VIEW_WIKI) {
		wc_printf(_("'%s' is not a Wiki room."), ChrPtr(WC->CurRoom.name) );
		return;
	}

	if (IsEmptyStr(pagename)) {
		strcpy(pagename, "home");
	}

	/* Found it!  Now read it. */

	if ((rev != NULL) && (strlen(rev) > 0)) {
		/* read an older revision */
		serv_printf("WIKI rev|%s|%s|%s", pagename, rev, (do_revert ? "revert" : "fetch") );
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			msgnum = extract_long(&buf[4], 0);
		}
	}
	else {
		/* read the current revision? */
		msgnum = locate_message_by_uid(pagename);
	}

	if (msgnum >= 0L) {
		read_message(WC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime);
		return;
	}

	wc_printf("<br><br>"
		"<div align=\"center\">"
		"<table border=\"0\" bgcolor=\"#ffffff\" cellpadding=\"10\">"
		"<tr><td align=\"center\">"
	);
	wc_printf("<br><b>");
	wc_printf(_("There is no page called '%s' here."), pagename);
	wc_printf("</b><br><br>");
	wc_printf(_("Select the 'Edit this page' link in the room banner "
		"if you would like to create this page."));
	wc_printf("<br><br>");
	wc_printf("</td></tr></table></div>\n");
}


/*
 * Display a specific page from a wiki room
 */
void display_wiki_page(void)
{
	char pagename[128];
	char rev[128];
	int do_revert = 0;

	output_headers(1, 1, 1, 0, 0, 0);
	safestrncpy(pagename, bstr("page"), sizeof pagename);
	str_wiki_index(pagename);
	safestrncpy(rev, bstr("rev"), sizeof rev);
	do_revert = atoi(bstr("revert"));
	display_wiki_page_backend(pagename, rev, do_revert);
	wDumpContent(1);
}


/*
 * Display the revision history for a wiki page (template callback)
 */
void tmplput_display_wiki_history(StrBuf *Target, WCTemplputParams *TP)
{
	char pagename[128];
	StrBuf *Buf;
	int row = 0;

	safestrncpy(pagename, bstr("page"), sizeof pagename);
	str_wiki_index(pagename);

	serv_printf("WIKI history|%s", pagename);
	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {

		time_t rev_date;
		char rev_date_displayed[64];
		StrBuf *rev_uuid = NewStrBuf();
		StrBuf *author = NewStrBuf();
		StrBuf *node = NewStrBuf();

		wc_printf("<table class=\"wiki_history_background\">");

		wc_printf("<th>%s</th>", _("Date"));
		wc_printf("<th>%s</th>", _("Author"));

		while((StrBuf_ServGetln(Buf) >= 0) &&  strcmp(ChrPtr(Buf), "000")) {

			rev_date = extract_long(ChrPtr(Buf), 1);
			webcit_fmt_date(rev_date_displayed, sizeof rev_date_displayed, rev_date, DATEFMT_FULL);
			StrBufExtract_token(author, Buf, 2, '|');

			wc_printf("<tr bgcolor=\"%s\">", ((row%2) ? "#FFFFFF" : "#DDDDDD"));
			wc_printf("<td>%s</td><td>", rev_date_displayed);
			if (!strcasecmp(ChrPtr(node), (char *)WC->serv_info->serv_nodename)) {
				escputs(ChrPtr(author));
				wc_printf(" @ ");
				escputs(ChrPtr(node));
			}
			else {
				wc_printf("<a href=\"showuser?who=");
				urlescputs(ChrPtr(author));
				wc_printf("\">");
				escputs(ChrPtr(author));
				wc_printf("</a>");
			}
			wc_printf("</td>");

			if (row == 0) {
				wc_printf("<td><a href=\"wiki?page=%s\">%s</a></td>",
					bstr("page"),
					_("(show)")
				);
				wc_printf("<td>(%s)</td>", _("Current version"));
			}

			else {
				wc_printf("<td><a href=\"wiki?page=%s?rev=%s\">%s</a></td>",
					bstr("page"),
					ChrPtr(rev_uuid),
					_("(show)")
				);
				wc_printf("<td><a href=\"javascript:GetLoggedInFirst(encodeURIComponent('wiki?page=%s?rev=%s?revert=1'))\">%s</a></td>",
					bstr("page"),
					ChrPtr(rev_uuid),
					_("(revert)")
				);
			}
			wc_printf("</tr>\n");

			/* Extract all fields except the author and date after displaying the row.  This
			 * is deliberate, because the timestamp reflects when the diff was written, not
			 * when the version which it reflects was written.  Similarly, the name associated
			 * with each diff is the author who created the newer version of the page that
			 * made the diff happen.
			 */
			StrBufExtract_token(rev_uuid, Buf, 0, '|');
			StrBufExtract_token(node, Buf, 3, '|');
			++row;
		}

		wc_printf("</table>\n");
		FreeStrBuf(&author);
		FreeStrBuf(&node);
		FreeStrBuf(&rev_uuid);
	}
	else {
		wc_printf("%s", ChrPtr(Buf));
	}

	FreeStrBuf(&Buf);
}



/*
 * Display the revision history for a wiki page
 */
void display_wiki_history(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	do_template("wiki_history");
	wDumpContent(1);
}


/*
 * Display a list of all pages in a Wiki room (template callback)
 */
void tmplput_display_wiki_pagelist(StrBuf *Target, WCTemplputParams *TP)
{
	StrBuf *Buf;
	int row = 0;

	if (!IsEmptyStr(bstr("query"))) {
		serv_printf("MSGS SEARCH|%s||4", bstr("query"));	/* search-reduced list */
	}
	else {
		serv_printf("MSGS ALL|||4");				/* full list */
	}

	Buf = NewStrBuf();
	StrBuf_ServGetln(Buf);
	if (GetServerStatus(Buf, NULL) == 1) {
		StrBuf *pagetitle = NewStrBuf();

		wc_printf("<table class=\"wiki_pagelist_background\">");
		wc_printf("<th>%s</th>", _("Page title"));

		while((StrBuf_ServGetln(Buf) >= 0) && strcmp(ChrPtr(Buf), "000")) {
			StrBufExtract_token(pagetitle, Buf, 1, '|');

			if (!bmstrcasestr((char *)ChrPtr(pagetitle), "_HISTORY_")) {	/* no history pages */
				wc_printf("<tr bgcolor=\"%s\">", ((row%2) ? "#FFFFFF" : "#DDDDDD"));
				wc_printf("<td><a href=\"wiki?page=");
				urlescputs(ChrPtr(pagetitle));
				wc_printf("\">");
				escputs(ChrPtr(pagetitle));
				wc_printf("</a></td>");
				wc_printf("</tr>\n");
				++row;
			}
		}
		wc_printf("</table>\n");
		FreeStrBuf(&pagetitle);
	}

	FreeStrBuf(&Buf);
}


/*
 * Display a list of all pages in a Wiki room.  Search requests in a Wiki room also go here.
 */
void display_wiki_pagelist(void)
{
	output_headers(1, 1, 1, 0, 0, 0);
	do_template("wiki_pagelist");
	wDumpContent(1);
}


int wiki_Cleanup(void **ViewSpecific)
{
	char pagename[5];
	safestrncpy(pagename, "home", sizeof pagename);
	display_wiki_page_backend(pagename, "", 0);
	wDumpContent(1);
	return 0;
}


int ConditionalHaveWikiPage(StrBuf *Target, WCTemplputParams *TP)
{
	const char *page;
	const char *pch;
	long len;

	page = BSTR("page");
	GetTemplateTokenString(Target, TP, 2, &pch, &len);
	return strcasecmp(page, pch) == 0;
}


int ConditionalHavewikiType(StrBuf *Target, WCTemplputParams *TP)
{
	wcsession *WCC = WC;
	const char *pch;
	long len;

	GetTemplateTokenString(Target, TP, 2, &pch, &len);
	return bmstrcasestr((char *)ChrPtr(WCC->Hdr->HR.ReqLine), pch) != NULL;
}


int wiki_PrintHeaderPage(SharedMessageStatus *Stat, void **ViewSpecific)
{
	/* this function was intentionaly left empty. */
	return 0;
}

int wiki_GetParamsGetServerCall(SharedMessageStatus *Stat, 
				void **ViewSpecific, 
				long oper, 
				char *cmd, 
				long len,
				char *filter,
				long flen)
{
	if (oper == do_search)
		display_wiki_pagelist();
	else 
		http_redirect("wiki?page=home");

	return 300;
}


void 
InitModule_WIKI
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_WIKI,
		wiki_GetParamsGetServerCall,
		wiki_PrintHeaderPage,
		NULL,
		NULL,
		NULL,
		NULL,
		wiki_Cleanup
	);

	WebcitAddUrlHandler(HKEY("wiki"), "", 0, display_wiki_page, 0);
	WebcitAddUrlHandler(HKEY("wiki_history"), "", 0, display_wiki_history, 0);
	WebcitAddUrlHandler(HKEY("wiki_pagelist"), "", 0, display_wiki_pagelist, 0);
	RegisterNamespace("WIKI:DISPLAYHISTORY", 0, 0, tmplput_display_wiki_history, NULL, CTX_NONE);
	RegisterNamespace("WIKI:DISPLAYPAGELIST", 0, 0, tmplput_display_wiki_pagelist, NULL, CTX_NONE);
	RegisterConditional(HKEY("COND:WIKI:PAGE"), 1, ConditionalHaveWikiPage, CTX_NONE);
	RegisterConditional(HKEY("COND:WIKI:TYPE"), 1, ConditionalHavewikiType, CTX_NONE);
}
