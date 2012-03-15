/*
 * This contains a simple multithreaded TCP server manager.  It sits around
 * waiting on the specified port for incoming HTTP connections.  When a
 * connection is established, it calls context_loop() from context_loop.c.
 *
 * Copyright (c) 1996-2012 by the citadel.org developers.
 * This program is released under the terms of the GNU General Public License v3.
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

extern int msock;			/* master listening socket */
extern int verbosity;		/* Logging level */
extern char static_icon_dir[PATH_MAX];          /* where should we find our mime icons */

int is_https = 0;		/* Nonzero if I am an HTTPS service */
int follow_xff = 0;		/* Follow X-Forwarded-For: header */
int home_specified = 0;		/* did the user specify a homedir? */
int DisableGzip = 0;
extern pthread_mutex_t SessionListMutex;
extern pthread_key_t MyConKey;

extern void run_tests(void);
extern int ig_tcp_server(char *ip_addr, int port_number, int queue_len);
extern int ig_uds_server(char *sockpath, int queue_len);
extern void webcit_calc_dirs_n_files(int relh, const char *basedir, int home, char *webcitdir, char *relhome);


char socket_dir[PATH_MAX];			/* where to talk to our citadel server */

char *server_cookie = NULL;	/* our Cookie connection to the client */
int http_port = PORT_NUM;	/* Port to listen on */
char *ctdlhost = DEFAULT_HOST;	/* our name */
char *ctdlport = DEFAULT_PORT;	/* our Port */
int running_as_daemon = 0;	/* should we deamonize on startup? */
char wizard_filename[PATH_MAX];

StrBuf *Username = NULL;                 /* the test user... */
StrBuf *Passvoid = NULL;                 /* the test user... */


extern int dbg_analyze_msg;
extern int dbg_bactrace_template_errors;
extern int DumpTemplateI18NStrings;
extern StrBuf *I18nDump;
void InitTemplateCache(void);
extern int LoadTemplates;



/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	size_t basesize = 2;            /* how big should strbufs be on creation? */
	pthread_attr_t attr;		/* Thread attributes */
	int a;	        	/* General-purpose variables */
	char tracefile[PATH_MAX];
	char ip_addr[256]="0.0.0.0";
	int relh=0;
	int home=0;
	int home_specified=0;
	char relhome[PATH_MAX]="";
	char webcitdir[PATH_MAX] = DATADIR;
	char *hdir;
	const char *basedir = NULL;
	char uds_listen_path[PATH_MAX];	/* listen on a unix domain socket? */
	FILE *rvfp = NULL;
	
	WildFireInitBacktrace(argv[0], 2);

	start_modules ();

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
	while ((a = getopt(argc, argv, "h:i:p:t:T:B:x:U:P:cf:Z")) != EOF)
		switch (a) {
		case 'U':
			Username = NewStrBufPlain(optarg, -1);
			break;
		case 'P':
			Passvoid = NewStrBufPlain(optarg, -1);
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
			home_specified = 1;
			home=1;
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
					syslog(2, "gethostname: %s\n",
						strerror(errno));
					free(server_cookie);
				}
			}
			break;
		default:
			fprintf(stderr, "usage: webcit "
				"[-i ip_addr] [-p http_port] "
				"[-t tracefile] [-c] [-f] "
				"[-T Templatedebuglevel] "
				"[-Z] [-G i18ndumpfile] "
#ifdef HAVE_OPENSSL
				"[-s] [-S cipher_suites]"
#endif
				"[-U Username -P Password]"
				""
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
	syslog(1, PACKAGE_STRING "\n");
	syslog(1, "Copyright (C) 1996-2009 by the Citadel development team.\n"
		"This software is distributed under the terms of the "
		"GNU General Public License.\n\n"
	);


	/* initialize the International Bright Young Thing */

	initialise_modules();
	initialize_viewdefs();
	initialize_axdefs();

	InitTemplateCache();

	/* Use our own prefix on tzid's generated from system tzdata */
	icaltimezone_set_tzid_prefix("/citadel.org/");

	/*
	 * Set up a place to put thread-specific data.
	 * We only need a single pointer per thread - it points to the
	 * wcsession struct to which the thread is currently bound.
	 */
	if (pthread_key_create(&MyConKey, NULL) != 0) {
		syslog(1, "Can't create TSD key: %s\n", strerror(errno));
	}
	InitialiseSemaphores ();


	/*
	 * Start up the housekeeping thread
	 */
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

	run_tests();

	ShutDownWebcit();
	FreeStrBuf(&Username);
	FreeStrBuf(&Passvoid);

	return 0;
}


