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
	char *converted_msg;
	int buffer_length = 1;
	int line_length = 0;
	int content_length = 0;
	int output_length = 0;
	char new_window[SIZ];
	int brak = 0;
	int alevel = 0;
	int i;
	int linklen;

	msg = strdup("");
	sprintf(new_window, "<A TARGET=\"%s\" HREF=", TARGET);

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

	converted_msg = malloc(content_length);
	strcpy(converted_msg, "");
	ptr = msgstart;
	while (ptr < msgend) {
		/* Change mailto: links to WebCit mail, by replacing the
		 * link with one that points back to our mail room.  Due to
		 * the way we parse URL's, it'll even handle mailto: links
		 * that have "?subject=" in them.
		 */
		if (!strncasecmp(ptr, "<A HREF=\"mailto:", 16)) {
			content_length += 64;
			converted_msg = realloc(converted_msg, content_length);
			sprintf(&converted_msg[output_length],
				"<A HREF=\"/display_enter"
				"?force_room=_MAIL_&recp=");
			output_length += 47;
			ptr = &ptr[16];
			++alevel;
		}
		/* Make links open in a separate window */
		else if (!strncasecmp(ptr, "<A HREF=", 8)) {
			content_length += 64;
			converted_msg = realloc(converted_msg, content_length);
			sprintf(&converted_msg[output_length], new_window);
			output_length += strlen(new_window);
			ptr = &ptr[8];
			++alevel;
		}
		/* Turn anything that looks like a URL into a real link, as long
		 * as it's not inside a tag already
		 */
		else if ( (brak == 0) && (alevel == 0)
		     && (!strncasecmp(ptr, "http://", 7))) {
				linklen = 0;
				/* Find the end of the link */
				for (i=0; i<=strlen(ptr); ++i) {
					if ((ptr[i]==0)
					   ||(isspace(ptr[i]))
					   ||(ptr[i]==10)
					   ||(ptr[i]==13)
					   ||(ptr[i]=='(')
					   ||(ptr[i]==')')
					   ||(ptr[i]=='<')
					   ||(ptr[i]=='>')
					   ||(ptr[i]=='[')
					   ||(ptr[i]==']')
					) linklen = i;
					if (linklen > 0) break;
				}
				if (linklen > 0) {
					content_length += (32 + linklen);
					converted_msg = realloc(converted_msg, content_length);
					sprintf(&converted_msg[output_length], new_window);
					output_length += strlen(new_window);
					converted_msg[output_length] = '\"';
					converted_msg[++output_length] = 0;
					for (i=0; i<linklen; ++i) {
						converted_msg[output_length] = ptr[i];
						converted_msg[++output_length] = 0;
					}
					sprintf(&converted_msg[output_length], "\">");
					output_length += 2;
					for (i=0; i<linklen; ++i) {
						converted_msg[output_length] = *ptr++;
						converted_msg[++output_length] = 0;
					}
					sprintf(&converted_msg[output_length], "</A>");
					output_length += 4;
				}
		}
		else {
			/*
			 * We need to know when we're inside a tag,
			 * so we don't turn things that look like URL's into
			 * links, when they're already links - or image sources.
			 */
			if (*ptr == '<') ++brak;
			if (*ptr == '>') --brak;
			if (!strncasecmp(ptr, "</A>", 3)) --alevel;
			converted_msg[output_length] = *ptr++;
			converted_msg[++output_length] = 0;
		}
	}

	/* Output our big pile of markup */
	client_write(converted_msg, output_length);

	/* A little trailing vertical whitespace... */
	wprintf("<BR><BR>\n");

	/* Now give back the memory */
	free(converted_msg);
	free(msg);
}

