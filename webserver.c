/*
 * webserver.c
 *
 * This contains a simple multithreaded TCP server manager.  It sits around
 * waiting on the specified port for incoming HTTP connections.  When a
 * connection is established, it calls context_loop() from context_loop.c.
 *
 * $Id$
 */

#include <ctype.h>
#include <stdlib.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#include <stdio.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#endif
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <pthread.h>
#include <signal.h>

#include "webcit.h"
#include "webserver.h"

#ifndef HAVE_SNPRINTF
int vsnprintf(char *buf, size_t max, const char *fmt, va_list argp);
#endif

int verbosity = 9;		/* Logging level */
int msock;			/* master listening socket */
extern void *context_loop(int);
extern void *housekeeping_loop(void);
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;


char *server_cookie = NULL;


char *defaulthost = DEFAULT_HOST;
char *defaultport = DEFAULT_PORT;

/*
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 */
int ig_tcp_server(int port_number, int queue_len)
{
	struct sockaddr_in sin;
	int s, i;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	if (port_number == 0) {
		lprintf(1, "Cannot start: no port number specified.\n");
		exit(1);
	}
	sin.sin_port = htons((u_short) port_number);

	s = socket(PF_INET, SOCK_STREAM, (getprotobyname("tcp")->p_proto));
	if (s < 0) {
		lprintf(1, "Can't create a socket: %s\n",
		       strerror(errno));
		exit(errno);
	}
	/* Set some socket options that make sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(1, "Can't bind: %s\n", strerror(errno));
		exit(errno);
	}
	if (listen(s, queue_len) < 0) {
		lprintf(1, "Can't listen: %s\n", strerror(errno));
		exit(errno);
	}
	return (s);
}


/*
 * Read data from the client socket.
 * Return values are:
 *      1       Requested number of bytes has been read.
 *      0       Request timed out.
 *	-1	Connection is broken, or other error.
 */
int client_read_to(int sock, char *buf, int bytes, int timeout)
{
	int len, rlen;
	fd_set rfds;
	struct timeval tv;
	int retval;

	len = 0;
	while (len < bytes) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select((sock) + 1,
				&rfds, NULL, NULL, &tv);
		if (FD_ISSET(sock, &rfds) == 0) {
			return (0);
		}
		rlen = read(sock, &buf[len], bytes - len);
		if (rlen < 1) {
			lprintf(2, "client_read() failed: %s\n",
			       strerror(errno));
			return(-1);
		}
		len = len + rlen;
	}
	return (1);
}

/*
 * Read data from the client socket with default timeout.
 * (This is implemented in terms of client_read_to() and could be
 * justifiably moved out of sysdep.c)
 */
int client_read(int sock, char *buf, int bytes)
{
	return (client_read_to(sock, buf, bytes, SLEEPING));
}


/*
 * client_gets()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int client_gets(int sock, char *buf)
{
	int i, retval;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		retval = client_read(sock, &buf[i], 1);
		if (retval != 1 || buf[i] == '\n' || i == 255)
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == 255)
		while (buf[i] != '\n' && retval == 1)
			retval = client_read(sock, &buf[i], 1);

	/*
	 * Strip any trailing non-printable characters.
	 */
	buf[i] = 0;
	while ((strlen(buf) > 0) && (!isprint(buf[strlen(buf) - 1]))) {
		buf[strlen(buf) - 1] = 0;
	}
	return (retval);
}


/*
 * Start running as a daemon.  Only close stdio if do_close_stdio is set.
 */
void start_daemon(int do_close_stdio)
{
	if (do_close_stdio) {
		/* close(0); */
		close(1);
		close(2);
	}
	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	if (fork() != 0)
		exit(0);
}

void spawn_another_worker_thread() {
	pthread_t SessThread;	/* Thread descriptor */
	pthread_attr_t attr;	/* Thread attributes */

	lprintf(3, "Creating a new thread\n");

	/* set attributes for the new thread */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* now create the thread */
	if (pthread_create(&SessThread, &attr,
			(void *(*)(void *)) worker_entry, NULL)
		   != 0) {
		lprintf(1, "Can't create thread: %s\n",
			strerror(errno));
	}
}

