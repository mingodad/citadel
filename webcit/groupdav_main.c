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

void groupdav_main(struct httprequest *req) {

	struct httprequest *rptr;
	char dav_method[SIZ];
	char dav_pathname[SIZ];

	if (!WC->logged_in) {
		wprintf(
			"HTTP/1.1 401 Authorization Required\n"
			"WWW-Authenticate: Basic realm=\"%s\"\n"
			"Connection: close\n",
			serv_info.serv_humannode
		);
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("GroupDAV sessions require HTTP authentication.\n");
		return;
	}

	extract_token(dav_method, req->line, 0, ' ');
	extract_token(dav_pathname, req->line, 1, ' ');

	/*
	 * We like the GET method ... it's nice and simple.
	 */
	if (!strcasecmp(dav_method, "GET")) {
		groupdav_get(dav_pathname);
		return;
	}

	/*
	 * Couldn't find what we were looking for.  Die in a car fire.
	 */
	wprintf(
		"HTTP/1.1 404 not found\n"
		"Connection: close\n"
		"Content-Type: text/plain\n"
		"\n"
	);
	wprintf("The object or resource \"%s\" was not found.\n", dav_pathname);

	/*
	 * FIXME ... after development is finished, get rid of all this
	 */
	wprintf("\n\n\n ** DEBUGGING INFO FOLLOWS ** \n\n");
	wprintf("WC->httpauth_user = %s\n", WC->httpauth_user);
	wprintf("WC->httpauth_pass = (%d characters)\n", strlen(WC->httpauth_pass));
	wprintf("WC->wc_session    = %d\n", WC->wc_session);
	
	for (rptr=req; rptr!=NULL; rptr=rptr->next) {
		wprintf("> %s\n", rptr->line);
	}
}
