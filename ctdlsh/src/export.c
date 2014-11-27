/*
 * (c) 2009-2014 by Art Cancro and citadel.org
 * This program is released under the terms of the GNU General Public License v3.
 */

#include "ctdlsh.h"

int cmd_export(int server_socket, char *cmdbuf) {
	char buf[1024];
	char export_file_name[1024];

	strcpy(export_file_name, &cmdbuf[7]);
	if (strlen(export_file_name) == 0) {
		strncpy(export_file_name, readline("Enter export file name: "), sizeof export_file_name);
	}

	sock_printf(server_socket, "MIGR export\n");
	sock_getln(server_socket, buf, sizeof buf);

	if (buf[0] != '1') {
		fprintf(stderr, "%s\n", &buf[4]);
		return(cmdret_error);
	}

	while (sock_getln(server_socket, buf, sizeof buf), strcmp(buf, "000")) {

		if (!strncmp(buf, "<progress>", 10)) {
			fprintf(stderr, "%s\n", buf);
		}
	}



	return(cmdret_ok);
}




