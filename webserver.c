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

#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#include "modules_init.h"
#ifndef HAVE_SNPRINTF
int vsnprintf(char *buf, size_t max, const char *fmt, va_list argp);
#endif

int verbosity = 9;		/* Logging level */
int msock;			/* master listening socket */
int is_https = 0;		/* Nonzero if I am an HTTPS service */
int follow_xff = 0;		/* Follow X-Forwarded-For: header */
int home_specified = 0;		/* did the user specify a homedir? */
int time_to_die = 0;            /* Nonzero if server is shutting down */
extern void *context_loop(int);
extern void *housekeeping_loop(void);
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;


char ctdl_key_dir[PATH_MAX]=SSL_DIR;
char file_crpt_file_key[PATH_MAX]="";
char file_crpt_file_csr[PATH_MAX]="";
char file_crpt_file_cer[PATH_MAX]="";

char socket_dir[PATH_MAX];			/* where to talk to our citadel server */
static const char editor_absolut_dir[PATH_MAX]=EDITORDIR;	/* nailed to what configure gives us. */
static char static_dir[PATH_MAX];		/* calculated on startup */
static char static_local_dir[PATH_MAX];		/* calculated on startup */
static char static_icon_dir[PATH_MAX];          /* where should we find our mime icons? */
char  *static_dirs[]={				/* needs same sort order as the web mapping */
	(char*)static_dir,			/* our templates on disk */
	(char*)static_local_dir,		/* user provided templates disk */
	(char*)editor_absolut_dir,		/* the editor on disk */
	(char*)static_icon_dir                  /* our icons... */
};

/*
 * Subdirectories from which the client may request static content
 *
 * (If you add more, remember to increment 'ndirs' below)
 */
char *static_content_dirs[] = {
	"static",                     /* static templates */
	"static.local",               /* site local static templates */
	"tiny_mce"                    /* rich text editor */
};

int ndirs=3;


char *server_cookie = NULL;	/* our Cookie connection to the client */
int http_port = PORT_NUM;	/* Port to listen on */
char *ctdlhost = DEFAULT_HOST;	/* our name */
char *ctdlport = DEFAULT_PORT;	/* our Port */
int setup_wizard = 0;		/* should we run the setup wizard? \todo */
char wizard_filename[PATH_MAX];	/* where's the setup wizard? */
int running_as_daemon = 0;	/* should we deamonize on startup? */


/* 
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 *
 * ip_addr 	IP address to bind
 * port_number	port number to bind
 * queue_len	number of incoming connections to allow in the queue
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
		exit(WC_EXIT_BIND);
	}
	sin.sin_port = htons((u_short) port_number);

	s = socket(PF_INET, SOCK_STREAM, (getprotobyname("tcp")->p_proto));
	if (s < 0) {
		lprintf(1, "Can't create a socket: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	/* Set some socket options that make sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	fcntl(s, F_SETFL, O_NONBLOCK); /* maide: this statement is incorrect
					  there should be a preceding F_GETFL
					  and a bitwise OR with the previous
					  fd flags */
	
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		lprintf(1, "Can't bind: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	if (listen(s, queue_len) < 0) {
		lprintf(1, "Can't listen: %s\n", strerror(errno));
		exit(WC_EXIT_BIND);
	}
	return (s);
}



/*
 * Create a Unix domain socket and listen on it
 * sockpath - file name of the unix domain socket
 * queue_len - Number of incoming connections to allow in the queue
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
		lprintf(1, "webcit: can't unlink %s: %s\n",
			sockpath, strerror(errno));
		exit(WC_EXIT_BIND);
	}

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	safestrncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		lprintf(1, "webcit: Can't create a socket: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	if (bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		lprintf(1, "webcit: Can't bind: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	if (listen(s, actual_queue_len) < 0) {
		lprintf(1, "webcit: Can't listen: %s\n",
			strerror(errno));
		exit(WC_EXIT_BIND);
	}

	chmod(sockpath, 0777);
	return(s);
}




/*
 * \brief Read data from the client socket.
 * \param sock socket fd to read from
 * \param buf buffer to read into 
 * \param bytes number of bytes to read
 * \param timeout Number of seconds to wait before timing out
 * \return values are\
 *      1       Requested number of bytes has been read.\
 *      0       Request timed out.\
 *	   -1   	Connection is broken, or other error.
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

/*
 * \brief write data to the client
 * \param buf data to write to the client
 * \param count size of buffer
 */
