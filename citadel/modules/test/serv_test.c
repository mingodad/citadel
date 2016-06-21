/*
 * This is an empty skeleton of a Citadel server module, included to demonstrate
 * how to add a new module to the system and how to activate it in the server.
 * 
 * Copyright (c) 1998-2016 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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


void CleanupTest(void) {
	syslog(LOG_DEBUG, "--- test of adding an unload hook --- \n");
}

void NewRoomTest(void) {
	syslog(LOG_DEBUG, "--- test module was told we're now in a new room ---\n");
}

void SessionStartTest(void) {
	syslog(LOG_DEBUG, "--- starting up session %d ---\n", CC->cs_pid);
}

void SessionStopTest(void) {
	syslog(LOG_DEBUG, "--- ending session %d ---\n", CC->cs_pid);
}

void LoginTest(void) {
	syslog(LOG_DEBUG, "--- Hello, %s ---\n", CC->curr_user);
}

/* To insert this module into the server activate the next block by changing the #if 0 to #if 1 */
CTDL_MODULE_INIT(test)
{
#if 0
	if (!threading)
	{
		CtdlRegisterCleanupHook(CleanupTest);
		CtdlRegisterSessionHook(NewRoomTest, EVT_NEWROOM, 1);
		CtdlRegisterSessionHook(SessionStartTest, EVT_START, 1);
		CtdlRegisterSessionHook(SessionStopTest, EVT_STOP, 1);
		CtdlRegisterSessionHook(LoginTest, EVT_LOGIN, 1);
	}
#endif

/* return our module name for the log */
return "test";
}
