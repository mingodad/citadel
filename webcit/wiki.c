/*
 * $Id$
 *
 * Functions pertaining to rooms with a wiki view
 */

#include "webcit.h"
#include "groupdav.h"

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
 */
void display_wiki_page_backend(const StrBuf *roomname, char *pagename)
{
	const StrBuf *Mime;
	long msgnum = (-1L);

	str_wiki_index(pagename);

	if (StrLength(roomname) > 0) {

		/* If we're not in the correct room, try going there. */
		if (strcasecmp(ChrPtr(roomname), ChrPtr(WC->wc_roomname))) {
			gotoroom(roomname);
		}
	
		/* If we're still not in the correct room, it doesn't exist. */
		if (strcasecmp(ChrPtr(roomname), ChrPtr(WC->wc_roomname))) {
			wprintf(_("There is no room called '%s'."), ChrPtr(roomname));
			return;
		}

	}

	if (WC->wc_view != VIEW_WIKI) {
		wprintf(_("'%s' is not a Wiki room."), ChrPtr(roomname));
		return;
	}

	if (IsEmptyStr(pagename)) {
		strcpy(pagename, "home");
	}

	/* Found it!  Now read it. */
	msgnum = locate_message_by_uid(pagename);
	if (msgnum >= 0L) {
		read_message(WC->WBuf, HKEY("view_message"), msgnum, NULL, &Mime);
		return;
	}

	wprintf("<br /><br />"
		"<div align=\"center\">"
		"<table border=\"0\" bgcolor=\"#ffffff\" cellpadding=\"10\">"
		"<tr><td align=\"center\">"
	);
	wprintf("<br><b>");
	wprintf(_("There is no page called '%s' here."), pagename);
	wprintf("</b><br><br>");
	wprintf(_("Select the 'Edit this page' link in the room banner "
		"if you would like to create this page."));
	wprintf("<br><br>");
	wprintf("</td></tr></table></div>\n");
}


/*
 * Display a specific page from a wiki room
 */
void display_wiki_page(void)
{
	const StrBuf *roomname;
	char pagename[128];

	output_headers(1, 1, 1, 0, 0, 0);
	roomname = sbstr("room");
	safestrncpy(pagename, bstr("page"), sizeof pagename);
	display_wiki_page_backend(roomname, pagename);
	wDumpContent(1);
}


int wiki_Cleanup(void **ViewSpecific)
{
	char pagename[5];
	safestrncpy(pagename, "home", sizeof pagename);
	display_wiki_page_backend(WC->wc_roomname, pagename);
	wDumpContent(1);
	return 0;
}

void 
InitModule_WIKI
(void)
{
	RegisterReadLoopHandlerset(
		VIEW_WIKI,
		NULL,
		NULL,
		NULL,
		NULL,
		wiki_Cleanup
	);

	WebcitAddUrlHandler(HKEY("wiki"), display_wiki_page, 0);
}


