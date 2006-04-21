/*
 * $Id:  $
 */
/**
 * \defgroup TabUtils Utility functions for creating tabbed dialogs
 * \ingroup WebcitDisplayItems
 */
/*@{*/
#include "webcit.h"

/**
 * \brief print tabbed dialog
 * \param num_tabs how many tabs do we have?
 * \param tabnames the headers of the tables
 */
void tabbed_dialog(int num_tabs, char *tabnames[]) {
	int i;

	wprintf("<script type=\"text/javascript\">						"
		"var previously_selected_tab = '0';						"
		"function tabsel(which_tab) {							"
		"	if (which_tab == previously_selected_tab) {				"
		" 		return;								"
		"	}									"
		"	$('tabtd'+previously_selected_tab).style.backgroundColor = '#cccccc';	"
		"	$('tabdiv'+previously_selected_tab).style.display = 'none';		"
		"	$('tabtd'+which_tab).style.backgroundColor = '#ffffff';			"
		"	$('tabdiv'+which_tab).style.display = 'block';				"
		"	previously_selected_tab = which_tab;					"
		"}										"
		"</script>									\n"
	);

	wprintf("<table id=\"TheTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\" width=\"100%%\">"
		"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
	);

	for (i=0; i<num_tabs; ++i) {
		wprintf("<td id=\"tabtd%d\" bgcolor=\"#%s\" "
			"onClick='tabsel(\"%d\");'"
			">"
			"<span class=\"tablabel\">",
			i,
			( (i==0) ? "ffffff" : "cccccc" ),
			i,
			i
		);
		wprintf("%s", tabnames[i]);
		wprintf("</span></td>");

		wprintf("<td>&nbsp;</td>\n");
	}

	wprintf("</tr></table>\n");
	wprintf("<table border=\"0\" width=\"100%%\" bgcolor=\"#ffffff\"><tr><td>");
}

/**
 * \brief print the tab-header
 * \param tabnum number of the tab to print
 * \param num_tabs total number oftabs to be printed
 */
void begin_tab(int tabnum, int num_tabs) {
	wprintf("<div id=\"tabdiv%d\" style=\"display:%s\">",
		tabnum,
		( (tabnum == 0) ? "block" : "none" )
	);
}

/**
 * \brief print the tab-footer
 * \param tabnum number of the tab to print
 * \param num_tabs total number oftabs to be printed
 */
void end_tab(int tabnum, int num_tabs) {
	wprintf("</div>\n");
	wprintf("<!-- end tab %d of %d -->\n", tabnum, num_tabs);

	if (tabnum == num_tabs-1) {
		wprintf("</td></tr></table>\n");
		wprintf("<script type=\"text/javascript\">"
			" Nifty(\"table#TheTabs td\", \"small transparent top\");"
			"</script>"
		);
			//" Nifty(\"td#tabtd1\", \"small transparent top\");"
	}
}


/*@}*/
