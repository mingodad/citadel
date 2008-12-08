/*
 * $Id: sysdep.c 5691 2007-11-04 23:19:17Z dothebart $
 *
 * Citadel "system dependent" stuff.
 * See copyright.txt for copyright information.
 *
 * Here's where we (hopefully) have most parts of the Citadel server that
 * would need to be altered to run the server in a non-POSIX environment.
 * 
 * If we ever port to a different platform and either have multiple
 * variants of this file or simply load it up with #ifdefs.
 *
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
#include <sys/syslog.h>

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
#include <sys/resource.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
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
#include "webcit.h"
#include "sysdep.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif

pthread_mutex_t Critters[MAX_SEMAPHORES];	/* Things needing locking */
pthread_key_t MyConKey;				/* TSD key for MyContext() */
pthread_key_t MyReq;				/* TSD key for MyReq() */

void InitialiseSemaphores(void)
{
	int i;

	/* Set up a bunch of semaphores to be used for critical sections */
	for (i=0; i<MAX_SEMAPHORES; ++i) {
		pthread_mutex_init(&Critters[i], NULL);
	}
}

/*
 * Obtain a semaphore lock to begin a critical section.
 */
void begin_critical_section(int which_one)
{
	/* lprintf(CTDL_DEBUG, "begin_critical_section(%d)\n", which_one); */
	pthread_mutex_lock(&Critters[which_one]);
}

/*
 * Release a semaphore lock to end a critical section.
 */
void end_critical_section(int which_one)
{
	pthread_mutex_unlock(&Critters[which_one]);
}

