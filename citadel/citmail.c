/*
 * citmail.c v4.2
 * $Id$
 *
 * This program may be used as a local mail delivery agent, which will allow
 * all Citadel users to receive Internet e-mail.  To enable this functionality,
 * you must tell sendmail, smail, or whatever mailer you are using, that this
 * program is your local mail delivery agent.  This program is a direct
 * replacement for lmail, deliver, or whatever.
 *
 * Usage:
 *
 * citmail <recipient>       - Deliver a message
 * citmail -t <recipient>    - Address test mode (will not deliver)
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <syslog.h>
#include <limits.h>
#include "citadel.h"
#include "config.h"
#include "internetmail.h"

/* message delivery classes */
enum {
	DELIVER_LOCAL,
	DELIVER_REMOTE,
	DELIVER_INTERNET,
	DELIVER_CCITADEL
};
	

#undef tolower
#define tolower(x) isupper(x) ? (x+'a'-'A') : x

char *monthdesc[] =
{"Jan", "Feb", "Mar", "Apr", "May", "Jun",
 "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

char ALIASES[128];
char CIT86NET[128];
char SENDMAIL[128];
char FALLBACK[128];
char GW_DOMAIN[128];
char TABLEFILE[128];
char OUTGOING_FQDN[128];
int RUN_NETPROC = 1;


long conv_date(char *sdbuf)
{
	int a, b, cpos, tend, tval;
	time_t now;
	struct tm *tmbuf;
	char dbuf[128];

	strcpy(dbuf, sdbuf);
	time(&now);
	tmbuf = (struct tm *) localtime(&now);

	/* get rid of + or - timezone mods */
	for (a = 0; a < strlen(dbuf); ++a)
		if ((dbuf[a] == '+') || (dbuf[a] == '-'))
			do {
				strcpy(&dbuf[a], &dbuf[a + 1]);
			} while ((dbuf[a] != 32) && (dbuf[a] != 0));

	/* try and extract the time by looking for colons */
	cpos = (-1);
	for (a = strlen(dbuf); a >= 0; --a)
		if ((dbuf[a] == ':') && (atoi(&dbuf[a - 1]) != 0))
			cpos = a;
	if (cpos >= 0) {
		cpos = cpos - 2;
		tend = strlen(dbuf);
		for (a = tend; a >= cpos; --a)
			if (dbuf[a] == ' ')
				tend = a;

		tmbuf->tm_hour = atoi(&dbuf[cpos]);
		tmbuf->tm_min = atoi(&dbuf[cpos + 3]);
		tmbuf->tm_sec = atoi(&dbuf[cpos + 6]);

		do {
			strcpy(&dbuf[cpos], &dbuf[cpos + 1]);
		} while ((dbuf[cpos] != 32) && (dbuf[cpos] != 0));
	}
	/* next try to extract a month */

	tval = (-1);
	for (a = 0; a < strlen(dbuf); ++a)
		for (b = 0; b < 12; ++b)
			if (!strncmp(&dbuf[a], monthdesc[b], 3)) {
				tval = b;
				cpos = a;
			}
	if (tval >= 0) {
		tmbuf->tm_mon = tval;
		strcpy(&dbuf[cpos], &dbuf[cpos + 3]);
	}
	/* now the year */

	for (a = 0; a < strlen(dbuf); ++a)
		if ((atoi(&dbuf[a]) >= 1900) && (dbuf[a] != 32)) {
			tmbuf->tm_year = atoi(&dbuf[a]) - 1900;
			strcpy(&dbuf[a], &dbuf[a + 4]);
		}
	/* whatever's left is the mday (hopefully) */

	for (a = 0; a < strlen(dbuf); ++a)
		if ((dbuf[a] != 32) && (atoi(&dbuf[a]) >= 1) && (atoi(&dbuf[a]) <= 31)
		    && ((a == 0) || (dbuf[a - 1] == ' '))) {
			tmbuf->tm_mday = atoi(&dbuf[a]);
			strcpy(&dbuf[a], &dbuf[a + 2]);
		}
	return ((long) mktime(tmbuf));
}


#ifndef HAVE_STRERROR
/*
 * replacement strerror() for systems that don't have it
 */
char *strerror(int e)
{
	static char buf[32];

	snprintf(buf, sizeof buf, "errno = %d", e);
	return (buf);
}
#endif

int haschar(char *st, int ch)
{
	int a, b;
	b = 0;
	for (a = 0; a < strlen(st); ++a)
		if (st[a] == ch)
			++b;
	return (b);
}

void strip_trailing_whitespace(char *buf)
{
	while (isspace(buf[strlen(buf) - 1]))
		buf[strlen(buf) - 1] = 0;
}

/* strip leading and trailing spaces */
void striplt(char *buf)
{
	while ((strlen(buf) > 0) && (buf[0] == 32))
		strcpy(buf, &buf[1]);
	while (buf[strlen(buf) - 1] == 32)
		buf[strlen(buf) - 1] = 0;
}


/*
 * Check to see if a given FQDN really maps to a Citadel network node
 */
void host_alias(char host[])
{

	int a;

	/* What name is the local host known by? */
	/* if (!strcasecmp(host, config.c_fqdn)) { */
	if (IsHostLocal(host)) {
		strcpy(host, config.c_nodename);
		return;
	}
	/* Other hosts in the gateway domain? */
	for (a = 0; a < strlen(host); ++a) {
		if ((host[a] == '.') && (!strcasecmp(&host[a + 1], GW_DOMAIN))) {
			host[a] = 0;
			for (a = 0; a < strlen(host); ++a) {
				if (host[a] == '.')
					host[a] = 0;
			}
			return;
		}
	}

	/* Otherwise, do nothing... */
}



/*
 * Split an RFC822-style address into userid, host, and full name
 */
void process_rfc822_addr(char *rfc822, char *user, char *node, char *name)
{
	int a;

	/* extract full name - first, it's From minus <userid> */
	strcpy(name, rfc822);
	for (a = 0; a < strlen(name); ++a)
		if (name[a] == '<') {
			do {
				strcpy(&name[a], &name[a + 1]);
			} while ((strlen(name) > 0) && (name[a] != '>'));
			strcpy(&name[a], &name[a + 1]);
		}
	/* strip anything to the left of a bang */
	while ((strlen(name) > 0) && (haschar(name, '!') > 0))
		strcpy(name, &name[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(name); ++a) {
		if (name[a] == '@')
			name[a] = 0;
		if (name[a] == '%')
			name[a] = 0;
	}

	/* but if there are parentheses, that changes the rules... */
	if ((haschar(rfc822, '(') == 1) && (haschar(rfc822, ')') == 1)) {
		strcpy(name, rfc822);
		while ((strlen(name) > 0) && (name[0] != '(')) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == ')')
				name[a] = 0;
	}
	/* but if there are a set of quotes, that supersedes everything */
	if (haschar(rfc822, 34) == 2) {
		strcpy(name, rfc822);
		while ((strlen(name) > 0) && (name[0] != 34)) {
			strcpy(&name[0], &name[1]);
		}
		strcpy(&name[0], &name[1]);
		for (a = 0; a < strlen(name); ++a)
			if (name[a] == 34)
				name[a] = 0;
	}
	/* extract user id */
	strcpy(user, rfc822);

	/* first get rid of anything in parens */
	for (a = 0; a < strlen(user); ++a)
		if (user[a] == '(') {
			do {
				strcpy(&user[a], &user[a + 1]);
			} while ((strlen(user) > 0) && (user[a] != ')'));
			strcpy(&user[a], &user[a + 1]);
		}
	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(user, '<') == 1) && (haschar(user, '>') == 1)) {
		while ((strlen(user) > 0) && (user[0] != '<')) {
			strcpy(&user[0], &user[1]);
		}
		strcpy(&user[0], &user[1]);
		for (a = 0; a < strlen(user); ++a)
			if (user[a] == '>')
				user[a] = 0;
	}
	/* strip anything to the left of a bang */
	while ((strlen(user) > 0) && (haschar(user, '!') > 0))
		strcpy(user, &user[1]);

	/* and anything to the right of a @ or % */
	for (a = 0; a < strlen(user); ++a) {
		if (user[a] == '@')
			user[a] = 0;
		if (user[a] == '%')
			user[a] = 0;
	}



	/* extract node name */
	strcpy(node, rfc822);

	/* first get rid of anything in parens */
	for (a = 0; a < strlen(node); ++a)
		if (node[a] == '(') {
			do {
				strcpy(&node[a], &node[a + 1]);
			} while ((strlen(node) > 0) && (node[a] != ')'));
			strcpy(&node[a], &node[a + 1]);
		}
	/* if there's a set of angle brackets, strip it down to that */
	if ((haschar(node, '<') == 1) && (haschar(node, '>') == 1)) {
		while ((strlen(node) > 0) && (node[0] != '<')) {
			strcpy(&node[0], &node[1]);
		}
		strcpy(&node[0], &node[1]);
		for (a = 0; a < strlen(node); ++a)
			if (node[a] == '>')
				node[a] = 0;
	}
	/* strip anything to the left of a @ */
	while ((strlen(node) > 0) && (haschar(node, '@') > 0))
		strcpy(node, &node[1]);

	/* strip anything to the left of a % */
	while ((strlen(node) > 0) && (haschar(node, '%') > 0))
		strcpy(node, &node[1]);

	/* reduce multiple system bang paths to node!user */
	while ((strlen(node) > 0) && (haschar(node, '!') > 1))
		strcpy(node, &node[1]);

	/* now get rid of the user portion of a node!user string */
	for (a = 0; a < strlen(node); ++a)
		if (node[a] == '!')
			node[a] = 0;



	/* strip leading and trailing spaces in all strings */
	striplt(user);
	striplt(node);
	striplt(name);
}

