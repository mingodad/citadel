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
#include "citadel.h"
#include "config.h"

void make_message(FILE *fp)
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
	fprintf(fp, "T%ld", now);
	putc(0, fp);
	fprintf(fp, "ACitadel");
	putc(0, fp);
	fprintf(fp, "OAide");
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
	char tempbase[32];
	char temptmp[64];
	char tempspool[64];
	FILE *tempfp, *spoolfp;
	int ch;

	get_config();
	snprintf(tempbase, sizeof tempbase, "ap.%d", getpid());
	snprintf(temptmp, sizeof temptmp, "/tmp/%s", tempbase);
	snprintf(tempspool, sizeof tempspool, "./network/spoolin/%s", tempbase);

	tempfp = fopen(temptmp, "wb+");
	if (tempfp == NULL) {
		perror("cannot open temp file");
		exit(errno);
	}
	/* Unlink the temp file, so it automatically gets deleted by the OS if
	 * this program is interrupted or crashes.
	 */ unlink(temptmp);

	/* Generate a message from stdin */
	make_message(tempfp);

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
