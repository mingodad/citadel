/* 
 * readlog.c
 * v1.4
 */

#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <time.h>
#include "citadel.h"

void get_config();
struct config config;

void last20(file,pos)
int file;
long pos;
	{
	int a,count;
	long aa;
	struct calllog calllog;
	struct calllog listing[20];
	struct tm *tm;
	char *tstring;
	
	count=0;
	for (a=0; a<20; ++a) {
		listing[a].CLfullname[0]=0;
		listing[a].CLtime=0L;
		listing[a].CLflags=0;
		}
	aa=pos-1;
	while(count<20) {
		if (aa<0L) aa=CALLLOG;
		lseek(file,(aa*sizeof(struct calllog)),0);
		a=read(file,(char *)&calllog,sizeof(struct calllog));
		if (calllog.CLflags==CL_LOGIN) {
			strcpy(listing[count].CLfullname,calllog.CLfullname);
			listing[count].CLtime=calllog.CLtime;
			listing[count].CLflags=calllog.CLflags;
			++count;
			}
		if (aa==pos) break;
		aa=aa-1;
		}
	for (a=19; a>=0; --a) {
		tm=(struct tm *)localtime(&listing[a].CLtime);
		tstring=(char *)asctime(tm);
		printf("%30s %s",listing[a].CLfullname,tstring);
		}
	}

void main(argc,argv)
int argc;
char *argv[]; {
	struct calllog calllog;
	int file,pos,a,b;
	char aaa[100];
	struct tm *tm;
	char *tstring;

	get_config();
	file=open("calllog.pos",O_RDONLY);
	a=read(file,(char *)&pos,sizeof(int));
	close(file);

	file=open("calllog",O_RDONLY);
	if (argc>=2) {
		if (!strcmp(argv[1],"-t")) last20(file,(long)pos);
		else fprintf(stderr,"%s: usage: %s [-t]\n",argv[0],argv[0]);
		close(file);
		exit(0);
		}
else {
	lseek(file,(long)(pos*sizeof(struct calllog)),0);
	for (a=0; a<CALLLOG; ++a) {
		if ((a+pos)==CALLLOG) lseek(file,0L,0);
		b=read(file,(char *)&calllog,sizeof(struct calllog));
	if (calllog.CLflags!=0) {
		strcpy(aaa,"");
		if (calllog.CLflags&CL_CONNECT)	strcpy(aaa,"Connect");
		if (calllog.CLflags&CL_LOGIN)	strcpy(aaa,"Login");
		if (calllog.CLflags&CL_NEWUSER)	strcpy(aaa,"New User");
		if (calllog.CLflags&CL_BADPW)	strcpy(aaa,"Bad PW Attempt");
		if (calllog.CLflags&CL_TERMINATE) strcpy(aaa,"Terminate");
		if (calllog.CLflags&CL_DROPCARR) strcpy(aaa,"Dropped Carrier");
		if (calllog.CLflags&CL_SLEEPING) strcpy(aaa,"Sleeping");
		if (calllog.CLflags&CL_PWCHANGE) strcpy(aaa,"Changed Passwd");
		tm=(struct tm *)localtime(&calllog.CLtime);
		tstring=(char *)asctime(tm);
		printf("%30s %20s %s",calllog.CLfullname,aaa,tstring);
		}
		}
	}
	close(file);
	exit(0);
}
	
