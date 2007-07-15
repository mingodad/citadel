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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"

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

char *serv_test_init(void)
{
   CtdlRegisterCleanupHook(CleanupTest);
   CtdlRegisterSessionHook(NewRoomTest, EVT_NEWROOM);
   CtdlRegisterSessionHook(SessionStartTest, EVT_START);
   CtdlRegisterSessionHook(SessionStopTest, EVT_STOP);
   CtdlRegisterSessionHook(LoginTest, EVT_LOGIN);

   /* return our Subversion id for the Log */
   return "$Id$";
}
