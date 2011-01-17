/*
 * citserver's main() function lives here.
 * 
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
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
#ifdef HAVE_SYS_PRCTL_H
#include <sys/prctl.h>
#endif
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "threads.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "database.h"
#include "user_ops.h"
#include "housekeeping.h"
#include "svn_revision.h"
#include "citadel_dirs.h"

#include "context.h"

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



void go_threading(void);

/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	char facility[32];
	int a;			/* General-purpose variables */
	struct passwd pw, *pwp = NULL;
	char pwbuf[SIZ];
	int drop_root_perms = 1;
	size_t size;
	int relh=0;
	int home=0;
	int dbg=0;
	int have_log=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	int syslog_facility = LOG_DAEMON;
#ifdef HAVE_RUN_DIR
	struct stat filestats;
#endif
#ifdef HAVE_BACKTRACE
	eCrashParameters params;
//	eCrashSymbolTable symbol_table;
#endif

#ifdef HAVE_GC
	GC_INIT();
	GC_find_leak = 1;
#endif


	/* initialise semaphores here. Patch by Matt and davew
	 * its called here as they are needed by syslog for thread safety
	 */
	InitialiseSemaphores();
	
	/* initialize the master context */
	InitializeMasterCC();

	/* parse command-line arguments */
	for (a=1; a<argc; ++a) {

		if (!strncmp(argv[a], "-l", 2)) {
			safestrncpy(facility, &argv[a][2], sizeof(facility));
			syslog_facility = SyslogFacility(facility);
		}

		/* run in the background if -d was specified */
		else if (!strcmp(argv[a], "-d")) {
			running_as_daemon = 1;
		}

		/* run a few stats if -s was specified */
		else if (!strncmp(argv[a], "-s", 2)) {
			statcount = atoi(&argv[a][2]);
		}

		else if (!strncmp(argv[a], "-h", 2)) {
			relh=argv[a][2]!='/';
			if (!relh) safestrncpy(ctdl_home_directory, &argv[a][2],
								   sizeof ctdl_home_directory);
			else
				safestrncpy(relhome, &argv[a][2],
							sizeof relhome);
			home=1;
		}

		else if (!strncmp(argv[a], "-x", 2)) {
			/* deprecated */
		}

		else if (!strncmp(argv[a], "-t", 2)) {
			if (freopen(&argv[a][2], "w", stderr) != stderr)
			{
				syslog(LOG_EMERG, 
					      "unable to open your trace log [%s]: %s", 
					      &argv[a][2], 
					      strerror(errno));
				exit(1);
			}
			have_log = 1;
		}

		else if (!strncmp(argv[a], "-D", 2)) {
			dbg = 1;
		}

		/* -r tells the server not to drop root permissions. don't use
		 * this unless you know what you're doing. this should be
		 * removed in the next release if it proves unnecessary. */
		else if (!strcmp(argv[a], "-r"))
			drop_root_perms = 0;

		/* any other parameter makes it crash and burn */
		else {
			fprintf(stderr,	"citserver: usage: "
					"citserver "
					"[-lLogFacility] "
					"[-d] [-D] [-s] "
					"[-tTraceFile] "
					"[-hHomeDir]\n"
			);
			exit(1);
		}

	}

	openlog("citserver",
		( running_as_daemon ? (LOG_PID) : (LOG_PID | LOG_PERROR) ),
		syslog_facility
	);

	calc_dirs_n_files(relh, home, relhome, ctdldir, dbg);
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
	params.signals[0]=SIGSEGV;
	params.signals[1]=SIGILL;
	params.signals[2]=SIGBUS;
	params.signals[3]=SIGABRT;
	eCrash_Init(&params);
	eCrash_RegisterThread("MasterThread", 0);
#endif

	/* Tell 'em who's in da house */
	syslog(LOG_NOTICE, "\n");
	syslog(LOG_NOTICE, "\n");
	syslog(LOG_NOTICE,
		"*** Citadel server engine v%d.%02d (build %s) ***\n",
		(REV_LEVEL/100), (REV_LEVEL%100), svn_revision());
	syslog(LOG_NOTICE, "Copyright (C) 1987-2010 by the Citadel development team.\n");
	syslog(LOG_NOTICE, "This program is distributed under the terms of the GNU "
					"General Public License.\n");
	syslog(LOG_NOTICE, "\n");
	syslog(LOG_DEBUG, "Called as: %s\n", argv[0]);
	syslog(LOG_INFO, "%s\n", libcitadel_version_string());

	/* Load site-specific parameters, and set the ipgm secret */
	syslog(LOG_INFO, "Loading citadel.config\n");
	get_config();
	config.c_ipgm_secret = rand();

	/* get_control() MUST MUST MUST be called BEFORE the databases are opened!! */
	syslog(LOG_INFO, "Acquiring control record\n");
	get_control();

	put_config();