/*
 * Copy line by line, ending at EOF or a "." 
 */
void loopcopy(FILE * to, FILE * from)
{
	char buf[1024];
	char *r;

	while (1) {
		r = fgets(buf, sizeof(buf), from);
		if (r == NULL)
			return;
		strip_trailing_whitespace(buf);
		if (!strcmp(buf, "."))
			return;
		fprintf(to, "%s\n", buf);
	}
}



/*
 * Try to extract a numeric message ID
 */
long extract_msg_id(char *id_string) {
	long msgid = 0L;
	int i, j;
	char buf[256];

	strncpy(buf, id_string, sizeof buf);
	id_string[255] = 0;

	for (i=0; i<strlen(buf); ++i) {
		if (buf[i]=='<') {
			strcpy(buf, &buf[i]);
			for (j=0; j<strlen(buf); ++j)
				if (buf[j]=='>') buf[j]=0;
		}
	}

	msgid = atol(buf);
	if (msgid) return(msgid);

	for (i=0; i<strlen(buf); ++i) {
		if (!isdigit(buf[i])) {
			strcpy(&buf[i], &buf[i+1]);
			i = 0;
		}
	}

	msgid = atol(buf);
	return(msgid);
}

	
/*
 * pipe message through netproc
 */
void do_citmail(char recp[], int dtype)
{

	time_t now;
	FILE *temp;
	int a;
	int format_type = 0;
	char buf[128];
	char from[512];
	char userbuf[256];
	char frombuf[256];
	char nodebuf[256];
	char destsys[256];
	char subject[256];
	long message_id = 0L;
	char targetroom[256];
	char content_type[256];
	char *extra_headers = NULL;


	if (dtype == DELIVER_REMOTE) {

		/* get the Citadel node name out of the path */
		strncpy(destsys, recp, sizeof(destsys));
		for (a = 0; a < strlen(destsys); ++a) {
			if ((destsys[a] == '!') || (destsys[a] == '.')) {
				destsys[a] = 0;
			}
		}

		/* chop the system name out, so we're left with a user */
		while (haschar(recp, '!'))
			strcpy(recp, &recp[1]);
	}
	/* Convert underscores to spaces */
	for (a = 0; a < strlen(recp); ++a)
		if (recp[a] == '_')
			recp[a] = ' ';

	/* Are we delivering to a room instead of a user? */
	if (!strncasecmp(recp, "room ", 5)) {
		strcpy(targetroom, &recp[5]);
		strcpy(recp, "");
	} else {
		strcpy(targetroom, MAILROOM);
	}

	time(&now);
	snprintf(from, sizeof from, "postmaster@%s", config.c_nodename);

	snprintf(buf, sizeof buf, "./network/spoolin/citmail.%d", getpid());
	temp = fopen(buf, "w");

	strcpy(subject, "");
	strcpy(nodebuf, config.c_nodename);
	strcpy(content_type, "text/plain");

	do {
		if (fgets(buf, 128, stdin) == NULL)
			strcpy(buf, ".");
		strip_trailing_whitespace(buf);

		if (!strncasecmp(buf, "Subject: ", 9))
			strcpy(subject, &buf[9]);
		else if (!strncasecmp(buf, "Date: ", 6))
			now = conv_date(&buf[6]);
		else if (!strncasecmp(buf, "From: ", 6))
			strcpy(from, &buf[6]);
		else if (!strncasecmp(buf, "Message-ID: ", 12))
			message_id = extract_msg_id(&buf[12]);
		else if (!strncasecmp(buf, "Content-type: ", 14))
			strcpy(content_type, &buf[14]);
		else if (!strncasecmp(buf, "From ", 5)) {	/* ignore */
		} else {
			if (extra_headers == NULL) {
				extra_headers = malloc(strlen(buf) + 2);
				strcpy(extra_headers, "");
			} else {
				extra_headers = realloc(extra_headers,
							(strlen(extra_headers) + strlen(buf) + 2));
			}
			strcat(extra_headers, buf);
			strcat(extra_headers, "\n");
		}
	} while ((strcmp(buf, ".")) && (strcmp(buf, "")));

	process_rfc822_addr(from, userbuf, nodebuf, frombuf);

	if (!strncasecmp(content_type, "text/plain", 10))
		format_type = 1;	/* plain ASCII message */
	else
		format_type = 4;	/* MIME message */

	/* now convert it to Citadel format */

	/* Header bytes */
	putc(255, temp);	/* 0xFF = start-of-message byte */
	putc(MES_NORMAL, temp);	/* Non-anonymous message */
	putc(format_type, temp);	/* Format type */

	/* Origination */
	fprintf(temp, "P%s@%s%c", userbuf, nodebuf, 0);
	if (message_id)
		fprintf(temp, "I%ld%c", message_id, 0);
	fprintf(temp, "T%ld%c", (long)now, 0);
	fprintf(temp, "A%s%c", userbuf, 0);

	/* Destination */
	if (strlen(targetroom) > 0) {
		fprintf(temp, "O%s%c", targetroom, 0);
	} else {
		fprintf(temp, "O%s%c", MAILROOM, 0);
	}

	fprintf(temp, "N%s%c", nodebuf, 0);
	fprintf(temp, "H%s%c", frombuf, 0);
	if (dtype == DELIVER_REMOTE) {
		fprintf(temp, "D%s%c", destsys, 0);
	}
	if (strlen(recp) > 0) {
		fprintf(temp, "R%s%c", recp, 0);
	}
	/* Subject and text */
	if (strlen(subject) > 0) {
		fprintf(temp, "U%s%c", subject, 0);
	}
	putc('M', temp);
	if (format_type == 4) {
		fprintf(temp, "Content-type: %s\n", content_type);
		if (extra_headers != NULL)
			fprintf(temp, "%s", extra_headers);
		fprintf(temp, "\n");
	}
	if (extra_headers != NULL)
		free(extra_headers);
	if (strcmp(buf, "."))
		loopcopy(temp, stdin);
	putc(0, temp);
	fclose(temp);
}


