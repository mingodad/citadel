/*
 * tools.c -- Miscellaneous routines 
 */



#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include <sys/time.h>
#include "webcit.h"




char *safestrncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
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

	for (a = 0; a < strlen(source); ++a)
		if (source[a] == '|')
			++count;
	return (count);
}

/*
 * extract()  -  extract a parameter from a series of "|" separated...
 */
void extract(char *dest, char *source, int parmnum)
{
	char buf[256];
	int count = 0;
	int n;

	if (strlen(source) == 0) {
		strcpy(dest, "");
		return;
	}
	n = num_parms(source);

	if (parmnum >= n) {
		strcpy(dest, "");
		return;
	}
	strcpy(buf, source);
	if ((parmnum == 0) && (n == 1)) {
		strcpy(dest, buf);
		for (n = 0; n < strlen(dest); ++n)
			if (dest[n] == '|')
				dest[n] = 0;
		return;
	}
	while (count++ < parmnum)
		do {
			strcpy(buf, &buf[1]);
		} while ((strlen(buf) > 0) && (buf[0] != '|'));
	if (buf[0] == '|')
		strcpy(buf, &buf[1]);
	for (count = 0; count < strlen(buf); ++count)
		if (buf[count] == '|')
			buf[count] = 0;
	strcpy(dest, buf);
}

/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(char *source, int parmnum)
{
	char buf[256];

	extract(buf, source, parmnum);
	return (atoi(buf));
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(char *source, long int parmnum)
{
	char buf[256];

	extract(buf, source, parmnum);
	return (atol(buf));
}


/*
 * check for the presence of a character within a string (returns count)
 */
int haschar(st, ch)
char st[];
char ch;
{
	int a, b;
	b = 0;
	for (a = 0; a < strlen(st); ++a)
		if (st[a] == ch)
			++b;
	return (b);
}


/*
 * Format a date/time stamp for output 
 */
void fmt_date(char *buf, time_t thetime) {
	struct tm *tm;

	char *ascmonths[] = {
		"Jan", "Feb", "Mar", "Apr", "May", "Jun",
		"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
	};

	strcpy(buf, "");
	tm = localtime(&thetime);

	sprintf(buf, "%s %d %d %2d:%02d%s",
		ascmonths[tm->tm_mon],
		tm->tm_mday,
		tm->tm_year + 1900,
		( (tm->tm_hour > 12) ? (tm->tm_hour - 12) : (tm->tm_hour) ),
		tm->tm_min,
		( (tm->tm_hour > 12) ? "pm" : "am" )
	);
}
