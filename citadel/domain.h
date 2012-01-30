/*
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

struct mx {
	int pref;
	char host[1024];
};

int getmx(char *mxbuf, char *dest);
int get_hosts(char *mxbuf, char *rectype);


/* HP/UX has old include files...these are from arpa/nameser.h */

#include "typesize.h"

#ifndef HFIXEDSZ
#define HFIXEDSZ	12		/* I hope! */
#endif
#ifndef INT16SZ
#define	INT16SZ		sizeof(cit_int16_t)
#endif
#ifndef INT32SZ
#define INT32SZ		sizeof(cit_int32_t)
#endif
