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
#include "webcit.h"
#include "webserver.h"

void groupdav_main(char *cmd) {

	if (!WC->logged_in) {
		wprintf(
			"HTTP/1.1 401 Authorization Required\n"
			"WWW-Authenticate: Basic realm=\"GroupDAV\"\n"
			"Connection: close\n"
		);
		output_headers(0, 0, 0, 0, 0, 0, 0);
		wprintf("Content-Type: text/plain\n");
		wprintf("\n");
		wprintf("GroupDAV sessions require HTTP authentication.\n");
		wDumpContent(0);
	}

	output_static("smiley.gif");	/* FIXME */
}
