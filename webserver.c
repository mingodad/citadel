/*
 * $Id$
 *
 * This contains a simple multithreaded TCP server manager.  It sits around
 * waiting on the specified port for incoming HTTP connections.  When a
 * connection is established, it calls context_loop() from context_loop.c.
 *
 */


#include "webcit.h"
#include "webserver.h"

#ifndef HAVE_SNPRINTF
int vsnprintf(char *buf, size_t max, const char *fmt, va_list argp);
#endif

int verbosity = 9;		/* Logging level */
int msock;			/* master listening socket */
int is_https = 0;		/* Nonzero if I am an HTTPS service */
int follow_xff = 0;		/* Follow X-Forwarded-For: header */
extern void *context_loop(int);
extern void *housekeeping_loop(void);
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;


char *server_cookie = NULL;


char *ctdlhost = DEFAULT_HOST;
char *ctdlport = DEFAULT_PORT;
int setup_wizard = 0;
char wizard_filename[PATH_MAX];

/*
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 */
int ig_tcp_server(char *ip_addr, int port_number, int queue_len)
{
	struct sockaddr_in sin;
	int s, i;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (ip_addr == NULL) {
		sin.sin_addr.s_addr = INADDR_ANY;
	} else {
		sin.sin_addr.s_addr = inet_addr(ip_addr);
	}

	if (sin.sin_addr.s_addr == INADDR_NONE) {
		sin.sin_addr.s_addr = INADDR_ANY;
	}

	if (port_number == 0) {
		lprintf(1, "Cannot start: no port number specified.\n");
		exit(1);
	}
	sin.sin_port = htons((u_short) port_number);

	s = socket(PF_INET, SOCK_STREAM, (getprotobyname("tcp")->p_proto));
	if (s < 0) {
		lprintf(1, "Can't create a socket: %s\n", strerror(errno));
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


#ifdef HAVE_OPENSSL
	if (is_https) {
		return (client_read_ssl(buf, bytes, timeout));
	}
#endif

	len = 0;
	while (len < bytes) {
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);
		tv.tv_sec = timeout;
		tv.tv_usec = 0;

		retval = select((sock) + 1, &rfds, NULL, NULL, &tv);
		if (FD_ISSET(sock, &rfds) == 0) {
			return (0);
		}

		rlen = read(sock, &buf[len], bytes - len);

		if (rlen < 1) {
			lprintf(2, "client_read() failed: %s\n",
				strerror(errno));
			return (-1);
		}
		len = len + rlen;
	}

#ifdef HTTP_TRACING
	write(2, "\033[32m", 5);
	write(2, buf, bytes);
	write(2, "\033[30m", 5);
#endif
	return (1);
}


ssize_t client_write(const void *buf, size_t count)
{

	if (WC->burst != NULL) {
		WC->burst =
		    realloc(WC->burst, (WC->burst_len + count + 2));
		memcpy(&WC->burst[WC->burst_len], buf, count);
		WC->burst_len += count;
		return (count);
	}
#ifdef HAVE_OPENSSL
	if (is_https) {
		client_write_ssl((char *) buf, count);
		return (count);
	}
#endif
#ifdef HTTP_TRACING
	write(2, "\033[34m", 5);
	write(2, buf, count);
	write(2, "\033[30m", 5);
#endif
	return (write(WC->http_sock, buf, count));
}


void begin_burst(void)
{
	if (WC->burst != NULL) {
		free(WC->burst);
		WC->burst = NULL;
	}
	WC->burst_len = 0;
	WC->burst = malloc(SIZ);
}


/*
 * compress_gzip() uses the same calling syntax as compress2(), but it
 * creates a stream compatible with HTTP "Content-encoding: gzip"
 */
