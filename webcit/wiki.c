/*
 * $Id$
 */
/**
 *
 * \defgroup Wiki Wiki; Functions pertaining to rooms with a wiki view
 * \ingroup WebcitDisplayItems
 */

/*@{*/
#include "webcit.h"
#include "groupdav.h"



/** 
 * \brief Convert a string to something suitable as a wiki index
 *
 * \param s The string to be converted.
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

/**
 * \brief Display a specific page from a wiki room
 */
void display_wiki_page(void)
{
	char roomname[128];
	char pagename[128];
	char errmsg[256];
	long msgnum = (-1L);

	safestrncpy(roomname, bstr("room"), sizeof roomname);
	safestrncpy(pagename, bstr("page"), sizeof pagename);
	str_wiki_index(pagename);

	if (!IsEmptyStr(roomname)) {

		/* If we're not in the correct room, try going there. */
		if (strcasecmp(roomname, WC->wc_roomname)) {
			gotoroom(roomname);
		}
	
		/* If we're still not in the correct room, it doesn't exist. */
		if (strcasecmp(roomname, WC->wc_roomname)) {
			snprintf(errmsg, sizeof errmsg,
				_("There is no room called '%s'."),
				roomname);
			convenience_page("FF0000", _("Error"), errmsg);
			return;
		}

	}

	if (WC->wc_view != VIEW_WIKI) {
		snprintf(errmsg, sizeof errmsg,
			_("'%s' is not a Wiki room."),
			roomname);
		convenience_page("FF0000", _("Error"), errmsg);
		return;
	}

	if (IsEmptyStr(pagename)) {
		strcpy(pagename, "home");
	}

	/* Found it!  Now read it. */
	msgnum = locate_message_by_uid(pagename);
	if (msgnum >= 0L) {
		output_headers(1, 1, 1, 0, 0, 0);
		read_message(msgnum, 0, "");
		wDumpContent(1);
		return;
	}

	output_headers(1, 1, 1, 0, 0, 0);
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
	wDumpContent(1);
}


/** @} */
