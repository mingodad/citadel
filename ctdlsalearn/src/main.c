/*
 * (c) 2009-2012 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>

int verbose = 0;

void do_room(int sock, char *roomname, char *salearnargs)
{
#define MAXMSGS 1000
	char buf[1024];
	long msgs[MAXMSGS];
	int num_msgs = 0;
	FILE *fp;
	int i;

	if (verbose) printf("%s: ", roomname);
	fflush(stdout);
	sock_printf(sock, "GOTO %s\n", roomname);
	sock_getln(sock, buf, sizeof buf);
	if (buf[0] != '2') {
		if (verbose) printf("%s\n", &buf[4]);
		return;
	}

	/* Only fetch enough message pointers to fill our buffer.
	 * Since we're going to delete them, if there are more we will get them on the next run.
	 */
	sock_printf(sock, "MSGS LAST|%d\n", MAXMSGS);
	sock_getln(sock, buf, sizeof buf);
	if (buf[0] != '1') {
		if (verbose) printf("%s\n", &buf[4]);
		return;
	}
	while (sock_getln(sock, buf, sizeof buf), strcmp(buf, "000")) {
		msgs[num_msgs++] = atol(buf);
	}
	if (verbose) printf("%d messages\n", num_msgs);

	if (num_msgs == 0) return;
	for (i=0; i<num_msgs; ++i) {
		snprintf(buf, sizeof buf, "sa-learn %s", salearnargs);
		if (!verbose) strcat(buf, " >/dev/null");
		fp = popen(buf, "w");
		if (fp == NULL) return;
		if (verbose) printf("Submitting message %ld\n", msgs[i]);
		sock_printf(sock, "MSG2 %ld\n", msgs[i]);
		sock_getln(sock, buf, sizeof buf);
		if (buf[0] == '1') {
			while (sock_getln(sock, buf, sizeof buf), strcmp(buf, "000")) {
				fprintf(fp, "%s\n", buf);
			}
		}
		if (pclose(fp) == 0) {
			sock_printf(sock, "DELE %ld\n", msgs[i]);
			sock_getln(sock, buf, sizeof buf);
			if (verbose) printf("%s\n", &buf[4]);
		}
	}
}


int main(int argc, char **argv)
{
	int server_socket = 0;
	char buf[1024];
	int ipgm_secret = (-1);
	int a, c, i = 0;
	char *ctdldir = "/usr/local/citadel" ;

	while ((a = getopt(argc, argv, "vh:")) != EOF) switch(a) {
		case 'v':
			verbose = 1;
			break;
		case 'h':
			ctdldir = strdup(optarg);
			break;
		default:
			fprintf(stderr, "%s: usage: %s [-v]\n", argv[0], argv[0]);
			return(1);
	}

	if (verbose) {
		printf("\nAuto-submit spam and ham to sa-learn for Citadel " PACKAGE_VERSION "\n");
		printf("(c) 2009-2011 citadel.org GPLv3\n");
	}

	if (chdir(ctdldir) != 0) {
		fprintf(stderr, "%s: cannot change directory to %s: %s\n", argv[0], ctdldir, strerror(errno));
		return(errno);
	}
	else if (verbose) {
		fprintf(stderr, "Changed directory to %s\n", ctdldir);
	}

	server_socket = (-1);

	if (verbose) fprintf(stderr, "Connecting to server...\n");
	server_socket = uds_connectsock("citadel-admin.socket");

	if (server_socket < 0) {
		if (verbose) fprintf(stderr, "Could not connect to Citadel server.\n");
		exit(1);
	}

	sock_getln(server_socket, buf, sizeof buf);
	if (verbose) printf("%s\n", &buf[4]);

	if (buf[0] == '2') {
		do_room(server_socket, "0000000001.spam", "--dbpath /home/spam/.spamassassin --spam");
		do_room(server_socket, "0000000001.ham", "--dbpath /home/spam/.spamassassin --ham");
	}

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	if (verbose) printf("%s\n", &buf[4]);
	close(server_socket);
	exit(0);
}
