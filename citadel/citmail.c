/*
 * 
 * Completely reworked version of "citmail"
 * This program attempts to act like a local MDA if you're using sendmail or
 * some other non-Citadel MTA.  It basically just forwards the message to
 * the Citadel SMTP listener on some non-standard port.
 *
 * $Id$
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <ctype.h>
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
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

int serv_sock;


void strip_trailing_nonprint(char *buf)
{
        while ( (strlen(buf)>0) && (!isprint(buf[strlen(buf) - 1])) )
                buf[strlen(buf) - 1] = 0;
}






void timeout(int signum)
{
	exit(signum);
}


int connectsock(char *host, char *service, char *protocol)
{
	struct hostent *phe;
	struct servent *pse;
	struct protoent *ppe;
	struct sockaddr_in sin;
	int s, type;
	struct sockaddr_un sun;

	if ( (!strcmp(protocol, "unix")) || (atoi(service)<0) ) {
		memset(&sun, 0, sizeof(sun));
		sun.sun_family = AF_UNIX;
		sprintf(sun.sun_path, USOCKPATH, 0-atoi(service) );

		s = socket(AF_UNIX, SOCK_STREAM, 0);
		if (s < 0) {
			fprintf(stderr, "Can't create socket: %s\n",
				strerror(errno));
			exit(3);
		}

		if (connect(s, (struct sockaddr *) &sun, sizeof(sun)) < 0) {
			fprintf(stderr, "can't connect: %s\n",
				strerror(errno));
			exit(3);
		}

		return s;
	}


	/* otherwise it's a network connection */

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;

	pse = getservbyname(service, protocol);
	if (pse) {
		sin.sin_port = pse->s_port;
	} else if ((sin.sin_port = htons((u_short) atoi(service))) == 0) {
		fprintf(stderr, "Can't get %s service entry: %s\n",
			service, strerror(errno));
		exit(3);
	}
	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		exit(3);
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		fprintf(stderr, "Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		exit(3);
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		fprintf(stderr, "Can't create socket: %s\n", strerror(errno));
		exit(3);
	}
	signal(SIGALRM, timeout);
	alarm(30);

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		fprintf(stderr, "can't connect to %s.%s: %s\n",
			host, service, strerror(errno));
		exit(3);
	}
	alarm(0);
	signal(SIGALRM, SIG_IGN);

	return (s);
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
		exit(3);
	}
	buf[1] = (((sin.sin_port) & 0xFF00) >> 8);
	buf[0] = ((sin.sin_port) & 0x00FF);

	phe = gethostbyname(host);
	if (phe) {
		memcpy(&sin.sin_addr, phe->h_addr, phe->h_length);
	} else if ((sin.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE) {
		fprintf(stderr, "Can't get %s host entry: %s\n",
			host, strerror(errno));
		exit(3);
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
		if (buf[i] == '\n' || i == 255)
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == 255)
		while (buf[i] != '\n')
			serv_read(&buf[i], 1);

	/* Strip all trailing nonprintables (crlf)
	 */
	buf[i] = 0;
	strip_trailing_nonprint(buf);
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





void cleanup(int exitcode) {
	char buf[1024];

	serv_puts("QUIT");
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	exit(exitcode);
}



int main(int argc, char **argv) {
	char buf[1024];
	char fromline[1024];
	FILE *fp;

	fp = tmpfile();
	if (fp == NULL) return(errno);
	sprintf(fromline, "From: someone@somewhere.org");
	while (fgets(buf, 1024, stdin) != NULL) {
		fprintf(fp, "%s", buf);
		if (!strncasecmp(buf, "From:", 5)) strcpy(fromline, buf);
	}
	strip_trailing_nonprint(fromline);

	sprintf(buf, "%d", SMTP_PORT);
	serv_sock = connectsock("localhost", buf, "tcp");
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='2') cleanup(1);

	serv_puts("HELO localhost");
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='2') cleanup(1);

	sprintf(buf, "MAIL %s", fromline);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='2') cleanup(1);

	sprintf(buf, "RCPT To: %s", argv[1]);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='2') cleanup(1);

	serv_puts("DATA");
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='3') cleanup(1);

	rewind(fp);
	while (fgets(buf, sizeof buf, fp) != NULL) {
		strip_trailing_nonprint(buf);
		serv_puts(buf);
	}
	serv_puts(".");
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);
	if (buf[0]!='2') cleanup(1);
	else cleanup(0);
	return(0);
}
