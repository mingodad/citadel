/*
 * Handle XMPP presence exchanges
 *
 * Copyright (c) 2007-2010 by Art Cancro
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include <assert.h>

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
 * Indicate the presence of another user to the client
 * (used in several places)
 */
void xmpp_indicate_presence(char *presence_jid)
{
	char xmlbuf[256];

	XMPP_syslog(LOG_DEBUG, "XMPP: indicating presence of <%s> to <%s>", presence_jid, XMPP->client_jid);
	cprintf("<presence from=\"%s\" ", xmlesc(xmlbuf, presence_jid, sizeof xmlbuf));
	cprintf("to=\"%s\"></presence>", xmlesc(xmlbuf, XMPP->client_jid, sizeof xmlbuf));
}



/*
 * Convenience function to determine whether any given session is 'visible' to any other given session,
 * and is capable of receiving instant messages from that session.
 */
int xmpp_is_visible(struct CitContext *cptr, struct CitContext *to_whom) {
	int aide = (to_whom->user.axlevel >= AxAideU);

	if (	(cptr->logged_in)
		&& 	(((cptr->cs_flags&CS_STEALTH)==0) || (aide))	/* aides see everyone */
		&&	(cptr->user.usernum != to_whom->user.usernum)	/* don't show myself */
		&&	(cptr->can_receive_im)				/* IM-capable session */
	) {
		return(1);
	}
	else {
		return(0);
	}
}


/* 
 * Initial dump of the entire wholist
 */
void xmpp_wholist_presence_dump(void)
{
	struct CitContext *cptr = NULL;
	int nContexts, i;
	
	cptr = CtdlGetContextArray(&nContexts);
	if (!cptr) {
		return;
	}

	for (i=0; i<nContexts; i++) {
		if (xmpp_is_visible(&cptr[i], CC)) {
			xmpp_indicate_presence(cptr[i].cs_inet_email);
		}
	}
	free(cptr);
}


/*
 * Function to remove a buddy subscription and delete from the roster
 * (used in several places)
 */
void xmpp_destroy_buddy(char *presence_jid, int aggressively) {
	static int unsolicited_id = 1;
	char xmlbuf1[256];
	char xmlbuf2[256];

	if (!presence_jid) return;
	if (!XMPP) return;
	if (!XMPP->client_jid) return;

	/* Transmit non-presence information */
	cprintf("<presence type=\"unavailable\" from=\"%s\" to=\"%s\"></presence>",
		xmlesc(xmlbuf1, presence_jid, sizeof xmlbuf1),
		xmlesc(xmlbuf2, XMPP->client_jid, sizeof xmlbuf2)
	);

	/*
	 * Setting the "aggressively" flag also sends an "unsubscribed" presence update.
	 * We only ask for this when flushing the client side roster, because if we do it
	 * in the middle of a session when another user logs off, some clients (Jitsi) interpret
	 * it as a rejection of a subscription request.
	 */
	if (aggressively) {
		cprintf("<presence type=\"unsubscribed\" from=\"%s\" to=\"%s\"></presence>",
			xmlesc(xmlbuf1, presence_jid, sizeof xmlbuf1),
			xmlesc(xmlbuf2, XMPP->client_jid, sizeof xmlbuf2)
		);
	}

	// FIXME ... we should implement xmpp_indicate_nonpresence so we can use it elsewhere

	/* Do an unsolicited roster update that deletes the contact. */
	cprintf("<iq from=\"%s\" to=\"%s\" id=\"unbuddy_%x\" type=\"result\">",
		xmlesc(xmlbuf1, CC->cs_inet_email, sizeof xmlbuf1),
		xmlesc(xmlbuf2, XMPP->client_jid, sizeof xmlbuf2),
		++unsolicited_id
	);
	cprintf("<query xmlns=\"jabber:iq:roster\">");
	cprintf("<item jid=\"%s\" subscription=\"remove\">", xmlesc(xmlbuf1, presence_jid, sizeof xmlbuf1));
	cprintf("<group>%s</group>", xmlesc(xmlbuf1, config.c_humannode, sizeof xmlbuf1));
	cprintf("</item>");
	cprintf("</query>"
		"</iq>"
	);
}


