/* aidepost.c
 * This is just a little hack to copy standard input to a message in Aide>
 * v2.0
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include "citadel.h"
#include "config.h"

void make_message(FILE *fp, char *target_room)
{
	int a;
	long bb, cc;
	time_t now;
	time(&now);
	putc(255, fp);
	putc(MES_NORMAL, fp);
	putc(1, fp);
	fprintf(fp, "Proom_aide");
	putc(0, fp);
	fprintf(fp, "T%ld", (long)now);
	putc(0, fp);
	fprintf(fp, "ACitadel");
	putc(0, fp);
	fprintf(fp, "O%s", target_room);
	putc(0, fp);
	fprintf(fp, "N%s", NODENAME);
	putc(0, fp);
	putc('M', fp);
	bb = ftell(fp);
	while (a = getc(stdin), a > 0) {
		if (a != 8)
			putc(a, fp);
		else {
			cc = ftell(fp);
			if (cc != bb)
				fseek(fp, (-1L), 1);
		}
	}
	putc(0, fp);
	putc(0, fp);
	putc(0, fp);
}

int main(int argc, char **argv)
{
	char tempspool[64];
	char target_room[ROOMNAMELEN];
	FILE *tempfp, *spoolfp;
	int ch;
	int i;

	get_config();

	strcpy(target_room, "Aide");
	for (i=1; i<argc; ++i) {
		if (!strncasecmp(argv[i], "-r", 2)) {
			strncpy(target_room, &argv[i][2], sizeof(target_room));
			target_room[sizeof(target_room)-1] = 0;
		} else {
			fprintf(stderr, "%s: usage: %s [-rTargetRoom]\n",
				argv[0], argv[0]);
			exit(1);
		}
	}


	snprintf(tempspool, sizeof tempspool, "./network/spoolin/ap.%d",
		getpid());

	tempfp = tmpfile();
	if (tempfp == NULL) {
		perror("cannot open temp file");
		exit(errno);
	}

	/* Generate a message from stdin */
	make_message(tempfp, target_room);

	/* Copy it to a new temp file in the spool directory */
	rewind(tempfp);

	spoolfp = fopen(tempspool, "wb");
	if (spoolfp == NULL) {
		perror("cannot open spool file");
		exit(errno);
	}
	while (ch = getc(tempfp), (ch >= 0))
		putc(ch, spoolfp);

	fclose(tempfp);
	fclose(spoolfp);

	execlp("./netproc", "netproc", "-i", NULL);
	perror("cannot run netproc");
	exit(errno);
}
