/*
 * $Id:  $ 
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
 * TODO: handle queries on some or all of these namespaces

xmpp_query_namespace(purple5b5c1e58, splorph.xand.com, http://jabber.org/protocol/disco#items:query)
xmpp_query_namespace(purple5b5c1e59, splorph.xand.com, http://jabber.org/protocol/disco#info:query)
xmpp_query_namespace(purple5b5c1e5a, , vcard-temp:query)
xmpp_query_namespace(purple5b5c1e5b, , jabber:iq:roster:query)
 
 */

void xmpp_query_namespace(char *iq_id, char *iq_to, char *query_xmlns) {

	lprintf(CTDL_DEBUG, "[31mxmpp_query_namespace(%s, %s, %s)[0m\n", iq_id, iq_to, query_xmlns);

	/*
	 * Unknown query.  Return the XML equivalent of a blank stare (empty result)
	 */
	cprintf("<iq type=\"result\" id=\"%s\">", iq_id);
	cprintf("</iq>");

}

#endif	/* HAVE_EXPAT */
