/*
 * $Id$
 *
 * Entry point for GroupDAV functions
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
 * Output HTTP headers which are common to all requests.
 */
void groupdav_common_headers(void) {
	wprintf(
		"Server: %s / %s\n"
		"Connection: close\n",
		SERVER, serv_info.serv_software
	);
}


/*
 * Main entry point for GroupDAV requests
 */
void groupdav_main(struct httprequest *req) {

	struct httprequest *rptr;
	char dav_method[SIZ];
	char dav_pathname[SIZ];
	char dav_ifmatch[SIZ];

	strcpy(dav_method, "");
	strcpy(dav_pathname, "");
	strcpy(dav_ifmatch, "");

	for (rptr=req; rptr!=NULL; rptr=rptr->next) {
		lprintf(9, "> %s\n", rptr->line);	/* FIXME we can eventually remove this trace */
		if (!strncasecmp(rptr->line, "Host: ", 6)) {
                        safestrncpy(WC->http_host, &rptr->line[6], sizeof WC->http_host);
                }
		if (!strncasecmp(rptr->line, "If-Match: ", 10)) {
                        safestrncpy(dav_ifmatch, &rptr->line[10], sizeof dav_ifmatch);
                }

	}

	if (!WC->logged_in) {
		wprintf("HTTP/1.1 401 Unauthorized\n");
		groupdav_common_headers();
		wprintf("WWW-Authenticate: Basic realm=\"%s\"\n", serv_info.serv_humannode);
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("GroupDAV sessions require HTTP authentication.\n");
		return;
	}

	extract_token(dav_method, req->line, 0, ' ');
	extract_token(dav_pathname, req->line, 1, ' ');
	unescape_input(dav_pathname);

	/*
	 * If there's an If-Match: header, strip out the quotes if present, and
	 * then if all that's left is an asterisk, make it go away entirely.
	 */
	if (strlen(dav_ifmatch) > 0) {
		if (dav_ifmatch[0] == '\"') {
			strcpy(dav_ifmatch, &dav_ifmatch[1]);
			if (strtok(dav_ifmatch, "\"") != NULL) {
				strcpy(strtok(dav_ifmatch, "\""), "");
			}
		}
		if (!strcmp(dav_ifmatch, "*")) {
			strcpy(dav_ifmatch, "");
		}
	}

	/*
	 * The PROPFIND method is basically used to list all objects in a room.
	 */
	if (!strcasecmp(dav_method, "PROPFIND")) {
		groupdav_propfind(dav_pathname);
		return;
	}

	/*
	 * We like the GET method ... it's nice and simple.
	 */
	if (!strcasecmp(dav_method, "GET")) {
		groupdav_get(dav_pathname);
		return;
	}

	/*
	 * The DELETE method kills, maims, and destroys.
	 */
	if (!strcasecmp(dav_method, "DELETE")) {
		groupdav_delete(dav_pathname, dav_ifmatch);
		return;
	}

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
	wprintf("HTTP/1.1 501 Method not implemented\n");
	groupdav_common_headers();
	wprintf("Content-Type: text/plain\n"
		"\n"
		"GroupDAV method \"%s\" is not implemented.\n",
		dav_method
	);
}
