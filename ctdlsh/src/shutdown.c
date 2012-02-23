/*
 * (c) 2011 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include "ctdlsh.h"

int cmd_shutdown(int server_socket, char *cmdbuf) {
	char buf[1024];

	char *p1 = readline("Do you really want to shut down the Citadel server? ");

	if (strncasecmp(p1, "y", 1)) {
		return(cmdret_ok);
	}

	sock_puts("DOWN");
	sock_getln(server_socket, buf, sizeof buf);
	if (buf[0] != '2') {
		fprintf(stderr, "%s\n", &buf[4]);
		return(cmdret_error);
	}

	fprintf(stderr, "%s\n", &buf[4]);
	return(cmdret_ok);
}
