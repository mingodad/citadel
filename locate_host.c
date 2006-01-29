/*
 * $Id$
 */
/**
 * \defgroup Hostlookup Examine a socket and determine the name/address of the originating host.
 * \ingroup WebcitHttpServer
 */
/*@{*/

#include "webcit.h"

/**
 * \brief get a hostname 
 * \todo buffersize?
 * \param tbuf the returnbuffer
 * \param client_socket the sock fd where the client is connected
 */
void locate_host(char *tbuf, int client_socket)
{
	struct sockaddr_in cs;
	struct hostent *ch;
	socklen_t len;
	char *i;
	int a1, a2, a3, a4;

	len = sizeof(cs);
	if (getpeername(client_socket, (struct sockaddr *) &cs, &len) < 0) {
		strcpy(tbuf, "<unknown>");
		return;
	}
	if ((ch = gethostbyaddr((char *) &cs.sin_addr, sizeof(cs.sin_addr),
				AF_INET)) == NULL) {
		i = (char *) &cs.sin_addr;
		a1 = ((*i++) & 0xff);
		a2 = ((*i++) & 0xff);
		a3 = ((*i++) & 0xff);
		a4 = ((*i++) & 0xff);
		sprintf(tbuf, "%d.%d.%d.%d", a1, a2, a3, a4);
		return;
	}
	safestrncpy(tbuf, ch->h_name, 64);
}

/*@}*/
