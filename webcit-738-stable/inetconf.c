/* 
 * $Id$
 *
 * Functions which handle Internet domain configuration etc.
 */

#include "webcit.h"

/*
 * display the inet config dialog 
 */
void display_inetconf(void)
{
	char buf[SIZ];
	char ename[SIZ];
	char etype[SIZ];
	int i;
	int which;
	int bg = 0;

	enum {
		ic_localhost,
		ic_directory,
		ic_smarthost,
		ic_rbl,
		ic_spamass,
		ic_masq,
		ic_max
	};

	char *ic_spec[ic_max];
	char *ic_misc;
	char *ic_keyword[ic_max];
	char *ic_boxtitle[ic_max];
	char *ic_desc[ic_max];

	/* These are server config keywords; do not localize! */
	ic_keyword[0] = "localhost";
	ic_keyword[1] = "directory";
	ic_keyword[2] = "smarthost";
	ic_keyword[3] = "rbl";
	ic_keyword[4] = "spamassassin";
	ic_keyword[5] = "masqdomain";

	ic_boxtitle[0] = _("Local host aliases");
	ic_boxtitle[1] = _("Directory domains");
	ic_boxtitle[2] = _("Smart hosts");
	ic_boxtitle[3] = _("RBL hosts");
	ic_boxtitle[4] = _("SpamAssassin hosts");
	ic_boxtitle[5] = _("Masqueradable domains");

	ic_desc[0] = _("(domains for which this host receives mail)");
	ic_desc[1] = _("(domains mapped with the Global Address Book)");
	ic_desc[2] = _("(if present, forward all outbound mail to one of these hosts)");
	ic_desc[3] = _("(hosts running a Realtime Blackhole List)");
	ic_desc[4] = _("(hosts running the SpamAssassin service)");
	ic_desc[5] = _("(Domains as which users are allowed to masquerade)");

	for (i=0; i<ic_max; ++i) {
		ic_spec[i] = strdup("");
	}

	ic_misc = strdup("");

	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') while (serv_getln(buf, sizeof buf), strcmp(buf, "000")) {

		extract_token(ename, buf, 0, '|', sizeof ename);
		extract_token(etype, buf, 1, '|', sizeof etype);
		which = (-1);
		for (i=0; i<ic_max; ++i) {
			if (!strcasecmp(etype, ic_keyword[i])) {
				which = i;
			}
		}

		if (which >= 0) {
			ic_spec[which] = realloc(ic_spec[which], strlen(ic_spec[which]) + strlen(ename) + 2);
			if (!IsEmptyStr(ic_spec[which])) strcat(ic_spec[which], "\n");
			strcat(ic_spec[which], ename);
		}
		else {
			ic_misc = realloc(ic_misc, strlen(ic_misc) + strlen(buf) + 2);
			if (!IsEmptyStr(ic_misc)) strcat(ic_misc, "\n");
			strcat(ic_misc, buf);
		}

	}

	output_headers(1, 1, 2, 0, 0, 0);
	wprintf("<div id=\"banner\">\n");
	wprintf("<h1>");
	wprintf(_("Internet configuration"));
	wprintf("</h1>");
	wprintf("</div>\n");

	wprintf("<div id=\"content\" class=\"service\">\n");

	wprintf("<div class=\"fix_scrollbar_bug\">"
		"<table border=0 width=100%% cellspacing=\"10px\" cellpadding=\"10px\"> "
		"<tr><td valign=top width=50%%>\n");
	for (which=0; which<ic_max; ++which) {
		if (which == (ic_max / 2)) {
			wprintf("</td><td valign=top>");
		}
		svput("BOXTITLE", WCS_STRING, ic_boxtitle[which]);
		do_template("beginbox");
		wprintf("<span class=\"menudesc\">");
		escputs(ic_desc[which]);
		wprintf("</span><br />");
		wprintf("<table border=0 cellspacing=\"2px\" cellpadding=\"2px\" width=94%% "
			"class=\"altern\" >\n");
		bg = 0;
		if (!IsEmptyStr(ic_spec[which])) {
			for (i=0; i<num_tokens(ic_spec[which], '\n'); ++i) {
                        	bg = 1 - bg;
				wprintf("<tr class=\"%s\">",
                                	(bg ? "even" : "odd")
                        	);
				wprintf("<td align=left>");
				extract_token(buf, ic_spec[which], i, '\n', sizeof buf);
				escputs(buf);
				wprintf("</td><td align=left>"
					"<span class=\"button_link\">"
					"<a href=\"save_inetconf?oper=delete&ename=");
				escputs(buf);
				wprintf("&etype=%s\" ", ic_keyword[which]);
				wprintf("onClick=\"return confirm('%s');\">",
					_("Delete this entry?"));
				wprintf(_("Delete"));
				wprintf("</a></span></td></tr>\n");
			}

		}
		wprintf("<form method=\"post\" action=\"save_inetconf\">\n");
		wprintf("<input type=\"hidden\" name=\"nonce\" value=\"%ld\">\n", WC->nonce);
		wprintf("<tr><td>"
			"<input type=\"text\" name=\"ename\" maxlength=\"64\">"
			"<input type=\"hidden\" name=\"etype\" VALUE=\"%s\">", ic_keyword[which]);
		wprintf("</td><td align=left>"
			"<input type=\"submit\" name=\"oper\" value=\"Add\">"
			"</td></tr></table></form>\n");
		do_template("endbox");
		wprintf("<br />");
	}
	wprintf("</td></tr></table></div>\n");
	wDumpContent(1);

	for (i=0; i<ic_max; ++i) {
		free(ic_spec[i]);
	}
	free(ic_misc);
}


/*
 * save changes to the inet config
 */
void save_inetconf(void) {
	char *buf;
	char *ename;
	char *etype;
	char *newconfig;

	buf = malloc(SIZ);
	ename = malloc(SIZ);
	etype = malloc(SIZ);
	newconfig = malloc(65536);

	strcpy(newconfig, "");
	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_getln(buf, SIZ);
	if (buf[0] == '1') while (serv_getln(buf, SIZ), strcmp(buf, "000")) {
		extract_token(ename, buf, 0, '|', SIZ);
		extract_token(etype, buf, 1, '|', SIZ);
		if (IsEmptyStr(buf)) {
			/* skip blank lines */
		}
		else if ((!strcasecmp(ename, bstr("ename")))
		   &&   (!strcasecmp(etype, bstr("etype")))
		   &&	(!strcasecmp(bstr("oper"), "delete"))
		) {
			sprintf(WC->ImportantMessage, _("%s has been deleted."), ename);
		}
		else {
			if (!IsEmptyStr(newconfig)) strcat(newconfig, "\n");
			strcat(newconfig, buf);
		}
	}

	serv_printf("CONF PUTSYS|application/x-citadel-internet-config");
	serv_getln(buf, SIZ);
	if (buf[0] == '4') {
		serv_puts(newconfig);
		if (!strcasecmp(bstr("oper"), "add")) {
			serv_printf("%s|%s", bstr("ename"), bstr("etype") );
			sprintf(WC->ImportantMessage, "%s added.", bstr("ename"));
		}
		serv_puts("000");
	}
	
	display_inetconf();

	free(buf);
	free(ename);
	free(etype);
	free(newconfig);
}
