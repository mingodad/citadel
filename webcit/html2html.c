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
		msg = realloc(msg, total_length + 1);
		strcpy(msgend, buf);
		msgend[line_length++] = '\n' ;
		msgend[line_length] = 0;
		msgend = &msgend[line_length];
	}

	ptr = msg;
	msgstart = msg;
	/* msgend is already set correctly */

	while (ptr < msgend) {

		/* Advance to next tag */
		ptr = strchr(ptr, '<');
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

