/* 
 * inetconf.c
 *
 * Functions which handle Internet domain configuration etc.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"



void display_inetconf(void)
{
	char buf[SIZ];
	char ename[SIZ];
	char etype[SIZ];
	int i;
	int which;

	enum {
		ic_localhost,
		ic_directory,
		ic_gwdom,
		ic_smarthost,
		ic_rbl,
		ic_spamass,
		ic_max
	};
	char *ic_spec[ic_max];
	char *ic_misc;

	char *ic_keyword[] = {
		"localhost",
		"directory",
		"gatewaydomain",
		"smarthost",
		"rbl",
		"spamassassin",
	};

	char *ic_boxtitle[] = {
		"Local host aliases",
		"Directory domains",
		"Gateway domains",
		"Smart hosts",
		"RBL hosts",
		"SpamAssassin hosts",
	};

	char *ic_desc[] = {
		"(domains for which this host receives mail)",
		"(domains mapped with the Global Address Book)",
		"(domains whose subdomains match Citadel hosts)",
		"(if present, forward all outbound mail to one of these hosts)",
		"(hosts running a Realtime Blackhole List)",
		"(hosts running the SpamAssassin service)",
	};

	for (i=0; i<ic_max; ++i) {
		ic_spec[i] = strdup("");
	}
	ic_misc = strdup("");

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Internet configuration</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");

	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {

		extract(ename, buf, 0);
		extract(etype, buf, 1);
		which = (-1);
		for (i=0; i<ic_max; ++i) {
			if (!strcasecmp(etype, ic_keyword[i])) {
				which = i;
			}
		}

		if (which >= 0) {
			ic_spec[which] = realloc(ic_spec[which], strlen(ic_spec[which]) + strlen(ename) + 2);
			if (strlen(ic_spec[which]) > 0) strcat(ic_spec[which], "\n");
			strcat(ic_spec[which], ename);
		}
		else {
			ic_misc = realloc(ic_misc, strlen(ic_misc) + strlen(buf) + 2);
			if (strlen(ic_misc) > 0) strcat(ic_misc, "\n");
			strcat(ic_misc, buf);
		}

	}

	wprintf("<TABLE border=0 width=100%%><TR><TD VALIGN=TOP>\n");
	for (which=0; which<ic_max; ++which) {
		if (which == (ic_max / 2)) {
			wprintf("</TD><TD VALIGN=TOP>");
		}
		svprintf("BOXTITLE", WCS_STRING, ic_boxtitle[which]);
		do_template("beginbox");
		wprintf("<span class=\"menudesc\">");
		escputs(ic_desc[which]);
		wprintf("</span><br>");
		wprintf("<TABLE border=0 cellspacing=0 cellpadding=0 width=100%%>\n");
		if (strlen(ic_spec[which]) > 0) {
			for (i=0; i<num_tokens(ic_spec[which], '\n'); ++i) {
				wprintf("<TR><TD ALIGN=LEFT>");
				extract_token(buf, ic_spec[which], i, '\n');
				escputs(buf);
				wprintf("</TD><TD ALIGN=RIGHT>"
					"<A HREF=\"/save_inetconf?oper=delete&ename=");
				escputs(buf);
				wprintf("&etype=%s\" ", ic_keyword[which]);
				wprintf("onClick=\"return confirm('Delete ");
				jsescputs(buf);
				wprintf("?');\">");
				wprintf("<font size=-1>(Delete)</font></a></TD></TR>\n");
			}
		}
		wprintf("<FORM METHOD=\"POST\" ACTION=\"/save_inetconf\">\n"
			"<TR><TD>"
			"<INPUT TYPE=\"text\" NAME=\"ename\" MAXLENGTH=\"64\">"
			"<INPUT TYPE=\"hidden\" NAME=\"etype\" VALUE=\"%s\">", ic_keyword[which]);
		wprintf("</TD><TD ALIGN=RIGHT>"
			"<INPUT TYPE=\"submit\" NAME=\"oper\" VALUE=\"Add\">"
			"</TD></TR></TABLE></FORM>\n");
		do_template("endbox");
	}
	wprintf("</TD></TR></TABLE>\n");

	wDumpContent(1);

	for (i=0; i<ic_max; ++i) {
		free(ic_spec[i]);
	}
	free(ic_misc);
}


void save_inetconf(void) {
	char buf[SIZ];
	char ename[SIZ];
	char etype[SIZ];
	char newconfig[65536];

	strcpy(newconfig, "");
	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		extract(ename, buf, 0);
		extract(etype, buf, 1);
		if (strlen(buf) == 0) {
			/* skip blank lines */
		}
		else if ((!strcasecmp(ename, bstr("ename")))
		   &&   (!strcasecmp(etype, bstr("etype")))
		   &&	(!strcasecmp(bstr("oper"), "delete"))
		) {
			sprintf(WC->ImportantMessage, "%s deleted.", ename);
		}
		else {
			if (strlen(newconfig) > 0) strcat(newconfig, "\n");
			strcat(newconfig, buf);
		}
	}

	serv_printf("CONF PUTSYS|application/x-citadel-internet-config");
	serv_gets(buf);
	if (buf[0] == '4') {
		serv_puts(newconfig);
		if (!strcasecmp(bstr("oper"), "add")) {
			serv_printf("%s|%s", bstr("ename"), bstr("etype") );
			sprintf(WC->ImportantMessage, "%s added.", bstr("ename"));
		}
		serv_puts("000");
	}
	
	display_inetconf();
}
