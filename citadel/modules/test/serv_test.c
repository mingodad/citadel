/*
 * $Id$
 *
 * A skeleton module to test the dynamic loader.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "ctdl_module.h"

extern struct CitContext *ContextList;

void CleanupTest(void) {
	lprintf(CTDL_DEBUG, "--- test of adding an unload hook --- \n");
	}

void NewRoomTest(void) {
	lprintf(CTDL_DEBUG, "--- test module was told we're now in a new room ---\n");
	}

void SessionStartTest(void) {
	lprintf(CTDL_DEBUG, "--- starting up session %d ---\n",
		CC->cs_pid);
	}

void SessionStopTest(void) {
	lprintf(CTDL_DEBUG, "--- ending session %d ---\n", 
		CC->cs_pid);
	}

void LoginTest(void) {
	lprintf(CTDL_DEBUG, "--- Hello, %s ---\n", CC->curr_user);
	}

/* To insert this module into the server activate the next block by changing the #if 0 to #if 1 */
CTDL_MODULE_INIT(test)
{
#if 0
	if (!threading)
	{
		CtdlRegisterCleanupHook(CleanupTest);
		CtdlRegisterSessionHook(NewRoomTest, EVT_NEWROOM);
		CtdlRegisterSessionHook(SessionStartTest, EVT_START);
		CtdlRegisterSessionHook(SessionStopTest, EVT_STOP);
		CtdlRegisterSessionHook(LoginTest, EVT_LOGIN);
	}
#endif

   /* return our Subversion id for the Log */
   return "$Id$";
}
