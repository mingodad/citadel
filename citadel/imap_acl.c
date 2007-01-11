/*
 * $Id:  $
 *
 * Functions which implement RFC2086/RFC4314 (IMAP ACL extension)
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
#include <ctype.h>
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
#include "tools.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_misc.h"
#include "genstamp.h"



/*
 * Implements the SETACL command.
 */
void imap_setacl(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the DELETEACL command.
 */
void imap_deleteacl(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}



/*
 * Implements the GETACL command.
 */
void imap_getacl(int num_parms, char *parms[]) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	struct ctdluser temp;
	struct cdbdata *cdbus;
	int ra;
	char rights[32];

	if (num_parms != 3) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	ret = imap_grabroom(roomname, parms[2], 0);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	usergoto(roomname, 0, 0, &msgs, &new);

	cprintf("* ACL");
	cprintf(" ");
	imap_strout(parms[2]);

	/*
	 * Traverse the userlist
	 */
	cdb_rewind(CDB_USERS);
	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(&temp, 0, sizeof temp);
		memcpy(&temp, cdbus->ptr, sizeof temp);
		cdb_free(cdbus);

		CtdlRoomAccess(&CC->room, &temp, &ra, NULL);
		if (strlen(temp.fullname) > 0) {
			strcpy(rights, "");

			/* l - lookup (mailbox is visible to LIST/LSUB commands, SUBSCRIBE mailbox)
			 * r - read (SELECT the mailbox, perform STATUS)
			 * s - keep seen/unseen information across sessions (set or clear \SEEN flag
			 *     via STORE, also set \SEEN during APPEND/COPY/ FETCH BODY[...])
			 */
			if (	(ra & UA_KNOWN)					/* known rooms */
			   ||	((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))	/* zapped rooms */
			   ) {
							strcat(rights, "l");
							strcat(rights, "r");
							strcat(rights, "s");
			}

			/* w - write (set or clear flags other than \SEEN and \DELETED via
			 * STORE, also set them during APPEND/COPY)
			 */
			/* Never granted in Citadel because our store doesn't support other flags */

			/* i - insert (perform APPEND, COPY into mailbox)
			 * p - post (send mail to submission address for mailbox, not enforced by IMAP)
			 */
			if (ra & UA_POSTALLOWED) {
							strcat(rights, "i");
							strcat(rights, "p");
			}

			/* k - create mailboxes (CREATE new sub-mailboxes in any
			 * implementation-defined hierarchy, parent mailbox for the new
			 * mailbox name in RENAME) */

			/* x - delete mailbox (DELETE mailbox, old mailbox name in RENAME) */

			/* t - delete messages (set or clear \DELETED flag via STORE, set
			 * \DELETED flag during APPEND/COPY) */

			/* e - perform EXPUNGE and expunge as a part of CLOSE */

			/* a - administer (perform SETACL/DELETEACL/GETACL/LISTRIGHTS) */

			if (strlen(rights) > 0) {
				cprintf(" ");
				imap_strout(temp.fullname);
				cprintf(" %s", rights);
			}
		}
	}

	cprintf("\r\n");

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK GETACL completed\r\n", parms[0]);
}


/*
 * Implements the LISTRIGHTS command.
 */
void imap_listrights(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


/*
 * Implements the MYRIGHTS command.
 */
void imap_myrights(int num_parms, char *parms[]) {

	cprintf("%s BAD not yet implemented FIXME\r\n", parms[0]);
	return;
}


