/*
 * $Id$
 *
 * This file contains functions which handle the mapping of Internet addresses
 * to users on the Citadel system.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "internet_addressing.h"

/*
 * Return 0 if a given string fuzzy-matches a Citadel user account
 *
 * FIX ... this needs to be updated to match any and all ways of addressing
 *         a user.  It may even be appropriate to move this out of SMTP and
 *         into the server core.
 */
int fuzzy_match(struct usersupp *us, char *matchstring) {
	int a;

	for (a=0; a<strlen(us->fullname); ++a) {
		if (!strncasecmp(&us->fullname[a],
		   matchstring, strlen(matchstring))) {
			return 0;
		}
	}
	return -1;
}


