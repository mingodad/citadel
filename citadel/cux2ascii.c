/*
 * cux2ascii v2.3
 * see copyright.doc for copyright information
 *
 * This program is a filter which converts Citadel/UX binary message format
 * to standard UseNet news format.  Useful for Citadel<->News gateways.
 *
 * $Id$
 *
 */

#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>
#include "citadel.h"

long finduser();

void get_config();
struct config config;


main(argc, argv)
int argc;
char *argv[];
{
	struct tm *tm;
	int a, b, e, mtype, aflag;
	char bbb[100], ngn[100];
	long tmid;
	char tsid[64];
	char tuid[64];
	FILE *fp, *tfp;
	long now, msglen;

	int cuunbatch = 0;

	/* experimental cuunbatch header generation */
	for (a = 0; a < argc; ++a) {
		if (!strcmp(argv[a], "-c"))
			cuunbatch = 1;
	}

	get_config();

	fp = stdin;
	while (1) {
		do {
			e = getc(fp);
			if (e < 0)
				exit(0);
		} while (e != 255);
		mtype = getc(fp);
		aflag = getc(fp);

		tmid = 0L;
		strcpy(tsid, FQDN);
		strcpy(tuid, "postmaster");

		tfp = tmpfile();
		do {
			b = getc(fp);
			if (b == 'M') {
				fprintf(tfp, "Message-ID: <%ld@%s>\n", tmid, tsid);
				fprintf(tfp, "\n");
				if (aflag != 1)
					fmout(80, fp, tfp);
				else
					while (a = getc(fp), a > 0) {
						putc(a, tfp);
						if (a == 13)
							putc(10, tfp);
					}
			}
			if ((b != 'M') && (b > 0))
				fpgetfield(fp, bbb);
			if (b == 'I')
				tmid = atol(bbb);
			if (b == 'N') {
				strcpy(tsid, bbb);
				if (!strcmp(tsid, NODENAME))
					strcpy(tsid, FQDN);
				for (a = 0; a < strlen(tuid); ++a)
					if (tuid[a] == ' ')
						tuid[a] = '_';
				fprintf(tfp, "From: %s@%s ", tuid, tsid);
				for (a = 0; a < strlen(tuid); ++a)
					if (tuid[a] == '_')
						tuid[a] = ' ';
				fprintf(tfp, "(%s)\n", tuid);
			}
			if (b == 'P')
				fprintf(tfp, "Path: %s\n", bbb);
			if (b == 'A')
				strcpy(tuid, bbb);
			if (b == 'O') {
				xref(bbb, ngn);
				fprintf(tfp, "Newsgroups: %s\n", ngn);
			}
			if (b == 'R')
				fprintf(tfp, "To: %s\n", bbb);
			if (b == 'U')
				fprintf(tfp, "Subject: %s\n", bbb);
			if (b == 'T') {
				now = atol(bbb);
				tm = (struct tm *) localtime(&now);
				fprintf(tfp, "Date: %s", asctime(tm));
			}
		} while ((b != 'M') && (b > 0));
		msglen = ftell(tfp);

		if (cuunbatch) {
			printf("#! cuunbatch %ld\n", msglen);
		} else {
			printf("#! rnews %ld\n", msglen);
		}

		rewind(tfp);
		while (msglen--)
			putc(getc(tfp), stdout);
		fclose(tfp);
	}
	exit(0);
}

fpgetfield(fp, string)		/* level-2 break out next null-terminated string */
FILE *fp;
char string[];
{
	int a, b;
	strcpy(string, "");
	a = 0;
	do {
		b = getc(fp);
		if (b < 1) {
			string[a] = 0;
			return (0);
		}
		string[a] = b;
		++a;
	} while (b != 0);
	return (0);
}

fmout(width, fp, mout)
int width;
FILE *fp, *mout;
{
	int a, b, c, real, old;
	char aaa[140];

	strcpy(aaa, "");
	old = 255;
	c = 1;			/* c is the current pos */
      FMTA:old = real;
	a = getc(fp);
	real = a;
	if (a <= 0)
		goto FMTEND;

	if (((a == 13) || (a == 10)) && (old != 13) && (old != 10))
		a = 32;
	if (((old == 13) || (old == 10)) && (isspace(real))) {
		fprintf(mout, "\n");
		c = 1;
	}
	if (a > 126)
		goto FMTA;

	if (a > 32) {
		if (((strlen(aaa) + c) > (width - 1)) && (strlen(aaa) > (width - 1))) {
			fprintf(mout, "\n%s", aaa);
			c = strlen(aaa);
			aaa[0] = 0;
		}
		b = strlen(aaa);
		aaa[b] = a;
		aaa[b + 1] = 0;
	}
	if (a == 32) {
		if ((strlen(aaa) + c) > (width - 1)) {
			fprintf(mout, "\n");
			c = 1;
		}
		fprintf(mout, "%s ", aaa);
		++c;
		c = c + strlen(aaa);
		strcpy(aaa, "");
		goto FMTA;
	}
	if ((a == 13) || (a == 10)) {
		fprintf(mout, "%s\n", aaa);
		c = 1;
		strcpy(aaa, "");
		goto FMTA;
	}
	goto FMTA;

      FMTEND:fprintf(mout, "\n");
	return (0);
}

xref(roomname, newsgroup)
char *roomname, *newsgroup;
{
	char tbuf[128];
	FILE *fp;
	int commapos, a;

	strcpy(newsgroup, roomname);
	fp = fopen("./network/rnews.xref", "r");
	if (fp == NULL)
		return (1);
	while (fgets(tbuf, 128, fp) != NULL) {
		tbuf[strlen(tbuf) - 1] = 0;	/* strip off the newline */
		a = strlen(tbuf);
		while (a--)
			if (tbuf[a] == ',')
				commapos = a;
		tbuf[commapos] = 0;
		if (!strcasecmp(&tbuf[commapos + 1], roomname))
			strcpy(newsgroup, tbuf);
	}
	fclose(fp);
	return (0);
}
