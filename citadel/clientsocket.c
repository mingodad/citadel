/*
 * This module handles client-side sockets opened by the Citadel server (for
 * the client side of Internet protocols, etc.)   It does _not_ handle client
 * sockets for the Citadel client; for that you must look in ipc_c_tcp.c
 * (which, uncoincidentally, bears a striking similarity to this file).
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <libcitadel.h>
#include "ctdl_module.h"
#include "clientsocket.h"

int sock_connect(char *host, char *service)
{
	struct in6_addr serveraddr;
	struct addrinfo hints;
	struct addrinfo *res = NULL;
	struct addrinfo *ai = NULL;
	int rc = (-1);
	int sock = (-1);

	if ((host == NULL) || IsEmptyStr(host))
		return (-1);
	if ((service == NULL) || IsEmptyStr(service))
		return (-1);

	memset(&hints, 0x00, sizeof(hints));
	hints.ai_flags = AI_NUMERICSERV;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	/*
	 * Handle numeric IPv4 and IPv6 addresses
	 */
	rc = inet_pton(AF_INET, host, &serveraddr);
	if (rc == 1) {						/* dotted quad */
		hints.ai_family = AF_INET;
		hints.ai_flags |= AI_NUMERICHOST;
	} else {
		rc = inet_pton(AF_INET6, host, &serveraddr);
		if (rc == 1) {					/* IPv6 address */
			hints.ai_family = AF_INET6;
			hints.ai_flags |= AI_NUMERICHOST;
		}
	}

	/* Begin the connection process */

	rc = getaddrinfo(host, service, &hints, &res);
	if (rc != 0) {
		syslog(LOG_ERR, "%s: %s", host, gai_strerror(rc));
		return(-1);
	}

	/*
	 * Try all available addresses until we connect to one or until we run out.
	 */
	for (ai = res; ai != NULL; ai = ai->ai_next) {
		sock = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
		if (sock < 0) {
			syslog(LOG_ERR, "socket() failed: %s", strerror(errno));
			freeaddrinfo(res);
			return(-1);
		}
		rc = connect(sock, ai->ai_addr, ai->ai_addrlen);
		if (rc >= 0) {
                        freeaddrinfo(res);
			return(sock);
		}
		else {
			syslog(LOG_ERR, "connect() failed: %s", strerror(errno));
			close(sock);
		}
	}
	freeaddrinfo(res);
	return(-1);
}



/*
 * Read data from the client socket.
 *
 * sock		socket fd to read from
 * buf		buffer to read into 
 * bytes	number of bytes to read
 * timeout	Number of seconds to wait before timing out
 *
 * Possible return values:
 *      1       Requested number of bytes has been read.
 *      0       Request timed out.
 *	-1   	Connection is broken, or other error.
 */
int socket_read_blob(int *Socket, StrBuf * Target, int bytes, int timeout)
{
	CitContext *CCC = MyContext();
	const char *Error;
	int retval = 0;


	retval = StrBufReadBLOBBuffered(Target,
					CCC->SBuf.Buf,
					&CCC->SBuf.ReadWritePointer,
					Socket, 1, bytes, O_TERM, &Error);
	
	if (retval < 0) {
		syslog(LOG_CRIT, "socket_read_blob() failed: %s", Error);
	}
	return retval;
}


int CtdlSockGetLine(int *sock, StrBuf * Target, int nSec)
{
	CitContext *CCC = MyContext();
	const char *Error;
	int rc;

	FlushStrBuf(Target);
	rc = StrBufTCP_read_buffered_line_fast(Target,
					       CCC->SBuf.Buf,
					       &CCC->SBuf.ReadWritePointer,
					       sock, nSec, 1, &Error);
	if ((rc < 0) && (Error != NULL))
		syslog(LOG_CRIT, "CtdlSockGetLine() failed: %s", Error);
	return rc;
}


