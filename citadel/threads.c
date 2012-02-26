/*
 * Thread handling stuff for Citadel server
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

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <syslog.h>

#include "sysdep.h"
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

#ifdef HAVE_SYSCALL_H
# include <syscall.h>
#else 
# if HAVE_SYS_SYSCALL_H
#  include <sys/syscall.h>
# endif
#endif

#include <libcitadel.h>

#include "threads.h"
#include "ctdl_module.h"
#include "modules_init.h"
#include "housekeeping.h"
#include "config.h"
#include "citserver.h"
#include "sysdep_decls.h"
#include "context.h"
#include "event_client.h"


int num_workers = 0;				/* Current number of worker threads */
int active_workers = 0;				/* Number of ACTIVE worker threads */
pthread_key_t ThreadKey;
pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
struct thread_tsd masterTSD;
int server_shutting_down = 0;			/* set to nonzero during shutdown */



void InitializeSemaphores(void)
{
	int i;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (i=0; i<MAX_SEMAPHORES; ++i) {
		pthread_mutex_init(&Critters[i], NULL);
	}
}




/*
 * Obtain a semaphore lock to begin a critical section.
 * but only if no one else has one
 */
int try_critical_section(int which_one)
{
	/* For all types of critical sections except those listed here,
	 * ensure nobody ever tries to do a critical section within a
	 * transaction; this could lead to deadlock.
	 */
	if (	(which_one != S_FLOORCACHE)
		&& (which_one != S_RPLIST)
	) {
		cdb_check_handles();
	}
	return (pthread_mutex_trylock(&Critters[which_one]));
}


/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	/* For all types of critical sections except those listed here,
	 * ensure nobody ever tries to do a critical section within a
	 * transaction; this could lead to deadlock.
	 */
	if (	(which_one != S_FLOORCACHE)
		&& (which_one != S_RPLIST)
	) {
		cdb_check_handles();
	}
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	pthread_mutex_unlock(&Critters[which_one]);
}




/*
 * Return a pointer to our thread-specific (not session-specific) data.
 */ 
struct thread_tsd *MyThread(void) {
        register struct thread_tsd *c;
        return ((c = (struct thread_tsd *) pthread_getspecific(ThreadKey), c == NULL) ? &masterTSD : c);
}



/* 
 * Called by CtdlThreadCreate()
 * We have to pass through here before starting our thread in order to create a set of data
 * that is thread-specific rather than session-specific.
 */
void *CTC_backend(void *supplied_start_routine)
{
	struct thread_tsd *mytsd;
	void *(*start_routine)(void*) = supplied_start_routine;

	mytsd = (struct thread_tsd *) malloc(sizeof(struct thread_tsd));
	memset(mytsd, 0, sizeof(struct thread_tsd));
	pthread_setspecific(ThreadKey, (const void *) mytsd);

	start_routine(NULL);

	free(mytsd);
	return(NULL);
}

 
/*
 * Function to create a thread.
 */ 
void CtdlThreadCreate(void *(*start_routine)(void*))
{
	pthread_t thread;
	pthread_attr_t attr;
	int ret = 0;


	ret = pthread_attr_init(&attr);
	ret = pthread_attr_setstacksize(&attr, THREADSTACKSIZE);
	ret = pthread_create(&thread, &attr, CTC_backend, (void *)start_routine);
	if (ret != 0) syslog(LOG_EMERG, "pthread_create() : %s", strerror(errno));
}


void InitializeMasterTSD(void) {
	memset(&masterTSD, 0, sizeof(struct thread_tsd));
}

extern void ShutDownEventQueues(void);

int EventQShuttingDown = 0;
int EVQShutDown = 0;
/*
 * Initialize the thread system
 */
void go_threading(void)
{
	if (pthread_key_create(&ThreadKey, NULL) != 0) {
		syslog(LOG_EMERG, "pthread_key_create() : %s", strerror(errno));
		abort();
	}

	/* Second call to module init functions now that threading is up */
	initialise_modules(1);

	/* Begin with one worker thread.  We will expand the pool if necessary */
	CtdlThreadCreate(worker_thread);

	/* The supervisor thread monitors worker threads and spawns more of them if it finds that
	 * they are all in use.  FIXME make the 256 max threads a configurable value.
	 */
	while (!server_shutting_down) {
		if ((active_workers == num_workers) && (num_workers < 256)) {
			CtdlThreadCreate(worker_thread);
		}
		usleep(1000);
	}

	/* When we get to this point we are getting ready to shut down our Citadel server */
	if (!EventQShuttingDown)
	{
		EventQShuttingDown = 1;
		ShutDownEventQueues();
	}
	while (!EVQShutDown)
		usleep(1000);


	terminate_all_sessions();		/* close all client sockets */
	CtdlShutdownServiceHooks();		/* close all listener sockets to prevent new connections */
	PerformSessionHooks(EVT_SHUTDOWN);	/* run any registered shutdown hooks */

	int countdown = 30;
	while ( (num_workers > 0) && (countdown-- > 0)) {
		syslog(LOG_DEBUG, "Waiting %d seconds for %d worker threads to exit",
			countdown, num_workers
		);
		usleep(1000);
	}
}
