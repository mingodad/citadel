/*
 * $Id$
 *
 * netproc calls this to export Citadel mail to RFC822-compliant mailers.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <time.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>
#include <syslog.h>
#include "citadel.h"
#include "genstamp.h"

void LoadInternetConfig(void);
void get_config(void);
struct config config;

char temp[PATH_MAX];

char ALIASES[128];
char CIT86NET[128];
char SENDMAIL[128];
char FALLBACK[128];
char GW_DOMAIN[128];
char TABLEFILE[128];
int RUN_NETPROC = 1;

int haschar(char *st, int ch)
{
	int a, b;
	b = 0;
	for (a = 0; a < strlen(st); ++a)
		if (st[a] == ch)
			++b;
	return (b);
}



void fpgetfield(FILE * fp, char *string)
{
	int a, b;
	strcpy(string, "");
	a = 0;
	do {
		b = getc(fp);
		if (b < 1) {
			string[a] = 0;
			return;
		}
		string[a] = b;
		++a;
	} while (b != 0);
}


/* This is NOT the same msgform() found in the main program. It has been
 * modified to format 80 columns into a temporary file, and extract the
 * sender and recipient names for use within the main() loop.
 */
void msgform(char *msgfile, FILE * mfout, char *sbuf, char *rbuf, char *nbuf,
		char *pbuf, time_t * mid_buf, char *rmname, char *subj)
			/* sender */
			/* recipient (in this case, an Internet address) */
			/* source node */
			/* path */
			/* message ID */
			/* room name */
			/* subject */
{
	int a, b, c, e, old, mtype, aflag;
	int real = 0;
	char aaa[128], bbb[128];
	FILE *fp;
	int width;
	int generate_subject = 1;

	strcpy(subj, "");
	strcpy(pbuf, "");
	strcpy(nbuf, NODENAME);
	strcpy(rmname, "");
	time(mid_buf);
	width = 80;
	fp = fopen(msgfile, "rb");
	if (fp == NULL) {
		fprintf(stderr, "netmailer: can't open message file\n");
		return;
	}
	strcpy(aaa, "");
	old = 255;
	c = 1;			/* c is the current pos */
	e = getc(fp);
	if (e != 255) {
		fprintf(stderr, "netmailer: This is not a Citadel message.\n");
		goto END;
	}
	mtype = getc(fp);
	aflag = getc(fp);
	goto BONFGM;

A:	if (aflag == 1)
		goto AFLAG;
	old = real;
	a = getc(fp);
	real = a;
	if (a == 0)
		goto END;
	if (a < 0)
		goto END;

	/* generate subject... */
	if ((generate_subject == 1) && (strlen(subj) < 60)) {
		subj[strlen(subj) + 1] = 0;
		subj[strlen(subj)] = (((a > 31) && (a < 127)) ? a : 32);
	}
	if (((a == 13) || (a == 10)) && (old != 13) && (old != 10))
		a = 32;
	if (((old == 13) || (old == 10)) && ((real == 32) || (real == 13) || (real == 10))) {
		fprintf(mfout, "\n");
		c = 1;
	}
	if (a != 32) {
		if (((strlen(aaa) + c) > (width - 5)) && (strlen(aaa) > (width - 5))) {
			fprintf(mfout, "\n%s", aaa);
			c = strlen(aaa);
			aaa[0] = 0;
		}
		b = strlen(aaa);
		aaa[b] = a;
		aaa[b + 1] = 0;
	}
	if (a == 32) {
		if ((strlen(aaa) + c) > (width - 5)) {
			fprintf(mfout, "\n");
			c = 1;
		}
		fprintf(mfout, "%s ", aaa);
		++c;
		c = c + strlen(aaa);
		strcpy(aaa, "");
		goto A;
	}
	if ((a == 13) || (a == 10)) {
		fprintf(mfout, "%s\n", aaa);
		c = 1;
		strcpy(aaa, "");
		goto A;
	}
	goto A;

AFLAG:	a = getc(fp);
	if (a == 0)
		goto END;
	if (a == 13) {
		putc(10, mfout);
	} else {
		putc(a, mfout);
	}
	goto AFLAG;

END:	fclose(fp);
	return;

BONFGM:	b = getc(fp);
	if (b < 0)
		goto END;
	if (b == 'M')
		goto A;
	fpgetfield(fp, bbb);
	if (b == 'A')
		strcpy(sbuf, bbb);
	if (b == 'R')
		strcpy(rbuf, bbb);
	if (b == 'N')
		strcpy(nbuf, bbb);
	if (b == 'P')
		strcpy(pbuf, bbb);
	if (b == 'I')
		*mid_buf = atol(bbb);
	if (b == 'O')
		strcpy(rmname, bbb);
	if (b == 'U') {
		strcpy(subj, bbb);
		generate_subject = 0;	/* have a real subj so don't gen one */
	}
	goto BONFGM;
}

