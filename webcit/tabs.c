#include <stdarg.h>
#define SHOW_ME_VAPPEND_PRINTF
#include "webcit.h"

/*
 * print tabbed dialog
 */
void tabbed_dialog(int num_tabs, const char *tabnames[]) {
	int i;

	StrBufAppendPrintf(WC->trailing_javascript,
		"var previously_selected_tab = '0';						\n"
		"function tabsel(which_tab) {							\n"
		"	if (which_tab == previously_selected_tab) {				\n"
		" 		return;								\n"
		"	}									\n"
		"	$('tabdiv'+previously_selected_tab).style.display = 'none';		\n"
		"	$('tabdiv'+which_tab).style.display = 'block';				\n"
		"	$('tabtd'+previously_selected_tab).className = 'tab_cell_edit';		\n"
		"	$('tabtd'+which_tab).className = 'tab_cell_label';			\n"
		"	previously_selected_tab = which_tab;					\n"
		"}										\n"
	);

	wc_printf("<table id=\"TheTabs\" border=\"0\" cellspacing=\"0\" cellpadding=\"0\">"
		"<tr align=\"center\" style=\"cursor:pointer\"><td>&nbsp;</td>"
	);

	for (i=0; i<num_tabs; ++i) {
		wc_printf("<td id=\"tabtd%d\" class=\"%s\" "
			"onClick='tabsel(\"%d\");'"
			">",
			i,
			( (i==0) ? "tab_cell_label" : "tab_cell_edit" ),
			i
			);
		wc_printf("%s", tabnames[i]);
		wc_printf("</td>");

		wc_printf("<td>&nbsp;</td>\n");
	}

	wc_printf("</tr></table>\n");
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
		wc_printf("<!-- begin tab-common epilogue -->\n");
		wc_printf("<div class=\"tabcontent_submit\">");
	}

	else {
		wc_printf("<!-- begin tab %d of %d -->\n", tabnum, num_tabs);
		wc_printf("<div id=\"tabdiv%d\" style=\"display:%s\" class=\"tabcontent\" >",
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
		wc_printf("</div> <!-- end of 'tabcontent_submit' div -->\n");
		wc_printf("<!-- end tab-common epilogue -->\n");
	}

	else {
		wc_printf("</div>\n");
		wc_printf("<!-- end tab %d of %d -->\n", tabnum, num_tabs);
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
		StrEscAppend(Target, tabnames[i], NULL, 0, 0);
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
void StrBeginTab(StrBuf *Target, int tabnum, int num_tabs, StrBuf **Names) {

	if (tabnum == num_tabs) {
		StrBufAppendBufPlain(
			Target, 
			HKEY("<!-- begin tab-common epilogue ["), 0);
		StrEscAppend(Target, Names[tabnum], NULL, 0, 2);
		StrBufAppendBufPlain(
			Target, 
			HKEY("] -->\n<div class=\"tabcontent_submit\">"), 0);
	}

	else {
		StrBufAppendBufPlain(
			Target, 
			HKEY("<!-- begin tab "), 0);
		StrBufAppendPrintf(
			Target,  "%d of %d [",
			tabnum, num_tabs);
		
		StrEscAppend(Target, Names[tabnum], NULL, 0, 2);

		StrBufAppendPrintf(
			Target, 
			"] -->\n<div id=\"tabdiv%d\" style=\"display:%s\" class=\"tabcontent\" >",
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
			"<!-- end tab %d of %d -->\n", tabnum, num_tabs
		);
	}
	if (havebstr("last_tabsel"))
	{
		StrBufAppendPrintf(Target, "<script type=\"text/javascript\">tabsel(%s);</script>", BSTR("last_tabsel"));
	}
}


