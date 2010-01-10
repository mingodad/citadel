/*
 * $Id$
 */

#include "webcit.h"

/*
 * String to unset the cookie.
 * Any date "in the past" will work, so I chose my birthday, right down to
 * the exact minute.  :)
 */
static char *unset = "; expires=28-May-1971 18:10:00 GMT";
typedef unsigned char byte;	      /* Byte type used by cookie_to_stuff() */

/*
 * Pack all session info into one easy-to-digest cookie. Healthy and delicious!
 */
void stuff_to_cookie(int unset_cookies)
{
	wcsession *WCC = WC;
	char buf[SIZ];

	if (unset_cookies) {
		hprintf("Set-cookie: webcit=%s; path=/\r\n", unset);
	}
	else
	{
		StrBufAppendPrintf(WCC->HBuf, "Set-cookie: webcit=");
		snprintf(buf, sizeof(buf), "%d", WCC->wc_session);
		StrBufHexescAppend(WCC->HBuf, NULL, buf);
		StrBufHexescAppend(WCC->HBuf, NULL, "|");
		StrBufHexescAppend(WCC->HBuf, WCC->wc_username, NULL);
		StrBufHexescAppend(WCC->HBuf, NULL, "|");
		StrBufHexescAppend(WCC->HBuf, WCC->wc_password, NULL);
		StrBufHexescAppend(WCC->HBuf, NULL, "|");
		StrBufHexescAppend(WCC->HBuf, WCC->wc_roomname, NULL);
		StrBufHexescAppend(WCC->HBuf, NULL, "|");
		StrBufHexescAppend(WCC->HBuf, NULL, get_selected_language());
		StrBufHexescAppend(WCC->HBuf, NULL, "|");

		if (server_cookie != NULL) {
			StrBufAppendPrintf(WCC->HBuf, 
					   ";path=/ \r\n%s\r\n", 
					   server_cookie);
		}
		else {
			StrBufAppendBufPlain(WCC->HBuf,
					     HKEY("; path=/\r\n"), 0);
		}
	}
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
