/*
 * $Id$
 *
 * Server-side functions which handle message moderation.
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
	struct SuppMsgInfo smi;
	int is_message_in_room;

	/* user must be at least a Room Aide to moderate */
	if (CtdlAccessCheck(ac_room_aide)) return;

	msgnum = extract_long(argbuf, 0);
	newlevel = extract_int(argbuf, 1);

	if ( (newlevel < (-63)) || (newlevel > (+63)) ) {
		cprintf("%d %d is not a valid moderation level.\n",
			ERROR+ILLEGAL_VALUE, newlevel);
		return;
	}

	is_message_in_room = CtdlForEachMessage(MSGS_EQ, msgnum, (-127),
				NULL, NULL, NULL, NULL);
	if (!is_message_in_room) {
		cprintf("%d Message %ld is not in this room.\n",
			ERROR+ILLEGAL_VALUE, msgnum);
		return;
	}

	GetSuppMsgInfo(&smi, msgnum);
	smi.smi_mod = newlevel;
	PutSuppMsgInfo(&smi);

	cprintf("%d Message %ld is moderated to %d\n", OK, msgnum, newlevel);
}


char *Dynamic_Module_Init(void)
{
        CtdlRegisterProtoHook(cmd_mmod, "MMOD", "Moderate a message");
        return "$Id$";
}
