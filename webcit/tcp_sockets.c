/*
 * tcp_sockets.c
 * 
 * TCP socket module for WebCit
 *
 * $Id$
 */


#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>
#include "webcit.h"

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

RETSIGTYPE timeout(int signum)
{
	fprintf(stderr, "Connection timed out.\n");
	exit(3);
}



/*
 * Connect a unix domain socket
 */
int uds_connectsock(char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "Can't create socket: %s\n",
			strerror(errno));
		return(-1);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, "can't connect: %s\n",
			strerror(errno));
		return(-1);
	}

	return s;
}


/*
 * Connect a TCP/IP socket
 */
int tcp_connectsock(char *host, char *service)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, "tcp");
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		fprintf(stderr, "Can't get %s service entry\n", service);
		return (-1);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		return (-1);
	}
	if ((ppe = getprotobyname("tcp")) == 0) {
		fprintf(stderr, "Can't get TCP protocol entry: %s\n",
			strerror(errno));
		return (-1);
	}

	s = socket(PF_INET, SOCK_STREAM, ppe->p_proto);
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
