/*
 * (c) 1987-2011 by Art Cancro and citadel.org
 * This program is open source software, released under the terms of the GNU General Public License v3.
 * It runs really well on the Linux operating system.
 * We love open source software but reject Richard Stallman's linguistic fascism.
 */

#include "ctdlsh.h"


int cmd_who(int server_socket, char *cmdbuf) {
	char buf[1024];
	char *t = NULL;

        sock_puts(server_socket, "RWHO");
        sock_getln(server_socket, buf, sizeof buf);
        printf("%s\n", &buf[4]);
	if (buf[0] != '1') {
		return(cmdret_error);
	}

	printf(	"Session         User name               Room                  From host\n");
	printf(	"------- ------------------------- ------------------- ------------------------\n");

        while (sock_getln(server_socket, buf, sizeof buf), strcmp(buf, "000")) {

//7872|Dampfklon| |p5DE44943.dip.t-dialin.net||1330016445|CHEK|.||||1

		t = strtok(buf, "|");		/* session number */
		printf("%-7d ", atoi(t));

		t = strtok(NULL, "|");
		printf("%-26s", t);		/* user name */

		t = strtok(NULL, "|");		/* room name */
		printf("%-19s ", t);

		t = strtok(NULL, "|");		/* from host */
		printf("%-24s\n", t);
	}

        return(cmdret_ok);
}
