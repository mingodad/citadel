/*
 * $Id$
 *
 * Handles GroupDAV PROPFIND requests.
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
void groupdav_propfind(char *dav_pathname) {
	char dav_roomname[SIZ];
	char buf[SIZ];

	/* First, break off the "/groupdav/" prefix */
	remove_token(dav_pathname, 0, '/');
	remove_token(dav_pathname, 0, '/');

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

	/*
	 * Be rude.  Completely ignore the XML request and simply send them
	 * everything we know about (which is going to simply be the ETag and
	 * nothing else).  Let the client-side parser sort it out.
	 */
	wprintf("HTTP/1.0 207 Multi-Status\n");
	groupdav_common_headers();
	wprintf("Content-type: text/xml\n"
		"\n"
		"<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
     		"<D:multistatus xmlns:D=\"DAV:\">\n"
	);

	serv_puts("MSGS ALL");
	serv_gets(buf);
	if (buf[0] == '1') while (serv_gets(buf), strcmp(buf, "000")) {
		wprintf(" <D:response>\n");
		wprintf("  <D:href>%s://%s/groupdav/Calendar/%s</D:href>\n",
			(is_https ? "https" : "http"),
			WC->http_host,
			buf
		);
		wprintf("   <D:propstat>\n");
		wprintf("    <D:status>HTTP/1.1 200 OK</D:status>\n");
		wprintf("    <D:prop><D:getetag>\"%s\"</D:getetag></D:prop>\n", buf);
		wprintf("   </D:propstat>\n");
		wprintf(" </D:response>\n");
	}

	wprintf("</D:multistatus>\n");
}
