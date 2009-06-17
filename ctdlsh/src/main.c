/*
 * (c) 2009 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include <config.h>
#include <stdio.h>
#include <readline/readline.h>

int main(int argc, char **argv)
{
	char *cmd = NULL;
	char *prompt = "> ";

	while (cmd = readline(prompt)) {

		if ((cmd) && (*cmd)) {
			add_history(cmd);
		}

		printf("\nHaha, you said: '%s'\n\n", cmd);
		free(cmd);
	}

	exit(0);
}
