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

	wprintf("<script type=\"text/javascript\">					\n"
		"function ToggleSievePanels() {						\n"
		" d = ($('sieveform').bigaction.options[$('sieveform').bigaction.selectedIndex].value);	\n"
		" for (i=0; i<3; ++i) {							\n"
		"  if (i == d) {							\n"
		"   $('sievediv' + i).style.display = 'block';				\n"
		"  }									\n"
		"  else {								\n"
		"   $('sievediv' + i).style.display = 'none';				\n"
		"  }									\n"
		" }									\n"
		"}									\n"
		"</script>								\n"
	);

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


	wprintf("<form id=\"sieveform\" method=\"post\" action=\"save_sieve\">\n");

	wprintf(_("When new mail arrives: "));
        wprintf("<select name=\"bigaction\" size=1 onChange=\"ToggleSievePanels();\">\n");

	wprintf("<option value=\"0\">");
	wprintf(_("Leave it in my inbox without filtering"));
	wprintf("</option>\n");

	wprintf("<option value=\"1\">");
	wprintf(_("Filter it according to rules selected below"));
	wprintf("</option>\n");

	wprintf("<option value=\"2\">");
	wprintf(_("Filter it through a manually edited script (advanced users only)"));
	wprintf("</option>\n");

	wprintf("</select>");



	/* The "no filtering" div */

	wprintf("<div id=\"sievediv0\" style=\"display:none\">\n");
	wprintf("<div align=\"center\"><br /><br />");
	wprintf(_("Your incoming mail will not be filtered through any scripts."));
	wprintf("<br /><br /></div>\n");
	wprintf("</div>\n");

	/* The "webcit managed scripts" div */

	wprintf("<div id=\"sievediv1\" style=\"display:none\">\n");
	wprintf("<div align=\"center\"><br /><br />");
	wprintf("FIXME div 1 isn't finished yet");
	wprintf("<br /><br /></div>\n");
	wprintf("</div>\n");


	/* The "I'm smart and can write my own Sieve scripts" div */

	wprintf("<div id=\"sievediv2\" style=\"display:none\">\n");
	wprintf("<div align=\"center\"><br /><br />");
	wprintf("FIXME div 2 isn't finished yet");
	wprintf("<br /><br /></div>\n");
	wprintf("</div>\n");



	/* The rest of this is common for all panels... */

	wprintf("<div align=\"center\"><br>");
	wprintf("<input type=\"submit\" name=\"ok_button\" value=\"%s\">", _("Save changes"));
	wprintf("&nbsp;");
	wprintf("<input type=\"submit\" name=\"cancel_button\" value=\"%s\">\n", _("Cancel"));
	wprintf("</div></form>\n");

	wprintf("</td></tr></table></div>\n");

	wprintf("<script type=\"text/javascript\">	\n"
		"ToggleSievePanels();			\n"
		"</script>				\n"
	);

	wDumpContent(1);

}




/*@}*/
