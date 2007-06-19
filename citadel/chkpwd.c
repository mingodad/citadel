/*
 * $Id$
 *
 * a setuid helper program for machines which use shadow passwords
 * by Nathan Bryant, March 1999
 *
 */

#include <pwd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include "auth.h"
#include "config.h"
#include "citadel_dirs.h"
#include "citadel.h"

int main(void)
{
	uid_t uid;
	char buf[SIZ];

	while (1) {
		read(0, &uid, sizeof(uid_t));	/* uid */
		read(0, buf, 256);	/* password */

		if (validate_password(uid, buf)) {
			write(1, "PASS", 4);
		}
		else {
			write(1, "FAIL", 4);
		}
	}

	return(0);
}
