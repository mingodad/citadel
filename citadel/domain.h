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
