#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "citadel.h"
#include <unistd.h>
#include "ipc.h"

/*
 * num_parms()  -  discover number of parameters...
 */
int num_parms(char *source)
{
	int a;
	int count = 1;

	for (a=0; a<strlen(source); ++a) 
		if (source[a]=='|') ++count;
	return(count);
	}


/*
 * extract()  -  extract a parameter from a series of "|" separated...
 */
void extract(char *dest, char *source, int parmnum)
{
	char buf[256];
	int count = 0;
	int n;

	n = num_parms(source);

	if (parmnum >= n) {
		strcpy(dest,"");
		return;
		}
	strcpy(buf,source);
	if ( (parmnum == 0) && (n == 1) ) {
		strcpy(dest,buf);
		return;
		}

	while (count++ < parmnum) do {
		strcpy(buf,&buf[1]);
		} while( (strlen(buf)>0) && (buf[0]!='|') );
	if (buf[0]=='|') strcpy(buf,&buf[1]);
	for (count = 0; count<strlen(buf); ++count)
		if (buf[count] == '|') buf[count] = 0;
	strcpy(dest,buf);
	}

/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(char *source, int parmnum)
{
	char buf[256];
	
	extract(buf,source,parmnum);
	return(atoi(buf));
	}


/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(char *source, int parmnum)
{
	char buf[256];
	
	extract(buf,source,parmnum);
	return(atol(buf));
	}


void logoff(int code)
{
	exit(code);
	}

void userlist(void) { 
	char buf[256];
	char fl[256];
	struct tm *tmbuf;
	long lc;

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
	char buf[256];

	attach_to_server(argc,argv);
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

	sprintf(buf,"errno = %d",e);
	return(buf);
	}
#endif





