
/* 
 * Main source module for the Citadel server
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
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
#include <sys/types.h>
#include <sys/stat.h>

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

#if HAVE_BACKTRACE
#include <execinfo.h>
#endif

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "threads.h"
#include "citserver.h"
#include "config.h"
#include "database.h"
#include "housekeeping.h"
#include "user_ops.h"
#include "msgbase.h"
#include "support.h"
#include "locate_host.h"
#include "room_ops.h"
#include "control.h"
#include "euidindex.h"
#include "context.h"
#include "svn_revision.h"
#include "ctdl_module.h"

void cmd_log_get(char *argbuf)
{
	long HKLen;
	const char *ch;
	HashPos *Pos;
	void *vptr;

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Log modules enabled:\n", LISTING_FOLLOWS);

	Pos = GetNewHashPos(LogDebugEntryTable, 0);

	while (GetNextHashPos(LogDebugEntryTable, Pos, &HKLen, &ch, &vptr)) {
		LogDebugEntry *E = (LogDebugEntry*)vptr;
		cprintf("%s|%d\n", ch, *E->LogP);
	}
	
	DeleteHashPos(&Pos);
	cprintf("000\n");
}
void cmd_log_set(char *argbuf)
{
	void *vptr;
	int lset;
	int wlen;
	char which[SIZ] = "";

	if (CtdlAccessCheck(ac_aide)) return;

	wlen = extract_token(which, argbuf, 0, '|', sizeof(which));
	if (wlen < 0) wlen = 0;
	lset = extract_int(argbuf, 1);
	if (lset != 0) lset = 1;
	if (GetHash(LogDebugEntryTable, which, wlen, &vptr) && 
	    (vptr != NULL))
	{
		LogDebugEntry *E = (LogDebugEntry*)vptr;
		E->F(lset);
		cprintf("%d %s|%d\n", CIT_OK, which, lset);
	}
	else {
		cprintf("%d Log setting %s not known\n", 
			ERROR, which);
	}
}


/*
 * Shut down the server
 */
void cmd_down(char *argbuf) {
	char *Reply ="%d Shutting down server.  Goodbye.\n";

	if (CtdlAccessCheck(ac_aide)) return;

	if (!IsEmptyStr(argbuf))
	{
		int state = CIT_OK;
		restart_server = extract_int(argbuf, 0);
		
		if (restart_server > 0)
		{
			Reply = "%d citserver will now shut down and automatically restart.\n";
		}
		if ((restart_server > 0) && !running_as_daemon)
		{
			syslog(LOG_ERR, "The user requested restart, but not running as daemon! Geronimooooooo!\n");
			Reply = "%d Warning: citserver is not running in daemon mode and is therefore unlikely to restart automatically.\n";
			state = ERROR;
		}
		cprintf(Reply, state);
	}
	else
	{
		cprintf(Reply, CIT_OK + SERVER_SHUTTING_DOWN); 
	}
	CC->kill_me = KILLME_SERVER_SHUTTING_DOWN;
	server_shutting_down = 1;
}


/*
 * Halt the server without exiting the server process.
 */
void cmd_halt(char *argbuf) {

	if (CtdlAccessCheck(ac_aide)) return;

	cprintf("%d Halting server.  Goodbye.\n", CIT_OK);
	server_shutting_down = 1;
	shutdown_and_halt = 1;
}


/*
 * Schedule or cancel a server shutdown
 */
void cmd_scdn(char *argbuf)
{
	int new_state;
	int state = CIT_OK;
	char *Reply = "%d %d\n";

	if (CtdlAccessCheck(ac_aide)) return;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 2) || (new_state == 3))
	{
		restart_server = 1;
		if (!running_as_daemon)
		{
			syslog(LOG_ERR, "The user requested restart, but not running as deamon! Geronimooooooo!\n");
			Reply = "%d %d Warning, not running in deamon mode. maybe we will come up again, but don't lean on it.\n";
			state = ERROR;
		}

		restart_server = extract_int(argbuf, 0);
		new_state -= 2;
	}
	if ((new_state == 0) || (new_state == 1)) {
		ScheduledShutdown = new_state;
	}
	cprintf(Reply, state, ScheduledShutdown);
}

/*
 * Manually initiate log file cull.
 */
void cmd_cull(char *argbuf) {
	if (CtdlAccessCheck(ac_internal)) return;
	cdb_cull_logs();
	cprintf("%d Database log file cull completed.\n", CIT_OK);
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(syscmd)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_log_get, "LOGP", "Print Log-parameters");
		CtdlRegisterProtoHook(cmd_log_set, "LOGS", "Set Log-parameters");

		CtdlRegisterProtoHook(cmd_down, "DOWN", "perform a server shutdown");
		CtdlRegisterProtoHook(cmd_halt, "HALT", "halt the server without exiting the server process");
		CtdlRegisterProtoHook(cmd_scdn, "SCDN", "schedule or cancel a server shutdown");

		CtdlRegisterProtoHook(cmd_cull, "CULL", "Cull database logs");
	}
        /* return our id for the Log */
	return "syscmd";
}