/*
 * client_getln()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int sock_getln(int *sock, char *buf, int bufsize)
{
	int i, retval;
	CitContext *CCC = MyContext();
	const char *pCh;

	FlushStrBuf(CCC->sMigrateBuf);
	retval = CtdlSockGetLine(sock, CCC->sMigrateBuf, 5);

	i = StrLength(CCC->sMigrateBuf);
	pCh = ChrPtr(CCC->sMigrateBuf);

	memcpy(buf, pCh, i + 1);

	FlushStrBuf(CCC->sMigrateBuf);
	if (retval < 0) {
		safestrncpy(&buf[i], "000", bufsize - i);
		i += 3;
	}
	return i;
}


/*
 * sock_write() - send binary to server.
 * Returns the number of bytes written, or -1 for error.
 */
int sock_write(int *sock, const char *buf, int nbytes) 
{ return sock_write_timeout(sock, buf, nbytes, 50); }
int sock_write_timeout(int *sock, const char *buf, int nbytes, int timeout)
{
	int nSuccessLess = 0;
	int bytes_written = 0;
	int retval;
	fd_set rfds;
        int fdflags;
	int IsNonBlock;
	struct timeval tv;
	int selectresolution = 100;

	fdflags = fcntl(*sock, F_GETFL);
	IsNonBlock = (fdflags & O_NONBLOCK) == O_NONBLOCK;

	while ((nSuccessLess < timeout) && 
	       (*sock != -1) && 
	       (bytes_written < nbytes)) 
	{
		if (IsNonBlock){
			tv.tv_sec = selectresolution;
			tv.tv_usec = 0;
			
			FD_ZERO(&rfds);
			FD_SET(*sock, &rfds);
			if (select(*sock + 1, NULL, &rfds, NULL, &tv) == -1) {
///				*Error = strerror(errno);
				close (*sock);
				*sock = -1;
				return -1;
			}
		}
		if (IsNonBlock && !  FD_ISSET(*sock, &rfds)) {
			nSuccessLess ++;
			continue;
		}
		retval = write(*sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			sock_close(*sock);
			*sock = -1;
			return (-1);
		}
		bytes_written = bytes_written + retval;
		if (IsNonBlock && (bytes_written == nbytes)){
			tv.tv_sec = selectresolution;
			tv.tv_usec = 0;
			
			FD_ZERO(&rfds);
			FD_SET(*sock, &rfds);
			if (select(*sock + 1, NULL, &rfds, NULL, &tv) == -1) {
///				*Error = strerror(errno);
				close (*sock);
				*sock = -1;
				return -1;
			}
		}
	}
	return (bytes_written);
}



/*
 * client_getln()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int sock_getln_err(int *sock, char *buf, int bufsize, int *rc, int nSec)
{
	int i, retval;
	CitContext *CCC = MyContext();
	const char *pCh;

	FlushStrBuf(CCC->sMigrateBuf);
	*rc = retval = CtdlSockGetLine(sock, CCC->sMigrateBuf, nSec);

	i = StrLength(CCC->sMigrateBuf);
	pCh = ChrPtr(CCC->sMigrateBuf);

	memcpy(buf, pCh, i + 1);

	FlushStrBuf(CCC->sMigrateBuf);
	if (retval < 0) {
		safestrncpy(&buf[i], "000", bufsize - i);
		i += 3;
	}
	return i;
}

/*
 * Multiline version of sock_gets() ... this is a convenience function for
 * client side protocol implementations.  It only returns the first line of
 * a multiline response, discarding the rest.
 */
int ml_sock_gets(int *sock, char *buf, int nSec)
{
	int rc = 0;
	char bigbuf[1024];
	int g;

	g = sock_getln_err(sock, buf, SIZ, &rc, nSec);
	if (rc < 0)
		return rc;
	if (g < 4)
		return (g);
	if (buf[3] != '-')
		return (g);

	do {
		g = sock_getln_err(sock, bigbuf, SIZ, &rc, nSec);
		if (rc < 0)
			return rc;
		if (g < 0)
			return (g);
	} while ((g >= 4) && (bigbuf[3] == '-'));

	return (strlen(buf));
}


/*
 * sock_puts() - send line to server - implemented in terms of serv_write()
 * Returns the number of bytes written, or -1 for error.
 */
int sock_puts(int *sock, char *buf)
{
	int i, j;

	i = sock_write(sock, buf, strlen(buf));
	if (i < 0)
		return (i);
	j = sock_write(sock, "\n", 1);
	if (j < 0)
		return (j);
	return (i + j);
}
