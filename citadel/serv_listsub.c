/*
 * $Id$
 *
 * This module handles self-service subscription/unsubscription to mail lists.
 *
 * Copyright (C) 2002 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
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
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


/*
 * Generate a randomizationalisticized token to use for authentication of
 * a subscribe or unsubscribe request.
 */
void listsub_generate_token(char *buf) {
	char sourcebuf[SIZ];

	/* Theo, please sit down and shut up.  This key doesn't have to be
	 * tinfoil-hat secure, it just needs to be reasonably unguessable.
	 */
	sprintf(sourcebuf, "%d%d%ld",
		rand,
		getpid(),
		time(NULL)
	);

	/* Convert it to base64 so it looks cool */	
	encode_base64(buf, sourcebuf);
}



void cmd_subs(char *cmdbuf) {
	cprintf("%d not yet implemented, dumbass...\n", ERROR);
}


/*
 * Module entry point
 */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_subs, "SUBS", "List subscribe/unsubscribe");
	return "$Id$";
}
