/*
 * $Id$
 *
 */
#include <stdarg.h>
#define SHOW_ME_VAPPEND_PRINTF
#include "webcit.h"

/*
 * print tabbed dialog
 */
void tabbed_dialog(int num_tabs, char *tabnames[]) {
	int i;

	StrBufAppendPrintf(WC->trailing_javascript,
		"var previously_selected_tab = '0';						"
		"function tabsel(which_tab) {							"
		"	if (which_tab == previously_selected_tab) {				"
		" 		return;								"
		"	}									"
		"	$('tabdiv'+previously_selected_tab).style.display = 'none';		"
		"	$('tabdiv'+which_tab).style.display = 'block';				"
		"	$('tabtd'+previously_selected_tab).className = 'tab_cell_edit';		"
		"	$('tabtd'+which_tab).className = 'tab_cell_label';			"
		"	previously_selected_tab = which_tab;					"
		"}										"
	);

	wprintf("<table id=\"TheTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">"
		"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
	);

	for (i=0; i<num_tabs; ++i) {
		wprintf("<td id=\"tabtd%d\" class=\"%s\" "
			"onClick='tabsel(\"%d\");'"
			">",
			i,
			( (i==0) ? "tab_cell_label" : "tab_cell_edit" ),
			i
			);
		wprintf("%s", tabnames[i]);
		wprintf("</td>");

		wprintf("<td>&nbsp;</td>\n");
	}

	wprintf("</tr></table>\n");
}

/*
 * print the tab-header
 *
 * tabnum:	 number of the tab to print
 * num_tabs:	 total number oftabs to be printed
 *
 */
void begin_tab(int tabnum, int num_tabs) {

	if (tabnum == num_tabs) {
		wprintf("<!-- begin tab-common epilogue -->\n");
		wprintf("<div class=\"tabcontent_submit\">");
	}

	else {
		wprintf("<!-- begin tab %d of %d -->\n", tabnum, num_tabs);
		wprintf("<div id=\"tabdiv%d\" style=\"display:%s\" class=\"tabcontent\" >",
			tabnum,
			( (tabnum == 0) ? "block" : "none" )
		);
	}
}

/*
 * print the tab-footer
 * tabnum:	 number of the tab to print
 * num_tabs:	 total number of tabs to be printed
 *
 */
void end_tab(int tabnum, int num_tabs) {

	if (tabnum == num_tabs) {
		wprintf("</div>\n");
		wprintf("<!-- end tab-common epilogue -->\n");
	}

	else {
		wprintf("</div>\n");
		wprintf("<!-- end tab %d of %d -->\n", tabnum, num_tabs);
	
		if (tabnum == num_tabs-1) {
			wprintf("<script type=\"text/javascript\">"
				" Nifty(\"table#TheTabs td\", \"small transparent top\");"
				"</script>"
			);
		}
	}
}


/*
 * print tabbed dialog
 */
void StrTabbedDialog(StrBuf *Target, int num_tabs, StrBuf *tabnames[]) {
	int i;

	StrBufAppendBufPlain(
		Target, 
		HKEY(
			"<script type=\"text/javascript\">						"
			"var previously_selected_tab = '0';						"
			"function tabsel(which_tab) {							"
			"	if (which_tab == previously_selected_tab) {				"
			" 		return;								"
			"	}									"
			"	$('tabdiv'+previously_selected_tab).style.display = 'none';		"
			"	$('tabdiv'+which_tab).style.display = 'block';				"
			"	$('tabtd'+previously_selected_tab).className = 'tab_cell_edit';		"
			"	$('tabtd'+which_tab).className = 'tab_cell_label';			"
			"	previously_selected_tab = which_tab;					"
			"}										"
			"</script>									\n"
			), 0);

	StrBufAppendBufPlain(
		Target, 
		HKEY(
			"<table id=\"TheTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">"
			"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
			), 0);

	for (i=0; i<num_tabs; ++i) {
		StrBufAppendPrintf(
			Target, 
			"<td id=\"tabtd%d\" class=\"%s\" "
			"onClick='tabsel(\"%d\");'"
			">",
			i,
			( (i==0) ? "tab_cell_label" : "tab_cell_edit" ),
			i
			);
		StrBufAppendBuf(Target, tabnames[i], 0);
		StrBufAppendBufPlain(
			Target, 
			HKEY(
				"</td>"
				"<td>&nbsp;</td>\n"), 0);
	}

	StrBufAppendBufPlain(
		Target, 
		HKEY("</tr></table>\n"), 0);
}

/*
 * print the tab-header
 *
 * tabnum:	 number of the tab to print
 * num_tabs:	 total number oftabs to be printed
 *
 */
void StrBeginTab(StrBuf *Target, int tabnum, int num_tabs) {

	if (tabnum == num_tabs) {
		StrBufAppendBufPlain(
			Target, 
			HKEY(
				"<!-- begin tab-common epilogue -->\n"
				"<div class=\"tabcontent_submit\">"), 0);
	}

	else {
		StrBufAppendPrintf(
			Target, 
			"<!-- begin tab %d of %d -->\n"
			"<div id=\"tabdiv%d\" style=\"display:%s\" class=\"tabcontent\" >",
			tabnum, num_tabs, 
			tabnum,
			( (tabnum == 0) ? "block" : "none" )
			);
	}
}

/*
 * print the tab-footer
 * tabnum:	 number of the tab to print
 * num_tabs:	 total number of tabs to be printed
 *
 */
void StrEndTab(StrBuf *Target, int tabnum, int num_tabs) {

	if (tabnum == num_tabs) {
		StrBufAppendBufPlain(
			Target, 
			HKEY(
				"</div>\n"
				"<!-- end tab-common epilogue -->\n"), 0);
	}

	else {
		StrBufAppendPrintf(
			Target, 
			"</div>\n",
			"<!-- end tab %d of %d -->\n", tabnum, num_tabs);
	
		if (tabnum == num_tabs-1) {
			StrBufAppendBufPlain(
				Target, 
				HKEY(
					"<script type=\"text/javascript\">"
					" Nifty(\"table#TheTabs td\", \"small transparent top\");"
					"</script>"), 0);
		}
	}
}


