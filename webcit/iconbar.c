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



void display_customize_iconbar(void) {
	char iconbar[SIZ];
	char buf[SIZ];
	char key[SIZ], value[SIZ];
	int i;

	/* The initialized values of these variables also happen to
	 * specify the default values for users who haven't customized
	 * their iconbars.  These should probably be set in a master
	 * configuration somewhere.
	 */
	int ib_logo = 1;	/* Site logo */
	int ib_rooms = 1;	/* Rooms icon */
	int ib_users = 1;	/* Users icon */
	int ib_advanced = 1;	/* Advanced Options icon */
	int ib_logoff = 1;	/* Logoff button */
	int ib_citadel = 1;	/* 'Powered by Citadel' logo */
	/*
	 */

	output_headers(3);
	svprintf("BOXTITLE", WCS_STRING, "Customize the icon bar");
	do_template("beginbox");

	wprintf("<CENTER>Select the icons you would like to see displayed "
		"in the &quot;icon bar&quot; menu on the left side of the "
		"screen.</CENTER><HR>\n"
	);

	get_preference("iconbar", iconbar);
	for (i=0; i<num_tokens(iconbar, '|'); ++i) {
		extract_token(buf, iconbar, i, '|');
		extract_token(key, buf, 0, '=');
		extract_token(value, buf, 1, '=');

		if (!strcasecmp(key, "ib_logo")) ib_logo = atoi(value);
		if (!strcasecmp(key, "ib_rooms")) ib_rooms = atoi(value);
		if (!strcasecmp(key, "ib_users")) ib_users = atoi(value);
		if (!strcasecmp(key, "ib_advanced")) ib_advanced = atoi(value);
		if (!strcasecmp(key, "ib_logoff")) ib_logoff = atoi(value);
		if (!strcasecmp(key, "ib_citadel")) ib_citadel = atoi(value);

	}

	wprintf("(FIXME this is not done yet)");

	do_template("endbox");
	wDumpContent(2);
}
