/*
 * $Id$
 * 
 * Client-side IPC functions
 *
 */

#define	UDS			"_UDS_"

#define DEFAULT_HOST		UDS
#define DEFAULT_PORT		"citadel"


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include "citadel.h"
#include "citadel_decls.h"
#include "ipc.h"
#include "tools.h"
#if defined(HAVE_OPENSSL) && defined(CIT_CLIENT)
#include "client_crypto.h"
#endif
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
#ifdef CIT_CLIENT
#include "screen.h"
#else
extern int err_printf(char *fmt, ...);
#endif

/*
 * If server_is_local is set to nonzero, the client assumes that it is running
 * on the same computer as the server.  Several things happen when this is
 * the case, including the ability to map a specific tty to a particular login
 * session in the "<W>ho is online" listing, the ability to run external
 * programs, and the ability to download files directly off the disk without
 * having to first fetch them from the server.
 * Set the flag to 1 if this IPC is local (as is the case with pipes, or a
 * network session to the local machine) or 0 if the server is executing on
 * a remote computer.
 */
int server_is_local = 0;

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

int serv_sock;

#if defined(HAVE_OPENSSL) && defined(CIT_CLIENT)
extern int ssl_is_connected;
#endif


void connection_died(void) {
#ifdef CIT_CLIENT
	screen_delete();
#endif
	err_printf("\rYour connection to this Citadel server is broken.\n"
			"Please re-connect and log in again.\n");
	logoff(3);
}


void timeout(int signum)
{
	err_printf("\rConnection timed out.\n");
	logoff(3);
}


static int connectsock(char *host, char *service, char *protocol, int defaultPort)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse != NULL) {
		sin.sin_port = pse->s_port;
	}
	else if (atoi(service) > 0) {
		sin.sin_port = htons(atoi(service));
	}
	else {
		sin.sin_port = htons(defaultPort);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		err_printf("Can't get %s host entry: %s\n",
			host, strerror(errno));
		logoff(3);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		err_printf("Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		logoff(3);
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		err_printf("Can't create socket: %s\n", strerror(errno));
		logoff(3);
	}
	signal(SIGALRM, timeout);
	alarm(30);

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		err_printf("can't connect to %s.%s: %s\n",
			host, service, strerror(errno));
		logoff(3);
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);

	return (s);
}

int uds_connectsock(char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		err_printf("Can't create socket: %s\n", strerror(errno));
		logoff(3);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		err_printf("can't connect: %s\n", strerror(errno));
		logoff(3);
	}

	server_is_local = 1;
	return s;
}


/*
 * input binary data from socket
 */
void serv_read(char *buf, int bytes)
{
	int len, rlen;

#if defined(HAVE_OPENSSL) && defined(CIT_CLIENT)
	if (ssl_is_connected) {
		serv_read_ssl(buf, bytes);
		return;
	}
#endif
	len = 0;
	while (len < bytes) {
		rlen = read(serv_sock, &buf[len], bytes - len);
		if (rlen < 1) {
			connection_died();
			return;
		}
		len += rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;

#if defined(HAVE_OPENSSL) && defined(CIT_CLIENT)
	if (ssl_is_connected) {
		serv_write_ssl(buf, nbytes);
		return;
	}
#endif
	while (bytes_written < nbytes) {
		retval = write(serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			connection_died();
			return;
		}
		bytes_written += retval;
	}
}



/*
 * input string from socket - implemented in terms of serv_read()
 */
void serv_gets(char *buf)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		serv_read(&buf[i], 1);
		if (buf[i] == '\n' || i == (SIZ-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (SIZ-1))
		while (buf[i] != '\n')
			serv_read(&buf[i], 1);

	/* Strip the trailing newline.
	 */
	buf[i] = 0;
}


/*
 * send line to server - implemented in terms of serv_write()
 */
void serv_puts(char *buf)
{
	/* err_printf("< %s\n", buf); */
	serv_write(buf, strlen(buf));
	serv_write("\n", 1);
}


/*
 * attach to server
 */
void attach_to_server(int argc, char **argv, char *hostbuf, char *portbuf)
{
	int a;
	char cithost[SIZ];
	char citport[SIZ];
	char sockpath[SIZ];

	strcpy(cithost, DEFAULT_HOST);	/* default host */
	strcpy(citport, DEFAULT_PORT);	/* default port */

	for (a = 0; a < argc; ++a) {
		if (a == 0) {
			/* do nothing */
		} else if (a == 1) {
			strcpy(cithost, argv[a]);
		} else if (a == 2) {
			strcpy(citport, argv[a]);
		}
   		else {
			err_printf("%s: usage: ",argv[0]);
			err_printf("%s [host] [port] ",argv[0]);
			logoff(2);
   		}
	}

	if ((!strcmp(cithost, "localhost"))
	   || (!strcmp(cithost, "127.0.0.1"))) {
		server_is_local = 1;
	}

	/* If we're using a unix domain socket we can do a bunch of stuff */
	if (!strcmp(cithost, UDS)) {
		sprintf(sockpath, "citadel.socket");
		serv_sock = uds_connectsock(sockpath);
		if (hostbuf != NULL) strcpy(hostbuf, cithost);
		if (portbuf != NULL) strcpy(portbuf, sockpath);
		return;
	}

	serv_sock = connectsock(cithost, citport, "tcp", 504);
	if (hostbuf != NULL) strcpy(hostbuf, cithost);
	if (portbuf != NULL) strcpy(portbuf, citport);
	return;
}

/*
 * return the file descriptor of the server socket so we can select() on it.
 */
int getsockfd(void)
{
	return serv_sock;
}


/*
 * return one character
 */
char serv_getc(void)
{
	char buf[2];
	char ch;

	serv_read(buf, 1);
	ch = (int) buf[0];

	return (ch);
}
