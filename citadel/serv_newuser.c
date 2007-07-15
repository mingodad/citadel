/*
 * $Id$
 *
 * Automaticalyl copies the contents of a "New User Greetings" room to the
 * inbox of any new user upon account creation.
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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"

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
		CtdlCopyMsgsToRoom(msglist, num_msgs, mailboxname);
	}

	/* Now free the memory we used, and go away. */
	if (msglist != NULL) free(msglist);
}


char *serv_newuser_init(void)
{
   CtdlRegisterSessionHook(CopyNewUserGreetings, EVT_LOGIN);

   /* return our Subversion id for the Log */
   return "$Id$";
}