int main(int argc, char **argv)
{
	int a;
	FILE *fp, *rmail;
	char sbuf[200], rbuf[200], cstr[100], fstr[128];
	char nbuf[64], pbuf[128], rmname[128], buf[128];
	char datestamp[256];
	char subject[256];
	time_t mid_buf;
	time_t now;
	int mlist = 0;

	openlog("netmailer", LOG_PID, LOG_USER);
	get_config();
	LoadInternetConfig();
	strcpy(temp, tmpnam(NULL));	/* temp file name */

	if ((argc < 2) || (argc > 3)) {
		fprintf(stderr, "netmailer: usage: "
			"netmailer <filename> [mlist]\n");
		exit(1);
	}
	/*
	 * If we are running in mailing list mode, the room is being two-way
	 * gatewayed to an Internet mailing list.  Since many listprocs only
	 * accept postings from subscribed addresses, we must always use the
	 * room's address as the originating user.
	 */
	if ((argc == 3) && (!strcasecmp(argv[2], "mlist"))) {
		mlist = 1;
	}
	/* convert to ASCII & get info */
	fp = fopen(temp, "w");
	msgform(argv[1], fp, sbuf, rbuf, nbuf, pbuf, &mid_buf, rmname, subject);
	fclose(fp);

	strcpy(buf, rmname);
	strcpy(rmname, "room_");
	strcat(rmname, buf);
	for (a = 0; a < strlen(rmname); ++a) {
		if (rmname[a] == ' ')
			rmname[a] = '_';
		rmname[a] = tolower(rmname[a]);
	}

	sprintf(cstr, SENDMAIL, rbuf);
	rmail = (FILE *) popen(cstr, "w");

	strcpy(fstr, sbuf);
	for (a = 0; a < strlen(sbuf); ++a)
		if (sbuf[a] == 32)
			sbuf[a] = '_';
	for (a = 0; a < strlen(rbuf); ++a)
		if (rbuf[a] == 32)
			rbuf[a] = '_';

	/*
	 * This logic attempts to compose From and From: lines that are
	 * as RFC822-compliant as possible.  The return addresses are correct
	 * if you're using Citadel's 'citmail' delivery agent to allow BBS
	 * users to receive Internet mail.
	 */
	fprintf(rmail, "From ");
	if (strcasecmp(nbuf, NODENAME))
		fprintf(rmail, "%s!", nbuf);

	if (!strcasecmp(nbuf, NODENAME))
		strcpy(nbuf, FQDN);

	if (mlist) {
		fprintf(rmail, "%s\n", rmname);
		fprintf(rmail, "From: %s@%s (%s)\n", rmname, FQDN, fstr);
	} else {

		if (!strcasecmp(nbuf, NODENAME)) {	/* from this system */
			fprintf(rmail, "%s\n", pbuf);
			fprintf(rmail, "From: %s@%s (%s)\n",
				sbuf, FQDN, fstr);
		} else if (haschar(nbuf, '.')) {	/* from an FQDN */
			fprintf(rmail, "%s\n", sbuf);
			fprintf(rmail, "From: %s@%s (%s)\n",
				sbuf, nbuf, fstr);
		} else {	/* from another Cit */
			fprintf(rmail, "%s\n", sbuf);
			fprintf(rmail, "From: %s@%s.%s (%s)\n",
				sbuf, nbuf, GW_DOMAIN, fstr);
		}

	}

	/*
	 * Everything else is pretty straightforward.
	 */
	fprintf(rmail, "To: %s\n", rbuf);
	time(&now);
	generate_rfc822_datestamp(datestamp, now);
	fprintf(rmail, "Date: %s\n", datestamp);
	fprintf(rmail, "Message-Id: <%ld@%s>\n", (long) mid_buf, nbuf);
	fprintf(rmail, "X-Mailer: %s\n", CITADEL);
	fprintf(rmail, "Subject: %s\n", subject);
	fprintf(rmail, "\n");
	fp = fopen(temp, "r");
	if (fp != NULL) {
		do {
			a = getc(fp);
			if (a >= 0)
				putc(a, rmail);
		} while (a >= 0);
		fclose(fp);
	}
	fprintf(rmail, "\n");
	pclose(rmail);

	unlink(temp);		/* get rid of the ASCII file */
	execlp("./netproc", "netproc", "-i", NULL);
	exit(0);		/* go back to the main program */
}
