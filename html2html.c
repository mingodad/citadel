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
	int buffer_length = 1;
	int line_length = 0;
	int content_length = 0;

	msg = strdup("");

	while (serv_gets(buf), strcmp(buf, "000")) {
		line_length = strlen(buf);
		buffer_length = content_length + line_length + 2;
		msg = realloc(msg, buffer_length);
		if (msg == NULL) {
			wprintf("<B>realloc() error!  "
				"couldn't get %d bytes: %s</B><BR><BR>\n",
				buffer_length + 1,
				strerror(errno));
			return;
		}
		strcpy(&msg[content_length], buf);
		content_length += line_length;
		strcpy(&msg[content_length], "\n");
		content_length += 1;
	}

	ptr = msg;
	msgstart = msg;
	msgend = &msg[content_length];

	while (ptr < msgend) {

		/* Advance to next tag */
		ptr = strchr(ptr, '<');
		if ((ptr == NULL) || (ptr >= msgend)) break;
		++ptr;
		if ((ptr == NULL) || (ptr >= msgend)) break;

		/* Any of these tags cause everything up to and including
		 * the tag to be removed.
		 */	
		if ( (!strncasecmp(ptr, "HTML", 4))
		   ||(!strncasecmp(ptr, "HEAD", 4))
		   ||(!strncasecmp(ptr, "/HEAD", 5))
		   ||(!strncasecmp(ptr, "BODY", 4)) ) {
			ptr = strchr(ptr, '>');
			if ((ptr == NULL) || (ptr >= msgend)) break;
			++ptr;
			if ((ptr == NULL) || (ptr >= msgend)) break;
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

	/* A little trailing vertical whitespace... */
	wprintf("<BR><BR>\n");

	/* Now give back the memory */
	free(msg);

}

