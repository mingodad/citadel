/* $Id$ */
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include "support.h"


/*
 * strproc()  -  make a string 'nice'
 */
void strproc(char *string)
{
	int a;

	if (strlen(string)==0) return;

	/* Convert non-printable characters to blanks */
	for (a=0; a<strlen(string); ++a) {
		if (string[a]<32) string[a]=32;
		if (string[a]>126) string[a]=32;
		}

	/* Remove leading and trailing blanks */
	while( (string[0]<33) && (strlen(string)>0) )
		strcpy(string,&string[1]);
	while( (string[strlen(string)-1]<33) && (strlen(string)>0) )
		string[strlen(string)-1]=0;

	/* Remove double blanks */
	for (a=0; a<strlen(string); ++a) {
		if ((string[a]==32)&&(string[a+1]==32)) {
			strcpy(&string[a],&string[a+1]);
			a=0;
			}
		}

	/* remove characters which would interfere with the network */
	for (a=0; a<strlen(string); ++a) {
		while (string[a]=='!') strcpy(&string[a],&string[a+1]);
		while (string[a]=='@') strcpy(&string[a],&string[a+1]);
		while (string[a]=='_') strcpy(&string[a],&string[a+1]);
		while (string[a]==',') strcpy(&string[a],&string[a+1]);
		while (string[a]=='%') strcpy(&string[a],&string[a+1]);
		while (string[a]=='|') strcpy(&string[a],&string[a+1]);
		}

	}



/*
 * get a line of text from a file
 * ignores lines starting with #
 */
int getstring(FILE *fp, char *string)
{
	int a,c;
	do {
		strcpy(string,"");
		a=0;
		do {
			c=getc(fp);
			if (c<0) {
				string[a]=0;
				return(-1);
				}
			string[a++]=c;
			} while(c!=10);
			string[a-1]=0;
		} while(string[0]=='#');
	return(strlen(string));
	}


/*
 * pattern2()  -  searches for patn within search string, returns pos 
 */ 
int pattern2(char *search, char *patn)
{
	int a;
	for (a=0; a<strlen(search); ++a) {
		if (!strncasecmp(&search[a],patn,strlen(patn))) return(a);
		}
	return(-1);
	}


/*
 * mesg_locate()  -  locate a message or help file, case insensitive
 */
void mesg_locate(char *targ, char *searchfor, int numdirs, char **dirs)
{
	int a;
	char buf[256];
	FILE *ls;

	for (a=0; a<numdirs; ++a) {
		sprintf(buf,"cd %s; exec ls",dirs[a]);
		ls = (FILE *) popen(buf,"r");
		if (ls != NULL) {
			while(fgets(buf,255,ls)!=NULL) {
				while (isspace(buf[strlen(buf)-1]))
					buf[strlen(buf)-1] = 0;
				if (!strcasecmp(buf,searchfor)) {
					pclose(ls);
					sprintf(targ,"%s/%s",dirs[a],buf);
					return;
					}
				}
			pclose(ls);
			}
		}
	strcpy(targ,"");
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

