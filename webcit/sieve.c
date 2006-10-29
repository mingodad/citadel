/* 
 * $Id: $
 */
/**
 * \defgroup Sieve view/edit sieve config
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"


/**
 * \brief view/edit sieve config
 */
void display_sieve(void)
{
	output_headers(1, 1, 2, 0, 0, 0);

	wprintf("<div id=\"banner\">\n");
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">");
	wprintf(_("View/edit server-side mail filters"));
	wprintf("</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");
	wprintf("</div>\n<div id=\"content\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% bgcolor=\"#FFFFFF\">"
		"<tr><td valign=top>\n");

	wprintf("FIXME there is nothing here yet.");

	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

}




/*@}*/
