/*
 * Given a socket, supply the name of the host at the other end.
 *
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
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
