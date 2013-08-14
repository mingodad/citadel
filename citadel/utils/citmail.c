/*
 * This program attempts to act like a local MDA if you're using sendmail or
 * some other non-Citadel MTA.  It basically just contacts the Citadel LMTP
 * listener on a unix domain socket and transmits the message.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "citadel_dirs.h"

int serv_sock;
int debug = 0;

void strip_trailing_nonprint(char *buf)
{
        while ( (!IsEmptyStr(buf)) && (!isprint(buf[strlen(buf) - 1])) )
                buf[strlen(buf) - 1] = 0;
}


void timeout(int signum)
{
	exit(signum);
}


int uds_connectsock(char *sockpath)
{
	int s;
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "citmail: Can't create socket: %s\n",
			strerror(errno));
		exit(3);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, "citmail: can't connect: %s\n",
			strerror(errno));
		close(s);
		exit(3);
	}

	return s;
}


/*
 * input binary data from socket
 */
void serv_read(char *buf, int bytes)
{
	int len, rlen;

	len = 0;
	while (len < bytes) {
		rlen = read(serv_sock, &buf[len], bytes - len);
		if (rlen < 1) {
			return;
		}
		len = len + rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			return;
		}
		bytes_written = bytes_written + retval;
	}
}



/*
 * input string from socket - implemented in terms of serv_read()
 */
void serv_gets(char *buf)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		serv_read(&buf[i], 1);
		if (buf[i] == '\n' || i == (SIZ-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (SIZ-1))
		while (buf[i] != '\n')
			serv_read(&buf[i], 1);

	/* Strip all trailing nonprintables (crlf)
	 */
	buf[i] = 0;
	strip_trailing_nonprint(buf);
	if (debug) fprintf(stderr, "> %s\n", buf);
}


/*
 * send line to server - implemented in terms of serv_write()
 */
void serv_puts(char *buf)
{
	if (debug) fprintf(stderr, "< %s\n", buf);
	serv_write(buf, strlen(buf));
	serv_write("\n", 1);
}



void cleanup(int exitcode) {
	char buf[1024];

	if (exitcode != 0) {
		fprintf(stderr, "citmail: error #%d occurred while sending mail.\n", exitcode);
		fprintf(stderr, "Please check your Citadel configuration.\n");
	}
	serv_puts("QUIT");
	serv_gets(buf);
	exit(exitcode);
}



int main(int argc, char **argv) {
	char buf[1024];
	char fromline[1024];
	FILE *fp;
	int i;
	struct passwd *pw;
	int from_header = 0;
	int in_body = 0;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	char *sp, *ep;
	char hostname[256];
	char **recipients = NULL;
	int num_recipients = 0;
	int to_or_cc = 0;
	int read_recipients_from_headers = 0;
	char *add_these_recipients = NULL;

	for (i=1; i<argc; ++i) {
		if (!strcmp(argv[i], "-d")) {
			debug = 1;
		}
		else if (!strcmp(argv[i], "-t")) {
			read_recipients_from_headers = 1;
		}
		else if (argv[i][0] != '-') {
			++num_recipients;
			recipients = realloc(recipients, (num_recipients * sizeof (char *)));
			recipients[num_recipients - 1] = strdup(argv[i]);
		}
	}
	       
	/* TODO: should we be able to calculate relative dirs? */
	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);

	pw = getpwuid(getuid());

	fp = tmpfile();
	if (fp == NULL) return(errno);
	serv_sock = uds_connectsock(file_lmtp_socket);	/* FIXME: if called as 'sendmail' connect to file_lmtp_unfiltered_socket */
	serv_gets(buf);
	if (buf[0] != '2') {
		fprintf(stderr, "%s\n", &buf[4]);
		if (debug) fprintf(stderr, "citmail: could not connect to LMTP socket.\n");
		cleanup(1);
	}

	sp = strchr (buf, ' ');
	if (sp == NULL) {
		if (debug) fprintf(stderr, "citmail: could not calculate hostname.\n");
		cleanup(2);
	}
	sp ++;
	ep = strchr (sp, ' ');
	if (ep == NULL) {
		if (debug) fprintf(stderr, "citmail: error parsing hostname\n");
		cleanup(3);
	}
	else
		*ep = '\0';

	strncpy(hostname, sp, sizeof hostname);

	snprintf(fromline, sizeof fromline, "From: %s@%s", pw->pw_name, hostname);
	while (fgets(buf, 1024, stdin) != NULL) {
		if ( ( (buf[0] == 13) || (buf[0] == 10)) && (in_body == 0) ) {
			in_body = 1;
			if (from_header == 0) {
				fprintf(fp, "%s%s", fromline, buf);
			}
		}
		if (in_body == 0 && !strncasecmp(buf, "From:", 5)) {
			strcpy(fromline, buf);
			from_header = 1;
		}

		if (read_recipients_from_headers) {
			add_these_recipients = NULL;
			if ((isspace(buf[0])) && (to_or_cc)) {
				add_these_recipients = buf;
			}
			else {
				if ((!strncasecmp(buf, "To:", 3)) || (!strncasecmp(buf, "Cc:", 3))) {
					to_or_cc = 1;
				}
				else {
					to_or_cc = 0;
				}
				if (to_or_cc) {
					add_these_recipients = &buf[3];
				}
			}

			if (add_these_recipients) {
				int num_recp_on_this_line;
				char this_recp[256];

				num_recp_on_this_line = num_tokens(add_these_recipients, ',');
				for (i=0; i<num_recp_on_this_line; ++i) {
					extract_token(this_recp, add_these_recipients,
						i, ',', sizeof this_recp);
					striplt(this_recp);
					if (!IsEmptyStr(this_recp)) {
						++num_recipients;
						recipients = realloc(recipients,
							(num_recipients * sizeof (char *)));
						recipients[num_recipients - 1] = strdup(this_recp);
					}
				}
			}
		}

		fprintf(fp, "%s", buf);
	}
	strip_trailing_nonprint(fromline);

	sprintf(buf, "LHLO %s", hostname);
	serv_puts(buf);
	do {
		serv_gets(buf);
		strcat(buf, "    ");
	} while (buf[3] == '-');
	if (buf[0] != '2') {
		if (debug) fprintf(stderr, "citmail: LHLO command failed\n");
		cleanup(4);
	}

	snprintf(buf, sizeof buf, "MAIL %s", fromline);
	serv_puts(buf);
	serv_gets(buf);
	if (buf[0] != '2') {
		if (debug) fprintf(stderr, "citmail: MAIL command failed\n");
		cleanup(5);
	}

	for (i=0; i<num_recipients; ++i) {
		snprintf(buf, sizeof buf, "RCPT To: %s", recipients[i]);
		serv_puts(buf);
		serv_gets(buf);
		free(recipients[i]);
	}
	free(recipients);

	serv_puts("DATA");
	serv_gets(buf);
	if (buf[0]!='3') {
		if (debug) fprintf(stderr, "citmail: DATA command failed\n");
		cleanup(6);
	}

	rewind(fp);
	while (fgets(buf, sizeof buf, fp) != NULL) {
		strip_trailing_nonprint(buf);
		serv_puts(buf);
	}
	serv_puts(".");
	serv_gets(buf);
	if (buf[0] != '2') {
		fprintf(stderr, "%s\n", &buf[4]);
		cleanup(7);
	}
	else {
		cleanup(0);
	}

	/* We won't actually reach this statement but the compiler will
	 * display a spurious warning about an invalid return type if
	 * we don't return an int.
	 */
	return(0);
}
