/*
 * $Id$
 *
 * IMAP METADATA extension
 *
 * This is a partial implementation of draft-daboo-imap-annotatemore-11
 * intended to help a specific connector product work with Citadel.
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
 * Implements the SETMETADATA command.
 *
 * Again, the only thing we're interested in setting here is the folder type.
 *
 * Attempting to set anything else calls a stub which fools the client into
 * thinking that there is no remaining space available to store annotations.
 */
void imap_setmetadata(int num_parms, char *parms[]) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;
	int setting_user_value = 0;
	char set_value[32];
	int set_view = VIEW_BBS;

	if (num_parms != 6) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	/*
	 * Don't allow other types of metadata to be set
	 */
	if (strcasecmp(parms[3], "/vendor/kolab/folder-type")) {
		cprintf("%s NO [METADATA TOOMANY] SETMETADATA failed\r\n", parms[0]);
		return;
	}

	if (!strcasecmp(parms[4], "(value.shared")) {
		setting_user_value = 0;				/* global view */
	}
	else if (!strcasecmp(parms[4], "(value.priv")) {
		setting_user_value = 1;				/* per-user view */
	}
	else {
		cprintf("%s NO [METADATA TOOMANY] SETMETADATA failed\r\n", parms[0]);
		return;
	}

	/*
	 * Extract the folder type without any parentheses.  Then learn
	 * the Citadel view type based on the supplied folder type.
	 */
	extract_token(set_value, parms[5], 0, ')', sizeof set_value);
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

	/*
	 * FIXME ... NOW DO SOMETHING
	roomname
	set_view
	setting_user_value
	 * on success: cprintf("%s OK SETANNOTATION complete\r\n", parms[0]);
	 */
	lprintf(CTDL_DEBUG, "*** SETMETADATA room='%s' user=%d value=%d\n",
		roomname, setting_user_value, set_view);

	cprintf("%s NO [METADATA TOOMANY] SETMETADATA failed\r\n", parms[0]);

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}
	return;
}


/*
 * Implements the GETMETADATA command.
 *
 * Regardless of what the client asked for, we are going to supply them with
 * the folder type.  It's the only metadata we have anyway.
 */
void imap_getmetadata(int num_parms, char *parms[]) {
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int ret;

	if (num_parms > 5) {
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

	cprintf("* METADATA ");
	imap_strout(parms[2]);
	cprintf(" \"/vendor/kolab/folder-type\" (\"value.shared\" \"");

	/* If it's one of our hard-coded default rooms, we know what to do... */

	if (!strcasecmp(&CC->room.QRname[11], MAILROOM)) {
		cprintf("mail.inbox");
	}
	else if (!strcasecmp(&CC->room.QRname[11], SENTITEMS)) {
		cprintf("mail.sentitems");
	}
	else if (!strcasecmp(&CC->room.QRname[11], USERCALENDARROOM)) {
		cprintf("event.default");
	}
	else if (!strcasecmp(&CC->room.QRname[11], USERCONTACTSROOM)) {
		cprintf("contact.default");
	}
	else if (!strcasecmp(&CC->room.QRname[11], USERNOTESROOM)) {
		cprintf("note.default");
	}
	else if (!strcasecmp(&CC->room.QRname[11], USERTASKSROOM)) {
		cprintf("task.default");
	}

	/* Otherwise, use the view for this room to determine the type of data.
	 * We are going with the default view rather than the user's view, because
	 * the default view almost always defines the actual contents, while the
	 * user's view might only make changes to presentation.  It also saves us
	 * an extra database access because we don't need to load the visit record.
	 */

	else if (CC->room.QRdefaultview == VIEW_CALENDAR) {
		cprintf("event");
	}
	else if (CC->room.QRdefaultview == VIEW_ADDRESSBOOK) {
		cprintf("contact");
	}
	else if (CC->room.QRdefaultview == VIEW_TASKS) {
		cprintf("task");
	}
	else if (CC->room.QRdefaultview == VIEW_NOTES) {
		cprintf("note");
	}
	else if (CC->room.QRdefaultview == VIEW_JOURNAL) {
		cprintf("journal");
	}

	/* If none of the above conditions were met, consider it an ordinary mailbox. */
	else {
		cprintf("mail");
	}

	/* "mail.outbox" and "junkemail" are not implemented. */

	cprintf("\")\r\n");

	/*
	 * If a different folder was previously selected, return there now.
	 */
	if ( (IMAP->selected) && (strcasecmp(roomname, savedroom)) ) {
		usergoto(savedroom, 0, 0, &msgs, &new);
	}

	cprintf("%s OK GETMETADATA complete\r\n", parms[0]);
	return;
}

