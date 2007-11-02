/*
 * $Id$
 *
 * This module handles client-side sockets opened by the Citadel server (for
 * the client side of Internet protocols, etc.)   It does _not_ handle client
 * sockets for the Citadel client; for that you must look in ipc_c_tcp.c
 * (which, uncoincidentally, bears a striking similarity to this file).
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include "citadel.h"
#include "server.h"
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#include "sysdep_decls.h"
#include "config.h"
#include "clientsocket.h"
#include "tools.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

int sock_connect(char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	struct sockaddr_in egress_sin;
	int s, type;

	if ((host == NULL) || IsEmptyStr(host)) 
		return(-1);
	if ((service == NULL) || IsEmptyStr(service)) 
		return(-1);
	if ((protocol == NULL) || IsEmptyStr(protocol)) 
		return(-1);

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		lprintf(CTDL_CRIT, "Can't get %s service entry: %s\n",
			service, strerror(errno));
		return(-1);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		lprintf(CTDL_ERR, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		return(-1);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		lprintf(CTDL_CRIT, "Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		return(-1);
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		lprintf(CTDL_CRIT, "Can't create socket: %s\n", strerror(errno));
		return(-1);
	}

	/* If citserver is bound to a specific IP address on the host, make
	 * sure we use that address for outbound connections.
	 */
	memset(&egress_sin, 0, sizeof(egress_sin));
	egress_sin.sin_family = AF_INET;
	if (!IsEmptyStr(config.c_ip_addr)) {
		egress_sin.sin_addr.s_addr = inet_addr(config.c_ip_addr);
        	if (egress_sin.sin_addr.s_addr == !INADDR_ANY) {
                	egress_sin.sin_addr.s_addr = INADDR_ANY;
		}

		/* If this bind fails, no problem; we can still use INADDR_ANY */
		bind(s, (struct sockaddr *)&egress_sin, sizeof(egress_sin));
        }

	/* Now try to connect to the remote host. */
	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(CTDL_ERR, "Can't connect to %s:%s: %s\n",
			host, service, strerror(errno));
		close(s);
		return(-1);
	}

	return (s);
}



/*
 * sock_read_to() - input binary data from socket, with a settable timeout.
 * Returns the number of bytes read, or -1 for error.
 * If keep_reading_until_full is nonzero, we keep reading until we get the number of requested bytes
 */
int sock_read_to(int sock, char *buf, int bytes, int timeout, int keep_reading_until_full)
{
	int len,rlen;
	fd_set rfds;
	struct timeval tv;
	int retval;

	len = 0;
	while (len<bytes) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select(sock+1, &rfds, NULL, NULL, &tv);

		if (FD_ISSET(sock, &rfds) == 0) {	/* timed out */
			lprintf(CTDL_ERR, "sock_read() timed out.\n");
			return(-1);
		}

		rlen = read(sock, &buf[len], bytes-len);
		if (rlen<1) {
			lprintf(CTDL_ERR, "sock_read() failed: %s\n",
				strerror(errno));
			return(-1);
		}
		len = len + rlen;
		if (!keep_reading_until_full) return(len);
	}
	return(bytes);
}


/*
 * sock_read() - input binary data from socket.
 * Returns the number of bytes read, or -1 for error.
 */
INLINE int sock_read(int sock, char *buf, int bytes, int keep_reading_until_full)
{
	return sock_read_to(sock, buf, bytes, CLIENT_TIMEOUT, keep_reading_until_full);
}


/*
 * sock_write() - send binary to server.
 * Returns the number of bytes written, or -1 for error.
 */
int sock_write(int sock, char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			return (-1);
		}
		bytes_written = bytes_written + retval;
	}
	return (bytes_written);
}



/*
 * Input string from socket - implemented in terms of sock_read()
 * 
 */
int sock_getln(int sock, char *buf, int bufsize)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		if (sock_read(sock, &buf[i], 1, 1) < 0) return(-1);
		if (buf[i] == '\n' || i == (bufsize-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (bufsize-1))
		while (buf[i] != '\n')
			if (sock_read(sock, &buf[i], 1, 1) < 0) return(-1);

	/* Strip any trailing CR and LF characters.
	 */
	buf[i] = 0;
	while ( (i > 0)
		&& ( (buf[i - 1]==13)
		     || ( buf[i - 1]==10)) ) {
		i--;
		buf[i] = 0;
	}
	return(i);
}

/*
 * Multiline version of sock_gets() ... this is a convenience function for
 * client side protocol implementations.  It only returns the first line of
 * a multiline response, discarding the rest.
 */
int ml_sock_gets(int sock, char *buf) {
	char bigbuf[1024];
	int g;

	g = sock_getln(sock, buf, SIZ);
	if (g < 4) return(g);
	if (buf[3] != '-') return(g);

	do {
		g = sock_getln(sock, bigbuf, SIZ);
		if (g < 0) return(g);
	} while ( (g >= 4) && (bigbuf[3] == '-') );

	return(strlen(buf));
}


/*
 * sock_puts() - send line to server - implemented in terms of serv_write()
 * Returns the number of bytes written, or -1 for error.
 */
int sock_puts(int sock, char *buf)
{
	int i, j;

	i = sock_write(sock, buf, strlen(buf));
	if (i<0) return(i);
	j = sock_write(sock, "\n", 1);
	if (j<0) return(j);
	return(i+j);
}
