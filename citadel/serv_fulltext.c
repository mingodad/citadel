/*
 * $Id$
 *
 * This module handles fulltext indexing of the message base.
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
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "database.h"
#include "msgbase.h"
#include "control.h"
#include "tools.h"
#include "serv_fulltext.h"
#include "ft_wordbreaker.h"


void do_fulltext_indexing(void) {
	lprintf(CTDL_DEBUG, "do_fulltext_indexing() started\n");

	/*
	 * Check to see whether the fulltext index is up to date; if there
	 * are no messages to index, don't waste any more time trying.
	 */
	lprintf(CTDL_DEBUG, "CitControl.MMhighest  = %ld\n", CitControl.MMhighest);
	lprintf(CTDL_DEBUG, "CitControl.MMfulltext = %ld\n", CitControl.MMfulltext);
	if (CitControl.MMfulltext >= CitControl.MMhighest) {
		lprintf(CTDL_DEBUG, "Nothing to do!\n");
		return;
	}

	/*
	 * If we've switched wordbreaker modules, burn the index and start
	 * over.  FIXME write this...
	 */


	lprintf(CTDL_DEBUG, "do_fulltext_indexing() finished\n");
	return;
}


/*****************************************************************************/

char *serv_fulltext_init(void)
{
	CtdlRegisterSessionHook(do_fulltext_indexing, EVT_TIMER);
	return "$Id$";
}