void do_uudecode(char *target)
{
	static char buf[1024];
	FILE *fp;

	snprintf(buf, sizeof buf, "cd %s; uudecode", target);

	fp = popen(buf, "w");
	if (fp == NULL)
		return;
	while (fgets(buf, 1024, stdin) != NULL) {
		fprintf(fp, "%s", buf);
	}
	pclose(fp);

}

int alias(char *name)
{
	FILE *fp;
	int a;
	char abuf[256];

	fp = fopen(ALIASES, "r");
	if (fp == NULL) {
		syslog(LOG_ERR, "cannot open %s: %s", ALIASES, strerror(errno));
		return (2);
	}
	while (fgets(abuf, 256, fp) != NULL) {
		strip_trailing_whitespace(abuf);
		for (a = 0; a < strlen(abuf); ++a) {
			if (abuf[a] == ',') {
				abuf[a] = 0;
				if (!strcasecmp(name, abuf)) {
					strcpy(name, &abuf[a + 1]);
				}
			}
		}
	}
	fclose(fp);
	return (0);
}


void deliver(char recp[], int is_test, int deliver_to_ignet)
{

	/* various ways we can deliver mail... */

	if (deliver_to_ignet) {
		syslog(LOG_NOTICE, "to Citadel network user %s", recp);
		if (is_test == 0)
			do_citmail(recp, DELIVER_REMOTE);
	} else if (!strcmp(recp, "uudecode")) {
		syslog(LOG_NOTICE, "uudecoding to bit bucket directory");
		if (is_test == 0)
			do_uudecode(config.c_bucket_dir);
	} else if (!strcmp(recp, "cit86net")) {
		syslog(LOG_NOTICE, "uudecoding to Cit86net spool");
		if (is_test == 0) {
			do_uudecode(CIT86NET);
			system("exec ./BatchTranslate86");
		}
	} else if (!strcmp(recp, "null")) {
		syslog(LOG_NOTICE, "zapping nulled message");
	} else {
		/* Otherwise, the user is local (or an unknown name was
		 * specified, in which case we let netproc handle the bounce)
		 */
		syslog(LOG_NOTICE, "to Citadel recipient %s", recp);
		if (is_test == 0)
			do_citmail(recp, DELIVER_LOCAL);
	}

}



