/*
 * $Id$
 */

#include "webcit.h"

typedef unsigned char byte;	      /* Byte type used by cookie_to_stuff() */

/*
 * Pack all session info into one easy-to-digest cookie. Healthy and delicious!
 */
void stuff_to_cookie(char *cookie, size_t clen, int session,
		StrBuf *user, StrBuf *pass, StrBuf *room, const char *language)
{
	char buf[SIZ];
	int i;
	int len;

	len = snprintf(buf, SIZ, "%d|%s|%s|%s|%s|", 
		session, 
		ChrPtr(user), 
		ChrPtr(pass), 
		ChrPtr(room),
		language
	);

	strcpy(cookie, "");
	for (i=0; (i < len) && (i * 2 < clen); ++i) {
		snprintf(&cookie[i*2], clen - i * 2, "%02X", buf[i]);
	}
}

/*
 * Convert unpacked hex string to an integer
 */
int xtoi(const char *in, size_t len)
{
	int val = 0;
	char c = 0;
	while (!IsEmptyStr(in) && isxdigit((byte) *in) && (len-- > 0))
	{
		c = *in++;
		val <<= 4;
		if (!isdigit((unsigned char)c)) {
			c = tolower((unsigned char) c);
			if ((c < 'a') || (c > 'f'))
				return 0;
			val += c  - 'a' + 10 ;
		}
		else
			val += c - '0';
	}
	return val;
}

/*
 * Extract all that fun stuff out of the cookie.
 */
void cookie_to_stuff(StrBuf *cookie,
			int *session,
			StrBuf *user,
			StrBuf *pass,
			StrBuf *room,
			StrBuf *language)
{
	if (session != NULL) {
		*session = StrBufExtract_int(cookie, 0, '|');
	}
	if (user != NULL) {
		StrBufExtract_token(user, cookie, 1, '|');
	}
	if (pass != NULL) {
		StrBufExtract_token(pass, cookie, 2, '|');
	}
	if (room != NULL) {
		StrBufExtract_token(room, cookie, 3, '|');
	}
	if (language != NULL) {
		StrBufExtract_token(language, cookie, 4, '|');
	}
}
