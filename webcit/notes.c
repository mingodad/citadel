/*
 * $Id$
 *
 * Functions which handle "sticky notes"
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

void display_note(long msgnum) {
	wprintf("<TABLE border=2><TR><TD>\n");
	wprintf("FIXME note #%ld\n", msgnum);
	wprintf("</TD></TR></TABLE\n");
}
