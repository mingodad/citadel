/*
 * $Id$
 *
 * Output an HTML message, modifying it slightly to make sure it plays nice
 * with the rest of our web framework.
 *
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
#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"
#include "vcard.h"
#include "webserver.h"


/*
 */
void output_html(void) {
	char buf[SIZ];
	char *msg;
	char *ptr;
	char *msgstart;
	char *msgend;
	int total_length = 1;
	int line_length = 0;

	msg = strdup("");
	msgstart = msg;
	msgend = msg;

	while (serv_gets(buf), strcmp(buf, "000")) {
		line_length = strlen(buf);
		total_length = total_length + line_length + 1;
		msg = realloc(msg, total_length);
		strcpy(msgend, buf);
		strcat(msgend, "\n");
		msgend = &msgend[line_length + 1];
	}

	ptr = msg;
	msgstart = msg;
	msgend = &msg[total_length];

	fprintf(stderr, "msg looks like this:\n%s\n", ptr);

	while (ptr < msgend) {

		/* Advance to next tag */
		ptr = strchr(ptr, '<');
		++ptr;

		/* Any of these tags cause everything up to and including
		 * the tag to be removed.
		 */	
		if ( (!strncasecmp(ptr, "HTML", 4))
		   ||(!strncasecmp(ptr, "HEAD", 4))
		   ||(!strncasecmp(ptr, "/HEAD", 5))
		   ||(!strncasecmp(ptr, "BODY", 4)) ) {
			ptr = strchr(ptr, '>');
			++ptr;
			msgstart = ptr;
		}

		/* Any of these tags cause everything including and following
		 * the tag to be removed.
		 */
		if ( (!strncasecmp(ptr, "/HTML", 5))
		   ||(!strncasecmp(ptr, "/BODY", 5)) ) {
			--ptr;
			msgend = ptr;
			strcpy(ptr, "");
			
		}

		++ptr;
	}

	write(WC->http_sock, msgstart, strlen(msgstart));

	/* Close a bunch of tags that might have been opened 
	wprintf("</I></B></FONT></TD></TR></TABLE></TT></PRE></A><BR>\n");
	 */

	/* Now give back the memory */
	free(msg);
}

