/*
 * $Id$
 *
 * A skeleton module to test the dynamic loader.
 *
 *
 * Copyright (c) 1998-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
	CtdlLogPrintf(CTDL_DEBUG, "--- test of adding an unload hook --- \n");
	}

void NewRoomTest(void) {
	CtdlLogPrintf(CTDL_DEBUG, "--- test module was told we're now in a new room ---\n");
	}

void SessionStartTest(void) {
	CtdlLogPrintf(CTDL_DEBUG, "--- starting up session %d ---\n",
		CC->cs_pid);
	}

void SessionStopTest(void) {
	CtdlLogPrintf(CTDL_DEBUG, "--- ending session %d ---\n", 
		CC->cs_pid);
	}

void LoginTest(void) {
	CtdlLogPrintf(CTDL_DEBUG, "--- Hello, %s ---\n", CC->curr_user);
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
