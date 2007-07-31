/*
 * $Id$
 *
 * Handles functions related to yellow sticky notes.
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
#include "tools.h"

#include "ctdl_module.h"



/*
 * If we are in a "notes" view room, and the client has sent an RFC822
 * message containing an X-KOrg-Note-Id: field (Aethera does this, as
 * do some Kolab clients) then set both the Subject and the Exclusive ID
 * of the message to that.  It's going to be a UUID so we want to replace
 * any existing message containing that UUID.
 */
int serv_notes_beforesave(struct CtdlMessage *msg)
{
	char *p;
	int a, i;
	char uuid[SIZ];

	/* First determine if this room has the "notes" view set */

	if (CC->room.QRdefaultview != VIEW_NOTES) {
		return(0);			/* not notes; do nothing */
	}

	/* It must be an RFC822 message! */
	if (msg->cm_format_type != 4) {
		return(0);	/* You tried to save a non-RFC822 message! */
	}
	
	/* Find the X-KOrg-Note-Id: header */
	strcpy(uuid, "");
	p = msg->cm_fields['M'];
	a = strlen(p);
	while (--a > 0) {
		if (!strncasecmp(p, "X-KOrg-Note-Id: ", 16)) {	/* Found it */
			safestrncpy(uuid, p + 16, sizeof(uuid));
			for (i = 0; i<strlen(uuid); ++i) {
				if ( (uuid[i] == '\r') || (uuid[i] == '\n') ) {
					uuid[i] = 0;
				}
			}

			lprintf(9, "UUID of note is: %s\n", uuid);
			if (strlen(uuid) > 0) {

				if (msg->cm_fields['E'] != NULL) {
					free(msg->cm_fields['E']);
				}
				msg->cm_fields['E'] = strdup(uuid);

				if (msg->cm_fields['U'] != NULL) {
					free(msg->cm_fields['U']);
				}
				msg->cm_fields['U'] = strdup(uuid);
			}
		}
		p++;
	}
	
	return(0);
}


CTDL_MODULE_INIT(notes)
{
	CtdlRegisterMessageHook(serv_notes_beforesave, EVT_BEFORESAVE);

	/* return our Subversion id for the Log */
	return "$Id$";
}
