/*
 * This module supplies statistics about the activity levels of your Citadel
 * system.  We didn't bother writing a reporting module, because there is
 * already an excellent tool called MRTG (Multi Router Traffic Grapher) which
 * is available at http://www.mrtg.org that can fetch data using external
 * scripts.  This module supplies data in the format expected by MRTG.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"


#include "ctdl_module.h"


/*
 * Other functions call this one to output data in MRTG format
 */
void mrtg_output(long value1, long value2) {
	time_t uptime_t;
	int uptime_days, uptime_hours, uptime_minutes;
	
	uptime_t = time(NULL) - server_startup_time;
	uptime_days = (int) (uptime_t / 86400L);
	uptime_hours = (int) ((uptime_t % 86400L) / 3600L);
	uptime_minutes = (int) ((uptime_t % 3600L) / 60L);

	cprintf("%d ok\n", LISTING_FOLLOWS);
	cprintf("%ld\n", value1);
	cprintf("%ld\n", value2);
	cprintf("%d days, %d hours, %d minutes\n",
		uptime_days, uptime_hours, uptime_minutes);
	cprintf("%s\n", CtdlGetConfigStr("c_humannode"));
	cprintf("000\n");
}




/*
 * Tell us how many users are online
 */
void mrtg_users(void) {
	long connected_users = 0;
	long active_users = 0;
	
	struct CitContext *cptr;

	begin_critical_section(S_SESSION_TABLE);
        for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {

		if (cptr->internal_pgm == 0) {
			++connected_users;

			if ( (time(NULL) - (cptr->lastidle)) < 900L) {
				++active_users;
			}
		}

	}
	end_critical_section(S_SESSION_TABLE);
	
	mrtg_output(connected_users, active_users);
}


/*
 * Volume of messages submitted
 */
void mrtg_messages(void) {
	mrtg_output(CitControl.MMhighest, 0L);
}


struct num_accounts {
	long total;
	long active;
};

/*
 * Helper function for mrtg_accounts()
 */
void tally_account(struct ctdluser *EachUser, void *userdata)
{
	struct num_accounts *n = (struct num_accounts *) userdata;

	++n->total;
	if ( (time(NULL) - EachUser->lastcall) <= 2592000 ) ++n->active;
}


/*
 * Number of accounts and active accounts
 */
void mrtg_accounts(void) {
	struct num_accounts n = {
		0,
		0
	};

	ForEachUser(tally_account, (void *)&n );
	mrtg_output(n.total, n.active);
}


/*
 * Fetch data for MRTG
 */
void cmd_mrtg(char *argbuf) {
	char which[32];

	extract_token(which, argbuf, 0, '|', sizeof which);

	if (!strcasecmp(which, "users")) {
		mrtg_users();
	}
	else if (!strcasecmp(which, "messages")) {
		mrtg_messages();
	}
	else if (!strcasecmp(which, "accounts")) {
		mrtg_accounts();
	}
	else {
		cprintf("%d Unrecognized keyword '%s'\n", ERROR + ILLEGAL_VALUE, which);
	}
}


CTDL_MODULE_INIT(mrtg)
{
	if (!threading)
	{
	        CtdlRegisterProtoHook(cmd_mrtg, "MRTG", "Supply stats to MRTG");
	}
	
	/* return our module name for the log */
        return "mrtg";
}
