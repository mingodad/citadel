
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
static byte dtable[256];	      /* base64 encode / decode table */

/*
 * decode_base64() and encode_base64() are adaptations of code by
 * John Walker, found in full in the file "base64.c" included with the Citadel
 * server.  The difference between those functions and these is that
 * these are intended to encode/decode small string buffers, and those are
 * intended to encode/decode entire MIME parts.
 */

void encode_base64(char *dest, char *source)
{
    int i, hiteof = FALSE;
    int spos = 0;
    int dpos = 0;

    /*	Fill dtable with character encodings.  */

    for (i = 0; i < 26; i++) {
        dtable[i] = 'A' + i;
        dtable[26 + i] = 'a' + i;
    }
    for (i = 0; i < 10; i++) {
        dtable[52 + i] = '0' + i;
    }
    dtable[62] = '+';
    dtable[63] = '/';

    while (!hiteof) {
	byte igroup[3], ogroup[4];
	int c, n;

	igroup[0] = igroup[1] = igroup[2] = 0;
	for (n = 0; n < 3; n++) {
	    c = source[spos++];
	    if (c == 0) {
		hiteof = TRUE;
		break;
	    }
	    igroup[n] = (byte) c;
	}
	if (n > 0) {
	    ogroup[0] = dtable[igroup[0] >> 2];
	    ogroup[1] = dtable[((igroup[0] & 3) << 4) | (igroup[1] >> 4)];
	    ogroup[2] = dtable[((igroup[1] & 0xF) << 2) | (igroup[2] >> 6)];
	    ogroup[3] = dtable[igroup[2] & 0x3F];

            /* Replace characters in output stream with "=" pad
	       characters if fewer than three characters were
	       read from the end of the input stream. */

	    if (n < 3) {
                ogroup[3] = '=';
		if (n < 2) {
                    ogroup[2] = '=';
		}
	    }
	    for (i = 0; i < 4; i++) {
		dest[dpos++] = ogroup[i];
		dest[dpos] = 0;
	    }
	}
    }
}



void decode_base64(char *dest, char *source)
{
    int i;
    int dpos = 0;
    int spos = 0;

    for (i = 0; i < 255; i++) {
	dtable[i] = 0x80;
    }
    for (i = 'A'; i <= 'Z'; i++) {
        dtable[i] = 0 + (i - 'A');
    }
    for (i = 'a'; i <= 'z'; i++) {
        dtable[i] = 26 + (i - 'a');
    }
    for (i = '0'; i <= '9'; i++) {
        dtable[i] = 52 + (i - '0');
    }
    dtable['+'] = 62;
    dtable['/'] = 63;
    dtable['='] = 0;

    /*CONSTANTCONDITION*/
    while (TRUE) {
	byte a[4], b[4], o[3];

	for (i = 0; i < 4; i++) {
	    int c = source[spos++];

	    if (c == 0) {
		if (i > 0) {
		    return;
		}
		return;
	    }
	    if (dtable[c] & 0x80) {
		/* Ignoring errors: discard invalid character. */
		i--;
		continue;
	    }
	    a[i] = (byte) c;
	    b[i] = (byte) dtable[c];
	}
	o[0] = (b[0] << 2) | (b[1] >> 4);
	o[1] = (b[1] << 4) | (b[2] >> 2);
	o[2] = (b[2] << 6) | b[3];
        i = a[2] == '=' ? 1 : (a[3] == '=' ? 2 : 3);
	if (i>=1) dest[dpos++] = o[0];
	if (i>=2) dest[dpos++] = o[1];
	if (i>=3) dest[dpos++] = o[2];
	dest[dpos] = 0;
	if (i < 3) {
	    return;
	}
    }
}





/*
 * Pack all session info into one easy-to-digest cookie.  Healthy and delicious!
 */
void stuff_to_cookie(char *cookie, int session, char *user, char *pass, char *room)
{
	char buf[256];

	sprintf(buf, "%d|%s|%s|%s", session, user, pass, room);
	encode_base64(cookie, buf);
}


/*
 * Extract all that fun stuff out of the cookie.
 */
void cookie_to_stuff(char *cookie, int *session, char *user, char *pass, char *room)
{
	char buf[256];

	decode_base64(buf, cookie);

	if (session != NULL)
		*session = extract_int(buf, 0);
	if (user != NULL)
		extract(user, buf, 1);
	if (pass != NULL)
		extract(pass, buf, 2);
	if (room != NULL)
		extract(room, buf, 3);
}
