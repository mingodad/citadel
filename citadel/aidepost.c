/* aidepost.c
 * This is just a little hack to copy standard input to a message in Aide>
 * v1.6
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <time.h>
#include <limits.h>
#include "citadel.h"
#include "config.h"

void make_message(char *filename)
{
	FILE *fp;
	int a;
	long bb,cc;
	time_t now;
	time(&now);
	fp=fopen(filename,"wb"); if (fp==NULL) exit(22);
	putc(255,fp);
	putc(MES_NORMAL,fp);
	putc(1,fp);
	fprintf(fp,"Proom_aide"); putc(0,fp);
	fprintf(fp,"T%ld",now); putc(0,fp);
	fprintf(fp,"ACitadel"); putc(0,fp);
	fprintf(fp,"OAide"); putc(0,fp);
	fprintf(fp,"N%s",NODENAME); putc(0,fp);
	putc('M',fp);
	bb=ftell(fp);
	while (a=getc(stdin), a>0) {
		if (a!=8) putc(a,fp);
		else {
			cc=ftell(fp);
			if (cc!=bb) fseek(fp,(-1L),1);
			}
		}
	putc(0,fp);
	putc(0,fp);
	putc(0,fp);
	fclose(fp);
	}

int main(int argc, char **argv)
{
	char tempbase[32];
	char temptmp[64];
	char tempspool[64];
	char movecmd[256];
	
	get_config();
	snprintf(tempbase,sizeof tempbase,ap.%d",getpid());
	snprintf(temptmp,sizeof temptmp,"/tmp/%s", tempbase);
	snprintf(tempspool,sizeof tempspool,"./network/spoolin/%s", tempbase);
	make_message(temptmp);

	snprintf(movecmd, sizeof movecmd, "/bin/mv %s %s", temptmp, tempspool);
	system(movecmd);

	execlp("./netproc","netproc",NULL);
	fprintf(stderr,"aidepost: could not run netproc\n");
	exit(1);
	}
