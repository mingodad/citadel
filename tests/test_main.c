/*
 * $Id: webserver.c 7847 2009-10-03 17:57:14Z dothebart $
 *
 * This contains a simple multithreaded TCP server manager.  It sits around
 * waiting on the specified port for incoming HTTP connections.  When a
 * connection is established, it calls context_loop() from context_loop.c.
 *
 * Copyright (c) 1996-2009 by the citadel.org developers.
 * This program is released under the terms of the GNU General Public License v3.
 *
 */

#include "../webcit.h"
#include "../webserver.h"
#include "../modules_init.h"


#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>
#include <CUnit/TestDB.h>


CU_EXPORT void         CU_automated_run_tests(void);
CU_EXPORT CU_ErrorCode CU_list_tests_to_file(void);
CU_EXPORT void         CU_set_output_filename(const char* szFilenameRoot);


#if HAVE_BACKTRACE
#include <execinfo.h>
#endif
#ifndef HAVE_SNPRINTF
int vsnprintf(char *buf, size_t max, const char *fmt, va_list argp);
#endif

int verbosity = 9;		/* Logging level */
int msock;			/* master listening socket */
int is_https = 0;		/* Nonzero if I am an HTTPS service */
int follow_xff = 0;		/* Follow X-Forwarded-For: header */
int home_specified = 0;		/* did the user specify a homedir? */
int time_to_die = 0;            /* Nonzero if server is shutting down */
int DisableGzip = 0;
extern void *context_loop(ParsedHttpHdrs *Hdr);
extern void *housekeeping_loop(void);
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;

extern int ig_tcp_server(char *ip_addr, int port_number, int queue_len);
extern int ig_uds_server(char *sockpath, int queue_len);


char ctdl_key_dir[PATH_MAX]=SSL_DIR;
char file_crpt_file_key[PATH_MAX]="";
char file_crpt_file_csr[PATH_MAX]="";
char file_crpt_file_cer[PATH_MAX]="";

char socket_dir[PATH_MAX];			/* where to talk to our citadel server */
const char editor_absolut_dir[PATH_MAX]=EDITORDIR;	/* nailed to what configure gives us. */
char static_dir[PATH_MAX];		/* calculated on startup */
char static_local_dir[PATH_MAX];		/* calculated on startup */
char static_icon_dir[PATH_MAX];          /* where should we find our mime icons? */
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






/* #define DBG_PRINNT_HOOKS_AT_START */
#ifdef DBG_PRINNT_HOOKS_AT_START
const char foobuf[32];
const char *nix(void *vptr) {snprintf(foobuf, 32, "%0x", (long) vptr); return foobuf;}
#endif 
extern int dbg_analyze_msg;
extern int dbg_bactrace_template_errors;
extern int DumpTemplateI18NStrings;
extern StrBuf *I18nDump;
void InitTemplateCache(void);
extern int LoadTemplates;

extern HashList *HandlerHash;



void
webcit_calc_dirs_n_files(int relh, const char *basedir, int home, char *webcitdir, char *relhome)
{
	char dirbuffer[PATH_MAX]="";
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
	StripSlashes(static_dir, 1);
	StripSlashes(static_icon_dir, 1);
	StripSlashes(static_local_dir, 1);

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
}
/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	size_t basesize = 2;            /* how big should strbufs be on creation? */
	pthread_t SessThread;		/* Thread descriptor */
	pthread_attr_t attr;		/* Thread attributes */
	int a, i;	        	/* General-purpose variables */
	char tracefile[PATH_MAX];
	char ip_addr[256]="0.0.0.0";
	int relh=0;
	int home=0;
	int home_specified=0;
	char relhome[PATH_MAX]="";
	char webcitdir[PATH_MAX] = DATADIR;
	char *pidfile = NULL;
	char *hdir;
	const char *basedir = NULL;
	char uds_listen_path[PATH_MAX];	/* listen on a unix domain socket? */
	const char *I18nDumpFile = NULL;
	FILE *rvfp = NULL;
	int rv = 0;

	WildFireInitBacktrace(argv[0], 2);

	start_modules ();

#ifdef DBG_PRINNT_HOOKS_AT_START
/*	dbg_PrintHash(HandlerHash, nix, NULL);*/
#endif

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
	while ((a = getopt(argc, argv, "h:i:p:t:T:B:x:dD:G:cfsS:Z")) != EOF)
#else
	while ((a = getopt(argc, argv, "h:i:p:t:T:B:x:dD:G:cfZ")) != EOF)
