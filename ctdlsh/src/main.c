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
#include <readline/readline.h>
#include "ctdlsh.h"

#define CTDLDIR	"/root/ctdl/trunk/citadel"


int discover_ipgm_secret(void) {
	int fd;
	struct partial_config ccc;

	fd = open(CTDLDIR "/citadel.config", O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "citadel.config: %s\n", strerror(errno));
		return(-1);
	}

	if (read(fd, &ccc, sizeof(struct partial_config)) != sizeof(struct partial_config)) {
		fprintf(stderr, "citadel.config: %s\n", strerror(errno));
		return(-1);
	}
	if (close(fd) != 0) {
		fprintf(stderr, "citadel.config: %s\n", strerror(errno));
		return(-1);
	}
	return(ccc.c_ipgm_secret);
}


void do_main_loop(int server_socket) {
	char *cmd = NULL;
	char prompt[1024];
	char buf[1024];
	char server_reply[1024];
	int i;

	strcpy(prompt, "> ");

	/* Do an INFO command and learn the hostname for the prompt */
	sock_puts(server_socket, "INFO");
	sock_getln(server_socket, buf, sizeof buf);
	if (buf[0] == '1') {
		i = 0;
		while(sock_getln(server_socket, buf, sizeof buf), strcmp(buf, "000")) {
			if (i == 1) {
				sprintf(prompt, "\n%s> ", buf);
			}
			++i;
		}
	}

	/* Here we go ... main command loop */
	while (cmd = readline(prompt)) {

		if ((cmd) && (*cmd)) {
			add_history(cmd);

			sock_puts(server_socket, cmd);
			sock_getln(server_socket, server_reply, sizeof server_reply);
			printf("%s\n", server_reply);

			if ((server_reply[0] == '4') || (server_reply[0] == '8')) {
				// FIXME
			}

			if ((server_reply[0] == '1') || (server_reply[0] == '8')) {
				while(sock_getln(server_socket, buf, sizeof buf), strcmp(buf, "000")) {
					printf("%s\n", buf);
				}
			}

		}

		free(cmd);
	}
}

int main(int argc, char **argv)
{
	int server_socket = 0;
	char buf[1024];
	int ipgm_secret = (-1);

	printf("\nCitadel administration shell v" PACKAGE_VERSION "\n");
	printf("(c) 2009 citadel.org GPLv3\n");

	ipgm_secret = discover_ipgm_secret();
	if (ipgm_secret < 0) {
		exit(1);
	}

	printf("Attaching to server...\r");
	fflush(stdout);
	server_socket = uds_connectsock(CTDLDIR "/citadel.socket");
	if (server_socket < 0) {
		exit(1);
	}
	printf("                      \r");

	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);

	sock_printf(server_socket, "IPGM %d\n", ipgm_secret);
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);

	if (buf[0] == '2') {
		do_main_loop(server_socket);
	}

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);
	close(server_socket);
	exit(0);
}
