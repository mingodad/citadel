/*
 * tcp_sockets.c
 * 
 * TCP socket module for WebCit
 *
 * $Id$
 */

#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>
#include <stdarg.h>
#include "webcit.h"

char server_is_local = 0;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

extern int errno;

RETSIGTYPE timeout(int signum)
{
	fprintf(stderr, "Connection timed out.\n");
	exit(3);
}

int connectsock(char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;

	bzero((char *) &sin, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		fprintf(stderr, "Can't get %s service entry\n", service);
		return (-1);
	}
	phe = gethostbyname(host);
	if (phe) {
		bcopy(phe->h_addr, (char *) &sin.sin_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		return (-1);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		fprintf(stderr, "Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		return (-1);
	}
	if (!strcmp(protocol, "udp"))
		type = SOCK_DGRAM;
	else
		type = SOCK_STREAM;

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		fprintf(stderr, "Can't create socket: %s\n", strerror(errno));
		return (-1);
	}
	signal(SIGALRM, timeout);
	alarm(30);

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		fprintf(stderr, "can't connect to %s.%s: %s\n",
			host, service, strerror(errno));
		return (-1);
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);

	return (s);
}




/*
 * Input binary data from socket
 */
void serv_read(char *buf, int bytes)
{
	int len, rlen;

	len = 0;
	while (len < bytes) {
		rlen = read(WC->serv_sock, &buf[len], bytes - len);
		if (rlen < 1) {
			fprintf(stderr, "Server connection broken: %s\n",
				strerror(errno));
			WC->connected = 0;
			WC->logged_in = 0;
			return;
		}
		len = len + rlen;
	}
}


/*
 * input string from pipe
 */
void serv_gets(char *strbuf)
{
	int ch, len;
	char buf[2];

	len = 0;
	strcpy(strbuf, "");
	do {
		serv_read(&buf[0], 1);
		ch = buf[0];
		strbuf[len++] = ch;
	} while ((ch != 10) && (ch != 13) && (ch != 0) && (len < 255));
	strbuf[len - 1] = 0;
	/* fprintf(stderr, ">%s\n", strbuf); */
}



/*
 * send binary to server
 */
void serv_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(WC->serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			fprintf(stderr, "Server connection broken: %s\n",
				strerror(errno));
			WC->connected = 0;
			WC->logged_in = 0;
			return;
		}
		bytes_written = bytes_written + retval;
	}
}


/*
 * send line to server
 */
void serv_puts(char *string)
{
	char buf[256];

	sprintf(buf, "%s\n", string);
	serv_write(buf, strlen(buf));
}


/*
 * convenience function to send stuff to the server
 */
void serv_printf(const char *format,...)
{
	va_list arg_ptr;
	char buf[256];

	va_start(arg_ptr, format);
	vsprintf(buf, format, arg_ptr);
	va_end(arg_ptr);

	strcat(buf, "\n");
	serv_write(buf, strlen(buf));
	/* fprintf(stderr, "<%s", buf); */
}
