/*
 * IMAP METADATA extension
 *
 * This is an implementation of the Bynari variant of the METADATA extension.
 *
 * Copyright (c) 2007-2009 by the citadel.org team
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
 * Implements the SETMETADATA command.
 *
 * Again, the only thing we're interested in setting here is the folder type.
 *
 * Attempting to set anything else calls a stub which fools the client into
 * thinking that there is no remaining space available to store annotations.
 */
void imap_setmetadata(int num_parms, ConstStr *Params) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	int setting_user_value = 0;
	char set_value[32];
	int set_view = VIEW_BBS;
	visit vbuf;

	if (num_parms != 6) {
		IReply("BAD usage error");
		return;
	}

	/*
	 * Don't allow other types of metadata to be set
	 */
	if (strcasecmp(Params[3].Key, "/vendor/kolab/folder-type")) {
		IReply("NO [METADATA TOOMANY] SETMETADATA failed");
		return;
	}

	if (!strcasecmp(Params[4].Key, "(value.shared")) {
		setting_user_value = 0;				/* global view */
	}
	else if (!strcasecmp(Params[4].Key, "(value.priv")) {
		setting_user_value = 1;				/* per-user view */
	}
	else {
		IReply("NO [METADATA TOOMANY] SETMETADATA failed");
		return;
	}

	/*
	 * Extract the folder type without any parentheses.  Then learn
	 * the Citadel view type based on the supplied folder type.
	 */
	extract_token(set_value, Params[5].Key, 0, ')', sizeof set_value);
	if (!strncasecmp(set_value, "mail", 4)) {
		set_view = VIEW_MAILBOX;
	}
	else if (!strncasecmp(set_value, "event", 5)) {
		set_view = VIEW_CALENDAR;
	}
	else if (!strncasecmp(set_value, "contact", 7)) {
		set_view = VIEW_ADDRESSBOOK;
	}
	else if (!strncasecmp(set_value, "journal", 7)) {
		set_view = VIEW_JOURNAL;
	}
	else if (!strncasecmp(set_value, "note", 4)) {
		set_view = VIEW_NOTES;
	}
	else if (!strncasecmp(set_value, "task", 4)) {
		set_view = VIEW_TASKS;
	}
	else {
		set_view = VIEW_MAILBOX;
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

	/*
	 * Always set the per-user view to the requested one.
	 */
	CtdlGetRelationship(&vbuf, &CC->user, &CC->room);
	vbuf.v_view = set_view;
	CtdlSetRelationship(&vbuf, &CC->user, &CC->room);

	/* If this is a "value.priv" set operation, we're done. */

	if (setting_user_value)
	{
		IReply("OK SETANNOTATION complete");
	}

	/* If this is a "value.shared" set operation, we are allowed to perform it
	 * under certain conditions.
	 */
	else if (	(is_room_aide())					/* aide or room aide */
		||	(	(CC->room.QRflags & QR_MAILBOX)
			&&	(CC->user.usernum == atol(CC->room.QRname))	/* mailbox owner */
			)
		||	(msgs == 0)		/* hack: if room is empty, assume we just created it */
	) {
		CtdlGetRoomLock(&CC->room, CC->room.QRname);
		CC->room.QRdefaultview = set_view;
		CtdlPutRoomLock(&CC->room);
		IReply("OK SETANNOTATION complete");
	}

	/* If we got to this point, we don't have permission to set the default view. */
	else {
		IReply("NO [METADATA TOOMANY] SETMETADATA failed");
	}

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new, NULL, NULL);
	}
	return;
}


/*
 * Implements the GETMETADATA command.
 *
 * Regardless of what the client asked for, we are going to supply them with
 * the folder type.  It's the only metadata we have anyway.
 */
void imap_getmetadata(int num_parms, ConstStr *Params) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	int found = 0;

/* this doesn't work if you have rooms/floors with spaces. 
   we need this for the bynari connector.
	if (num_parms > 5) {
		IReply("BAD usage error");
		return;
	}
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

	IAPuts("* METADATA ");
	IPutCParamStr(2);
	IAPuts(" \"/vendor/kolab/folder-type\" (\"value.shared\" \"");

	/* If it's one of our hard-coded default rooms, we know what to do... */

	if (CC->room.QRname[10] == '.')
	{
		if (!strcasecmp(&CC->room.QRname[11], MAILROOM)) {
			found = 1;
			IAPuts("mail.inbox");
		}
		else if (!strcasecmp(&CC->room.QRname[11], SENTITEMS)) {
			found = 1;
			IAPuts("mail.sentitems");
		}
		else if (!strcasecmp(&CC->room.QRname[11], USERDRAFTROOM)) {
			found = 1;
			IAPuts("mail.drafts");
		}
		else if (!strcasecmp(&CC->room.QRname[11], USERCALENDARROOM)) {
			found = 1;
			IAPuts("event.default");
		}
		else if (!strcasecmp(&CC->room.QRname[11], USERCONTACTSROOM)) {
			found = 1;
			IAPuts("contact.default");
		}
		else if (!strcasecmp(&CC->room.QRname[11], USERNOTESROOM)) {
			found = 1;
			IAPuts("note.default");
		}
		else if (!strcasecmp(&CC->room.QRname[11], USERTASKSROOM)) {
			found = 1;
			IAPuts("task.default");
		}
	}

	/* Otherwise, use the view for this room to determine the type of data.
	 * We are going with the default view rather than the user's view, because
	 * the default view almost always defines the actual contents, while the
	 * user's view might only make changes to presentation.  It also saves us
	 * an extra database access because we don't need to load the visit record.
	 */
	if (!found)
	{
		if (CC->room.QRdefaultview == VIEW_CALENDAR) {
			IAPuts("event");
		}
		else if (CC->room.QRdefaultview == VIEW_ADDRESSBOOK) {
			IAPuts("contact");
		}
		else if (CC->room.QRdefaultview == VIEW_TASKS) {
			IAPuts("task");
		}
		else if (CC->room.QRdefaultview == VIEW_NOTES) {
			IAPuts("note");
		}
		else if (CC->room.QRdefaultview == VIEW_JOURNAL) {
			IAPuts("journal");
		}
	}
	/* If none of the above conditions were met, consider it an ordinary mailbox. */

	if (!found) {
		IAPuts("mail");
	}

	/* "mail.outbox" and "junkemail" are not implemented. */

	IAPuts("\")\r\n");

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new, NULL, NULL);
	}

	IReply("OK GETMETADATA complete");
	return;
}

