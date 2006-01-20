/*
 * $Id$
 */
/**
 * \defgroup CookieConversion Grep Cookies
 * Utility functions which convert the HTTP cookie format we use to and
 * from user/password/room strings.
 *
 */
/*@{*/
#include "webcit.h"


#define TRUE  1    /**< for sure? */
#define FALSE 0    /**< nope. */

typedef unsigned char byte;	      /**< Byte type */

/**
 * \brief find cookie
 * Pack all session info into one easy-to-digest cookie. Healthy and delicious!
 * \param cookie cookie string to create???
 * \param session the session we want to convert into a cookie
 * \param user the user to be associated with the cookie
 * \param pass his passphrase
 * \param room the room he wants to enter
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

/**
 * \brief some bytefoo ????
 * \param in the string to chop
 * \param len the length of the string
 * \return the corrosponding integer value
 */
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

/**
 * \brief Extract all that fun stuff out of the cookie.
 * \param cookie the cookie string
 * \param session the corrosponding session to return
 * \param user the user string
 * \param user_len the user stringlength
 * \param pass the passphrase
 * \param pass_len length of the passphrase string 
 * \param room the room he is in
 * \param room_len the length of the room string
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
/*@}*/
