#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdarg.h>
#include "webcit.h"
#include "child.h"

/*
 * browser_braindamage_check()
 * 
 * Given the value of the HTTP "User-agent:" directive supplied by
 * a web browser, determine using a local configuration file whether
 * the browser is capable of handling JavaScript.
 *
 * This function returns one of the following values:
 * B_YES        (Yes, it's ok to use JavaScript)
 * B_NO         (No, fall back to HTML)
 * B_ASK        (We don't know; ask the user)
 */

int browser_braindamage_check(char *browser)
{
	FILE *fp;
	char buf[256];
	int thisval;

	fp = fopen("static/braindamage", "r");
	if (fp == NULL)
		return (B_ASK);

	while (fgets(buf, 256, fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		thisval = (-1);
		if (!strncasecmp(buf, "YES", 3)) {
			thisval = B_YES;
			strcpy(buf, &buf[3]);
		} else if (!strncasecmp(buf, "NO", 2)) {
			thisval = B_NO;
			strcpy(buf, &buf[2]);
		} else if (!strncasecmp(buf, "ASK", 3)) {
			thisval = B_ASK;
			strcpy(buf, &buf[3]);
		}
		if (thisval >= 0) {
			while (isspace(buf[0])) strcpy(buf, &buf[1]);
			if (wildmat(buf, browser)) {
				return(thisval);
			}
		}
	}

	fclose(fp);
	return (B_ASK);
}