/*
 * When a user logs in or out of the local Citadel system, notify all XMPP sessions about it.
 * THIS FUNCTION HAS A BUG IN IT THAT ENUMERATES THE SESSIONS WRONG.
 */
void xmpp_presence_notify(char *presence_jid, int event_type) {
	struct CitContext *cptr;
	static int unsolicited_id = 12345;
	int visible_sessions = 0;
	int nContexts, i;
	int which_cptr_is_relevant = (-1);

	if (IsEmptyStr(presence_jid)) return;
	if (CC->kill_me) return;

	cptr = CtdlGetContextArray(&nContexts);
	if (!cptr) {
		return;
	}

	/* Count the visible sessions for this user */
	for (i=0; i<nContexts; i++) {
		if ( (!strcasecmp(cptr[i].cs_inet_email, presence_jid))
		   && (xmpp_is_visible(&cptr[i], CC))
		)  {
			++visible_sessions;
			which_cptr_is_relevant = i;
		}
	}

	syslog(LOG_DEBUG, "%d sessions for <%s> are now visible to session %d\n", visible_sessions, presence_jid, CC->cs_pid);

	if ( (event_type == XMPP_EVT_LOGIN) && (visible_sessions == 1) ) {

		syslog(LOG_DEBUG, "Telling session %d that <%s> logged in\n", CC->cs_pid, presence_jid);

		/* Do an unsolicited roster update that adds a new contact. */
		assert(which_cptr_is_relevant >= 0);
		cprintf("<iq id=\"unsolicited_%x\" type=\"result\">", ++unsolicited_id);
		cprintf("<query xmlns=\"jabber:iq:roster\">");
		xmpp_roster_item(&cptr[which_cptr_is_relevant]);
		cprintf("</query></iq>");

		/* Transmit presence information */
		xmpp_indicate_presence(presence_jid);
	}

	if (visible_sessions == 0) {
		syslog(LOG_DEBUG, "Telling session %d that <%s> logged out\n",
			    CC->cs_pid, presence_jid);
		xmpp_destroy_buddy(presence_jid, 0);	/* non aggressive presence update */
	}

	free(cptr);
}



void xmpp_fetch_mortuary_backend(long msgnum, void *userdata) {
	HashList *mortuary = (HashList *) userdata;
	struct CtdlMessage *msg;
	char *ptr = NULL;
	char *lasts = NULL;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		return;
	}

	/* now add anyone we find into the hashlist */

	/* skip past the headers */
	ptr = strstr(msg->cm_fields[eMesageText], "\n\n");
	if (ptr != NULL) {
		ptr += 2;
	}
	else {
		ptr = strstr(msg->cm_fields[eMesageText], "\n\r\n");
		if (ptr != NULL) {
			ptr += 3;
		}
	}

	/* the remaining lines are addresses */
	if (ptr != NULL) {
		ptr = strtok_r(ptr, "\n", &lasts);
		while (ptr != NULL) {
			char *pch = strdup(ptr);
			Put(mortuary, pch, strlen(pch), pch, NULL);
			ptr = strtok_r(NULL, "\n", &lasts);
		}
	}

	CM_Free(msg);
}



/*
 * Fetch the "mortuary" - a list of dead buddies which we keep around forever
 * so we can remove them from any client's roster that still has them listed
 */
HashList *xmpp_fetch_mortuary(void) {
	HashList *mortuary = NewHash(1, NULL);
	if (!mortuary) {
		syslog(LOG_ALERT, "NewHash() failed!\n");
		return(NULL);
	}

        if (CtdlGetRoom(&CC->room, USERCONFIGROOM) != 0) {
		/* no config room exists - no further processing is required. */
                return(mortuary);
        }
        CtdlForEachMessage(MSGS_LAST, 1, NULL, XMPPMORTUARY, NULL,
                xmpp_fetch_mortuary_backend, (void *)mortuary );

	return(mortuary);
}



/*
 * Fetch the "mortuary" - a list of dead buddies which we keep around forever
 * so we can remove them from any client's roster that still has them listed
 */