#endif
		switch (a) {
		case 'h':
			hdir = strdup(optarg);
			relh=hdir[0]!='/';
			if (!relh) {
				safestrncpy(webcitdir, hdir, sizeof webcitdir);
			}
			else {
				safestrncpy(relhome, relhome, sizeof relhome);
			}
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
		case 'B': /* Basesize */
			basesize = atoi(optarg);
			if (basesize > 2)
				StartLibCitadel(basesize);
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
			rvfp = freopen(tracefile, "w", stdout);
			rvfp = freopen(tracefile, "w", stderr);
			rvfp = freopen(tracefile, "r", stdin);
			break;
		case 'T':
			LoadTemplates = atoi(optarg);
			dbg_analyze_msg = (LoadTemplates && (1<<1)) != 0;
			dbg_bactrace_template_errors = (LoadTemplates && (1<<2)) != 0;
			break;
		case 'Z':
			DisableGzip = 1;
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
#ifdef HAVE_OPENSSL
		case 's':
			is_https = 1;
			break;
		case 'S':
			is_https = 1;
			ssl_cipher_list = strdup(optarg);
			break;
#endif
		case 'G':
			DumpTemplateI18NStrings = 1;
			I18nDump = NewStrBufPlain(HKEY("int templatestrings(void)\n{\n"));
			I18nDumpFile = optarg;
			break;
		default:
			fprintf(stderr, "usage: webcit "
				"[-i ip_addr] [-p http_port] "
				"[-t tracefile] [-c] [-f] "
				"[-T Templatedebuglevel] "
				"[-d] [-Z] [-G i18ndumpfile] "
#ifdef HAVE_OPENSSL
				"[-s] [-S cipher_suites]"
#endif
				"[remotehost [remoteport]]\n");
			return 1;
		}

	if (optind < argc) {
		ctdlhost = argv[optind];
		if (++optind < argc)
			ctdlport = argv[optind];
	}

	webcit_calc_dirs_n_files(relh, basedir, home, webcitdir, relhome);
	LoadIconDir(static_icon_dir);

	/* Tell 'em who's in da house */
	lprintf(1, PACKAGE_STRING "\n");
	lprintf(1, "Copyright (C) 1996-2009 by the Citadel development team.\n"
		"This software is distributed under the terms of the "
		"GNU General Public License.\n\n"
	);


	/* initialize the International Bright Young Thing */

	initialise_modules();
	initialize_viewdefs();
	initialize_axdefs();

	InitTemplateCache();
	if (DumpTemplateI18NStrings) {
		FILE *fd;
		StrBufAppendBufPlain(I18nDump, HKEY("}\n"), 0);
	        if (StrLength(I18nDump) < 50) {
			lprintf(1, "********************************************************************************\n");
			lprintf(1, "*        No strings found in templates! are you shure they're there?           *\n");
			lprintf(1, "********************************************************************************\n");
			return -1;
		}
		fd = fopen(I18nDumpFile, "w");
	        if (fd == NULL) {
			lprintf(1, "********************************************************************************\n");
			lprintf(1, "*                  unable to open I18N dumpfile [%s]         *\n", I18nDumpFile);
			lprintf(1, "********************************************************************************\n");
			return -1;
		}
		rv = fwrite(ChrPtr(I18nDump), 1, StrLength(I18nDump), fd);
		fclose(fd);
		return 0;
	}


	/* Tell libical to return an error instead of aborting if it sees badly formed iCalendar data. */
	icalerror_errors_are_fatal = 0;

	/* Use our own prefix on tzid's generated from system tzdata */
	icaltimezone_set_tzid_prefix("/citadel.org/");

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
	if (msock < 0)
	{
		ShutDownWebcit();
		return -msock;
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

	/* now the original thread becomes another worker */
	worker_entry();
	ShutDownLibCitadel ();
	return 0;
}


void ShutDownWebcit(void)
{
	free_zone_directory ();
	icaltimezone_release_zone_tab ();
	icalmemory_free_ring ();
	ShutDownLibCitadel ();
	shutdown_modules ();
#ifdef HAVE_OPENSSL
	if (is_https) {
		shutdown_ssl();
	}
#endif
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
	ParsedHttpHdrs Hdr;

	memset(&Hdr, 0, sizeof(ParsedHttpHdrs));
	Hdr.HR.eReqType = eGET;
	http_new_modules(&Hdr);	
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
			{/* EINTR and EAGAIN are thrown but not of interest. */
				lprintf(2, "accept() failed:%d %s\n",
					errno, strerror(errno));
			}
			else if ((ret > 0) && (msock > 0) && FD_ISSET(msock, &tempset))
			{/* Successfully selected, and still not shutting down? Accept! */
				ssock = accept(msock, NULL, 0);
			}
			
		} while ((msock > 0) && (ssock < 0)  && (time_to_die == 0));

		if ((msock == -1)||(time_to_die))
		{/* ok, we're going down. */
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
			{/* we're the one to cleanup the mess. */
				http_destroy_modules(&Hdr);
				lprintf(2, "I'm master shutdown: tagging sessions to be killed.\n");
				shutdown_sessions();
				lprintf(2, "master shutdown: waiting for others\n");
				sleeeeeeeeeep(1); /* wait so some others might finish... */
				lprintf(2, "master shutdown: cleaning up sessions\n");
				do_housekeeping();
				lprintf(2, "master shutdown: cleaning up libical\n");

				ShutDownWebcit();

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
		} else { /* Got it? do some real work! */
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
			else 
#endif
			{
				int fdflags; 
				fdflags = fcntl(ssock, F_GETFL);
				if (fdflags < 0)
					lprintf(1, "unable to get server socket flags! %s \n",
						strerror(errno));
				fdflags = fdflags | O_NONBLOCK;
				if (fcntl(ssock, F_SETFL, fdflags) < 0)
					lprintf(1, "unable to set server socket nonblocking flags! %s \n",
						strerror(errno));
			}

			if (fail_this_transaction == 0) {
				Hdr.http_sock = ssock;

				/* Perform an HTTP transaction... */
				context_loop(&Hdr);

				/* Shut down SSL/TLS if required... */
#ifdef HAVE_OPENSSL
				if (is_https) {
					endtls();
				}
#endif

				/* ...and close the socket. */
				if (Hdr.http_sock > 0)
					lingering_close(ssock);
				http_detach_modules(&Hdr);

			}

		}

	} while (!time_to_die);

	http_destroy_modules(&Hdr);
	lprintf (1, "bye\n");
	pthread_exit(NULL);
}

/*
 * print log messages 
 * logs to stderr if loglevel is lower than the verbosity set at startup
 *
 * loglevel	level of the message
 * format	the printf like format string
 * ...		the strings to put into format
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
 * print the actual stack frame.
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




void spawn_another_worker_thread()
{/* do nothing... */}
