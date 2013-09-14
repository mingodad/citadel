/* 
 * Server functions which perform operations on user objects.
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sysdep.h"
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

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

#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "auth.h"
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "sysdep_decls.h"
#include "support.h"
#include "room_ops.h"
#include "svn_revision.h"
#include "control.h"
#include "msgbase.h"
#include "config.h"
#include "citserver.h"
#include "citadel_dirs.h"
#include "genstamp.h"
#include "threads.h"
#include "citadel_ldap.h"
#include "context.h"
#include "ctdl_module.h"

void cmd_noop(char *argbuf)
{
	cprintf("%d%cok\n", CIT_OK, CtdlCheckExpress() );
}


void cmd_qnop(char *argbuf)
{
	/* do nothing, this command returns no response */
}

/*
 * Set or unset asynchronous protocol mode
 */
void cmd_asyn(char *argbuf)
{
	int new_state;

	new_state = extract_int(argbuf, 0);
	if ((new_state == 0) || (new_state == 1)) {
		CC->is_async = new_state;
	}
	cprintf("%d %d\n", CIT_OK, CC->is_async);
}



/*
 * cmd_info()  -  tell the client about this server
 */
void cmd_info(char *cmdbuf) {
	cprintf("%d Server info:\n", LISTING_FOLLOWS);
	cprintf("%d\n", CC->cs_pid);
	cprintf("%s\n", config.c_nodename);
	cprintf("%s\n", config.c_humannode);
	cprintf("%s\n", config.c_fqdn);
	cprintf("%s\n", CITADEL);
	cprintf("%d\n", REV_LEVEL);
	cprintf("%s\n", config.c_site_location);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%d\n", SERVER_TYPE);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("1\n");	/* 1 = yes, this system supports floors */
	cprintf("1\n"); /* 1 = we support the extended paging options */
	cprintf("\n");	/* nonce no longer supported */
	cprintf("1\n"); /* 1 = yes, this system supports the QNOP command */

#ifdef HAVE_LDAP
	cprintf("1\n"); /* 1 = yes, this server is LDAP-enabled */
#else
	cprintf("0\n"); /* 1 = no, this server is not LDAP-enabled */
#endif

	if ((config.c_auth_mode == AUTHMODE_NATIVE) &&
	    (config.c_disable_newu == 0))
	{
		cprintf("%d\n", config.c_disable_newu);
	}
	else {
		cprintf("1\n");	/* "create new user" does not work with non-native auth modes */
	}

	cprintf("%s\n", config.c_default_cal_zone);

	/* thread load averages -- temporarily disabled during refactoring of this code */
	cprintf("0\n");		/* load average */
	cprintf("0\n");		/* worker average */
	cprintf("0\n");		/* thread count */

	cprintf("1\n");		/* yes, Sieve mail filtering is supported */
	cprintf("%d\n", config.c_enable_fulltext);
	cprintf("%s\n", svn_revision());

	if (config.c_auth_mode == AUTHMODE_NATIVE) {
		cprintf("%d\n", openid_level_supported); /* OpenID is enabled when using native auth */
	}
	else {
		cprintf("0\n");	/* OpenID is disabled when using non-native auth */
	}

	cprintf("%d\n", config.c_guest_logins);
	
	cprintf("000\n");
}

/*
 * echo 
 */
void cmd_echo(char *etext)
{
	cprintf("%d %s\n", CIT_OK, etext);
}

/* 
 * get the paginator prompt
 */
void cmd_more(char *argbuf) {
	cprintf("%d %s\n", CIT_OK, config.c_moreprompt);
}


/*
 * the client is identifying itself to the server
 */
void cmd_iden(char *argbuf)
{
        CitContext *CCC = MyContext();
	int dev_code;
	int cli_code;
	int rev_level;
	char desc[128];
	char from_host[128];

	if (num_parms(argbuf)<4) {
		cprintf("%d usage error\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	dev_code = extract_int(argbuf,0);
	cli_code = extract_int(argbuf,1);
	rev_level = extract_int(argbuf,2);
	extract_token(desc, argbuf, 3, '|', sizeof desc);

	safestrncpy(from_host, config.c_fqdn, sizeof from_host);
	from_host[sizeof from_host - 1] = 0;
	if (num_parms(argbuf)>=5) extract_token(from_host, argbuf, 4, '|', sizeof from_host);

	CCC->cs_clientdev = dev_code;
	CCC->cs_clienttyp = cli_code;
	CCC->cs_clientver = rev_level;
	safestrncpy(CCC->cs_clientname, desc, sizeof CCC->cs_clientname);
	CCC->cs_clientname[31] = 0;

	/* For local sockets and public clients, trust the hostname supplied by the client */
	if ( (CCC->is_local_socket) || (CtdlIsPublicClient()) ) {
		safestrncpy(CCC->cs_host, from_host, sizeof CCC->cs_host);
		CCC->cs_host[sizeof CCC->cs_host - 1] = 0;
		CCC->cs_addr[0] = 0;
	}

	syslog(LOG_NOTICE, "Client %d/%d/%01d.%02d (%s) from %s\n",
		dev_code,
		cli_code,
		(rev_level / 100),
		(rev_level % 100),
		desc,
		CCC->cs_host
	);
	cprintf("%d Ok\n",CIT_OK);
}

/*
 * Terminate another running session
 */
void cmd_term(char *cmdbuf)
{
	int session_num;
	int terminated = 0;

	session_num = extract_int(cmdbuf, 0);

	terminated = CtdlTerminateOtherSession(session_num);

	if (terminated < 0) {
		cprintf("%d You can't kill your own session.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (terminated & TERM_FOUND) {
		if (terminated == TERM_KILLED) {
			cprintf("%d Session terminated.\n", CIT_OK);
		}
		else {
			cprintf("%d You are not allowed to do that.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
		}
	}
	else {
		cprintf("%d No such session.\n", ERROR + ILLEGAL_VALUE);
	}
}


void cmd_time(char *argbuf)
{
   time_t tv;
   struct tm tmp;
   
   tv = time(NULL);
   localtime_r(&tv, &tmp);
   
   /* timezone and daylight global variables are not portable. */
#ifdef HAVE_STRUCT_TM_TM_GMTOFF
   cprintf("%d %ld|%ld|%d|%ld\n", CIT_OK, (long)tv, tmp.tm_gmtoff, tmp.tm_isdst, server_startup_time);
#else
   cprintf("%d %ld|%ld|%d|%ld\n", CIT_OK, (long)tv, timezone, tmp.tm_isdst, server_startup_time);
#endif
}




/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(serv_session)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_noop, "NOOP", "no operation");
		CtdlRegisterProtoHook(cmd_qnop, "QNOP", "no operation with no response");
;
		CtdlRegisterProtoHook(cmd_asyn, "ASYN", "enable asynchronous server responses");
		CtdlRegisterProtoHook(cmd_info, "INFO", "fetch server capabilities and configuration");
		CtdlRegisterProtoHook(cmd_echo, "ECHO", "echo text back to the client");
		CtdlRegisterProtoHook(cmd_more, "MORE", "fetch the paginator prompt");
		CtdlRegisterProtoHook(cmd_iden, "IDEN", "identify the client software and location");
		CtdlRegisterProtoHook(cmd_term, "TERM", "terminate another running session");

		CtdlRegisterProtoHook(cmd_time, "TIME", "fetch the date and time from the server");
	}
        /* return our id for the Log */
	return "serv_session";
}
