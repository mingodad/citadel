/* $Id$
 *
 * This program simply feeds its standard input to the networker.  It is
 * used primarily to hook up to UUCP feeds of Citadel data.
 *
 * 
 * usage:
 *	rcit [-z] [-s]
 * flags:
 *	-z	Input is compressed, run uncompress on it before processing
 *	-s	Don't run netproc now, just accept the input into spoolin
 */

#define UNCOMPRESS "/usr/bin/gunzip"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include "citadel.h"

void get_config(void);
struct config config;

int main(int argc, char **argv)
{
	int a;
	char flnm[128],tname[128];
	FILE *minput, *mout;
	char compressed_input = 0;
	char spool_only = 0;

	get_config();
	sprintf(flnm,"./network/spoolin/rcit.%ld", (long)getpid());
	strcpy(tname, tmpnam(NULL));

	for (a=1; a<argc; ++a) {
		if (!strcmp(argv[a],"-z")) compressed_input = 1;
		if (!strcmp(argv[a],"-s")) spool_only = 1;
	}

	minput=stdin;
	if (compressed_input) minput=popen(UNCOMPRESS,"r");
	if (minput==NULL) fprintf(stderr,"rnews: can't open input!!!!\n");

	mout=fopen(flnm,"w");
	while ((a=getc(minput))>=0) putc(a,mout);
	putc(0,mout);
	fclose(mout);

	unlink(tname);
	if (compressed_input) pclose(minput);
	if (!spool_only) execlp("./netproc", "netproc", "-i", NULL);
	exit(0);
}


