/*
 * (c) 2009-2012 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include "ctdlsh.h"


int cmd_quit(int sock, char *cmdbuf) {
	return(cmdret_exit);
}


/*
 * Commands understood by ctdlsh
 */
typedef struct {
	char *name;
	ctdlsh_cmdfunc_t *func;
	char *doc;
} COMMAND;

COMMAND commands[] = {
	{	"?",		cmd_help,	"Display this message"			},
	{	"help",		cmd_help,	"Display this message"			},
	{	"quit",		cmd_quit,	"Quit using ctdlsh"			},
	{	"exit",		cmd_quit,	"Quit using ctdlsh"			},
	{	"date",		cmd_datetime,	"Print the server's date and time"	},
	{	"time",		cmd_datetime,	"Print the server's date and time"	},
	{	"passwd",	cmd_passwd,	"Set or change an account password"	},
	{	"who",		cmd_who,	"Display a list of online users"	},
	{	"shutdown",	cmd_shutdown,	"Shut down the Citadel server"		},
	{	NULL,		NULL,		NULL					}
};


int cmd_help(int sock, char *cmdbuf) {
	int i;

	for (i=0; commands[i].func != NULL; ++i) {
		printf("%-10s %s\n", commands[i].name, commands[i].doc);
	}
}


/* Auto-completer function */
char *command_generator(const char *text, int state) {
	static int list_index;
	static int len;
	char *name;

	if (!state) {
		list_index = 0;
		len = strlen(text);
	}

	while (name = commands[list_index].name) {
		++list_index;

		if (!strncmp(name, text, len)) {
			return(strdup(name));
		}
	}

	return(NULL);
}


/* Auto-completer function */
char **ctdlsh_completion(const char *text, int start, int end) {
	char **matches = (char **) NULL;

	if (start == 0) {
		matches = rl_completion_matches(text, command_generator);
	}
	else {
		rl_bind_key('\t', rl_abort);
	}

	return (matches);
}


void do_main_loop(int server_socket) {
	char *cmd = NULL;
	char prompt[1024];
	char buf[1024];
	char server_reply[1024];
	int i;
	int ret = (-1);

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

	/* Tell libreadline how we will help with auto-completion of commands */
	rl_attempted_completion_function = ctdlsh_completion;

	/* Here we go ... main command loop */
	while ((ret != cmdret_exit) && (cmd = readline(prompt))) {

		if ((cmd) && (*cmd)) {
			add_history(cmd);

			for (i=0; commands[i].func != NULL; ++i) {
				if (!strncasecmp(cmd, commands[i].name, strlen(commands[i].name))) {
					ret = (*commands[i].func) (server_socket, cmd);
				}
			}

		}

		free(cmd);
	}
}


/*
 * If you don't know what main() does by now you probably shouldn't be reading this code.
 */
int main(int argc, char **argv)
{
	int server_socket = 0;
	char buf[1024];
	int c;
	char *ctdldir = CTDLDIR;

	printf("\nCitadel administration shell v" PACKAGE_VERSION " (c) 2009-2012 by citadel.org\n"
		"This is open source software made available to you under the terms\n"
		"of the GNU General Public License v3.  All other rights reserved.\n"
	);

	opterr = 0;
	while ((c = getopt (argc, argv, "h:")) != -1) {
		switch(c) {
		case 'h':
			ctdldir = optarg;
			break;
		case '?':
			if (optopt == 'h') {
				fprintf(stderr, "Option -%c requires an argument\n", optopt);
			}
			else {
				fprintf(stderr, "Unknown option '-%c'\n", optopt);
				fprintf(stderr, "usage: %s [-h citadel_dir]\n", argv[0]);
			}
			exit(1);
		}
	}

	printf("Trying %s...\n", ctdldir);
	sprintf(buf, "%s/citadel-admin.socket", ctdldir);
	server_socket = uds_connectsock(buf);
	if (server_socket < 0) {
		exit(1);
	}

	sock_getln(server_socket, buf, sizeof buf);
	if (buf[0] == '2') {
		do_main_loop(server_socket);
	}

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);
	close(server_socket);
	exit(0);
}
