/*
 * (c) 2009 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include <config.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <readline/readline.h>

int main(int argc, char **argv)
{
	char *cmd = NULL;
	char *prompt = "> ";
	int server_socket = 0;
	char buf[1024];

	printf("\nCitadel administration shell v" PACKAGE_VERSION "\n");
	printf("(c) 2009 citadel.org GPLv3\n");

	printf("Attaching to server...\r");
	fflush(stdout);
	server_socket = uds_connectsock("/root/ctdl/trunk/citadel/citadel.socket");
	if (server_socket < 0) {
		exit(1);
	}
	printf("                      \r");

	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);

	while (cmd = readline(prompt)) {

		if ((cmd) && (*cmd)) {
			add_history(cmd);
		}

		printf("\nHaha, you said: '%s'\n\n", cmd);
		free(cmd);
	}
	printf("\r");

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);
	close(server_socket);
	exit(0);
}
