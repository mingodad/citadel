/*
 * $Id$ 
 *
 * Handle XMPP presence exchanges
 *
 * Copyright (c) 2007-2009 by Art Cancro
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
#include <expat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "internet_addressing.h"
#include "md5.h"
#include "ctdl_module.h"
#include "serv_xmpp.h"


/* 
 * Initial dump of the entire wholist
 */
void jabber_wholist_presence_dump(void)
{
	struct CitContext *cptr = NULL;
	int nContexts, i;
	
	int aide = (CC->user.axlevel >= 6);

	cptr = CtdlGetContextArray(&nContexts);
	if (!cptr)
		return ; /** FIXME: Does jabber need to send something to maintain the protocol?  */
		
	for (i=0; i<nContexts; i++) {
		if (cptr[i].logged_in) {
			if (
				(((cptr[i].cs_flags&CS_STEALTH)==0) || (aide))	/* aides see everyone */
				&& (cptr[i].user.usernum != CC->user.usernum)	/* don't show myself */
				&& (cptr[i].can_receive_im)			/* IM-capable session */
			   ) {
				cprintf("<presence type=\"available\" from=\"%s\"></presence>",
					cptr[i].cs_inet_email);
			}
		}
	}
	free(cptr);
}



/*
 * When a user logs in or out of the local Citadel system, notify all Jabber sessions
 * about it.
 */
void xmpp_presence_notify(char *presence_jid, int event_type) {
	struct CitContext *cptr;
	static int unsolicited_id;
	int visible_sessions = 0;
	int nContexts, i;
	int aide = (CC->user.axlevel >= 6);

	if (IsEmptyStr(presence_jid)) return;

	cptr = CtdlGetContextArray(&nContexts);
	if (!cptr)
		return ; /** FIXME: Does jabber need to send something to maintain the protocol?  */
		
	/* Count the visible sessions for this user */
	for (i=0; i<nContexts; i++) {
		if (cptr[i].logged_in) {
			if (
				(!strcasecmp(cptr[i].cs_inet_email, presence_jid)) 
				&& (((cptr[i].cs_flags&CS_STEALTH)==0) || (aide))
				&& (cptr[i].can_receive_im)
			) {
				++visible_sessions;
			}
		}
	}

	CtdlLogPrintf(CTDL_DEBUG, "%d sessions for <%s> are now visible to session %d\n",
		visible_sessions, presence_jid, CC->cs_pid);

	if ( (event_type == XMPP_EVT_LOGIN) && (visible_sessions == 1) ) {

		CtdlLogPrintf(CTDL_DEBUG, "Telling session %d that <%s> logged in\n", CC->cs_pid, presence_jid);

		/* Do an unsolicited roster update that adds a new contact. */
		for (i=0; i<nContexts; i++) {
			if (cptr[i].logged_in) {
				if (!strcasecmp(cptr[i].cs_inet_email, presence_jid)) {
					cprintf("<iq id=\"unsolicited_%x\" type=\"result\">",
						++unsolicited_id);
					cprintf("<query xmlns=\"jabber:iq:roster\">");
					jabber_roster_item(&cptr[i]);
					cprintf("</query>"
						"</iq>");
				}
			}
		}

		/* Transmit presence information */
		cprintf("<presence type=\"available\" from=\"%s\"></presence>", presence_jid);
	}

	if (visible_sessions == 0) {
		CtdlLogPrintf(CTDL_DEBUG, "Telling session %d that <%s> logged out\n", CC->cs_pid, presence_jid);

		/* Transmit non-presence information */
		cprintf("<presence type=\"unavailable\" from=\"%s\"></presence>", presence_jid);
		cprintf("<presence type=\"unsubscribed\" from=\"%s\"></presence>", presence_jid);

		/* Do an unsolicited roster update that deletes the contact. */
		cprintf("<iq id=\"unsolicited_%x\" type=\"result\">", ++unsolicited_id);
		cprintf("<query xmlns=\"jabber:iq:roster\">");
		cprintf("<item jid=\"%s\" subscription=\"remove\">", presence_jid);
		cprintf("<group>%s</group>", config.c_humannode);
		cprintf("</item>");
		cprintf("</query>"
			"</iq>");
	}
	free(cptr);
}
