/*
 * $Id$ 
 *
 * Handle <iq> <get> <query> type situations (namespace queries)
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
 * Output a single roster item, for roster queries or pushes
 */
void jabber_roster_item(struct CitContext *cptr) {
	cprintf("<item jid=\"%s\" name=\"%s\" subscription=\"both\">",
		cptr->cs_inet_email,
		cptr->user.fullname
	);
	cprintf("<group>%s</group>", config.c_humannode);
	cprintf("</item>");
}

/* 
 * Return the results for a "jabber:iq:roster:query"
 *
 * Since we are not yet managing a roster, we simply return the entire wholist
 * (minus any entries for this user -- don't tell me about myself)
 *
 */
void jabber_iq_roster_query(void)
{
	struct CitContext *cptr;
	int nContexts, i;
	int aide = (CC->user.axlevel >= 6);

	cprintf("<query xmlns=\"jabber:iq:roster\">");

	cptr = CtdlGetContextArray(&nContexts);
	if (!cptr)
		return ; /** FIXME: Does jabber need to send something to maintain the protocol?  */
		
	for (i=0; i<nContexts; i++) {
		if (cptr[i].logged_in) {
			if (
			   (((cptr[i].cs_flags&CS_STEALTH)==0) || (aide))
			   && (cptr[i].user.usernum != CC->user.usernum)
			   ) {
				jabber_roster_item(&cptr[i]);
			}
		}
	}
	free (cptr);
	cprintf("</query>");
}


/*
 * TODO: handle queries on some or all of these namespaces
 *
xmpp_query_namespace(purple5b5c1e58, splorph.xand.com, http://jabber.org/protocol/disco#items:query)
xmpp_query_namespace(purple5b5c1e59, splorph.xand.com, http://jabber.org/protocol/disco#info:query)
xmpp_query_namespace(purple5b5c1e5a, , vcard-temp:query)
 *
 */

void xmpp_query_namespace(char *iq_id, char *iq_from, char *iq_to, char *query_xmlns) {

	lprintf(CTDL_DEBUG, "xmpp_query_namespace(%s, %s, %s, %s)\n", iq_id, iq_from, iq_to, query_xmlns);

	/*
	 * Beginning of query result.
	 */
	cprintf("<iq type=\"result\" ");
	if (!IsEmptyStr(iq_from)) {
		cprintf("to=\"%s\" ", iq_from);
	}
	cprintf("id=\"%s\">", iq_id);

	/*
	 * Is this a query we know how to handle?
	 */

	if (!strcasecmp(query_xmlns, "jabber:iq:roster:query")) {
		jabber_iq_roster_query();
	}

	else if (!strcasecmp(query_xmlns, "jabber:iq:auth:query")) {
		cprintf("<query xmlns=\"jabber:iq:auth\">"
			"<username/><password/><resource/>"
			"</query>"
		);
	}

	/*
	 * End of query result.  If we didn't hit any known namespaces then we will
	 * have simply delivered an empty result stanza, which should be ok.
	 */
	cprintf("</iq>");

}
