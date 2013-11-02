/*
 * Handle messages sent and received using XMPP (Jabber) protocol
 *
 * Copyright (c) 2007-2010 by Art Cancro
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 * 
 * 
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * 
 * 
 * 
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
 * This function is called by the XMPP service's async loop.
 * If the client session has instant messages waiting, it outputs
 * unsolicited XML stanzas containing them.
 */
void xmpp_output_incoming_messages(void)
{
	struct CitContext *CCC = CC;
	citxmpp *Xmpp = XMPP;
	struct ExpressMessage *ptr;

	while (CCC->FirstExpressMessage != NULL) {

		begin_critical_section(S_SESSION_TABLE);
		ptr = CCC->FirstExpressMessage;
		CCC->FirstExpressMessage = CCC->FirstExpressMessage->next;
		end_critical_section(S_SESSION_TABLE);

		XPrint(HKEY("message"), 0,
		       XCPROPERTY("type", "chat"),
		       XPROPERTY("to", Xmpp->client_jid, strlen(Xmpp->client_jid)),
		       XPROPERTY("from", ptr->sender_email, strlen(ptr->sender_email)),
		       TYPE_ARGEND);

		if (ptr->text != NULL) {
			striplt(ptr->text);
			XPrint(HKEY("body"), XCLOSED,
			       XBODY(ptr->text, strlen(ptr->text)),
			       TYPE_ARGEND);
			free(ptr->text);
		}
		XPUT("</message>");
		free(ptr);
	}
	XUnbuffer();
}

/*
 * Client is sending a message.
 */
void xmpp_send_message(char *message_to, char *message_body) {
	struct CitContext *CCC = CC;
	char *recp = NULL;
	struct CitContext *cptr;

	if (message_body == NULL) return;
	if (message_to == NULL) return;
	if (IsEmptyStr(message_to)) return;
	if (!CCC->logged_in) return;

	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if (	(cptr->logged_in)
			&& (cptr->can_receive_im)
			&& (!strcasecmp(cptr->cs_inet_email, message_to))
		) {
			recp = cptr->user.fullname;
		}
	}

	if (recp) {
		PerformXmsgHooks(CCC->user.fullname, CCC->cs_inet_email, recp, message_body);
	}

	free(XMPP->message_body);
	XMPP->message_body = NULL;
	XMPP->message_to[0] = 0;
	time(&CCC->lastidle);
}
void xmpp_end_message(void *data, const char *supplied_el, const char **attr)
{
	xmpp_send_message(XMPP->message_to, XMPP->message_body);
	XMPP->html_tag_level = 0;
}



CTDL_MODULE_INIT(xmpp_message)
{
	if (!threading) {
		AddXMPPEndHandler(HKEY("message"),	 xmpp_end_message, 0);
	}
	return "xmpp_message";
}
