/*
 * $Id$
 *
 * Functions which implement RFC2086 (and maybe RFC4314) (IMAP ACL extension)
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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
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
 * Given the bits returned by CtdlRoomAccess(), populate a string buffer
 * with IMAP ACL format flags.   This code is common to GETACL and MYRIGHTS.
 */
void imap_acl_flags(char *rights, int ra)
{
	strcpy(rights, "");

	/* l - lookup (mailbox is visible to LIST/LSUB commands)
	 * r - read (SELECT the mailbox, perform STATUS et al)
	 * s - keep seen/unseen information across sessions (STORE SEEN flag)
	 */
	if (	(ra & UA_KNOWN)					/* known rooms */
	   ||	((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))	/* zapped rooms */
	   ) {
		strcat(rights, "l");
		strcat(rights, "r");
		strcat(rights, "s");

		/* Only output the remaining flags if the room is known */

		/* w - write (set or clear flags other than SEEN or DELETED, not supported in Citadel */

		/* i - insert (perform APPEND, COPY into mailbox) */
		/* p - post (send mail to submission address for mailbox - not enforced) */
		/* c - create (CREATE new sub-mailboxes) */
		if (ra & UA_POSTALLOWED) {
			strcat(rights, "i");
			strcat(rights, "p");
			strcat(rights, "c");
		}

		/* d - delete messages (STORE DELETED flag, perform EXPUNGE) */
		if (ra & UA_DELETEALLOWED) {
			strcat(rights, "d");
		}

		/* a - administer (perform SETACL/DELETEACL/GETACL/LISTRIGHTS) */
		if (ra & UA_ADMINALLOWED) {
			/*
			 * This is the correct place to put the "a" flag.  We are leaving
			 * it commented out for now, because it implies that we could
			 * perform any of SETACL/DELETEACL/GETACL/LISTRIGHTS.  Since these
			 * commands are not yet implemented, omitting the flag should
			 * theoretically prevent compliant clients from attempting to
			 * perform them.
			 */
			/* strcat(rights, "a"); * commented out */
		}
	}
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

	/*
	 * Search for the specified room or folder
	 */
	ret = imap_grabroom(roomname, parms[2], 1);
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
		if (!IsEmptyStr(temp.fullname)) {
			imap_acl_flags(rights, ra);
			if (!IsEmptyStr(rights)) {
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
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	struct recptypes *valid;
	struct ctdluser temp;

	if (num_parms != 4) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	/*
	 * Search for the specified room/folder
	 */
	ret = imap_grabroom(roomname, parms[2], 1);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * Search for the specified user
	 */
	ret = (-1);
	valid = validate_recipients(parms[3], NULL, 0);
	if (valid != NULL) {
		if (valid->num_local == 1) {
			ret = getuser(&temp, valid->recp_local);
		}
		free_recipients(valid);
	}
	if (ret != 0) {
		cprintf("%s NO Invalid user name or access denied\r\n",
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


	/*
	 * Now output the list of rights
	 */
	cprintf("* LISTRIGHTS ");
	imap_strout(parms[2]);
	cprintf(" ");
	imap_strout(parms[3]);
	cprintf(" ");
	imap_strout("");		/* FIXME ... do something here */
	cprintf("\r\n");


	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK LISTRIGHTS completed\r\n", parms[0]);
	return;
}


/*
 * Implements the MYRIGHTS command.
 */
void imap_myrights(int num_parms, char *parms[]) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	int ra;
	char rights[32];

	if (num_parms != 3) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	ret = imap_grabroom(roomname, parms[2], 1);
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

	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);
	imap_acl_flags(rights, ra);

	cprintf("* MYRIGHTS ");
	imap_strout(parms[2]);
	cprintf(" %s\r\n", rights);

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK MYRIGHTS completed\r\n", parms[0]);
	return;
}


