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

void groupdav_main(struct httprequest *req) {

	struct httprequest *rptr;

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

	wprintf(
		"HTTP/1.1 404 Not found - FIXME\n"
		"Connection: close\n"
		"Content-Type: text/plain\n"
		"\n"
	);
	wprintf("You are authenticated, but sent a bogus request.\n");
	wprintf("WC->httpauth_user=%s\n", WC->httpauth_user);
	wprintf("WC->httpauth_pass=%s\n", WC->httpauth_pass);	/* FIXME don't display this */
	wprintf("WC->wc_session   =%d\n", WC->wc_session);
	
	for (rptr=req; rptr!=NULL; rptr=rptr->next) {
		wprintf("> %s\n", rptr->line);
	}
}
