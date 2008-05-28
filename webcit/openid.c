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
	output_headers(1, 1, 1, 0, 0, 0);

	wprintf("<div class=\"fix_scrollbar_bug\">");

	svput("BOXTITLE", WCS_STRING, _("Manage Account/OpenID Associations"));
	do_template("beginbox");

	wprintf("FIXME -- we have to list the existing ones here");

	wprintf("<hr>\n");

        wprintf("<form method=\"POST\" action=\"openid_attach\">\n");
	wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
	wprintf(_("Add an OpenID: "));
        wprintf("<input type=\"text\" name=\"openid_url\" class=\"openid_urlarea\" size=\"40\">\n");
        wprintf("<input type=\"submit\" name=\"attach_button\" value=\"%s\">"
		"</form></center>\n", _("Attach"));
	do_template("endbox");
	wprintf("</div>");
	wDumpContent(2);
}


/*
 * Attempt to attach an OpenID to an existing, logged-in account
 */
void openid_attach(void) {
	char buf[4096];

	if (havebstr("attach_button")) {
		lprintf(CTDL_DEBUG, "Attempting to attach %s\n", bstr("openid_url"));

		snprintf(buf, sizeof buf,
			"OIDS %s|%s://%s/finalize_openid_login|%s://%s",
			bstr("openid_url"),
			(is_https ? "https" : "http"), WC->http_host,
			(is_https ? "https" : "http"), WC->http_host
		);

		serv_puts(buf);
		serv_getln(buf, sizeof buf);
		if (buf[0] == '2') {
			lprintf(CTDL_DEBUG, "OpenID server contacted; redirecting to %s\n", &buf[4]);
			http_redirect(&buf[4]);
			return;
		}
	}

	/* If we get to this point then something failed. */
	display_openids();
}


