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
#undef NDEBUG
#include <assert.h>
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
#include "ipc.h"
#include "citadel_decls.h"
#include "tools.h"
#if defined(HAVE_OPENSSL)
#include "client_crypto.h"
#endif
#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/*
 * If ipc->isLocal is set to nonzero, the client assumes that it is running on
 * the same computer as the server.  Several things happen when this is the
 * case, including the ability to map a specific tty to a particular login
 * session in the "<W>ho is online" listing, the ability to run external
 * programs, and the ability to download files directly off the disk without
 * having to first fetch them from the server.
 * Set the flag to 1 if this IPC is local (as is the case with pipes, or a
 * network session to the local machine) or 0 if the server is executing on
 * a remote computer.
 */

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

/*
 * FIXME: rewrite all of Ford's stuff here, it won't work with multiple
 * instances
 */

static void (*deathHook)(void) = NULL;
int (*error_printf)(char *s, ...) = (int (*)(char *, ...))printf;

void setIPCDeathHook(void (*hook)(void)) {
	deathHook = hook;
}

void setIPCErrorPrintf(int (*func)(char *s, ...)) {
	error_printf = func;
}

void connection_died(CtdlIPC *ipc) {
	if (deathHook != NULL)
		deathHook();

	error_printf("\rYour connection to this Citadel server is broken.\n"
			"Last error: %s\n"
			"Please re-connect and log in again.\n",
			strerror(errno));
#ifdef HAVE_OPENSSL
	SSL_shutdown(ipc->ssl);
	SSL_free(ipc->ssl);
	ipc->ssl = NULL;
#endif
	shutdown(ipc->sock, 2);
	ipc->sock = -1;
}


/*
static void ipc_timeout(int signum)
{
	error_printf("\rConnection timed out.\n");
	logoff(NULL, 3);
}
*/


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
		error_printf("Can't get %s host entry: %s\n",
			host, strerror(errno));
		return -1;
	}
	if ((ppe = getprotobyname(protocol)) == 0) {
		error_printf("Can't get %s protocol entry: %s\n",
			protocol, strerror(errno));
		return -1;
	}
	if (!strcmp(protocol, "udp")) {
		type = SOCK_DGRAM;
	} else {
		type = SOCK_STREAM;
	}

	s = socket(PF_INET, type, ppe->p_proto);
	if (s < 0) {
		error_printf("Can't create socket: %s\n", strerror(errno));
		return -1;
	}
	/*
	signal(SIGALRM, ipc_timeout);
	alarm(30);
	*/

	if (connect(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		error_printf("Can't connect to %s:%s: %s\n",
			host, service, strerror(errno));
		return -1;
	}
	/*
	alarm(0);
	signal(SIGALRM, SIG_IGN);
	*/

	return (s);
}

static int uds_connectsock(int *isLocal, char *sockpath)
{
	struct sockaddr_un addr;
	int s;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		error_printf("Can't create socket: %s\n", strerror(errno));
		return -1;
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		error_printf("can't connect: %s\n", strerror(errno));
		return -1;
	}

	*isLocal = 1;
	return s;
}


/*
 * input binary data from socket
 */
