/*
 * $Id$
 *
 */

struct mx {
	int pref;
	char host[1024];
};

int get_smarthosts(char *mxbuf);
int getmx(char *mxbuf, char *dest);


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
