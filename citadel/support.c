/*
 * Server-side utility functions
 */

#include "sysdep.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <libcitadel.h>

#include "citadel.h"
#include "support.h"

/*
 * strproc()  -  make a string 'nice'
 */
void strproc(char *string)
{
	int a, b;

	if (string == NULL) return;
	if (IsEmptyStr(string)) return;

	/* Convert non-printable characters to blanks */
	for (a=0; !IsEmptyStr(&string[a]); ++a) {
		if (string[a]<32) string[a]=32;
		if (string[a]>126) string[a]=32;
	}

	/* a is now the length of our string. */
	/* Remove leading and trailing blanks */
	while( (string[a-1]<33) && (!IsEmptyStr(string)) )
		string[--a]=0;
	b = 0;
	while( (string[b]<33) && (!IsEmptyStr(&string[b])) )
		b++;
	if (b > 0)
		memmove(string,&string[b], a - b + 1);

	/* Remove double blanks */
	for (a=0; !IsEmptyStr(&string[a]); ++a) {
		if ((string[a]==32)&&(string[a+1]==32)) {
			strcpy(&string[a],&string[a+1]);
			a=0;
		}
	}

	/* remove characters which would interfere with the network */
	for (a=0; !IsEmptyStr(&string[a]); ++a) {
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
 * mesg_locate()  -  locate a message or help file, case insensitive
 */
void mesg_locate(char *targ, size_t n, const char *searchfor,
		 int numdirs, const char * const *dirs)
{
	int a;
	char buf[SIZ];
	struct stat test;

	for (a=0; a<numdirs; ++a) {
		snprintf(buf, sizeof buf, "%s/%s", dirs[a], searchfor);
		if (!stat(buf, &test)) {
			snprintf(targ, n, "%s/%s", dirs[a], searchfor);
			return;
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

	snprintf(buf,sizeof buf,"errno = %d",e);
	return(buf);
	}
#endif

