/*
 * citserver's main() function lives here.
 *
 * $Id$
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <syslog.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/un.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <grp.h>
#include <pwd.h>
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "database.h"
#include "user_ops.h"
#include "housekeeping.h"
#include "tools.h"
#include "citadel_dirs.c"

#include "modules_init.h"
#include "ecrash.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif
const char *CitadelServiceUDS="citadel-UDS";
const char *CitadelServiceTCP="citadel-TCP";

/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	char facility[32];
	int a, i;			/* General-purpose variables */
	struct passwd pw, *pwp = NULL;
	char pwbuf[SIZ];
	int drop_root_perms = 1;
	size_t size;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
#ifdef HAVE_RUN_DIR
	struct stat filestats;
#endif
#ifdef HAVE_BACKTRACE
	eCrashParameters params;
//	eCrashSymbolTable symbol_table;
#endif
	/* initialise semaphores here. Patch by Matt and davew
	 * its called here as they are needed by lprintf for thread safety
	 */
	InitialiseSemaphores();
	
	/* initialize the master context */
	InitializeMasterCC();

	/* parse command-line arguments */
	for (a=1; a<argc; ++a) {

		if (!strncmp(argv[a], "-l", 2)) {
			safestrncpy(facility, &argv[a][2], sizeof(facility));
			syslog_facility = SyslogFacility(facility);
			enable_syslog = 1;
		}

		/* run in the background if -d was specified */
		else if (!strcmp(argv[a], "-d")) {
			running_as_daemon = 1;
		}

		/* -x specifies the desired logging level */
		else if (!strncmp(argv[a], "-x", 2)) {
			verbosity = atoi(&argv[a][2]);
		}

		else if (!strncmp(argv[a], "-h", 2)) {
			relh=argv[a][2]!='/';
			if (!relh) safestrncpy(ctdl_home_directory, &argv[a][2],
								   sizeof ctdl_home_directory);
			else
				safestrncpy(relhome, &argv[a][2],
							sizeof relhome);
			home_specified = 1;
			home=1;
		}

		else if (!strncmp(argv[a], "-t", 2)) {
			freopen(&argv[a][2], "w", stderr);
		}

		else if (!strncmp(argv[a], "-f", 2)) {
			do_defrag = 1;
		}

		/* -r tells the server not to drop root permissions. don't use
		 * this unless you know what you're doing. this should be
		 * removed in the next release if it proves unnecessary. */
		else if (!strcmp(argv[a], "-r"))
			drop_root_perms = 0;

		/* any other parameter makes it crash and burn */
		else {
			lprintf(CTDL_EMERG,	"citserver: usage: "
					"citserver "
					"[-lLogFacility] "
					"[-d] [-f]"
					" [-tTraceFile]"
					" [-xLogLevel] [-hHomeDir]\n");
			exit(1);
		}

	}

	calc_dirs_n_files(relh, home, relhome, ctdldir);
	/* daemonize, if we were asked to */
	if (running_as_daemon) {
		start_daemon(0);
		drop_root_perms = 1;
	}

#ifdef HAVE_BACKTRACE
	bzero(&params, sizeof(params));
	params.filename = file_pid_paniclog;
	panic_fd=open(file_pid_paniclog, O_APPEND|O_CREAT|O_DIRECT);
	params.filep = fopen(file_pid_paniclog, "a+");
	params.debugLevel = ECRASH_DEBUG_VERBOSE;
	params.dumpAllThreads = TRUE;
	params.useBacktraceSymbols = 1;
///	BuildSymbolTable(&symbol_table);
//	params.symbolTable = &symbol_table;
	params.signals[0]=SIGSEGV;
	params.signals[1]=SIGILL;
	params.signals[2]=SIGBUS;
	params.signals[3]=SIGABRT;

	eCrash_Init(&params);
		
	eCrash_RegisterThread("MasterThread", 0);

