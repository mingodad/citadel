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
#include "webserver.h"


char *ascmonths[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
};

char *ascdays[] = {
	"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
};


char *safestrncpy(char *dest, const char *src, size_t n)
{
	if (dest == NULL || src == NULL) {
		lprintf(1, "safestrncpy: NULL argument\n");
		abort();
	}
	strncpy(dest, src, n);
	dest[n - 1] = 0;
	return dest;
}



/*
 * num_tokens()  -  discover number of parameters/tokens in a string
 */
int num_tokens(char *source, char tok) {
	int a;
	int count = 1;

	if (source == NULL) return(0);
	for (a=0; a<strlen(source); ++a) {
		if (source[a]==tok) ++count;
	}
	return(count);
}

/*
 * extract_token()  -  a smarter string tokenizer
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
 * remove_token()  -  a tokenizer that kills, maims, and destroys
 */
void remove_token(char *source, int parmnum, char separator)
{
	int i;
	int len;
	int curr_parm;
	int start, end;

	len = 0;
	curr_parm = 0;
	start = (-1);
	end = (-1);

	if (strlen(source)==0) {
		return;
		}

	for (i=0; i<strlen(source); ++i) {
		if ( (start < 0) && (curr_parm == parmnum) ) {
			start = i;
		}

		if ( (end < 0) && (curr_parm == (parmnum+1)) ) {
			end = i;
		}

		if (source[i]==separator) {
			++curr_parm;
		}
	}

	if (end < 0) end = strlen(source);

	strcpy(&source[start], &source[end]);
}




/*
 * extract_int()  -  extract an int parm w/o supplying a buffer
 */
int extract_int(char *source, int parmnum)
{
	char buf[SIZ];
	
	extract_token(buf, source, parmnum, '|');
	return(atoi(buf));
}

/*
 * extract_long()  -  extract an long parm w/o supplying a buffer
 */
long extract_long(char *source, long int parmnum)
{
	char buf[SIZ];
	
	extract_token(buf, source, parmnum, '|');
	return(atol(buf));
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
	int hour;

	strcpy(buf, "");
	tm = localtime(&thetime);
	hour = tm->tm_hour;
	if (hour == 0) hour = 12;
	else if (hour > 12) hour = hour - 12;

	sprintf(buf, "%s %d %d %2d:%02d%s",
		ascmonths[tm->tm_mon],
		tm->tm_mday,
		tm->tm_year + 1900,
		hour,
		tm->tm_min,
		( (tm->tm_hour > 12) ? "pm" : "am" )
	);
}



/*
 * Format TIME ONLY for output 
 */
void fmt_time(char *buf, time_t thetime) {
	struct tm *tm;
	int hour;

	strcpy(buf, "");
	tm = localtime(&thetime);
	hour = tm->tm_hour;
	if (hour == 0) hour = 12;
	else if (hour > 12) hour = hour - 12;

	sprintf(buf, "%d:%02d%s",
		hour,
		tm->tm_min,
		( (tm->tm_hour > 12) ? "pm" : "am" )
	);
}




/*
 * Format a date/time stamp to the format used in HTTP headers
 */
void httpdate(char *buf, time_t thetime) {
	struct tm *tm;

	strcpy(buf, "");
	tm = localtime(&thetime);

	sprintf(buf, "%s, %02d %s %4d %02d:%02d:%02d",
		ascdays[tm->tm_wday],
		tm->tm_mday,
		ascmonths[tm->tm_mon],
		tm->tm_year + 1900,
		tm->tm_hour,
		tm->tm_min,
		tm->tm_sec
	);
}





/*
 * Utility function to "readline" from memory
 * (returns new pointer)
 */
char *memreadline(char *start, char *buf, int maxlen)
{
        char ch;
        char *ptr;
        int len = 0;    /* tally our own length to avoid strlen() delays */

        ptr = start;
        memset(buf, 0, maxlen);

        while (1) {
                ch = *ptr++;
                if ( (len < (maxlen - 1)) && (ch != 13) && (ch != 10) ) {
                        buf[strlen(buf) + 1] = 0;
                        buf[strlen(buf)] = ch;
                        ++len;
                }
                if ((ch == 10) || (ch == 0)) {
                        return ptr;
                }
        }
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
 * Strip leading and trailing spaces from a string
 */
void striplt(char *buf)
{
        while ((strlen(buf) > 0) && (isspace(buf[0])))
                strcpy(buf, &buf[1]);
        while (isspace(buf[strlen(buf) - 1]))
                buf[strlen(buf) - 1] = 0;
}


/*
 * Determine whether the specified message number is contained within the
 * specified set.
 */
int is_msg_in_mset(char *mset, long msgnum) {
	int num_sets;
	int s;
	char setstr[SIZ], lostr[SIZ], histr[SIZ];	/* was 1024 */
	long lo, hi;

	/*
	 * Now set it for all specified messages.
	 */
	num_sets = num_tokens(mset, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, mset, s, ',');

		extract_token(lostr, setstr, 0, ':');
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':');
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		if ((msgnum >= lo) && (msgnum <= hi)) return(1);
	}

	return(0);
}


/*
 * Strip a boundarized substring out of a string (for example, remove
 * parentheses and anything inside them).
 *
 * This improved version can strip out *multiple* boundarized substrings.
 */
void stripout(char *str, char leftboundary, char rightboundary) {
	int a;
        int lb = (-1);
        int rb = (-1);

	do {
		lb = (-1);
		rb = (-1);
	
        	for (a = 0; a < strlen(str); ++a) {
                	if (str[a] == leftboundary) lb = a;
                	if (str[a] == rightboundary) rb = a;
        	}

        	if ( (lb > 0) && (rb > lb) ) {
                	strcpy(&str[lb - 1], &str[rb + 1]);
        	}

	} while ( (lb > 0) && (rb > lb) );

}



/*
 * Replacement for sleep() that uses select() in order to avoid SIGALRM
 */
void sleeeeeeeeeep(int seconds)
{
	struct timeval tv;

	tv.tv_sec = seconds;
	tv.tv_usec = 0;
	select(0, NULL, NULL, NULL, &tv);
}

