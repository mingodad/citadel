/*
 * Automatically copies the contents of a "New User Greetings" room to the
 * inbox of any new user upon account creation.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
 */

/*
 * Name of the New User Greetings room.
 */
#define NEWUSERGREETINGS	"New User Greetings"


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

#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"






/*
 * Copy the contents of the New User Greetings> room to the user's Mail> room.
 */
void CopyNewUserGreetings(void) {
	struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	char mailboxname[ROOMNAMELEN];


	/* Only do this for new users. */
	if (CC->user.timescalled != 1) return;

	/* This user's mailbox. */
	CtdlMailboxName(mailboxname, sizeof mailboxname, &CC->user, MAILROOM);

	/* Go to the source room ... bail out silently if it's not there,
	 * or if it's not private.
	 */
	if (CtdlGetRoom(&CC->room, NEWUSERGREETINGS) != 0) return;
	if ((CC->room.QRflags & QR_PRIVATE) == 0) return;

	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));

	if (cdbfr != NULL) {
		msglist = malloc(cdbfr->len);
		memcpy(msglist, cdbfr->ptr, cdbfr->len);
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}

	if (num_msgs > 0) {
		CtdlSaveMsgPointersInRoom(mailboxname, msglist, num_msgs, 1, NULL, 0);
	}

	/* Now free the memory we used, and go away. */
	if (msglist != NULL) free(msglist);
}


CTDL_MODULE_INIT(newuser)
{
	if (!threading)
	{
		CtdlRegisterSessionHook(CopyNewUserGreetings, EVT_LOGIN, PRIO_LOGIN + 1);
	}
	
	/* return our module name for the log */
	return "newuser";
}