ssize_t client_write(const void *buf, size_t count)
{
        char *newptr;
        size_t newalloc;
        size_t bytesWritten = 0;
        ssize_t res;
        fd_set wset;
        int fdflags;

	if (WC->burst != NULL) {
		if ((WC->burst_len + count) >= WC->burst_alloc) {
			newalloc = (WC->burst_alloc * 2);
			if ((WC->burst_len + count) >= newalloc) {
				newalloc += count;
			}
			newptr = realloc(WC->burst, newalloc);
			if (newptr != NULL) {
				WC->burst = newptr;
				WC->burst_alloc = newalloc;
			}
		}
		if ((WC->burst_len + count) < WC->burst_alloc) {
			memcpy(&WC->burst[WC->burst_len], buf, count);
			WC->burst_len += count;
			return (count);
		}
		else {
			return(-1);
		}
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
	fdflags = fcntl(WC->http_sock, F_GETFL);

        while (bytesWritten < count) {
                if ((fdflags & O_NONBLOCK) == O_NONBLOCK) {
                        FD_ZERO(&wset);
                        FD_SET(WC->http_sock, &wset);
                        if (select(WC->http_sock + 1, NULL, &wset, NULL, NULL) == -1) {
                                lprintf(2, "client_write: Socket select failed (%s)\n", strerror(errno));
                                return -1;
                        }
                }

                if ((res = write(WC->http_sock, (char*)buf + bytesWritten,
                  count - bytesWritten)) == -1) {
                        lprintf(2, "client_write: Socket write failed (%s)\n", strerror(errno));
                        return res;
                }
                bytesWritten += res;
        }

	return bytesWritten;
}

/*
 * \brief Begin buffering HTTP output so we can transmit it all in one write operation later.
 */
void begin_burst(void)
{
	if (WC->burst != NULL) {
		free(WC->burst);
		WC->burst = NULL;
	}
	WC->burst_len = 0;
	WC->burst_alloc = 32768;
	WC->burst = malloc(WC->burst_alloc);
}


/*
 * \brief uses the same calling syntax as compress2(), but it
 * creates a stream compatible with HTTP "Content-encoding: gzip"
 */
#ifdef HAVE_ZLIB
#define DEF_MEM_LEVEL 8 /*< memlevel??? */
#define OS_CODE 0x03	/*< unix */
int ZEXPORT compress_gzip(Bytef * dest,         /*< compressed buffer*/
			  size_t * destLen,     /*< length of the compresed data */
			  const Bytef * source, /*< source to encode */
			  uLong sourceLen,      /*< length of source to encode */
			  int level)            /*< compression level */
{
	const int gz_magic[2] = { 0x1f, 0x8b };	/* gzip magic header */

	/* write gzip header */
	snprintf((char *) dest, *destLen, 
		 "%c%c%c%c%c%c%c%c%c%c",
		 gz_magic[0], gz_magic[1], Z_DEFLATED,
		 0 /*flags */ , 0, 0, 0, 0 /*time */ , 0 /* xflags */ ,
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

/*
 * \brief Finish buffering HTTP output.  [Compress using zlib and] output with a Content-Length: header.
 */
void end_burst(void)
{
	size_t the_len;
	char *the_data;

	if (WC->burst == NULL)
		return;

	the_len = WC->burst_len;
	the_data = WC->burst;

	WC->burst_len = 0;
	WC->burst_alloc = 0;
	WC->burst = NULL;

#ifdef HAVE_ZLIB
	/* Perform gzip compression, if enabled and supported by client */
	if (WC->gzip_ok) {
		char *compressed_data = NULL;
		size_t compressed_len;

		compressed_len = ((the_len * 101) / 100) + 100;
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
#endif	/* HAVE_ZLIB */

	wprintf("Content-length: %d\r\n\r\n", the_len);
	client_write(the_data, the_len);
	free(the_data);
	return;
}



/*
 * \brief Read data from the client socket with default timeout.
 * (This is implemented in terms of client_read_to() and could be
 * justifiably moved out of sysdep.c)
 * \param sock the socket fd to read from
 * \param buf the buffer to write to
 * \param bytes Number of bytes to read
 */
int client_read(int sock, char *buf, int bytes)
{
	return (client_read_to(sock, buf, bytes, SLEEPING));
}


/*
 * \brief Get a LF-terminated line of text from the client.
 * (This is implemented in terms of client_read() and could be
 * justifiably moved out of sysdep.c)
 * \param sock socket fd to get client line from
 * \param buf buffer to write read data to
 * \param bufsiz how many bytes to read
 * \return  number of bytes read???
 */
int client_getln(int sock, char *buf, int bufsiz)
{
	int i, retval;

	/* Read one character at a time.*/
	for (i = 0;; i++) {
		retval = client_read(sock, &buf[i], 1);
		if (retval != 1 || buf[i] == '\n' || i == (bufsiz-1))
			break;
		if ( (!isspace(buf[i])) && (!isprint(buf[i])) ) {
			/* Non printable character recieved from client */
			return(-1);
		}
	}

	/* If we got a long line, discard characters until the newline. */
	if (i == (bufsiz-1))
		while (buf[i] != '\n' && retval == 1)
			retval = client_read(sock, &buf[i], 1);

	/*
	 * Strip any trailing non-printable characters.
	 */
	buf[i] = 0;
	while ((i > 0) && (!isprint(buf[i - 1]))) {
		buf[--i] = 0;
	}
	return (retval);
}

/*
 * \brief shut us down the regular way.
 * param signum the signal we want to forward
 */
pid_t current_child;
void graceful_shutdown_watcher(int signum) {
	lprintf (1, "bye; shutting down watcher.");
	kill(current_child, signum);
	if (signum != SIGHUP)
		exit(0);
}

/*
 * \brief shut us down the regular way.
 * param signum the signal we want to forward
 */
pid_t current_child;
void graceful_shutdown(int signum) {
//	kill(current_child, signum);
	char wd[SIZ];
	FILE *FD;
	int fd;
	getcwd(wd, SIZ);
	lprintf (1, "bye going down gracefull.[%d][%s]\n", signum, wd);
	fd = msock;
	msock = -1;
	time_to_die = 1;
	FD=fdopen(fd, "a+");
	fflush (FD);
	fclose (FD);
	close(fd);
}


/*
 * \brief	Start running as a daemon.  
 *
 * param	do_close_stdio		Only close stdio if set.
 */

/*
 * Start running as a daemon.
 */
void start_daemon(char *pid_file) 
{
	int status = 0;
	pid_t child = 0;
	FILE *fp;
	int do_restart = 0;

	current_child = 0;

	/* Close stdin/stdout/stderr and replace them with /dev/null.
	 * We don't just call close() because we don't want these fd's
	 * to be reused for other files.
	 */
	chdir("/");

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	child = fork();
	if (child != 0) {
		exit(0);
	}

	setsid();
	umask(0);
	freopen("/dev/null", "r", stdin);
	freopen("/dev/null", "w", stdout);
	freopen("/dev/null", "w", stderr);
	signal(SIGTERM, graceful_shutdown_watcher);
	signal(SIGHUP, graceful_shutdown_watcher);

	do {
		current_child = fork();

	
		if (current_child < 0) {
			perror("fork");
			ShutDownLibCitadel ();
			exit(errno);
		}
	
		else if (current_child == 0) {	// child process
//			signal(SIGTERM, graceful_shutdown);
			signal(SIGHUP, graceful_shutdown);

			return; /* continue starting webcit. */
		}
	
		else { // watcher process
//			signal(SIGTERM, SIG_IGN);
//			signal(SIGHUP, SIG_IGN);
			if (pid_file) {
				fp = fopen(pid_file, "w");
				if (fp != NULL) {
					fprintf(fp, "%d\n", getpid());
					fclose(fp);
				}
			}
			waitpid(current_child, &status, 0);
		}

		do_restart = 0;

		/* Did the main process exit with an actual exit code? */
		if (WIFEXITED(status)) {

			/* Exit code 0 means the watcher should exit */
			if (WEXITSTATUS(status) == 0) {
				do_restart = 0;
			}

			/* Exit code 101-109 means the watcher should exit */
			else if ( (WEXITSTATUS(status) >= 101) && (WEXITSTATUS(status) <= 109) ) {
				do_restart = 0;
			}

			/* Any other exit code means we should restart. */
			else {
				do_restart = 1;
			}
		}

		/* Any other type of termination (signals, etc.) should also restart. */
		else {
			do_restart = 1;
		}

	} while (do_restart);

	if (pid_file) {
		unlink(pid_file);
	}
	ShutDownLibCitadel ();
	exit(WEXITSTATUS(status));
}

/*
 * \brief	Spawn an additional worker thread into the pool.
 */
void spawn_another_worker_thread()
{
	pthread_t SessThread;	/*< Thread descriptor */
	pthread_attr_t attr;	/*< Thread attributes */
	int ret;

	lprintf(3, "Creating a new thread\n");

	/* set attributes for the new thread */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	/*
	 * Our per-thread stacks need to be bigger than the default size, otherwise
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

const char foobuf[32];
const char *nix(void *vptr) {snprintf(foobuf, 32, "%0x", (long) vptr); return foobuf;}
/*
 * \brief Here's where it all begins.
 * \param argc number of commandline args
 * \param argv the commandline arguments
 */
int main(int argc, char **argv)
{
	pthread_t SessThread;	/*< Thread descriptor */
	pthread_attr_t attr;	/*< Thread attributes */
	int a, i;	        	/*< General-purpose variables */
	char tracefile[PATH_MAX];
	char ip_addr[256]="0.0.0.0";
	char dirbuffer[PATH_MAX]="";
	int relh=0;
	int home=0;
	int home_specified=0;
	char relhome[PATH_MAX]="";
	char webcitdir[PATH_MAX] = DATADIR;
	char *pidfile = NULL;
	char *hdir;
	const char *basedir;
#ifdef ENABLE_NLS
	char *locale = NULL;
	char *mo = NULL;
#endif /* ENABLE_NLS */
	char uds_listen_path[PATH_MAX];	/*< listen on a unix domain socket? */

	HandlerHash = NewHash(1, NULL);
	initialise_modules();
	dbg_PrintHash(HandlerHash, nix, NULL);

	/* Ensure that we are linked to the correct version of libcitadel */
	if (libcitadel_version_number() < LIBCITADEL_VERSION_NUMBER) {
		fprintf(stderr, " You are running libcitadel version %d.%02d\n",
			(libcitadel_version_number() / 100), (libcitadel_version_number() % 100));
		fprintf(stderr, "WebCit was compiled against version %d.%02d\n",
			(LIBCITADEL_VERSION_NUMBER / 100), (LIBCITADEL_VERSION_NUMBER % 100));
		return(1);
	}

	strcpy(uds_listen_path, "");

	/* Parse command line */
#ifdef HAVE_OPENSSL
	while ((a = getopt(argc, argv, "h:i:p:t:x:dD:cfs")) != EOF)
#else
	while ((a = getopt(argc, argv, "h:i:p:t:x:dD:cf")) != EOF)
#endif
		switch (a) {
		case 'h':
			hdir = strdup(optarg);
			relh=hdir[0]!='/';
			if (!relh) safestrncpy(webcitdir, hdir,
								   sizeof webcitdir);
			else
				safestrncpy(relhome, relhome,
							sizeof relhome);
			/* free(hdir); TODO: SHOULD WE DO THIS? */
			home_specified = 1;
			home=1;
			break;
		case 'd':
			running_as_daemon = 1;
			break;
		case 'D':
			pidfile = strdup(optarg);
			running_as_daemon = 1;
			break;
		case 'i':
			safestrncpy(ip_addr, optarg, sizeof ip_addr);
			break;
		case 'p':
			http_port = atoi(optarg);
			if (http_port == 0) {
				safestrncpy(uds_listen_path, optarg, sizeof uds_listen_path);
			}
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
			fprintf(stderr, "usage: webcit "
				"[-i ip_addr] [-p http_port] "
				"[-t tracefile] [-c] [-f] "
				"[-d] "
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

	/* daemonize, if we were asked to */
	if (running_as_daemon) {
		start_daemon(pidfile);
	}
	else {
///		signal(SIGTERM, graceful_shutdown);
		signal(SIGHUP, graceful_shutdown);
	}

	/* Tell 'em who's in da house */
	lprintf(1, PACKAGE_STRING "\n");
	lprintf(1, "Copyright (C) 1996-2008 by the Citadel development team.\n"
		"This software is distributed under the terms of the "
		"GNU General Public License.\n\n"
	);


	/* initialize the International Bright Young Thing */
#ifdef ENABLE_NLS
	initialize_locales();

	locale = setlocale(LC_ALL, "");

	mo = malloc(strlen(webcitdir) + 20);
	lprintf(9, "Message catalog directory: %s\n", bindtextdomain("webcit", LOCALEDIR"/locale"));
	free(mo);
	lprintf(9, "Text domain: %s\n", textdomain("webcit"));
	lprintf(9, "Text domain Charset: %s\n", bind_textdomain_codeset("webcit","UTF8"));
	preset_locale();
#endif


	/* calculate all our path on a central place */
    /* where to keep our config */
	
#define COMPUTE_DIRECTORY(SUBDIR) memcpy(dirbuffer,SUBDIR, sizeof dirbuffer);\
	snprintf(SUBDIR,sizeof SUBDIR,  "%s%s%s%s%s%s%s", \
			 (home&!relh)?webcitdir:basedir, \
             ((basedir!=webcitdir)&(home&!relh))?basedir:"/", \
             ((basedir!=webcitdir)&(home&!relh))?"/":"", \
			 relhome, \
             (relhome[0]!='\0')?"/":"",\
			 dirbuffer,\
			 (dirbuffer[0]!='\0')?"/":"");
	basedir=RUNDIR;
	COMPUTE_DIRECTORY(socket_dir);
	basedir=WWWDIR "/static";
	COMPUTE_DIRECTORY(static_dir);
	basedir=WWWDIR "/static/icons";
	COMPUTE_DIRECTORY(static_icon_dir);
	basedir=WWWDIR "/static.local";
	COMPUTE_DIRECTORY(static_local_dir);

	snprintf(file_crpt_file_key,
		 sizeof file_crpt_file_key, 
		 "%s/citadel.key",
		 ctdl_key_dir);
	snprintf(file_crpt_file_csr,
		 sizeof file_crpt_file_csr, 
		 "%s/citadel.csr",
		 ctdl_key_dir);
	snprintf(file_crpt_file_cer,
		 sizeof file_crpt_file_cer, 
		 "%s/citadel.cer",
		 ctdl_key_dir);

	/* we should go somewhere we can leave our coredump, if enabled... */
	lprintf(9, "Changing directory to %s\n", socket_dir);
	if (chdir(webcitdir) != 0) {
		perror("chdir");
	}
	LoadIconDir(static_icon_dir);
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
	InitialiseSemaphores ();

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

	if (!IsEmptyStr(uds_listen_path)) {
		lprintf(2, "Attempting to create listener socket at %s...\n", uds_listen_path);
		msock = ig_uds_server(uds_listen_path, LISTEN_QUEUE_LENGTH);
	}
	else {
		lprintf(2, "Attempting to bind to port %d...\n", http_port);
		msock = ig_tcp_server(ip_addr, http_port, LISTEN_QUEUE_LENGTH);
	}

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
	ShutDownLibCitadel ();
	return 0;
}


/*
 * Entry point for worker threads
 */
void worker_entry(void)
{
	int ssock;
	int i = 0;
	int fail_this_transaction = 0;
	int ret;
	struct timeval tv;
	fd_set readset, tempset;

	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	FD_ZERO(&readset);
	FD_SET(msock, &readset);

	do {
		/* Only one thread can accept at a time */
		fail_this_transaction = 0;
		ssock = -1; 
		errno = EAGAIN;
		do {
			ret = -1; /* just one at once should select... */
			begin_critical_section(S_SELECT);

			FD_ZERO(&tempset);
			if (msock > 0) FD_SET(msock, &tempset);
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
			if (msock > 0)	ret = select(msock+1, &tempset, NULL, NULL,  &tv);
			end_critical_section(S_SELECT);
			if ((ret < 0) && (errno != EINTR) && (errno != EAGAIN))
			{// EINTR and EAGAIN are thrown but not of interest.
				lprintf(2, "accept() failed:%d %s\n",
					errno, strerror(errno));
			}
			else if ((ret > 0) && (msock > 0) && FD_ISSET(msock, &tempset))
			{// Successfully selected, and still not shutting down? Accept!
				ssock = accept(msock, NULL, 0);
			}
			
		} while ((msock > 0) && (ssock < 0)  && (time_to_die == 0));

		if ((msock == -1)||(time_to_die))
		{// ok, we're going down.
			int shutdown = 0;

			/* the first to come here will have to do the cleanup.
			 * make shure its realy just one.
			 */
			begin_critical_section(S_SHUTDOWN);
			if (msock == -1)
			{
				msock = -2;
				shutdown = 1;
			}
			end_critical_section(S_SHUTDOWN);
			if (shutdown == 1)
			{// we're the one to cleanup the mess.
				lprintf(2, "I'm master shutdown: tagging sessions to be killed.\n");
				shutdown_sessions();
				lprintf(2, "master shutdown: waiting for others\n");
				sleeeeeeeeeep(1); // wait so some others might finish...
				lprintf(2, "master shutdown: cleaning up sessions\n");
				do_housekeeping();
				lprintf(2, "master shutdown: cleaning up libical\n");
				free_zone_directory ();
				icaltimezone_release_zone_tab ();
				icalmemory_free_ring ();
				ShutDownLibCitadel ();
				lprintf(2, "master shutdown exiting!.\n");				
				exit(0);
			}
			break;
		}
		if (ssock < 0 ) continue;

		if (msock < 0) {
			if (ssock > 0) close (ssock);
			lprintf(2, "inbetween.");
			pthread_exit(NULL);
		} else { // Got it? do some real work!
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

				/* Shut down SSL/TLS if required... */
#ifdef HAVE_OPENSSL
				if (is_https) {
					endtls();
				}
#endif

				/* ...and close the socket. */
				lingering_close(ssock);
			}

		}

	} while (!time_to_die);

	lprintf (1, "bye\n");
	pthread_exit(NULL);
}

/*
 * \brief logprintf. log messages 
 * logs to stderr if loglevel is lower than the verbosity set at startup
 * \param loglevel level of the message
 * \param format the printf like format string
 * \param ... the strings to put into format
 */
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


/*
 * \brief print the actual stack frame.
 */
void wc_backtrace(void)
{
#ifdef HAVE_BACKTRACE
	void *stack_frames[50];
	size_t size, i;
	char **strings;


	size = backtrace(stack_frames, sizeof(stack_frames) / sizeof(void*));
	strings = backtrace_symbols(stack_frames, size);
	for (i = 0; i < size; i++) {
		if (strings != NULL)
			lprintf(1, "%s\n", strings[i]);
		else
			lprintf(1, "%p\n", stack_frames[i]);
	}
	free(strings);
#endif
}

/*@}*/
