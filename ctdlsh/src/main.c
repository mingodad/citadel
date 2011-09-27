/*
 * (c) 2009-2011 by Art Cancro and citadel.org
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
	{	NULL,		NULL,		NULL					}
};


int cmd_help(int sock, char *cmdbuf) {
	int i;

	for (i=0; commands[i].func != NULL; ++i) {
		printf("%-10s %s\n", commands[i].name, commands[i].doc);
	}
}






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

int main(int argc, char **argv)
{
	int server_socket = 0;
	char buf[1024];
	int ipgm_secret = (-1);
	int c;
	char *ctdldir = CTDLDIR;

	printf("\nCitadel administration shell v" PACKAGE_VERSION "\n");
	printf("(c) 2009-2011 citadel.org GPLv3\n");

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

	ipgm_secret = discover_ipgm_secret(ctdldir);
	if (ipgm_secret < 0) {
		exit(1);
	}

	printf("Trying %s...\n", ctdldir);
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
		do_main_loop(server_socket);
	}

	sock_puts(server_socket, "QUIT");
	sock_getln(server_socket, buf, sizeof buf);
	printf("%s\n", buf);
	close(server_socket);
	exit(0);
}
