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

#include "citadel.h"
#include <unistd.h>
#include "ipc.h"
#include "tools.h"

void logoff(int code)
{
	exit(code);
	}

void userlist(void) { 
	char buf[SIZ];
	char fl[SIZ];
	struct tm *tmbuf;
	time_t lc;

	serv_puts("LIST");
	serv_gets(buf);
	if (buf[0]!='1') {
		printf("%s\n",&buf[4]);
		return;
		}
	printf("       User Name           Num  L  LastCall  Calls Posts\n");
	printf("------------------------- ----- - ---------- ----- -----\n");
	while (serv_gets(buf), strcmp(buf,"000")) {
		extract(fl,buf,0);
		printf("%-25s ",fl);
		printf("%5ld %d ",extract_long(buf,2),
			extract_int(buf,1));
		lc = extract_long(buf,3);
		tmbuf = (struct tm *)localtime(&lc);
		printf("%02d/%02d/%04d ",
			(tmbuf->tm_mon+1),
			tmbuf->tm_mday,
			(tmbuf->tm_year + 1900));
		printf("%5ld %5ld\n",
			extract_long(buf,4),extract_long(buf,5));
		}
	printf("\n");
	}


int main(int argc, char **argv)
{
	char buf[SIZ];
	char hostbuf[SIZ], portbuf[SIZ];

	attach_to_server(argc, argv, hostbuf, portbuf);
	serv_gets(buf);
	if ((buf[0]!='2')&&(strncmp(buf,"551",3))) {
		fprintf(stderr,"%s: %s\n",argv[0],&buf[4]);
		logoff(atoi(buf));
		}

	userlist();

	serv_puts("QUIT");
	serv_gets(buf);
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





