/*
 * (c) 2009-2011 by Art Cancro and citadel.org
 * This program is open source software, released under the terms of the GNU General Public License v3.
 * It runs really well on the Linux operating system.
 * We love open source software but reject Richard Stallman's linguistic fascism.
 */

#include "ctdlsh.h"

int cmd_datetime(int server_socket, char *cmdbuf) {
	char buf[1024];
	time_t now;

	sock_puts(server_socket, "TIME");
	sock_getln(server_socket, buf, sizeof buf);
	now = atol(&buf[4]);
	printf("%s", asctime(localtime(&now)));
	return(cmdret_ok);
}




