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
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "tools.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

/*
 * Here's where it all begins.
 */
int main(int argc, char **argv)
{
	char tracefile[128];		/* Name of file to log traces to */
	int a, i;			/* General-purpose variables */
	struct passwd *pw;
	int drop_root_perms = 1;
	struct worker_node *wnp;
	size_t size;
        
	/* specify default port name and trace file */
	strcpy(tracefile, "");

	/* initialize the master context */
	InitializeMasterCC();

	/* parse command-line arguments */
	for (a=1; a<argc; ++a) {

		/* -t specifies where to log trace messages to */
		if (!strncmp(argv[a], "-t", 2)) {
			safestrncpy(tracefile, argv[a], sizeof tracefile);
			strcpy(tracefile, &tracefile[2]);
			freopen(tracefile, "r", stdin);
			freopen(tracefile, "w", stdout);
			freopen(tracefile, "w", stderr);
			chmod(tracefile, 0600);
		}

		else if (!strncmp(argv[a], "-l", 2)) {
			safestrncpy(tracefile, argv[a], sizeof tracefile);
			strcpy(tracefile, &tracefile[2]);
			syslog_facility = SyslogFacility(tracefile);
			if (syslog_facility >= 0) {
				openlog("citadel", LOG_PID, syslog_facility);
			}
		}

		/* run in the background if -d was specified */
		else if (!strcmp(argv[a], "-d")) {
			start_daemon( (strlen(tracefile) > 0) ? 0 : 1 ) ;
		}

		/* -x specifies the desired logging level */
		else if (!strncmp(argv[a], "-x", 2)) {
			verbosity = atoi(&argv[a][2]);
		}

		else if (!strncmp(argv[a], "-h", 2)) {
			safestrncpy(bbs_home_directory, &argv[a][2],
				    sizeof bbs_home_directory);
			home_specified = 1;
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
			lprintf(1,	"citserver: usage: "
					"citserver [-tTraceFile] "
					"[-lLogFacility] "
					"[-d] [-f]"
					" [-xLogLevel] [-hHomeDir]\n");
			exit(1);
		}

	}

	/* Tell 'em who's in da house */
	lprintf(1, "\n");
	lprintf(1, "\n");
	lprintf(1,"*** Citadel/UX messaging server engine v%d.%02d ***\n",
		(REV_LEVEL/100),
		(REV_LEVEL%100) );
	lprintf(1,"Copyright (C) 1987-2003 by the Citadel/UX development team.\n");
	lprintf(1,"This program is distributed under the terms of the GNU ");
	lprintf(1,"General Public License.\n");
	lprintf(1, "\n");

	/* Initialize... */
	init_sysdep();

	/* Load site-specific parameters, and set the ipgm secret */
	lprintf(7, "Loading citadel.config\n");
	get_config();
	config.c_ipgm_secret = rand();
	put_config();

	/*
	 * Do non system dependent startup functions.
	 */
	master_startup();

	/*
	 * Bind the server to a Unix-domain socket.
	 */
	CtdlRegisterServiceHook(0,
				"citadel.socket",
				citproto_begin_session,
				do_command_loop);

	/*
	 * Bind the server to our favorite TCP port (usually 504).
	 */
	CtdlRegisterServiceHook(config.c_port_number,
				NULL,
				citproto_begin_session,
				do_command_loop);

	/*
	 * Load any server-side extensions available here.
	 */
	lprintf(7, "Initializing server extensions\n");
	size = strlen(bbs_home_directory) + 9;
	initialize_server_extensions();

	/*
	 * The rescan pipe exists so that worker threads can be woken up and
	 * told to re-scan the context list for fd's to listen on.  This is
	 * necessary, for example, when a context is about to go idle and needs
	 * to get back on that list.
	 */
	if (pipe(rescan)) {
		lprintf(1, "Can't create rescan pipe!\n");
		exit(errno);
	}

	init_master_fdset();

	/*
	 * Now that we've bound the sockets, change to the BBS user id and its
	 * corresponding group ids
	 */
	if (drop_root_perms) {
		if ((pw = getpwuid(BBSUID)) == NULL)
			lprintf(1, "WARNING: getpwuid(%ld): %s\n"
				   "Group IDs will be incorrect.\n", (long)BBSUID,
				strerror(errno));
		else {
			initgroups(pw->pw_name, pw->pw_gid);
			if (setgid(pw->pw_gid))
				lprintf(3, "setgid(%ld): %s\n", (long)pw->pw_gid,
					strerror(errno));
		}
		lprintf(7, "Changing uid to %ld\n", (long)BBSUID);
		if (setuid(BBSUID) != 0) {
			lprintf(3, "setuid() failed: %s\n", strerror(errno));
		}
	}

	/* We want to check for idle sessions once per minute */
	CtdlRegisterSessionHook(terminate_idle_sessions, EVT_TIMER);

	/*
	 * Now create a bunch of worker threads.
	 */
	lprintf(9, "Starting %d worker threads\n", config.c_min_workers-1);
	begin_critical_section(S_WORKER_LIST);
	for (i=0; i<(config.c_min_workers-1); ++i) {
		create_worker();
	}
	end_critical_section(S_WORKER_LIST);

	/* Now this thread can become a worker as well. */
	initial_thread = pthread_self();
	worker_thread(NULL);

	/* Server is exiting. Wait for workers to shutdown. */
	lprintf(7, "Waiting for worker threads to shut down\n");

	begin_critical_section(S_WORKER_LIST);
	while (worker_list != NULL) {
		wnp = worker_list;
		worker_list = wnp->next;

		/* avoid deadlock with an exiting thread */
		end_critical_section(S_WORKER_LIST);
		if ((i = pthread_join(wnp->tid, NULL)))
			lprintf(1, "pthread_join: %s\n", strerror(i));
		phree(wnp);
		begin_critical_section(S_WORKER_LIST);
	}
	end_critical_section(S_WORKER_LIST);

	master_cleanup();

	return(0);
}
