/*
 * Copyright (c) 1996-2014 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "webcit.h"
#include "webserver.h"

#include "modules_init.h"

extern int msock;				/* master listening socket */
extern char static_icon_dir[PATH_MAX];          /* where should we find our mime icons */
int is_https = 0;				/* Nonzero if I am an HTTPS service */
int follow_xff = 0;				/* Follow X-Forwarded-For: header? */
int DisableGzip = 0;
char *default_landing_page = NULL;
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;

extern void *housekeeping_loop(void);
extern int webcit_tcp_server(char *ip_addr, int port_number, int queue_len);
extern int webcit_uds_server(char *sockpath, int queue_len);
extern void graceful_shutdown_watcher(int signum);
extern void graceful_shutdown(int signum);
extern void start_daemon(char *pid_file);
extern void webcit_calc_dirs_n_files(int relh, const char *basedir, int home, char *webcitdir, char *relhome);
extern void worker_entry(void);
extern void drop_root(uid_t UID);

char socket_dir[PATH_MAX];	/* where to talk to our citadel server */
char *server_cookie = NULL;	/* our Cookie connection to the client */
int http_port = PORT_NUM;	/* Port to listen on */
char *ctdlhost = DEFAULT_HOST;	/* Host name or IP address of Citadel server */
char *ctdlport = DEFAULT_PORT;	/* Port number of Citadel server */
int setup_wizard = 0;		/* should we run the setup wizard? */
char wizard_filename[PATH_MAX];	/* location of file containing the last webcit version against which we ran setup wizard */
int running_as_daemon = 0;	/* should we deamonize on startup? */

/* #define DBG_PRINNT_HOOKS_AT_START */
#ifdef DBG_PRINNT_HOOKS_AT_START
extern HashList *HandlerHash;
const char foobuf[32];
const char *nix(void *vptr) {snprintf(foobuf, 32, "%0x", (long) vptr); return foobuf;}
#endif 
extern int dbg_analyze_msg;
extern int dbg_backtrace_template_errors;
extern int DumpTemplateI18NStrings;
extern StrBuf *I18nDump;
void InitTemplateCache(void);
extern int LoadTemplates;


/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	uid_t UID = -1;
	size_t basesize = 2;            /* how big should strbufs be on creation? */
	pthread_t SessThread;		/* Thread descriptor */
	pthread_attr_t attr;		/* Thread attributes */
	int a;		        	/* General-purpose variable */
	char ip_addr[256]="*";
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char webcitdir[PATH_MAX] = DATADIR;
	char *pidfile = NULL;
	char *hdir;
	const char *basedir = NULL;
	char uds_listen_path[PATH_MAX];	/* listen on a unix domain socket? */
	const char *I18nDumpFile = NULL;

	WildFireInitBacktrace(argv[0], 2);

	start_modules();

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
	while ((a = getopt(argc, argv, "u:h:i:p:t:T:B:x:g:dD:G:cfsS:Z")) != EOF)
#else
	while ((a = getopt(argc, argv, "u:h:i:p:t:T:B:x:g:dD:G:cfZ")) != EOF)
