/*
 * $Id$ 
 *
 * Handle messages sent and received using XMPP (Jabber) protocol
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
 * This function is called by the XMPP service's async loop.
 * If the client session has instant messages waiting, it outputs
 * unsolicited XML stanzas containing them.
 */
void jabber_output_incoming_messages(void) {

	struct ExpressMessage *ptr;

	while (CC->FirstExpressMessage != NULL) {

		begin_critical_section(S_SESSION_TABLE);
		ptr = CC->FirstExpressMessage;
		CC->FirstExpressMessage = CC->FirstExpressMessage->next;
		end_critical_section(S_SESSION_TABLE);

		/* FIXME we have to get the sender's email address.  This may involve tweaking
		 * the core IM module a bit.
	 	 */
		cprintf("<message to=\"%s\" from=\"FIXME@example.org\" type=\"chat\">", XMPP->client_jid);
		if (ptr->text != NULL) {
			striplt(ptr->text);
			cprintf("<body>%s</body>", ptr->text);
			free(ptr->text);
		}
		cprintf("</message>");
		free(ptr);
	}
}


#endif	/* HAVE_EXPAT */
