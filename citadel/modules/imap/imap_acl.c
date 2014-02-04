/*
 * Functions which implement RFC2086 (and maybe RFC4314) (IMAP ACL extension)
 *
 *
 * Copyright (c) 2007-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
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
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_misc.h"
#include "genstamp.h"
#include "ctdl_module.h"


/*
 * Implements the SETACL command.
 */
void imap_setacl(int num_parms, ConstStr *Params) {

	IReply("BAD not yet implemented FIXME");
	return;
}


/*
 * Implements the DELETEACL command.
 */
void imap_deleteacl(int num_parms, ConstStr *Params) {

	IReply("BAD not yet implemented FIXME");
	return;
}


/*
 * Given the bits returned by CtdlRoomAccess(), populate a string buffer
 * with IMAP ACL format flags.   This code is common to GETACL and MYRIGHTS.
 */
void imap_acl_flags(StrBuf *rights, int ra)
{
	FlushStrBuf(rights);

	/* l - lookup (mailbox is visible to LIST/LSUB commands)
	 * r - read (SELECT the mailbox, perform STATUS et al)
	 * s - keep seen/unseen information across sessions (STORE SEEN flag)
	 */
	if (	(ra & UA_KNOWN)					/* known rooms */
	   ||	((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))	/* zapped rooms */
	) {
		StrBufAppendBufPlain(rights, HKEY("l"), 0);
		StrBufAppendBufPlain(rights, HKEY("r"), 0);
		StrBufAppendBufPlain(rights, HKEY("s"), 0);

		/* Only output the remaining flags if the room is known */

		/* w - write (set or clear flags other than SEEN or DELETED, not supported in Citadel */

		/* i - insert (perform APPEND, COPY into mailbox) */
		/* p - post (send mail to submission address for mailbox - not enforced) */
		/* c - create (CREATE new sub-mailboxes) */
		if (ra & UA_POSTALLOWED) {
			StrBufAppendBufPlain(rights, HKEY("i"), 0);
			StrBufAppendBufPlain(rights, HKEY("p"), 0);
			StrBufAppendBufPlain(rights, HKEY("c"), 0);
		}

		/* d - delete messages (STORE DELETED flag, perform EXPUNGE) */
		if (ra & UA_DELETEALLOWED) {
			StrBufAppendBufPlain(rights, HKEY("d"), 0);
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
			 *
			 * StrBufAppendBufPlain(rights, HKEY("a"), 0);
			 */
		}
	}
}


/*
 * Implements the GETACL command.
 */
void imap_getacl(int num_parms, ConstStr *Params) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	struct ctdluser temp;
	struct cdbdata *cdbus;
	int ra;
	StrBuf *rights;

	if (num_parms != 3) {
		IReply("BAD usage error");
		return;
	}

	/*
	 * Search for the specified room or folder
	 */
	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		IReply("NO Invalid mailbox name or access denied");
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new, NULL, NULL);

	IAPuts("* ACL ");
	IPutCParamStr(2);

	/*
	 * Traverse the userlist
	 */
	rights = NewStrBuf();
	cdb_rewind(CDB_USERS);
	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) 
	{
		memset(&temp, 0, sizeof temp);
		memcpy(&temp, cdbus->ptr, sizeof temp);
		cdb_free(cdbus);

		CtdlRoomAccess(&CC->room, &temp, &ra, NULL);
		if (!IsEmptyStr(temp.fullname)) {
			imap_acl_flags(rights, ra);
			if (StrLength(rights) > 0) {
				IAPuts(" ");
				IPutStr(temp.fullname, strlen(temp.fullname));
				IAPuts(" ");
				iaputs(SKEY( rights));
			}
		}
	}
	FreeStrBuf(&rights);
	IAPuts("\r\n");

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new, NULL, NULL);
	}

	IReply("OK GETACL completed");
}


/*
 * Implements the LISTRIGHTS command.
 */
void imap_listrights(int num_parms, ConstStr *Params) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	recptypes *valid;
	struct ctdluser temp;

	if (num_parms != 4) {
		IReply("BAD usage error");
		return;
	}

	/*
	 * Search for the specified room/folder
	 */
	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		IReply("NO Invalid mailbox name or access denied");
		return;
	}

	/*
	 * Search for the specified user
	 */
	ret = (-1);
	valid = validate_recipients(Params[3].Key, NULL, 0);
	if (valid != NULL) {
		if (valid->num_local == 1) {
			ret = CtdlGetUser(&temp, valid->recp_local);
		}
		free_recipients(valid);
	}
	if (ret != 0) {
		IReply("NO Invalid user name or access denied");
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new, NULL, NULL);

	/*
	 * Now output the list of rights
	 */
	IAPuts("* LISTRIGHTS ");
	IPutCParamStr(2);
	IAPuts(" ");
	IPutCParamStr(3);
	IAPuts(" ");
	IPutStr(HKEY(""));		/* FIXME ... do something here */
	IAPuts("\r\n");

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new, NULL, NULL);
	}

	IReply("OK LISTRIGHTS completed");
	return;
}


/*
 * Implements the MYRIGHTS command.
 */
void imap_myrights(int num_parms, ConstStr *Params) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	int ra;
	StrBuf *rights;

	if (num_parms != 3) {
		IReply("BAD usage error");
		return;
	}

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		IReply("NO Invalid mailbox name or access denied");
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new, NULL, NULL);

	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);
	rights = NewStrBuf();
	imap_acl_flags(rights, ra);

	IAPuts("* MYRIGHTS ");
	IPutCParamStr(2);
	IAPuts(" ");
	IPutStr(SKEY(rights));
	IAPuts("\r\n");

	FreeStrBuf(&rights);

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new, NULL, NULL);
	}

	IReply("OK MYRIGHTS completed");
	return;
}