void xmpp_store_mortuary(HashList *mortuary) {
	HashPos *HashPos;
	long len;
	void *Value;
	const char *Key;
	StrBuf *themsg;

	themsg = NewStrBuf();
	StrBufPrintf(themsg,	"Content-type: " XMPPMORTUARY "\n"
				"Content-transfer-encoding: 7bit\n"
				"\n"
	);

	HashPos = GetNewHashPos(mortuary, 0);
	while (GetNextHashPos(mortuary, HashPos, &len, &Key, &Value) != 0)
	{
		StrBufAppendPrintf(themsg, "%s\n", (char *)Value);
	}
	DeleteHashPos(&HashPos);

	/* FIXME temp crap 
	StrBufAppendPrintf(themsg, "foo@bar.com\n");
	StrBufAppendPrintf(themsg, "baz@quux.com\n");
	StrBufAppendPrintf(themsg, "haha%c\n", 1);
	StrBufAppendPrintf(themsg, "baaaz@quux.com\n");
	StrBufAppendPrintf(themsg, "baaaz@quuuuuux.com\n");
	*/

	/* Delete the old mortuary */
	CtdlDeleteMessages(USERCONFIGROOM, NULL, 0, XMPPMORTUARY);

	/* And save the new one to disk */
	quickie_message("Citadel", NULL, NULL, USERCONFIGROOM, ChrPtr(themsg), 4, "XMPP Mortuary");
	FreeStrBuf(&themsg);
}



/*
 * Upon logout we make an attempt to delete the whole roster, in order to
 * try to keep "ghost" buddies from remaining in the client-side roster.
 *
 * Since the client is probably not still alive, also remember the current
 * roster for next time so we can delete dead buddies then.
 */
void xmpp_massacre_roster(void)
{
	struct CitContext *cptr;
	int nContexts, i;
	HashList *mortuary = xmpp_fetch_mortuary();

	cptr = CtdlGetContextArray(&nContexts);
	if (cptr) {
		for (i=0; i<nContexts; i++) {
			if (xmpp_is_visible(&cptr[i], CC)) {
				if (mortuary) {
					char *buddy = strdup(cptr[i].cs_inet_email);
					Put(mortuary, buddy, strlen(buddy), buddy, NULL);
				}
			}
		}
		free (cptr);
	}

	if (mortuary) {
		xmpp_store_mortuary(mortuary);
		DeleteHash(&mortuary);
	}
}



/*
 * Stupidly, XMPP does not specify a way to tell the client to flush its client-side roster
 * and prepare to receive a new one.  So instead we remember every buddy we've ever told the
 * client about, and push delete operations out at the beginning of a session.
 * 
 * We omit any users who happen to be online right now, but we still keep them in the mortuary,
 * which needs to be maintained as a list of every buddy the user has ever seen.  We don't know
 * when they're connecting from the same client and when they're connecting from a different client,
 * so we have no guarantee of what is in the client side roster at connect time.
 */
void xmpp_delete_old_buddies_who_no_longer_exist_from_the_client_roster(void)
{
	long len;
	void *Value;
	const char *Key;
	struct CitContext *cptr;
	int nContexts, i;
	int online_now = 0;
	HashList *mortuary = xmpp_fetch_mortuary();
	HashPos *HashPos = GetNewHashPos(mortuary, 0);

	/* we need to omit anyone who is currently online */
	cptr = CtdlGetContextArray(&nContexts);

	/* go through the list of users in the mortuary... */
	while (GetNextHashPos(mortuary, HashPos, &len, &Key, &Value) != 0)
	{

		online_now = 0;
		if (cptr) for (i=0; i<nContexts; i++) {
			if (xmpp_is_visible(&cptr[i], CC)) {
				if (!strcasecmp(cptr[i].cs_inet_email, (char *)Value)) {
					online_now = 1;
				}
			}
		}

		if (!online_now) {
			xmpp_destroy_buddy((char *)Value, 1);	/* aggressive presence update */
		}

	}
	DeleteHashPos(&HashPos);
	DeleteHash(&mortuary);
	free(cptr);
}

