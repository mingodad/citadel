/*
 * Given a socket, supply the name of the host at the other end.
 *
 * Copyright (c) 1996-2010 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
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
#include "webserver.h"

/*
 * IPv4/IPv6 locate_host()
 */
void locate_host(StrBuf *tbuf, int client_socket)
{
	struct sockaddr_in6 clientaddr;
	unsigned int addrlen = sizeof(clientaddr);
	char clienthost[NI_MAXHOST];

	getpeername(client_socket, (struct sockaddr *)&clientaddr, &addrlen);
	getnameinfo((struct sockaddr *)&clientaddr, addrlen, clienthost, sizeof(clienthost), NULL, 0, 0);
        StrBufAppendBufPlain(tbuf, clienthost, -1, 0);
	syslog(9, "Client is at %s\n", clienthost);
}
