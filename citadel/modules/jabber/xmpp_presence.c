/*
 * $Id$ 
 *
 * Handle XMPP presence exchanges
 *
 * Copyright (c) 2007 by Art Cancro
 * This code is released under the terms of the GNU General Public License.
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
#include <ctype.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"

#ifdef HAVE_EXPAT
#include <expat.h>
#include "serv_xmpp.h"


/* 
 * Initial dump of the entire wholist
 */
void jabber_wholist_presence_dump(void)
{
	struct CitContext *cptr = NULL;
	int aide = (CC->user.axlevel >= 6);

	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if (
		   (((cptr->cs_flags&CS_STEALTH)==0) || (aide))		/* aides can see everyone */
		   && (cptr->user.usernum != CC->user.usernum)		/* don't tell me about myself */
		   ) {
			cprintf("<presence type=\"available\" from=\"%s\"></presence>", cptr->cs_inet_email);
		}
	}
}



/*
 * When a user logs in or out of the local Citadel system, notify all Jabber sessions
 * about it.
 */
void xmpp_presence_notify(char *presence_jid, char *presence_type) {
	struct CitContext *cptr;
	static int unsolicited_id;

	/* FIXME subject this to the same conditions as above */

	/* FIXME make sure don't do this for multiple logins of the same user (login)
	 * or until the last concurrent login is logged out (logout)
	 */

	if (IsEmptyStr(presence_jid)) return;
	lprintf(CTDL_DEBUG, "Sending presence info about <%s> to session %d\n", presence_jid, CC->cs_pid);

	/* Transmit an unsolicited roster update if the presence is anything other than "unavailable" */
	if (strcasecmp(presence_type, "unavailable")) {
		for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
			if (!strcasecmp(cptr->cs_inet_email, presence_jid)) {
				cprintf("<iq id=\"unsolicited_%x\" type=\"result\">", ++unsolicited_id);
				cprintf("<query xmlns=\"jabber:iq:roster\">");
				jabber_roster_item(cptr);
				cprintf("</query>"
					"</iq>");
			}
		}
	}

	/* Now transmit unsolicited presence information */
	cprintf("<presence type=\"%s\" from=\"%s\"></presence>", presence_type, presence_jid);

	/* For "unavailable" we do an unsolicited roster update that deletes the contact. */
	if (!strcasecmp(presence_type, "unavailable")) {
		cprintf("<iq id=\"unsolicited_%x\" type=\"result\">", ++unsolicited_id);
		cprintf("<query xmlns=\"jabber:iq:roster\">");
		cprintf("<item jid=\"%s\" subscription=\"none\">", presence_jid);
		cprintf("<group>%s</group>", config.c_humannode);
		cprintf("</item>");
		cprintf("</query>"
			"</iq>");
	}
}


#endif	/* HAVE_EXPAT */
