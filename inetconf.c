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

	char *ic_localhost;
	char *ic_gwdom;
	char *ic_directory;
	char *ic_spamass;
	char *ic_rbl;
	char *ic_smarthost;
	char *ic_misc;

	char *which = NULL;

	ic_localhost = strdup("");
	ic_gwdom = strdup("");
	ic_directory = strdup("");
	ic_spamass = strdup("");
	ic_rbl = strdup("");
	ic_smarthost = strdup("");
	ic_misc = strdup("");

	output_headers(3);
	wprintf("<TABLE WIDTH=100%% BORDER=0 BGCOLOR=\"#444455\"><TR><TD>");
	wprintf("<SPAN CLASS=\"titlebar\">Internet configuration</SPAN>\n");
	wprintf("</TD></TR></TABLE>\n");

	serv_printf("CONF GETSYS|application/x-citadel-internet-config");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {

		extract(ename, buf, 0);
		extract(etype, buf, 0);
		which = NULL;
		if (!strcasecmp(etype, "localhost")) which = ic_localhost;
		else if (!strcasecmp(etype, "gatewaydomain")) which = ic_gwdom;
		else if (!strcasecmp(etype, "directory")) which = ic_directory;
		else if (!strcasecmp(etype, "spamassassin")) which = ic_directory;
		else if (!strcasecmp(etype, "rbl")) which = ic_rbl;
		else if (!strcasecmp(etype, "smarthost")) which = ic_smarthost;

		if (which != NULL) {
			which = realloc(which, strlen(which) + strlen(ename) + 2);
			if (strlen(which) > 0) strcat(which, "\n");
			strcat(which, ename);
		}
		else {
			ic_misc = realloc(ic_misc, strlen(ic_misc) + strlen(buf) + 2);
			if (strlen(ic_misc) > 0) strcat(ic_misc, "\n");
			strcat(which, buf);
		}

		/* FIXME finish this */
		escputs(buf);
		wprintf("<BR>\n");
	}

	wDumpContent(1);

	free(ic_localhost);
	free(ic_gwdom);
	free(ic_directory);
	free(ic_spamass);
	free(ic_rbl);
	free(ic_smarthost);
	free(ic_misc);
}


void save_inetconf(void) {

	strcpy(WC->ImportantMessage, "FIXME did we do anything?");

	display_inetconf();
}
