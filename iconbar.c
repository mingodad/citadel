/* $Id$ */

#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <time.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"


void do_iconbar(void) {
	do_template("iconbar");
}



void customize_iconbar(void) {
	char iconbar[SIZ];

	output_headers(3);
	svprintf("BOXTITLE", WCS_STRING, "Customize the icon bar");
	do_template("beginbox");

	wprintf("<CENTER>Select the icons you would like to see displayed "
		"in the &quot;icon bar&quot; menu on the left side of the "
		"screen.</CENTER><HR>\n"
	);

	wprintf("(FIXME this is not done yet)");

	do_template("endbox");
	wDumpContent(2);
}
