/*
 * $Id$
 *
 * Automaticalyl copies the contents of a "New User Greetings" room to the
 * inbox of any new user upon account creation.
 *
 * Copyright (c) 1987-2009 by the citadel.org team
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
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"


#include "ctdl_module.h"



extern struct CitContext *ContextList;


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
	MailboxName(mailboxname, sizeof mailboxname, &CC->user, MAILROOM);

	/* Go to the source room ... bail out silently if it's not there,
	 * or if it's not private.
	 */
	if (getroom(&CC->room, NEWUSERGREETINGS) != 0) return;
	if (! CC->room.QRflags & QR_PRIVATE ) return;

	cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));

	if (cdbfr != NULL) {
		msglist = malloc(cdbfr->len);
		memcpy(msglist, cdbfr->ptr, cdbfr->len);
		num_msgs = cdbfr->len / sizeof(long);
		cdb_free(cdbfr);
	}

	if (num_msgs > 0) {
		CtdlSaveMsgPointersInRoom(mailboxname, msglist, num_msgs, 1, NULL);
	}

	/* Now free the memory we used, and go away. */
	if (msglist != NULL) free(msglist);
}


CTDL_MODULE_INIT(newuser)
{
	if (!threading)
	{
		CtdlRegisterSessionHook(CopyNewUserGreetings, EVT_LOGIN);
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