#ifdef HAVE_ZLIB
#define DEF_MEM_LEVEL 8
#define OS_CODE 0x03	/* unix */
int ZEXPORT compress_gzip(Bytef * dest, uLongf * destLen,
			  const Bytef * source, uLong sourceLen, int level)
{
	const int gz_magic[2] = { 0x1f, 0x8b };	/* gzip magic header */

	/* write gzip header */
	sprintf((char *) dest, "%c%c%c%c%c%c%c%c%c%c",
		gz_magic[0], gz_magic[1], Z_DEFLATED,
		0 /*flags */ , 0, 0, 0, 0 /*time */ , 0 /*xflags */ ,
		OS_CODE);

	/* normal deflate */
	z_stream stream;
	int err;
	stream.next_in = (Bytef *) source;
	stream.avail_in = (uInt) sourceLen;
	stream.next_out = dest + 10L;	// after header
	stream.avail_out = (uInt) * destLen;
	if ((uLong) stream.avail_out != *destLen)
		return Z_BUF_ERROR;

	stream.zalloc = (alloc_func) 0;
	stream.zfree = (free_func) 0;
	stream.opaque = (voidpf) 0;

	err = deflateInit2(&stream, level, Z_DEFLATED, -MAX_WBITS,
			   DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY);
	if (err != Z_OK)
		return err;

	err = deflate(&stream, Z_FINISH);
	if (err != Z_STREAM_END) {
		deflateEnd(&stream);
		return err == Z_OK ? Z_BUF_ERROR : err;
	}
	*destLen = stream.total_out + 10L;

	/* write CRC and Length */
	uLong crc = crc32(0L, source, sourceLen);
	int n;
	for (n = 0; n < 4; ++n, ++*destLen) {
		dest[*destLen] = (int) (crc & 0xff);
		crc >>= 8;
	}
	uLong len = stream.total_in;
	for (n = 0; n < 4; ++n, ++*destLen) {
		dest[*destLen] = (int) (len & 0xff);
		len >>= 8;
	}
	err = deflateEnd(&stream);
	return err;
}
#endif

