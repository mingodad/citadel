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
	char dav_uid[SIZ];
	long dav_msgnum = (-1);
	char buf[SIZ];
	int found_content_type = 0;
	int n = 0;

	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

	/* Now extract the message euid */
	n = num_tokens(dav_pathname, '/');
	extract_token(dav_uid, dav_pathname, n-1, '/');
	remove_token(dav_pathname, n-1, '/');

	/* What's left is the room name.  Remove trailing slashes. */
	if (dav_pathname[strlen(dav_pathname)-1] == '/') {
		dav_pathname[strlen(dav_pathname)-1] = 0;
	}
	strcpy(dav_roomname, dav_pathname);

	/* Go to the correct room. */
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		gotoroom(dav_roomname);
	}
	if (strcasecmp(WC->wc_roomname, dav_roomname)) {
		wprintf("HTTP/1.1 404 not found\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\n"
			"\n"
			"There is no folder called \"%s\" on this server.\n",
			dav_roomname
		);
		return;
	}

	dav_msgnum = locate_message_by_uid(dav_uid);
	serv_printf("MSG2 %ld", dav_msgnum);
	serv_gets(buf);
	if (buf[0] != '1') {
		wprintf("HTTP/1.1 404 not found\n");
		groupdav_common_headers();
		wprintf(
			"Content-Type: text/plain\n"
			"\n"
			"Object \"%s\" was not found in the \"%s\" folder.\n",
			dav_uid,
			dav_roomname
		);
		return;
	}

	wprintf("HTTP/1.1 200 OK\n");
	groupdav_common_headers();
	wprintf("ETag: \"%ld\"\n", dav_msgnum);
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
