/*
 * $Id$
 *
 * Handles GroupDAV GET requests.
 *
 */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <limits.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <time.h>
#include <pthread.h>
#include "webcit.h"
#include "webserver.h"
#include "groupdav.h"


/*
 * The pathname is always going to be /groupdav/room_name/msg_num
 */
void groupdav_get(char *dav_pathname) {
	char dav_roomname[SIZ];
	char dav_msgnum[SIZ];
	char buf[SIZ];
	int found_content_type = 0;

	extract_token(dav_roomname, dav_pathname, 2, '/');
	extract_token(dav_msgnum, dav_pathname, 3, '/');

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		wprintf(
			"HTTP/1.1 404 not found\n"
			"Connection: close\n"
			"Content-Type: text/plain\n"
			"\n"
			"There is no folder called \"%s\" on this server.\n",
			dav_roomname
		);
		return;
	}

	serv_printf("MSG2 %s", dav_msgnum);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf(
			"HTTP/1.1 404 not found\n"
			"Connection: close\n"
			"Content-Type: text/plain\n"
			"\n"
			"Object \"%s\" was not found in the \"%s\" folder.\n",
			dav_msgnum,
			dav_roomname
		);
		return;
	}

	wprintf("HTTP/1.1 200 OK\n");
	wprintf("ETag: %s\n", dav_msgnum);
	while (serv_gets(buf), strcmp(buf, "000")) {
		if (!strncasecmp(buf, "Content-type: ", 14)) {
			found_content_type = 1;
		}
		if ((strlen(buf) == 0) && (found_content_type == 0)) {
			wprintf("Content-type: text/plain\n");
		}
		wprintf("%s\n", buf);
	}
}
