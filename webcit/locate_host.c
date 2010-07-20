/*
 * $Id$
 *
 * Given a socket, supply the name of the host at the other end.
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "webcit.h"


/*
 * IPv6 enabled locate_host()
 */
void locate_host(StrBuf *tbuf, int client_socket)
{
	struct sockaddr_in6 clientaddr;
	unsigned int addrlen = sizeof(clientaddr);
	char clienthost[NI_MAXHOST];
	char clientservice[NI_MAXSERV];

	getpeername(client_socket, (struct sockaddr *)&clientaddr, &addrlen);
	getnameinfo((struct sockaddr *)&clientaddr, addrlen,
		clienthost, sizeof(clienthost),
		clientservice, sizeof(clientservice),
		0
	);

        StrBufAppendBufPlain(tbuf, clienthost, -1, 0);
}


#if 0
/*
 * IPv4-only locate_host()
 */
void locate_host(StrBuf *tbuf, int client_socket)
{
	struct sockaddr_in cs;
	struct hostent *ch;
	socklen_t len;
	char *i;
	int a1, a2, a3, a4;

	len = sizeof(cs);
	if (getpeername(client_socket, (struct sockaddr *) &cs, &len) < 0) {
		StrBufAppendBufPlain(tbuf, HKEY("<unknown>"), 0);
		return;
	}
	if ((ch = gethostbyaddr((char *) &cs.sin_addr, sizeof(cs.sin_addr),
				AF_INET)) == NULL) {
		i = (char *) &cs.sin_addr;
		a1 = ((*i++) & 0xff);
		a2 = ((*i++) & 0xff);
		a3 = ((*i++) & 0xff);
		a4 = ((*i++) & 0xff);
		StrBufPrintf(tbuf, "%d.%d.%d.%d", a1, a2, a3, a4);
		return;
	}
	StrBufAppendBufPlain(tbuf, ch->h_name, -1, 0);
}
#endif
