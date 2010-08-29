/*
 * $Id$
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

	wc_printf("<div class=\"fix_scrollbar_bug\">");

	do_template("beginbox_1", NULL);
	StrBufAppendBufPlain(WCC->WBuf, _("Manage Account/OpenID Associations"), -1, 0);
	do_template("beginbox_2", NULL);

	if (WCC->serv_info->serv_supports_openid) {

		wc_printf("<table class=\"altern\">");
	
		serv_puts("OIDL");
		serv_getln(buf, sizeof buf);
		if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {
			bg = 1 - bg;
			wc_printf("<tr class=\"%s\">", (bg ? "even" : "odd"));
			wc_printf("<td><img src=\"static/openid-small.gif\"></td><td>");
			escputs(buf);
			wc_printf("</td><td>");
			wc_printf("<a href=\"openid_detach?id_to_detach=");
			urlescputs(buf);
			wc_printf("\" onClick=\"return confirm('%s');\">",
				_("Do you really want to delete this OpenID?"));
			wc_printf("%s</a>", _("(delete)"));
			wc_printf("</td></tr>\n");
		}
	
		wc_printf("</table><br />\n");
	
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

	do_template("endbox", NULL);
	wc_printf("</div>");
	wDumpContent(2);
}


/*
 * Attempt to attach an OpenID to an existing, logged-in account
 */
void openid_attach(void) {
	char buf[4096];

	if (havebstr("attach_button")) {
		wcsession *WCC = WC;

		lprintf(CTDL_DEBUG, "Attempting to attach %s\n", bstr("openid_url"));

		snprintf(buf, sizeof buf,
			 "OIDS %s|%s://%s/finalize_openid_login|%s://%s",
			 bstr("openid_url"),
			 (is_https ? "https" : "http"), ChrPtr(WCC->Hdr->HR.http_host),
			 (is_https ? "https" : "http"), ChrPtr(WCC->Hdr->HR.http_host)
		);

		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			lprintf(CTDL_DEBUG, "OpenID server contacted; redirecting to %s\n", &buf[4]);
			http_redirect(&buf[4]);
			return;
		}
		else {
			lprintf(CTDL_DEBUG, "OpenID attach failed: %s\n", &buf[4]);
		}
	}

	/* If we get to this point then something failed. */
	display_openids();
}


/*
 * Detach an OpenID from the currently logged-in account
 */
void openid_detach(void) {
	char buf[1024];

	if (havebstr("id_to_detach")) {
		serv_printf("OIDD %s", bstr("id_to_detach"));
		serv_getln(buf, sizeof buf);
		if (buf[0] != '2') {
			strcpy(WC->ImportantMessage, &buf[4]);
		}
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
