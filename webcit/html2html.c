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


/*
 * Here we go.  Please note that the buffer may be changed by this function!
 */
void output_text_html(char *partbuf, int total_length) {

	char *ptr;
	char *msgstart;
	char *msgend;

	ptr = partbuf;
	msgstart = partbuf;
	msgend = &partbuf[total_length];

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

	/* Close a bunch of tags that might have been opened */
	wprintf("</I></B></FONT></TD></TR></TABLE></TT></PRE></A><BR>\n");
}

