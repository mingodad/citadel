/*
 * $Id$
 *
 * Utility functions which convert the HTTP cookie format we use to and
 * from user/password/room strings.
 *
 */

#include "webcit.h"

#define TRUE  1
#define FALSE 0

typedef unsigned char byte;	      /* Byte type */

/*
 * Pack all session info into one easy-to-digest cookie. Healthy and delicious!
 */
void stuff_to_cookie(char *cookie, int session,
		char *user, char *pass, char *room)
{
	char buf[SIZ];
	int i;

	sprintf(buf, "%d|%s|%s|%s|", session, user, pass, room);
	strcpy(cookie, "");
	for (i=0; i<strlen(buf); ++i) {
		sprintf(&cookie[i*2], "%02X", buf[i]);
	}
}

int xtoi(char *in, size_t len)
{
    int val = 0;
    while (isxdigit((byte) *in) && (len-- > 0)) {
	char c = *in++;
	val <<= 4;
	val += isdigit((unsigned char)c)
	    ? (c - '0')
	    : (tolower((unsigned char)c) - 'a' + 10);
    }
    return val;
}

/*
 * Extract all that fun stuff out of the cookie.
 */
void cookie_to_stuff(char *cookie, int *session,
		char *user, size_t user_len,
		char *pass, size_t pass_len,
		char *room, size_t room_len)
{
	char buf[SIZ];
	int i, len;

	strcpy(buf, "");
	len = strlen(cookie) * 2 ;
	for (i=0; i<len; ++i) {
		buf[i] = xtoi(&cookie[i*2], 2);
		buf[i+1] = 0;
	}

	if (session != NULL)
		*session = extract_int(buf, 0);
	if (user != NULL)
		extract_token(user, buf, 1, '|', user_len);
	if (pass != NULL)
		extract_token(pass, buf, 2, '|', pass_len);
	if (room != NULL)
		extract_token(room, buf, 3, '|', room_len);
}
