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

	printf("Attaching to server...\r");
	fflush(stdout);
	server_socket = sock_connect("localhost", "504", "tcp");
	if (server_socket < 0) {
		exit(1);
	}
	printf("                      \r");

	printf("\nCitadel administration shell v" PACKAGE_VERSION "\n");
	printf("(c) 2009 citadel.org GPLv3\n");
	printf("Type a command.  Or don't.  We don't care.\n\n");

	while (cmd = readline(prompt)) {

		if ((cmd) && (*cmd)) {
			add_history(cmd);
		}

		printf("\nHaha, you said: '%s'\n\n", cmd);
		free(cmd);
	}

	exit(0);
}
