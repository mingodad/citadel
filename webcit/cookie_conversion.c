#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <ctype.h>
#include <string.h>
#include "webcit.h"
#include "child.h"

/*
 * Pack all session info into one easy-to-digest cookie.  Healthy and delicious!
 */
void stuff_to_cookie(char *cookie, int session, char *user, char *pass, char *room) {
	char buf[256];
	int i;

	sprintf(buf, "%d|%s|%s|%s", session, user, pass, room);
	strcpy(cookie, "");
	
	for (i=0; i<strlen(buf); ++i)
		sprintf(&cookie[strlen(cookie)], "%02X", buf[i]);

}


/*
 * Extract all that fun stuff out of the cookie.
 */
void cookie_to_stuff(char *cookie, int *session, char *user, char *pass, char *room) {
	char buf[256];
	int i;

	for (i=0; i<strlen(cookie); i=i+2) {
		sscanf(&cookie[i], "%02x", (unsigned int *)&buf[i/2]);
		buf[(i/2)+1] = 0;
	}

	if (session != NULL)	*session = extract_int(buf, 0);
	if (user != NULL)	extract(user, buf, 1);
	if (pass != NULL)	extract(pass, buf, 2);
	if (room != NULL)	extract(room, buf, 3);
}
