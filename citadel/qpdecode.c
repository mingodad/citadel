/*
 * $Id$ 
 *
 * Convert "quoted printable" encoding to binary (stdin to stdout)
 * Copyright (C) 1999 by Art Cancro
 * Distributed under the terms of the GNU General Public License.
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>

int main(int argc, char *argv[]) {
	char buf[80];
	int soft_line_break = 0;
	int ch;

	while (fgets(buf, 80, stdin) != NULL) {
		while (isspace(buf[strlen(buf)-1]))
			buf[strlen(buf)-1] = 0;
		soft_line_break = 0;

		while (strlen(buf) > 0) {
			if (!strcmp(buf, "=")) {
				soft_line_break = 1;
				strcpy(buf, "");
			} else if ( (strlen(buf)>=3) && (buf[0]=='=') ) {
				sscanf(&buf[1], "%02x", &ch);
				putc(ch, stdout);
				strcpy(buf, &buf[3]);
			} else {
				putc(buf[0], stdout);
				strcpy(buf, &buf[1]);
			}
		}
		if (soft_line_break == 0) printf("\r\n");
	}
	exit(0);
}
