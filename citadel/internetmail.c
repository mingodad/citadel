/*
 * $Id$
 *
 * Internet mail configurator for Citadel/UX
 * see copyright.doc for copyright information
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <syslog.h>
#include "citadel.h"

extern struct config config;

char metoo[10][128];
int mecount = 0;

extern char ALIASES[128];
extern char CIT86NET[128];
extern char SENDMAIL[128];
extern char FALLBACK[128];
extern char GW_DOMAIN[128];
extern char TABLEFILE[128];
extern int RUN_NETPROC;

void StripLeadingAndTrailingWhitespace(char *str)
{
	if (strlen(str) == 0)
		return;
	while (isspace(str[0]))
		strcpy(str, &str[1]);
	while (isspace(str[strlen(str) - 1]))
		str[strlen(str) - 1] = 0;
}

void LoadInternetConfig(void)
{
	char ParamName[256], ParamValue[256], buf[256];
	FILE *conf;
	int a, eqpos;


	conf = fopen("network/internetmail.config", "r");
	if (conf == NULL) {
		syslog(LOG_NOTICE, "Couldn't load internetmail.config");
		exit(1);
	}
	while (fgets(buf, 256, conf) != NULL) {
		if (strlen(buf) > 0)
			buf[strlen(buf) - 1] = 0;
		strcpy(ParamName, "");
		strcpy(ParamValue, "");
		if (buf[0] != '#') {
			eqpos = (-1);
			for (a = strlen(buf); a >= 0; --a) {
				if (buf[a] == '=')
					eqpos = a;
			}
			if (eqpos >= 0) {
				strcpy(ParamName, buf);
				ParamName[eqpos] = 0;
				strcpy(ParamValue, &buf[eqpos + 1]);
			}
			StripLeadingAndTrailingWhitespace(ParamName);
			StripLeadingAndTrailingWhitespace(ParamValue);

			if (!strcasecmp(ParamName, "aliases"))
				strcpy(ALIASES, ParamValue);
			if (!strcasecmp(ParamName, "cit86net spoolin"))
				strcpy(CIT86NET, ParamValue);
			if (!strcasecmp(ParamName, "sendmail"))
				strcpy(SENDMAIL, ParamValue);
			if (!strcasecmp(ParamName, "fallback"))
				strcpy(FALLBACK, ParamValue);
			if (!strcasecmp(ParamName, "gateway domain"))
				strcpy(GW_DOMAIN, ParamValue);
			if (!strcasecmp(ParamName, "table file"))
				strcpy(TABLEFILE, ParamValue);
			if (!strcasecmp(ParamName, "deliver local"))
				strcpy(metoo[mecount++], ParamValue);
			if (!strcasecmp(ParamName, "run netproc"))
				RUN_NETPROC = atoi(ParamValue);
		}
	}
	fclose(conf);
}


/* 
 * returns nonzero if the specified host is listed as local
 */
int IsHostLocal(char *WhichHost)
{
	int a;

	if (!strcasecmp(WhichHost, FQDN))
		return (1);

	if (mecount > 0) {
		for (a = 0; a < mecount; ++a) {
			if (!strcasecmp(WhichHost, metoo[a]))
				return (1);
		}
	}
	return (0);
}
