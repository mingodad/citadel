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
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
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

void connection_died(void) {
	fprintf(stderr, "\r"
			"Your connection to this Citadel server is broken.\n"
			"Please re-connect and log in again.\n");
	logoff(3);
}


void timeout(int signum)
{
	printf("\rConnection timed out.\n");
	logoff(3);
}


int connectsock(char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		fprintf(stderr, "Can't get %s service entry: %s\n",
			service, strerror(errno));
		logoff(3);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		logoff(3);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		fprintf(stderr, "Can't get %s protocol entry: %s\n",
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
		fprintf(stderr, "Can't create socket: %s\n", strerror(errno));
		logoff(3);
	}
	signal(SIGALRM, timeout);
	alarm(30);

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		fprintf(stderr, "can't connect to %s.%s: %s\n",
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
		fprintf(stderr, "Can't create socket: %s\n",
			strerror(errno));
		logoff(3);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, "can't connect: %s\n",
			strerror(errno));
		logoff(3);
	}

	server_is_local = 1;
	return s;
}

/*
 * convert service and host entries into a six-byte numeric in the format
 * expected by a SOCKS v4 server
 */
void numericize(char *buf, char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct sockaddr_in sin;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		fprintf(stderr, "Can't get %s service entry: %s\n",
			service, strerror(errno));
		logoff(3);
	}
	buf[1] = (((sin.sin_port) & 0xFF00) >> 8);
	buf[0] = ((sin.sin_port) & 0x00FF);

	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		logoff(3);
	}
	buf[5] = ((sin.sin_addr.s_addr) & 0xFF000000) >> 24;
	buf[4] = ((sin.sin_addr.s_addr) & 0x00FF0000) >> 16;
	buf[3] = ((sin.sin_addr.s_addr) & 0x0000FF00) >> 8;
	buf[2] = ((sin.sin_addr.s_addr) & 0x000000FF);
}

/*
 * input binary data from socket
 */
void serv_read(char *buf, int bytes)
{
	int len, rlen;

	len = 0;
	while (len < bytes) {
		rlen = read(serv_sock, &buf[len], bytes - len);
		if (rlen < 1) {
			connection_died();
			return;
		}
		len = len + rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(serv_sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			connection_died();
			return;
		}
		bytes_written = bytes_written + retval;
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
	/* printf("< %s\n", buf); */
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
	int host_copied = 0;
	char citport[SIZ];
	int port_copied = 0;
	char socks4[SIZ];
	char buf[SIZ];
	struct passwd *p;
	char sockpath[SIZ];

	strcpy(cithost, DEFAULT_HOST);	/* default host */
	strcpy(citport, DEFAULT_PORT);	/* default port */
	strcpy(socks4, "");	/* SOCKS v4 server */


	for (a = 0; a < argc; ++a) {
		if (a == 0) {
			/* do nothing */
		} else if (!strcmp(argv[a], "-s")) {
			strcpy(socks4, argv[++a]);
		} else if (host_copied == 0) {
			host_copied = 1;
			strcpy(cithost, argv[a]);
		} else if (port_copied == 0) {
			port_copied = 1;
			strcpy(citport, argv[a]);
		}
/*
   else {
   fprintf(stderr,"%s: usage: ",argv[0]);
   fprintf(stderr,"%s [host] [port] ",argv[0]);
   fprintf(stderr,"[-s socks_server]\n");
   logoff(2);
   }
 */
	}

	if ((!strcmp(cithost, "localhost"))
	    || (!strcmp(cithost, "127.0.0.1")))
		server_is_local = 1;

	/* If we're using a unix domain socket we can do a bunch of stuff */
	if (!strcmp(cithost, UDS)) {
		sprintf(sockpath, "citadel.socket");
		serv_sock = uds_connectsock(sockpath);
		if (hostbuf != NULL) strcpy(hostbuf, cithost);
		if (portbuf != NULL) strcpy(portbuf, sockpath);
		return;
	}

	/* if not using a SOCKS proxy server, make the connection directly */
	if (strlen(socks4) == 0) {
		serv_sock = connectsock(cithost, citport, "tcp");
		if (hostbuf != NULL) strcpy(hostbuf, cithost);
		if (portbuf != NULL) strcpy(portbuf, citport);
		return;
	}
	/* if using SOCKS, connect first to the proxy... */
	serv_sock = connectsock(socks4, "1080", "tcp");
	printf("Connected to SOCKS proxy at %s.\n", socks4);
	printf("Attaching to server...\r");
	fflush(stdout);

	snprintf(buf, sizeof buf, "%c%c",
		 4,		/* version 4 */
		 1);		/* method = connect */
	serv_write(buf, 2);

	numericize(buf, cithost, citport, "tcp");
	serv_write(buf, 6);	/* port and address */

	p = (struct passwd *) getpwuid(getuid());
	serv_write(p->pw_name, strlen(p->pw_name) + 1);
	/* user name */

	serv_read(buf, 8);	/* get response */

	if (buf[1] != 90) {
		printf("SOCKS server denied this proxy request.\n");
		logoff(3);
	}
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

