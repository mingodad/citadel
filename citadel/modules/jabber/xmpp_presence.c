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
	struct CitContext *cptr;
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


#endif	/* HAVE_EXPAT */
