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

#ifndef HFIXEDSZ
#define HFIXEDSZ	12		/* I hope! */
#endif
#ifndef INT16SZ
#define	INT16SZ		sizeof(int16)
#endif
#ifndef INT32SZ
#define INT32SZ		sizeof(int32)
#endif
