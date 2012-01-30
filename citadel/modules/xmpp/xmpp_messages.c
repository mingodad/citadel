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
void xmpp_output_incoming_messages(void) {

	struct ExpressMessage *ptr;
	char xmlbuf1[4096];
	char xmlbuf2[4096];

	while (CC->FirstExpressMessage != NULL) {

		begin_critical_section(S_SESSION_TABLE);
		ptr = CC->FirstExpressMessage;
		CC->FirstExpressMessage = CC->FirstExpressMessage->next;
		end_critical_section(S_SESSION_TABLE);

		cprintf("<message to=\"%s\" from=\"%s\" type=\"chat\">",
			xmlesc(xmlbuf1, XMPP->client_jid, sizeof xmlbuf1),
			xmlesc(xmlbuf2, ptr->sender_email, sizeof xmlbuf2)
		);
		if (ptr->text != NULL) {
			striplt(ptr->text);
			cprintf("<body>%s</body>", xmlesc(xmlbuf1, ptr->text, sizeof xmlbuf1));
			free(ptr->text);
		}
		cprintf("</message>");
		free(ptr);
	}
}

/*
 * Client is sending a message.
 */
void xmpp_send_message(char *message_to, char *message_body) {
	char *recp = NULL;
	struct CitContext *cptr;

	if (message_body == NULL) return;
	if (message_to == NULL) return;
	if (IsEmptyStr(message_to)) return;
	if (!CC->logged_in) return;

	for (cptr = ContextList; cptr != NULL; cptr = cptr->next) {
		if (	(cptr->logged_in)
			&& (cptr->can_receive_im)
			&& (!strcasecmp(cptr->cs_inet_email, message_to))
		) {
			recp = cptr->user.fullname;
		}
	}

	if (recp) {
		PerformXmsgHooks(CC->user.fullname, CC->cs_inet_email, recp, message_body);
	}

	free(XMPP->message_body);
	XMPP->message_body = NULL;
	XMPP->message_to[0] = 0;
	time(&CC->lastidle);
}

