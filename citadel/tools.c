/*
 * tools.c -- Miscellaneous routines used by both the client and server.
 * $Id$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tools.h"

char *safestrncpy(char *dest, const char *src, size_t n)
{
  if (dest == NULL || src == NULL)
    {
      fprintf(stderr, "safestrncpy: NULL argument\n");
      abort();
    }
  strncpy(dest, src, n);
  dest[n - 1] = 0;
  return dest;
}


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
void extract_token(char *dest, char *source, int parmnum, char separator)
{
	char buf[256];
	int count = 0;
	int n;

	if (strlen(source)==0) {
		strcpy(dest,"");
		return;
		}

	n = num_parms(source);

	if (parmnum >= n) {
		strcpy(dest,"");
		return;
		}
	strcpy(buf,source);
	if ( (parmnum == 0) && (n == 1) ) {
		strcpy(dest,buf);
		for (n=0; n<strlen(dest); ++n)
			if (dest[n]==separator) dest[n] = 0;
		return;
		}

	while (count++ < parmnum) do {
		strcpy(buf,&buf[1]);
		} while( (strlen(buf)>0) && (buf[0]!=separator) );
	if (buf[0]==separator) strcpy(buf,&buf[1]);
	for (count = 0; count<strlen(buf); ++count)
		if (buf[count] == separator) buf[count] = 0;
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
long extract_long(char *source, long int parmnum)
{
	char buf[256];
	
	extract(buf,source,parmnum);
	return(atol(buf));
	}