void serv_read(CtdlIPC *ipc, char *buf, int bytes)
{
	int len, rlen;

#if defined(HAVE_OPENSSL)
	if (ipc->ssl) {
		serv_read_ssl(ipc, buf, bytes);
		return;
	}
#endif
	len = 0;
	while (len < bytes) {
		rlen = read(ipc->sock, &buf[len], bytes - len);
		if (rlen < 1) {
			connection_died(ipc);
			return;
		}
		len += rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(CtdlIPC *ipc, const char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;

#if defined(HAVE_OPENSSL)
	if (ipc->ssl) {
		serv_write_ssl(ipc, buf, nbytes);
		return;
	}
#endif
	while (bytes_written < nbytes) {
		retval = write(ipc->sock, &buf[bytes_written],
			       nbytes - bytes_written);
		if (retval < 1) {
			connection_died(ipc);
			return;
		}
		bytes_written += retval;
	}
}



/*
 * input string from socket - implemented in terms of serv_read()
 */
void CtdlIPC_getline(CtdlIPC* ipc, char *buf)
{
	int i;

	/* Read one character at a time. */
	for (i = 0;; i++) {
		serv_read(ipc, &buf[i], 1);
		if (buf[i] == '\n' || i == (SIZ-1))
			break;
	}

	/* If we got a long line, discard characters until the newline. */
	if (i == (SIZ-1))
		while (buf[i] != '\n')
			serv_read(ipc, &buf[i], 1);

	/* Strip the trailing newline.
	 */
	buf[i] = 0;
}


/*
 * send line to server - implemented in terms of serv_write()
 */
void CtdlIPC_putline(CtdlIPC *ipc, const char *buf)
{
	/* error_printf("< %s\n", buf); */
	int watch_ssl = 0;
	if (ipc->ssl) watch_ssl = 1;
	assert(!watch_ssl || ipc->ssl);
	serv_write(ipc, buf, strlen(buf));
	assert(!watch_ssl || ipc->ssl);
	serv_write(ipc, "\n", 1);
	assert(!watch_ssl || ipc->ssl);
}


/*
 * attach to server
 */
CtdlIPC* CtdlIPC_new(int argc, char **argv, char *hostbuf, char *portbuf)
{
	int a;
	char cithost[SIZ];
	char citport[SIZ];
	char sockpath[SIZ];

	CtdlIPC *ipc = ialloc(CtdlIPC);
	if (!ipc) {
		error_printf("Out of memory creating CtdlIPC!\n");
		return 0;
	}
#if defined(HAVE_OPENSSL)
	ipc->ssl = NULL;
#endif
#if defined(HAVE_PTHREAD_H)
	pthread_mutex_init(&(ipc->mutex), NULL); /* Default fast mutex */
#endif
	ipc->sock = -1;			/* Not connected */
	ipc->isLocal = 0;		/* Not local, of course! */

	strcpy(cithost, DEFAULT_HOST);	/* default host */
	strcpy(citport, DEFAULT_PORT);	/* default port */

	for (a = 0; a < argc; ++a) {
		if (a == 0) {
			/* do nothing */
		} else if (a == 1) {
			strcpy(cithost, argv[a]);
		} else if (a == 2) {
			strcpy(citport, argv[a]);
		} else {
			error_printf("%s: usage: ",argv[0]);
			error_printf("%s [host] [port] ",argv[0]);
			ifree(ipc);
			return 0;
   		}
	}

	if ((!strcmp(cithost, "localhost"))
	   || (!strcmp(cithost, "127.0.0.1"))) {
		ipc->isLocal = 1;
	}

	/* If we're using a unix domain socket we can do a bunch of stuff */
	if (!strcmp(cithost, UDS)) {
		snprintf(sockpath, sizeof sockpath, "citadel.socket");
		ipc->sock = uds_connectsock(&(ipc->isLocal), sockpath);
		if (ipc->sock == -1) {
			ifree(ipc);
			return 0;
		}
		if (hostbuf != NULL) strcpy(hostbuf, cithost);
		if (portbuf != NULL) strcpy(portbuf, sockpath);
		return ipc;
	}

	ipc->sock = connectsock(cithost, citport, "tcp", 504);
	if (ipc->sock == -1) {
		ifree(ipc);
		return 0;
	}
	if (hostbuf != NULL) strcpy(hostbuf, cithost);
	if (portbuf != NULL) strcpy(portbuf, citport);
	return ipc;
}

/*
 * return the file descriptor of the server socket so we can select() on it.
 *
 * FIXME: This is only used in chat mode; eliminate it when chat mode gets
 * rewritten...
 */
int CtdlIPC_getsockfd(CtdlIPC* ipc)
{
	return ipc->sock;
}


/*
 * return one character
 *
 * FIXME: This is only used in chat mode; eliminate it when chat mode gets
 * rewritten...
 */
char CtdlIPC_get(CtdlIPC* ipc)
{
	char buf[2];
	char ch;

	serv_read(ipc, buf, 1);
	ch = (int) buf[0];

	return (ch);
}