/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	pthread_t SessThread;	/* Thread descriptor */
	pthread_attr_t attr;	/* Thread attributes */
	int a, i;		/* General-purpose variables */
	int port = PORT_NUM;	/* Port to listen on */
	char tracefile[PATH_MAX];

	/* Parse command line */
	while ((a = getopt(argc, argv, "hp:t:c")) != EOF)
		switch (a) {
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			strcpy(tracefile, optarg);
			freopen(tracefile, "w", stdout);
			freopen(tracefile, "w", stderr);
			freopen(tracefile, "r", stdin);
			break;
		case 'x':
			verbosity = atoi(optarg);
			break;
		case 'c':
			server_cookie = malloc(SIZ);
			if (server_cookie != NULL) {
				strcpy(server_cookie, "Set-cookie: wcserver=");
				if (gethostname(
				   &server_cookie[strlen(server_cookie)],
				   200) != 0) {
					lprintf(2, "gethostname: %s\n",
						strerror(errno));
					free(server_cookie);
				}
			}
			break;
		default:
			fprintf(stderr, "usage: webserver [-p localport] "
				"[-t tracefile] [-c] "
				"[remotehost [remoteport]]\n");
			return 1;
		}

	if (optind < argc) {
		defaulthost = argv[optind];
		if (++optind < argc)
			defaultport = argv[optind];
	}
	/* Tell 'em who's in da house */
	lprintf(1, SERVER "\n"
"Copyright (C) 1996-2003 by the Citadel/UX development team.\n"
"This software is distributed under the terms of the GNU General Public\n"
"License.  If you paid for this software, someone is ripping you off.\n\n");

	if (chdir(WEBCITDIR) != 0)
		perror("chdir");

        /*
         * Set up a place to put thread-specific data.
         * We only need a single pointer per thread - it points to the
         * wcsession struct to which the thread is currently bound.
         */
        if (pthread_key_create(&MyConKey, NULL) != 0) {
                lprintf(1, "Can't create TSD key: %s\n", strerror(errno));
        }

	/*
	 * Bind the server to our favorite port.
	 * There is no need to check for errors, because ig_tcp_server()
	 * exits if it doesn't succeed.
	 */
	lprintf(2, "Attempting to bind to port %d...\n", port);
	msock = ig_tcp_server(port, LISTEN_QUEUE_LENGTH);
	lprintf(2, "Listening on socket %d\n", msock);
	signal(SIGPIPE, SIG_IGN);

	pthread_mutex_init(&SessionListMutex, NULL);

	/*
	 * Start up the housekeeping thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&SessThread, &attr,
		       (void *(*)(void *)) housekeeping_loop, NULL);


	/* Start a few initial worker threads */
	for (i=0; i<(INITIAL_WORKER_THREADS); ++i) {
		spawn_another_worker_thread();
	}

	/* now the original thread can go away. */
	pthread_exit(NULL);
	return 0;
}


/*
 * Entry point for worker threads
 */
void worker_entry(void) {
	int ssock;
	int i = 0;
	int time_to_die = 0;
	time_t start_time, stop_time;

	do {
		/* Only one thread can accept at a time */
		start_time = time(NULL);
		ssock = accept(msock, NULL, 0);
		stop_time = time(NULL);

		/* Augment the thread pool if we're not blocking at all */
		if ( (stop_time - start_time) == 0L) {
			spawn_another_worker_thread();
		}

		if (ssock < 0) {
			lprintf(2, "accept() failed: %s\n", strerror(errno));
		} else {
			/* Set the SO_REUSEADDR socket option */
			i = 1;
			setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
			   	&i, sizeof(i));

			/* Perform an HTTP transaction... */
			context_loop(ssock);

			/* ...and close the socket. */
			lingering_close(ssock);
		}

	} while (!time_to_die);

	pthread_exit(NULL);
}


int lprintf(int loglevel, const char *format, ...)
{
	va_list ap;
	char buf[4096];

	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);

	if (loglevel <= verbosity) {
		struct timeval tv;
		struct tm *tim;

		gettimeofday(&tv, NULL);
		tim = localtime((time_t *)&(tv.tv_sec));

		if (WC && WC->wc_session) {
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%03ld [%ld:%d] %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec / 1000,
				(long)pthread_self(),
				WC->wc_session, buf);
		} else {
			fprintf(stderr,
				"%04d/%02d/%02d %2d:%02d:%02d.%03ld [%ld] %s",
				tim->tm_year + 1900, tim->tm_mon + 1,
				tim->tm_mday, tim->tm_hour, tim->tm_min,
				tim->tm_sec, (long)tv.tv_usec / 1000,
				(long)pthread_self(),
				buf);
		}
		fflush(stderr);
	}
	return 1;
}
