/*
 * $Id$
 * 
 * A server-side module for Citadel designed to filter idiots off the network.
 * 
 * Copyright (c) 2002 / released under the GNU General Public License
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
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly (delete the user's existing vCard in the config room
 * and in the global address book).
 */
int filter_the_idiots(struct CtdlMessage *msg, char *target_room) {

	if (msg == NULL) {
		return(0);
	}

	/* FIXME ... write it!  In the meantime, here's a temporary fix */

	if (msg->cm_fields['A'] != NULL) {
		if (!strcasecmp(msg->cm_fields['A'],
				"Curly Surmudgeon")) {
			return(1);
		}
	}

	return(0);
}


char *Dynamic_Module_Init(void)
{
	CtdlRegisterNetprocHook(filter_the_idiots);
	return "$Id$";
}