int main(int argc, char **argv)
{
	int is_test = 0;
	int deliver_to_ignet = 0;
	static char recp[1024], buf[1024];
	static char user[1024], node[1024], name[1024];
	int a;

	openlog("citmail", LOG_PID, LOG_USER);
	get_config();
	LoadInternetConfig();

	if (!strcmp(argv[1], "-t")) {
		is_test = 1;
		syslog(LOG_NOTICE, "test mode - will not deliver");
	}
	if (is_test == 0) {
		strcpy(recp, argv[1]);
	} else {
		strcpy(recp, argv[2]);
	}

/*** Non-SMTP delivery mode ***/
	syslog(LOG_NOTICE, "recp: %s", recp);
	for (a = 0; a < 2; ++a) {
		alias(recp);
	}

	/* did we alias it back to a remote address? */
	if ((haschar(recp, '%'))
	    || (haschar(recp, '@'))
	    || (haschar(recp, '!'))) {

		process_rfc822_addr(recp, user, node, name);
		host_alias(node);

		/* If there are dots, it's an Internet host, so feed it
		 * back to an external mail transport agent such as sendmail.
		 */
		if (haschar(node, '.')) {
			snprintf(buf, sizeof buf, SENDMAIL, recp);
			system(buf);
			exit(0);
		}
		/* Otherwise, we're dealing with Citadel mail. */
		else {
			snprintf(recp, sizeof recp, "%s!%s", node, user);
			deliver_to_ignet = 1;
		}

	}
	deliver(recp, is_test, deliver_to_ignet);

	if (RUN_NETPROC) {
		syslog(LOG_NOTICE, "running netproc");
		if (system("/bin/true") != 0) {
			syslog(LOG_ERR, "netproc failed: %s", strerror(errno));
		}
	} else {
		syslog(LOG_NOTICE, "skipping netproc");
	}
	exit(0);
}
