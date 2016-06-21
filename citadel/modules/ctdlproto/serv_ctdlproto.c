/* 
 * Citadel protocoll main dispatcher
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <libcitadel.h>

#include "citserver.h"
#include "ctdl_module.h"
#include "config.h"
/*
 * This loop recognizes all server commands.
 */
void do_command_loop(void) {
	struct CitContext *CCC = CC;
	char cmdbuf[SIZ];
	
	time(&CCC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_getln(cmdbuf, sizeof cmdbuf) < 1) {
		CTDLM_syslog(LOG_INFO, "Citadel client disconnected: ending session.");
		CCC->kill_me = KILLME_CLIENT_DISCONNECTED;
		return;
	}

	/* Log the server command, but don't show passwords... */
	if ( (strncasecmp(cmdbuf, "PASS", 4)) && (strncasecmp(cmdbuf, "SETP", 4)) ) {
		CTDL_syslog(LOG_DEBUG, "[%s(%ld)] %s",
			CCC->curr_user, CCC->user.usernum, cmdbuf
		);
	}
	else {
		CTDL_syslog(LOG_DEBUG, "[%s(%ld)] <password command hidden from log>",
			    CCC->curr_user, CCC->user.usernum
		);
	}

	buffer_output();

	/*
	 * Let other clients see the last command we executed, and
	 * update the idle time, but not NOOP, QNOP, PEXP, GEXP, RWHO, or TIME.
	 */
	if ( (strncasecmp(cmdbuf, "NOOP", 4))
	   && (strncasecmp(cmdbuf, "QNOP", 4))
	   && (strncasecmp(cmdbuf, "PEXP", 4))
	   && (strncasecmp(cmdbuf, "GEXP", 4))
	   && (strncasecmp(cmdbuf, "RWHO", 4))
	   && (strncasecmp(cmdbuf, "TIME", 4)) ) {
		strcpy(CCC->lastcmdname, "    ");
		safestrncpy(CCC->lastcmdname, cmdbuf, sizeof(CCC->lastcmdname));
		time(&CCC->lastidle);
	}
	
	if ((strncasecmp(cmdbuf, "ENT0", 4))
	   && (strncasecmp(cmdbuf, "MESG", 4))
	   && (strncasecmp(cmdbuf, "MSGS", 4)))
	{
	   CCC->cs_flags &= ~CS_POSTING;
	}
		   
	if (!DLoader_Exec_Cmd(cmdbuf)) {
		cprintf("%d Unrecognized or unsupported command.\n", ERROR + CMD_NOT_SUPPORTED);
	}	

	unbuffer_output();

	/* Run any after-each-command routines registered by modules */
	PerformSessionHooks(EVT_CMD);
}

