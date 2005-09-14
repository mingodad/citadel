/*
 * $Id$
 *
 * Autocompletion of email recipients, etc.
 */

#ifdef DLL_EXPORT
#define IN_LIBCIT
#endif

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
#include "serv_extensions.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "tools.h"
#include "msgbase.h"
#include "user_ops.h"
#include "room_ops.h"
#include "database.h"
#include "vcard.h"
#include "serv_autocompletion.h"


#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


/*
 * Back end for cmd_auto()
 */
void hunt_for_autocomplete(long msgnum, void *data) {
	char *search_string;
	struct CtdlMessage *msg;
	struct vCard *v;

	search_string = (char *) data;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;

	v = vcard_load(msg->cm_fields['M']);
	CtdlFreeMessage(msg);

	/*
	 * Try to match from a display name or something like that
	 */
	if (
		(bmstrcasestr(vcard_get_prop(v, "n", 0, 0, 0), search_string))
	) {
		cprintf("%s\n", vcard_get_prop(v, "email", 1, 0, 0));
	}

	vcard_free(v);
}



/*
 * Attempt to autocomplete an address based on a partial...
 */
void cmd_auto(char *argbuf) {
	char hold_rm[ROOMNAMELEN];
	char search_string[256];

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(search_string, argbuf, 0, '|', sizeof search_string);
	if (strlen(search_string) == 0) {
		cprintf("%d You supplied an empty partial.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	strcpy(hold_rm, CC->room.QRname);       /* save current room */

	if (getroom(&CC->room, USERCONTACTSROOM) != 0) {
		getroom(&CC->room, hold_rm);
		lprintf(CTDL_CRIT, "cannot get user contacts room\n");
		cprintf("%d Your address book was not found.\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	cprintf("%d try these:\n", LISTING_FOLLOWS);
	CtdlForEachMessage(MSGS_ALL, 0, "text/x-vcard", NULL, hunt_for_autocomplete, search_string);
	cprintf("000\n");

	getroom(&CC->room, hold_rm);    /* return to saved room */
}


char *serv_autocompletion_init(void)
{
	CtdlRegisterProtoHook(cmd_auto, "AUTO", "Perform recipient autocompletion");
	return "$Id$";
}