#ifdef HAVE_RUN_DIR
	/* on some dists rundir gets purged on startup. so we need to recreate it. */

	if (stat(ctdl_run_dir, &filestats)==-1){
#ifdef HAVE_GETPWUID_R
#ifdef SOLARIS_GETPWUID
		pwp = getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf));
#else // SOLARIS_GETPWUID
		getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf), &pwp);
#endif // SOLARIS_GETPWUID
#else // HAVE_GETPWUID_R
		pwp = NULL;
#endif // HAVE_GETPWUID_R

		if ((mkdir(ctdl_run_dir, 0755) != 0) && (errno != EEXIST))
			syslog(LOG_EMERG, 
				      "unable to create run directory [%s]: %s", 
				      ctdl_run_dir, strerror(errno));

		if (chown(ctdl_run_dir, config.c_ctdluid, (pwp==NULL)?-1:pw.pw_gid) != 0)
			syslog(LOG_EMERG, 
				      "unable to set the access rights for [%s]: %s", 
				      ctdl_run_dir, strerror(errno));
	}
			

#endif

	/* Initialize... */
	init_sysdep();

	/*
	 * Do non system dependent startup functions.
	 */
	master_startup();

	/*
	 * Check that the control record is correct and place sensible values if it isn't
	 */
	check_control();
	
	/*
	 * Run any upgrade entry points
	 */
	syslog(LOG_INFO, "Upgrading modules.\n");
	upgrade_modules();
	
/**
 * Load the user for the masterCC or create them if they don't exist
 */
	if (CtdlGetUser(&masterCC.user, "SYS_Citadel"))
	{
		/** User doesn't exist. We can't use create user here as the user number needs to be 0 */
		strcpy (masterCC.user.fullname, "SYS_Citadel") ;
		CtdlPutUser(&masterCC.user);
		CtdlGetUser(&masterCC.user, "SYS_Citadel"); /** Just to be safe */
	}
	
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
	syslog(LOG_INFO, "Initializing server extensions\n");
	size = strlen(ctdl_home_directory) + 9;
	
	initialise_modules(0);
	
	

	/*
	 * If we need host auth, start our chkpwd daemon.
	 */
	if (config.c_auth_mode == AUTHMODE_HOST) {
		start_chkpwd_daemon();
	}


	/*
	 * check, whether we're fired up another time after a crash.
	 * if, post an aide message, so the admin has a chance to react.
	 */
	checkcrash ();


	/*
	 * Now that we've bound the sockets, change to the Citadel user id and its
	 * corresponding group ids
	 */
	if (drop_root_perms) {
		cdb_chmod_data();	/* make sure we own our data files */

#ifdef HAVE_GETPWUID_R
#ifdef SOLARIS_GETPWUID
		pwp = getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf));
#else // SOLARIS_GETPWUID
		getpwuid_r(config.c_ctdluid, &pw, pwbuf, sizeof(pwbuf), &pwp);
#endif // SOLARIS_GETPWUID
#else // HAVE_GETPWUID_R
		pwp = NULL;
#endif // HAVE_GETPWUID_R

		if (pwp == NULL)
			syslog(LOG_CRIT, "WARNING: getpwuid(%ld): %s\n"
				   "Group IDs will be incorrect.\n", (long)CTDLUID,
				strerror(errno));
		else {
			initgroups(pw.pw_name, pw.pw_gid);
			if (setgid(pw.pw_gid))
				syslog(LOG_CRIT, "setgid(%ld): %s\n", (long)pw.pw_gid,
					strerror(errno));
		}
		syslog(LOG_INFO, "Changing uid to %ld\n", (long)CTDLUID);
		if (setuid(CTDLUID) != 0) {
			syslog(LOG_CRIT, "setuid() failed: %s\n", strerror(errno));
		}
#if defined (HAVE_SYS_PRCTL_H) && defined (PR_SET_DUMPABLE)
		prctl(PR_SET_DUMPABLE, 1);
#endif
	}

	/* We want to check for idle sessions once per minute */
	CtdlRegisterSessionHook(terminate_idle_sessions, EVT_TIMER);

	go_threading();
	
	
	master_cleanup(exit_signal);
	return(0);
}