void end_burst(void)
{
	size_t the_len;
	char *the_data;

	if (WC->burst == NULL)
		return;

	the_len = WC->burst_len;
	the_data = WC->burst;

	WC->burst_len = 0;
	WC->burst = NULL;

#ifdef HAVE_ZLIB
	/* Handle gzip compression */
	if (WC->gzip_ok) {
		char *compressed_data = NULL;
		uLongf compressed_len;

		compressed_len = (uLongf) ((the_len * 101) / 100) + 100;
		compressed_data = malloc(compressed_len);

		if (compress_gzip((Bytef *) compressed_data,
				  &compressed_len,
				  (Bytef *) the_data,
				  (uLongf) the_len, Z_BEST_SPEED) == Z_OK) {
			wprintf("Content-encoding: gzip\r\n");
			free(the_data);
			the_data = compressed_data;
			the_len = compressed_len;
		} else {
			free(compressed_data);
		}
	}
#endif				/* HAVE_ZLIB */

	wprintf("Content-length: %d\r\n\r\n", the_len);
	client_write(the_data, the_len);
	free(the_data);
	return;
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
 * client_getln()   ...   Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 */
int client_getln(int sock, char *buf, int bufsiz)
{
	int i, retval;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		retval = client_read(sock, &buf[i], 1);
		if (retval != 1 || buf[i] == '\n' || i == (bufsiz-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (bufsiz-1))
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

void spawn_another_worker_thread()
{
	pthread_t SessThread;	/* Thread descriptor */
	pthread_attr_t attr;	/* Thread attributes */
	int ret;

	lprintf(3, "Creating a new thread\n");

	/* set attributes for the new thread */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/* Our per-thread stacks need to be bigger than the default size, otherwise
	 * the MIME parser crashes on FreeBSD, and the IMAP service crashes on
	 * 64-bit Linux.
	 */
	if ((ret = pthread_attr_setstacksize(&attr, 1024 * 1024))) {
		lprintf(1, "pthread_attr_setstacksize: %s\n",
			strerror(ret));
		pthread_attr_destroy(&attr);
	}

	/* now create the thread */
	if (pthread_create(&SessThread, &attr,
			   (void *(*)(void *)) worker_entry, NULL)
	    != 0) {
		lprintf(1, "Can't create thread: %s\n", strerror(errno));
	}

	/* free up the attributes */
	pthread_attr_destroy(&attr);
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
	char ip_addr[256];
	char *webcitdir = WEBCITDIR;
	char *locale = NULL;
	char *mo = NULL;

	/* Parse command line */
#ifdef HAVE_OPENSSL
	while ((a = getopt(argc, argv, "h:i:p:t:x:cfs")) != EOF)
#else
	while ((a = getopt(argc, argv, "h:i:p:t:x:cf")) != EOF)
#endif
		switch (a) {
		case 'h':
			webcitdir = strdup(optarg);
			break;
		case 'i':
			safestrncpy(ip_addr, optarg, sizeof ip_addr);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 't':
			safestrncpy(tracefile, optarg, sizeof tracefile);
			freopen(tracefile, "w", stdout);
			freopen(tracefile, "w", stderr);
			freopen(tracefile, "r", stdin);
			break;
		case 'x':
			verbosity = atoi(optarg);
			break;
		case 'f':
			follow_xff = 1;
			break;
		case 'c':
			server_cookie = malloc(256);
			if (server_cookie != NULL) {
				safestrncpy(server_cookie,
				       "Set-cookie: wcserver=",
					256);
				if (gethostname
				    (&server_cookie[strlen(server_cookie)],
				     200) != 0) {
					lprintf(2, "gethostname: %s\n",
						strerror(errno));
					free(server_cookie);
				}
			}
			break;
		case 's':
			is_https = 1;
			break;
		default:
			fprintf(stderr, "usage: webserver "
				"[-i ip_addr] [-p http_port] "
				"[-t tracefile] [-c] [-f] "
#ifdef HAVE_OPENSSL
				"[-s] "
#endif
				"[remotehost [remoteport]]\n");
			return 1;
		}

	if (optind < argc) {
		ctdlhost = argv[optind];
		if (++optind < argc)
			ctdlport = argv[optind];
	}
	/* Tell 'em who's in da house */
	lprintf(1, SERVER "\n");
	lprintf(1, "Copyright (C) 1996-2005 by the Citadel development team.\n"
		"This software is distributed under the terms of the "
		"GNU General Public License.\n\n"
	);

	lprintf(9, "Changing directory to %s\n", webcitdir);
	if (chdir(webcitdir) != 0) {
		perror("chdir");
	}

	/* initialize the International Bright Young Thing */
#ifdef ENABLE_NLS
	locale = setlocale(LC_ALL, "");

	mo = malloc(strlen(webcitdir) + 20);
	sprintf(mo, "%s/locale", webcitdir);
	lprintf(9, "Message catalog directory: %s\n",
		bindtextdomain("webcit", mo)
	);
	free(mo);
	lprintf(9, "Text domain: %s\n",
		textdomain("webcit")
	);
#endif

	initialize_viewdefs();
	initialize_axdefs();

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * wcsession struct to which the thread is currently bound.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		lprintf(1, "Can't create TSD key: %s\n", strerror(errno));
	}

	/*
	 * Set up a place to put thread-specific SSL data.
	 * We don't stick this in the wcsession struct because SSL starts
	 * up before the session is bound, and it gets torn down between
	 * transactions.
	 */
#ifdef HAVE_OPENSSL
	if (pthread_key_create(&ThreadSSL, NULL) != 0) {
		lprintf(1, "Can't create TSD key: %s\n", strerror(errno));
	}
#endif

	/*
	 * Bind the server to our favorite port.
	 * There is no need to check for errors, because ig_tcp_server()
	 * exits if it doesn't succeed.
	 */
	lprintf(2, "Attempting to bind to port %d...\n", port);
	msock = ig_tcp_server(ip_addr, port, LISTEN_QUEUE_LENGTH);
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


	/*
	 * If this is an HTTPS server, fire up SSL
	 */
#ifdef HAVE_OPENSSL
	if (is_https) {
		init_ssl();
	}
#endif

	/* Start a few initial worker threads */
	for (i = 0; i < (MIN_WORKER_THREADS); ++i) {
		spawn_another_worker_thread();
	}

	/* now the original thread becomes another worker */
	worker_entry();
	return 0;
}


/*
 * Entry point for worker threads
 */
void worker_entry(void)
{
	int ssock;
	int i = 0;
	int time_to_die = 0;
	int fail_this_transaction = 0;

	do {
		/* Only one thread can accept at a time */
		fail_this_transaction = 0;
		ssock = accept(msock, NULL, 0);
		if (ssock < 0) {
			lprintf(2, "accept() failed: %s\n",
				strerror(errno));
		} else {
			/* Set the SO_REUSEADDR socket option */
			i = 1;
			setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
				   &i, sizeof(i));

			/* If we are an HTTPS server, go crypto now. */
#ifdef HAVE_OPENSSL
			if (is_https) {
				if (starttls(ssock) != 0) {
					fail_this_transaction = 1;
					close(ssock);
				}
			}
#endif

			if (fail_this_transaction == 0) {
				/* Perform an HTTP transaction... */
				context_loop(ssock);
				/* ...and close the socket. */
				lingering_close(ssock);
			}

		}

	} while (!time_to_die);

	pthread_exit(NULL);
}


int lprintf(int loglevel, const char *format, ...)
{
	va_list ap;

	if (loglevel <= verbosity) {
		va_start(ap, format);
		vfprintf(stderr, format, ap);
		va_end(ap);
		fflush(stderr);
	}
	return 1;
}
