/*
 * $Id$
 *//**
 * \defgroup AjaxAutoCompletion ajax-powered autocompletion...
 * \ingroup ClientPower
 */

/*@{*/
#include "webcit.h"

/**
 * \brief Recipient autocompletion results
 * \param partial the account to search for ??????
 */
void recp_autocomplete(char *partial) {
	char buf[1024];
	char name[128];

	output_headers(0, 0, 0, 0, 0, 0);

	wprintf("Content-type: text/html\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-store\r\n",
		SERVER);
	begin_burst();

	wprintf("<ul>");

	serv_printf("AUTO %s", partial);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(name, buf, 0, '|', sizeof name);
			wprintf("<li>");
			escputs(name);
			wprintf("</li>");
		}
	}

	wprintf("</ul>");

	wprintf("\r\n\r\n");
	wDumpContent(0);
}


/** @} */
