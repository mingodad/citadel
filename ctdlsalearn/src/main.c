/*
 * (c) 2009 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "ctdlsalearn.h"

int discover_ipgm_secret(char *dirname) {
	int fd;
	struct partial_config ccc;
	char configfile[1024];

	sprintf(configfile, "%s/citadel.config", dirname);
	fd = open(configfile, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "%s: %s\n", configfile, strerror(errno));
		return(-1);
	}

	if (read(fd, &ccc, sizeof(struct partial_config)) != sizeof(struct partial_config)) {
		fprintf(stderr, "%s: %s\n", configfile, strerror(errno));
		return(-1);
	}
	if (close(fd) != 0) {
		fprintf(stderr, "%s: %s\n", configfile, strerror(errno));
		return(-1);
	}
	return(ccc.c_ipgm_secret);
}


void do_room(int sock, char *roomname, char *salearnargs)
{
#define MAXMSGS 1000
	char buf[1024];
	long msgs[MAXMSGS];
	int num_msgs = 0;
	FILE *fp;
	int i;

	printf("Trying <%s>\n", roomname);
	sock_printf(sock, "GOTO %s\n", roomname);
	sock_getln(sock, buf, sizeof buf);
	printf("%s\n", buf);
	if (buf[0] != '2') return;

	/* Only fetch enough message pointers to fill our buffer.
	 * Since we're going to delete them, if there are more we will get them on the next run.
	 */
	sock_printf(sock, "MSGS LAST|%d\n", MAXMSGS);
	sock_getln(sock, buf, sizeof buf);
	if (buf[0] != '1') return;
	while (sock_getln(sock, buf, sizeof buf), strcmp(buf, "000")) {
		msgs[num_msgs++] = atol(buf);
	}
	printf("%d messages\n", num_msgs);

	if (num_msgs == 0) return;
	for (i=0; i<num_msgs; ++i) {
		snprintf(buf, sizeof buf, "sa-learn %s", salearnargs);
		fp = popen(buf, "w");
		if (fp == NULL) return;
		printf("Submitting message %ld\n", msgs[i]);
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
			printf("%s\n", buf);
		}
	}
}


int main(int argc, char **argv)
{
	int server_socket = 0;
	char buf[1024];
	int ipgm_secret = (-1);
	int c;
	char ctdldir[256];

	printf("\nAuto-submit spam and ham to sa-learn for Citadel " PACKAGE_VERSION "\n");
	printf("(c) 2009 citadel.org GPLv3\n");

	strcpy(ctdldir, "/usr/local/citadel");
	ipgm_secret = discover_ipgm_secret(ctdldir);
	if (ipgm_secret < 0) {
		strcpy(ctdldir, "/appl/citadel");
		ipgm_secret = discover_ipgm_secret(ctdldir);
	}
	if (ipgm_secret < 0) {
		strcpy(ctdldir, "/root/ctdl/trunk/citadel");
		ipgm_secret = discover_ipgm_secret(ctdldir);
	}
	if (ipgm_secret < 0) {
		exit(1);
	}

	printf("Attaching to server...\n");
	fflush(stdout);
	sprintf(buf, "%s/citadel.socket", ctdldir);
	server_socket = uds_connectsock(buf);
	if (server_socket < 0) {
		exit(1);
	}

	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);

	sock_printf(server_socket, "IPGM %d\n", ipgm_secret);
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);

	if (buf[0] == '2') {
		do_room(server_socket, "0000000001.spam", "--spam");
		do_room(server_socket, "0000000001.ham", "--ham");
	}

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);
	close(server_socket);
	exit(0);
}