///	signal(SIGSEGV, cit_panic_backtrace);
#endif
	/* Initialize the syslogger.  Yes, we are really using 0 as the
	 * facility, because we are going to bitwise-OR the facility to
	 * the severity of each message, allowing us to write to other
	 * facilities when we need to...
	 */
	if (enable_syslog) {
		openlog("citadel", LOG_NDELAY, 0);
		setlogmask(LOG_UPTO(verbosity));
	}
	
	/* Tell 'em who's in da house */
	lprintf(CTDL_NOTICE, "\n");
	lprintf(CTDL_NOTICE, "\n");
	lprintf(CTDL_NOTICE,
		"*** Citadel server engine v%d.%02d ***\n",
		(REV_LEVEL/100), (REV_LEVEL%100));
	lprintf(CTDL_NOTICE,
		"Copyright (C) 1987-2007 by the Citadel development team.\n");
	lprintf(CTDL_NOTICE,
		"This program is distributed under the terms of the GNU "
		"General Public License.\n");
	lprintf(CTDL_NOTICE, "\n");
	lprintf(CTDL_DEBUG, "Called as: %s\n", argv[0]);

	/* Load site-specific parameters, and set the ipgm secret */
	lprintf(CTDL_INFO, "Loading citadel.config\n");
	get_config();
	config.c_ipgm_secret = rand();
	put_config();

	lprintf(CTDL_INFO, "Acquiring control record\n");
	get_control();

#ifdef HAVE_RUN_DIR
	/* on some dists rundir gets purged on startup. so we need to recreate it. */

	if (stat(ctdl_run_dir, &filestats)==-1){
#ifdef SOLARIS_GETPWUID
		pwp = getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf));
#else
		getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf), &pwp);
#endif
		mkdir(ctdl_run_dir, 0755);
		chown(ctdl_run_dir, config.c_ctdluid, (pwp==NULL)?-1:pw.pw_gid);
	}
			

#endif

	/* Initialize... */
	init_sysdep();

	/*
	 * Do non system dependent startup functions.
	 */
	master_startup();

	/*
	 * Bind the server to a Unix-domain socket.
	 */
	CtdlRegisterServiceHook(0,
				file_citadel_socket,
				citproto_begin_session,
				do_command_loop,
				do_async_loop,
				CitadelServiceUDS);

	/*
	 * Bind the server to our favorite TCP port (usually 504).
	 */
	CtdlRegisterServiceHook(config.c_port_number,
				NULL,
				citproto_begin_session,
				do_command_loop,
				do_async_loop,
				CitadelServiceTCP);

	/*
	 * Load any server-side extensions available here.
	 */
	lprintf(CTDL_INFO, "Initializing server extensions\n");
	size = strlen(ctdl_home_directory) + 9;
	
/*
	initialize_server_extensions();
*/
	
	initialise_modules();
	
	

	/*
	 * If we need host auth, start our chkpwd daemon.
	 */
	if (config.c_auth_mode == 1) {
		start_chkpwd_daemon();
	}

	/*
	 * Now that we've bound the sockets, change to the Citadel user id and its
	 * corresponding group ids
	 */
	if (drop_root_perms) {
		cdb_chmod_data();	/* make sure we own our data files */

#ifdef SOLARIS_GETPWUID
		pwp = getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf));
#else
		getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf), &pwp);
#endif
		if (pwp == NULL)
			lprintf(CTDL_CRIT, "WARNING: getpwuid(%ld): %s\n"
				   "Group IDs will be incorrect.\n", (long)CTDLUID,
				strerror(errno));
		else {
			initgroups(pw.pw_name, pw.pw_gid);
			if (setgid(pw.pw_gid))
				lprintf(CTDL_CRIT, "setgid(%ld): %s\n", (long)pw.pw_gid,
					strerror(errno));
		}
		lprintf(CTDL_INFO, "Changing uid to %ld\n", (long)CTDLUID);
		if (setuid(CTDLUID) != 0) {
			lprintf(CTDL_CRIT, "setuid() failed: %s\n", strerror(errno));
		}
#if defined (HAVE_SYS_PRCTL_H) && defined (PR_SET_DUMPABLE)
		prctl(PR_SET_DUMPABLE, 1);
#endif
	}

	/* We want to check for idle sessions once per minute */
	CtdlRegisterSessionHook(terminate_idle_sessions, EVT_TIMER);

	/*
	 * Now create a bunch of worker threads.
	 */
	lprintf(CTDL_DEBUG, "Starting %d worker threads\n",
		config.c_min_workers-1);
	begin_critical_section(S_WORKER_LIST);
	for (i=0; i<(config.c_min_workers-1); ++i) {
		create_worker();
	}
	end_critical_section(S_WORKER_LIST);

	/* Create the maintenance threads. */
	create_maintenance_threads();

	/* This thread is now useless.  It can't be turned into a worker
	 * thread because its stack is too small, but it can't be killed
	 * either because the whole server process would exit.  So we just
	 * join to the first worker thread and exit when it exits.
	 */
	pthread_join(worker_list->tid, NULL);
	master_cleanup(0);
	return(0);
}
