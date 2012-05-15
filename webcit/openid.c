/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"

/*
 * Display the OpenIDs associated with an account
 */
void display_openids(void)
{
	wcsession *WCC = WC;
	char buf[1024];
	int bg = 0;

	output_headers(1, 1, 1, 0, 0, 0);

	do_template("box_begin_1");
	StrBufAppendBufPlain(WCC->WBuf, _("Manage Account/OpenID Associations"), -1, 0);
	do_template("box_begin_2");

	if (WCC->serv_info->serv_supports_openid) {

		wc_printf("<table class=\"altern\">");
	
		serv_puts("OIDL");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			bg = 1 - bg;
			wc_printf("<tr class=\"%s\">", (bg ? "even" : "odd"));
			wc_printf("<td><img src=\"static/webcit_icons/openid-small.gif\"></td><td>");
			escputs(buf);
			wc_printf("</td><td>");
			wc_printf("<a href=\"openid_detach?id_to_detach=");
			urlescputs(buf);
			wc_printf("\" onClick=\"return confirm('%s');\">",
				_("Do you really want to delete this OpenID?"));
			wc_printf("%s</a>", _("(delete)"));
			wc_printf("</td></tr>\n");
		}
	
		wc_printf("</table><br>\n");
	
	        wc_printf("<form method=\"POST\" action=\"openid_attach\">\n");
		wc_printf("<input type=\"hidden\" name=\"nonce\" value=\"%d\">\n", WCC->nonce);
		wc_printf(_("Add an OpenID: "));
	        wc_printf("<input type=\"text\" name=\"openid_url\" class=\"openid_urlarea\" size=\"40\">\n");
	        wc_printf("<input type=\"submit\" name=\"attach_button\" value=\"%s\">"
			"</form></center>\n", _("Attach"));
	}

	else {
		wc_printf(_("%s does not permit authentication via OpenID."), ChrPtr(WCC->serv_info->serv_humannode));
	}

	do_template("box_end");
	wDumpContent(2);
}


/*
 * Attempt to attach an OpenID to an existing, logged-in account
 */
void openid_attach(void) {
	char buf[4096];

	if (havebstr("attach_button")) {

		syslog(LOG_DEBUG, "Attempting to attach %s\n", bstr("openid_url"));

		snprintf(buf, sizeof buf,
			"OIDS %s|%s/finalize_openid_login?attach_existing=1|%s",
			bstr("openid_url"),
			ChrPtr(site_prefix),
			ChrPtr(site_prefix)
		);

		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			syslog(LOG_DEBUG, "OpenID server contacted; redirecting to %s\n", &buf[4]);
			http_redirect(&buf[4]);
			return;
		}
		else {
			syslog(LOG_DEBUG, "OpenID attach failed: %s\n", &buf[4]);
		}
	}

	/* If we get to this point then something failed. */
	display_openids();
}


/*
 * Detach an OpenID from the currently logged-in account
 */
void openid_detach(void) {
	StrBuf *Line;

	if (havebstr("id_to_detach")) {
		serv_printf("OIDD %s", bstr("id_to_detach"));
		Line = NewStrBuf();
		StrBuf_ServGetln(Line);
		GetServerStatusMsg(Line, NULL, 1, 2);
		FreeStrBuf(&Line);
	}

	display_openids();
}

void 
InitModule_OPENID
(void)
{
	WebcitAddUrlHandler(HKEY("display_openids"), "", 0, display_openids, 0);
	WebcitAddUrlHandler(HKEY("openid_attach"), "", 0, openid_attach, 0);
	WebcitAddUrlHandler(HKEY("openid_detach"), "", 0, openid_detach, 0);
}
