/*
 * $Id:  $
 *//**
 * \defgroup AjaxAutoCompletion ajax-powered autocompletion...
 * \ingroup ClientPower
 */

/*@{*/
#include "webcit.h"

/**
 * \brief Address book popup results
 */
void display_address_book_inner_div(void) {
	int i;

	begin_ajax_response();

	wprintf("<div align=center><form>"
		"<select name=\"whichaddr\" size=\"15\">\n");

	for (i=0; i<100; ++i) {
		wprintf("<option>Contact %d &lt;contact%d@example.com&gt;</option>\n", i, i);
	}

	wprintf("</select></form></div>\n");

	end_ajax_response();
}


/** @} */
