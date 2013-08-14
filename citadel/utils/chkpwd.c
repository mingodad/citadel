/*
 * a setuid helper program for machines which use shadow passwords
 * by Nathan Bryant, March 1999
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <pwd.h>
#include <stdio.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <sys/types.h>

#include <libcitadel.h>

#include "auth.h"
#include "config.h"
#include "citadel_dirs.h"
#include "citadel.h"

int main(void)
{
	uid_t uid;
	char buf[SIZ];

	while (1) {
		buf[0] = '\0';
		read(0, &uid, sizeof(uid_t));	/* uid */
		read(0, buf, 256);	/* password */

		if (buf[0] == '\0') 
			return (0);
		if (validate_password(uid, buf)) {
			write(1, "PASS", 4);
		}
		else {
			write(1, "FAIL", 4);
		}
	}

	return(0);
}
