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
 * extract()  -  a smarter string tokenizer
 */
void extract_token(char *dest, char *source, int parmnum, char separator)
{
	int i;
	int len;
	int curr_parm;

	strcpy(dest,"");
	len = 0;
	curr_parm = 0;

	if (strlen(source)==0) {
		return;
		}

	for (i=0; i<strlen(source); ++i) {
		if (source[i]==separator) {
			++curr_parm;
		}
		else if (curr_parm == parmnum) {
			dest[len+1] = 0;
			dest[len++] = source[i];
		}
	}
}

/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(char *source, int parmnum)
{
	char buf[256];
	
	extract_token(buf, source, parmnum, '|');
	return(atoi(buf));
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(char *source, long int parmnum)
{
	char buf[256];
	
	extract_token(buf, source, parmnum, '|');
	return(atol(buf));
}