#endif
		switch (a) {
		case 'u':
			UID = atol(optarg);
			break;
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
			home=1;
			break;
		case 'd':
			running_as_daemon = 1;
			break;
		case 'D':
			pidfile = strdup(optarg);
			running_as_daemon = 1;
			break;
		case 'g':
			default_landing_page = strdup(optarg);
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
			/* no longer used, but ignored so old scripts don't break */
			break;
		case 'T':
			LoadTemplates = atoi(optarg);
			dbg_analyze_msg = (LoadTemplates && (1<<1)) != 0;
			dbg_backtrace_template_errors = (LoadTemplates && (1<<2)) != 0;
			break;
		case 'Z':
			DisableGzip = 1;
			break;
		case 'x':
			/* no longer used, but ignored so old scripts don't break */
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
					syslog(LOG_INFO, "gethostname: %s", strerror(errno));
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
				"[-c] [-f] "
				"[-T Templatedebuglevel] "
				"[-d] [-Z] [-G i18ndumpfile] "
#ifdef HAVE_OPENSSL
				"[-s] [-S cipher_suites]"
#endif
				"[remotehost [remoteport]]\n");
			return 1;
		}

	/* Start the logger */
	openlog("webcit",
		( running_as_daemon ? (LOG_PID) : (LOG_PID | LOG_PERROR) ),
		LOG_DAEMON
	);

	if (optind < argc) {
		ctdlhost = argv[optind];
		if (++optind < argc)
			ctdlport = argv[optind];
	}

	/* daemonize, if we were asked to */
	if (!DumpTemplateI18NStrings && running_as_daemon) {
		start_daemon(pidfile);
	}
	else {
		signal(SIGINT, graceful_shutdown);
		signal(SIGHUP, graceful_shutdown);
	}

	webcit_calc_dirs_n_files(relh, basedir, home, webcitdir, relhome);
	LoadIconDir(static_icon_dir);

	/* Tell 'em who's in da house */
	syslog(LOG_NOTICE, "%s", PACKAGE_STRING);
	syslog(LOG_NOTICE, "Copyright (C) 1996-2014 by the citadel.org team");
	syslog(LOG_NOTICE, " ");
	syslog(LOG_NOTICE, "This program is open source software: you can redistribute it and/or");
	syslog(LOG_NOTICE, "modify it under the terms of the GNU General Public License, version 3.");
	syslog(LOG_NOTICE, " ");
	syslog(LOG_NOTICE, "This program is distributed in the hope that it will be useful,");
	syslog(LOG_NOTICE, "but WITHOUT ANY WARRANTY; without even the implied warranty of");
	syslog(LOG_NOTICE, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the");
	syslog(LOG_NOTICE, "GNU General Public License for more details.");
	syslog(LOG_NOTICE, " ");

	/* initialize various subsystems */

	initialise_modules();
	initialise2_modules();
	InitTemplateCache();
	if (DumpTemplateI18NStrings) {
		FILE *fd;
		StrBufAppendBufPlain(I18nDump, HKEY("}\n"), 0);
	        if (StrLength(I18nDump) < 50) {
			syslog(LOG_INFO, "*******************************************************************\n");
			syslog(LOG_INFO, "*   No strings found in templates!  Are you sure they're there?   *\n");
			syslog(LOG_INFO, "*******************************************************************\n");
			return -1;
		}
		fd = fopen(I18nDumpFile, "w");
	        if (fd == NULL) {
			syslog(LOG_INFO, "***********************************************\n");
			syslog(LOG_INFO, "*   unable to open I18N dumpfile [%s]         *\n", I18nDumpFile);
			syslog(LOG_INFO, "***********************************************\n");
			return -1;
		}
		fwrite(ChrPtr(I18nDump), 1, StrLength(I18nDump), fd);
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
		syslog(LOG_EMERG, "Can't create TSD key: %s", strerror(errno));
	}
	InitialiseSemaphores();

	/*
	 * Set up a place to put thread-specific SSL data.
	 * We don't stick this in the wcsession struct because SSL starts
	 * up before the session is bound, and it gets torn down between
	 * transactions.
	 */
#ifdef HAVE_OPENSSL
	if (pthread_key_create(&ThreadSSL, NULL) != 0) {
		syslog(LOG_EMERG, "Can't create TSD key: %s", strerror(errno));
	}
#endif

	/*
	 * Bind the server to our favorite port.
	 * There is no need to check for errors, because webcit_tcp_server()
	 * exits if it doesn't succeed.
	 */

	if (!IsEmptyStr(uds_listen_path)) {
		syslog(LOG_DEBUG, "Attempting to create listener socket at %s...", uds_listen_path);
		msock = webcit_uds_server(uds_listen_path, LISTEN_QUEUE_LENGTH);
	}
	else {
		syslog(LOG_DEBUG, "Attempting to bind to port %d...", http_port);
		msock = webcit_tcp_server(ip_addr, http_port, LISTEN_QUEUE_LENGTH);
	}
	if (msock < 0)
	{
		ShutDownWebcit();
		return -msock;
	}

	syslog(LOG_INFO, "Listening on socket %d", msock);
	signal(SIGPIPE, SIG_IGN);

	pthread_mutex_init(&SessionListMutex, NULL);

	/*
	 * Start up the housekeeping thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&SessThread, &attr, (void *(*)(void *)) housekeeping_loop, NULL);

	/*
	 * If this is an HTTPS server, fire up SSL
	 */
#ifdef HAVE_OPENSSL
	if (is_https) {
		init_ssl();
	}
#endif
	drop_root(UID);

	/* Become a worker thread.  More worker threads will be spawned as they are needed. */
	worker_entry();
	ShutDownLibCitadel();
	return 0;
}







