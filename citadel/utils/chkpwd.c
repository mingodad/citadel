/*
 * a setuid helper program for machines which use shadow passwords
 * by Nathan Bryant, March 1999
 *
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
	size_t siz;
	char buf[SIZ];

	while (1) {
		buf[0] = '\0';
		siz = read(0, &uid, sizeof(uid_t));	/* uid */
		siz = read(0, buf, 256);	/* password */

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
