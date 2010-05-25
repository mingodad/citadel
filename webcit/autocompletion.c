/*
 * $Id$
 *
 * ajax-powered autocompletion...
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"

/*
 * Recipient autocompletion results
 */
void recp_autocomplete(char *partial) {
	char buf[1024];
	char name[128];

	output_headers(0, 0, 0, 0, 0, 0);

	hprintf("Content-type: text/html\r\n"
		"Server: %s\r\n"
		"Connection: close\r\n"
		"Pragma: no-cache\r\n"
		"Cache-Control: no-store\r\n"
		"Expires: -1\r\n"
		,
		PACKAGE_STRING);
	begin_burst();

	wc_printf("<ul>");

	serv_printf("AUTO %s", partial);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		while(serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			extract_token(name, buf, 0, '|', sizeof name);
			wc_printf("<li>");
			escputs(name);
			wc_printf("</li>");
		}
	}

	wc_printf("</ul>");

	wc_printf("\r\n\r\n");
	wDumpContent(0);
}


void _recp_autocomplete(void) {recp_autocomplete(bstr("recp"));}
void _cc_autocomplete(void)   {recp_autocomplete(bstr("cc"));} 
void _bcc_autocomplete(void)  {recp_autocomplete(bstr("bcc"));}


void 
InitModule_AUTO_COMPLETE
(void)
{
	WebcitAddUrlHandler(HKEY("recp_autocomplete"), "", 0, _recp_autocomplete, 0);
	WebcitAddUrlHandler(HKEY("cc_autocomplete"),   "", 0, _cc_autocomplete, 0);
	WebcitAddUrlHandler(HKEY("bcc_autocomplete"),  "", 0, _bcc_autocomplete, 0);
}
