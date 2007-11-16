/*
 * $Id$
 *
 * Command-line user list utility.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <libcitadel.h>
#include "citadel.h"
#include <unistd.h>
#include "citadel_ipc.h"
#include "citadel_dirs.h"

void logoff(int code)
{
	exit(code);
}

void userlist(CtdlIPC *ipc) { 
	char buf[SIZ];
	char fl[SIZ];
	struct tm tmbuf;
	time_t lc;
	char *listing = NULL;
	int r;

	r = CtdlIPCUserListing(ipc, "", &listing, buf);
	if (r / 100 != 1) {
		printf("%s\n", buf);
		return;
	}
	printf("       User Name           Num  L  LastCall  Calls Posts\n");
	printf("------------------------- ----- - ---------- ----- -----\n");
	while (strlen(listing) > 0) {
		extract_token(buf, listing, 0, '\n', sizeof buf);
		remove_token(listing, 0, '\n');
		extract_token(fl, buf, 0, '|', sizeof fl);
		printf("%-25s ",fl);
		printf("%5ld %d ", extract_long(buf,2),
			extract_int(buf,1));
		lc = extract_long(buf,3);
		localtime_r(&lc, &tmbuf);
		printf("%02d/%02d/%04d ",
			(tmbuf.tm_mon+1),
			tmbuf.tm_mday,
			(tmbuf.tm_year + 1900));
		printf("%5ld %5ld\n",
			extract_long(buf,4),extract_long(buf,5));
	}
	printf("\n");
}


int main(int argc, char **argv)
{
	char buf[SIZ];
	char hostbuf[SIZ], portbuf[SIZ];
	CtdlIPC *ipc = NULL;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;

	CtdlInitBase64Table();
	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);

	ipc = CtdlIPC_new(argc, argv, hostbuf, portbuf);
	CtdlIPC_chat_recv(ipc, buf);
	if ((buf[0]!='2')&&(strncmp(buf,"551",3))) {
		fprintf(stderr,"%s: %s\n",argv[0],&buf[4]);
		logoff(atoi(buf));
	}

	userlist(ipc);

	CtdlIPCQuit(ipc);
	exit(0);
}


#ifndef HAVE_STRERROR
/*
 * replacement strerror() for systems that don't have it
 */
char *strerror(int e)
{
	static char buf[32];

	snprintf(buf, sizeof buf, "errno = %d",e);
	return(buf);
}
#endif
