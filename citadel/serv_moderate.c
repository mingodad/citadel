/*
 * $Id$
 *
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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"



/*
 * moderate a message
 */
void cmd_mmod(char *argbuf) {
	long msgnum;
	int newlevel;

	/* user must be at least a Room Aide to moderate */
	if (CtdlAccessCheck(ac_room_aide)) return;

	msgnum = extract_long(argbuf, 0);
	newlevel = extract_int(argbuf, 1);

	if ( (newlevel < (-63)) || (newlevel > (+63)) ) {
		cprintf("%d %d is not a valid moderation level.\n",
			newlevel, ERROR+ILLEGAL_VALUE);
	}

	cprintf("%d FIXME ... actually do this!!!!!!!!\n", OK);
}


char *Dynamic_Module_Init(void)
{
        CtdlRegisterProtoHook(cmd_mmod, "MMOD", "Moderate a message");
        return "$Id$";
}
