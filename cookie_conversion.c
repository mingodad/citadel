
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
#include "webcit.h"

#define TRUE  1
#define FALSE 0

typedef unsigned char byte;	      /* Byte type */

/*
 * Pack all session info into one easy-to-digest cookie.  Healthy and delicious!
 */
void stuff_to_cookie(char *cookie, int session, char *user, char *pass, char *room)
{
	char buf[SIZ];

	sprintf(buf, "%d|%s|%s|%s|END", session, user, pass, room);
	CtdlEncodeBase64(cookie, buf, strlen(buf));
}


/*
 * Extract all that fun stuff out of the cookie.
 */
void cookie_to_stuff(char *cookie, int *session, char *user, char *pass, char *room)
{
	char buf[SIZ];

	CtdlDecodeBase64(buf, cookie, strlen(cookie));

	if (session != NULL)
		*session = extract_int(buf, 0);
	if (user != NULL)
		extract(user, buf, 1);
	if (pass != NULL)
		extract(pass, buf, 2);
	if (room != NULL)
		extract(room, buf, 3);
}
