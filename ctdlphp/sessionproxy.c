/*
 * $Id$
 *
 * Session proxy for Citadel PHP bindings
 *
 * This is an unfinished session proxy ... it is a C version of the
 * session proxy implemented in sessionproxy.php ... that version is pure PHP
 * so we should probably stick with it as long as it works.
 *
 * Copyright (c) 2003 by Art Cancro <ajc@uncensored.citadel.org>
 * This program is released under the terms of the GNU General Public License.
 */

#define SIZ 4096

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


#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif


void timeout(int signum)
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
		fprintf(stderr, "Can't connect: %s\n",
			strerror(errno));
		close(s);
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
		fprintf(stderr, "Can't connect to %s.%s: %s\n",
			host, service, strerror(errno));
		close(s);
		return (-1);
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);

	return (s);
}




/*
 * Input binary data from socket
 */
int sock_read(int sock, char *buf, int bytes)
{
	int len, rlen;

	len = 0;
	while (len < bytes) {
		rlen = read(sock, &buf[len], bytes - len);
		if (rlen < 1) {
			fprintf(stderr, "Server connection broken: %s\n",
				strerror(errno));
			return(-1);
		}
		len = len + rlen;
	}
	return(len);
}


/*
 * input string from pipe
 */
int sock_gets(int sock, char *strbuf)
{
	int ch, len;
	char buf[2];

	len = 0;
	strcpy(strbuf, "");
	do {
		if (sock_read(sock, &buf[0], 1) < 0) return(-1);
		ch = buf[0];
		strbuf[len++] = ch;
	} while ((ch != 10) && (ch != 0) && (len < (SIZ-1)));
	if (strbuf[len-1] == 10) strbuf[--len] = 0;
	if (strbuf[len-1] == 13) strbuf[--len] = 0;
	return(len);
}



/*
 * send binary to server
 */
int sock_write(int sock, char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			fprintf(stderr, "Server connection broken: %s\n",
				strerror(errno));
			return(-1);
		}
		bytes_written = bytes_written + retval;
	}
	return(bytes_written);
}


/*
 * send line to server
 */
int sock_puts(int sock, char *string)
{
	char buf[SIZ];

	sprintf(buf, "%s\n", string);
	return sock_write(sock, buf, strlen(buf));
}


/*
 * Create a Unix domain socket and listen on it
 */
int ig_uds_server(char *sockpath, int queue_len)
{
	struct sockaddr_un addr;
	int s;
	int i;
	int actual_queue_len;

	actual_queue_len = queue_len;
	if (actual_queue_len < 5) actual_queue_len = 5;

	i = unlink(sockpath);
	if (i != 0) if (errno != ENOENT) {
		fprintf(stderr, "can't unlink %s: %s\n",
			sockpath, strerror(errno));
		return(-1);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "Can't create a socket: %s\n",
			strerror(errno));
		return(-1);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		fprintf(stderr, "Can't bind: %s\n",
			strerror(errno));
		return(-1);
	}

	if (listen(s, actual_queue_len) < 0) {
		fprintf(stderr, "Can't listen: %s\n", strerror(errno));
		return(-1);
	}

	chmod(sockpath, 0700);	/* only me me me can talk to this */
	return(s);
}



/*
 * main loop
 */
int main(int argc, char **argv) {

	char buf[SIZ];
	char dbuf[SIZ];
	int i, f;
	int ctdl_sock;
	int listen_sock;
	int cmd_sock;

	/* Fail if we weren't supplied with the right number of arguments
	 */
	if (argc != 2) {
		exit(1);
	}

	/* Fail if we can't connect to Citadel
	 */
	ctdl_sock = uds_connectsock("/appl/citadel/citadel.socket");
	if (ctdl_sock < 0) {
		exit(2);
	}

	/* Fail if we can't read the server greeting message
	 */
	if (sock_gets(ctdl_sock, buf) < 0) {
		exit(3);
	}

	/* Fail if the server isn't giving us an error-free startup
	 */
	if (buf[0] != '2') {
		exit(4);
	}

	/* Now we're solid with the Citadel server.  Nice.  It's time to
	 * open our proxy socket so PHP can talk to us.  Fail if we can't
	 * set this up.
	 */
	listen_sock = ig_uds_server(argv[1], 5);
	if (listen_sock < 0) {
		close(ctdl_sock);
		exit(5);
	}

	/* The socket is ready to listen for connections, so it's time for
	 * this program to go into the background.  Fork, then close all file
	 * descriptors so that the PHP script that called us can continue
	 * its processing.
	 */
	f = fork();
	if (f < 0) {
		close(ctdl_sock);
		close(listen_sock);
		unlink(argv[1]);
		exit(6);
	}
	if (f != 0) {
		exit(0);
	}
	if (f == 0) {
		setpgrp();
		for (i=0; i<256; ++i) {
			/* Close fd's so PHP doesn't get all, like, whatever */
			if ( (i != ctdl_sock) && (i != listen_sock) ) {
				close(i);
			}
		}
	}

	/* Listen for connections. */

	signal(SIGPIPE, SIG_IGN);
	while (cmd_sock = accept(listen_sock, NULL, 0), cmd_sock >= 0) {

		while (sock_gets(cmd_sock, buf) >= 0) {
			if (sock_puts(ctdl_sock, buf) < 0) goto CTDL_BAIL;
			if (sock_gets(ctdl_sock, buf) < 0) goto CTDL_BAIL;
			sock_puts(cmd_sock, buf);

			if (buf[0] == '1') do {
				if (sock_gets(ctdl_sock, dbuf) < 0) {
					goto CTDL_BAIL;
				}
				sock_puts(cmd_sock, dbuf);
			} while (strcmp(dbuf, "000"));

			else if (buf[0] == '4') do {
				sock_gets(cmd_sock, dbuf);
				if (sock_puts(ctdl_sock, dbuf) < 0) {
					goto CTDL_BAIL;
				}
			} while (strcmp(dbuf, "000"));

		}
		close(cmd_sock);
	}

CTDL_BAIL:
	/* Clean up and go away.
	 */
	close(ctdl_sock);
	close(listen_sock);
	unlink(argv[1]);
	exit(0);
}
