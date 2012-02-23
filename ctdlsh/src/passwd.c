/*
 * (c) 2009-2011 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include "ctdlsh.h"

int cmd_passwd(int server_socket, char *cmdbuf) {
	char buf[1024];
	time_t now;
	char account_name[1024];
	char *p1;
	char *p2;

	strcpy(account_name, &cmdbuf[7]);
	if (strlen(account_name) == 0) {
		strncpy(account_name, readline("Enter account name: "), sizeof account_name);
	}
	sock_printf(server_socket, "AGUP %s\n", account_name);
	sock_getln(server_socket, buf, sizeof buf);
	if (buf[0] != '2') {
		fprintf(stderr, "No such user.\n");
		return(cmdret_error);
	}

	p1 = readline("Enter new password: ");
	p2 = readline("Enter it again: ");

	if (strcmp(p1, p2)) {
		fprintf(stderr, "Passwords do not match.  Account password is unchanged.\n");
		return(cmdret_error);
	}

	sock_printf(server_socket, "ASUP %s|%s\n", account_name, p2);
	sock_getln(server_socket, buf, sizeof buf);
	if (buf[0] != '2') {
		fprintf(stderr, "%s\n", &buf[4]);
		return(cmdret_error);
	}

	printf("Password has been changed.\n");
	return(cmdret_ok);
}




