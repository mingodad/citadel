/*
 * This is just a little hack to copy standard input to a message in Aide>
 *
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include "citadel.h"
#include "citadel_dirs.h"

/*
 * Simplified function to generate a message in our format
 */
static void ap_make_message(FILE *fp, char *target_room, char *author, char *subject)
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
	fprintf(fp, "A%s", author);
	putc(0, fp);
	fprintf(fp, "O%s", target_room);
	putc(0, fp);
	if (strlen(subject) > 0) {
		fprintf(fp, "U%s%c", subject, 0);
	}
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
	char author[64];
	char subject[256];
	FILE *tempfp, *spoolfp;
	int ch;
	int i;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;

	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);

	strcpy(target_room, "Aide");
	strcpy(author, "Citadel");
	strcpy(subject, "");
	for (i=1; i<argc; ++i) {
		if (!strncasecmp(argv[i], "-r", 2)) {
			strncpy(target_room, &argv[i][2], sizeof(target_room));
			target_room[sizeof(target_room)-1] = 0;
		}
		else if (!strncasecmp(argv[i], "-a", 2)) {
			strncpy(author, &argv[i][2], sizeof(author));
			author[sizeof(author)-1] = 0;
		}
		else if (!strncasecmp(argv[i], "-s", 2)) {
			strncpy(subject, &argv[i][2], sizeof(subject));
			subject[sizeof(subject)-1] = 0;
		} else {
			fprintf(stderr, "%s: usage: %s "
					"[-rTargetRoom] [-aAuthor] [-sSubject]\n",
				argv[0], argv[0]);
			exit(1);
		}
	}

	snprintf(tempspool, sizeof tempspool, "%s/ap.%04x.%08lx", ctdl_netin_dir, getpid(), time(NULL));
	unlink(tempspool);	/* unlinking it for now, keeps it from being posted prematurely */

	tempfp = fopen(tempspool, "w+b");
	unlink(tempspool);
	if (tempfp == NULL) {
		perror("cannot open temp file");
		exit(errno);
	}

	/* Generate a message from stdin */
	ap_make_message(tempfp, target_room, author, subject);

	/* Copy it to a new temp file in the spool directory */
	rewind(tempfp);

	spoolfp = fopen(tempspool, "wb");
	if (spoolfp == NULL) {
		perror("cannot open spool file");
		exit(errno);
	}
	while (ch = getc(tempfp), (ch >= 0)) {
		putc(ch, spoolfp);
	}

	fclose(tempfp);
	return(fclose(spoolfp));
}
