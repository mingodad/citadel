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
#include "room_ops.h"
#include "tools.h"
#include "serv_fulltext.h"
#include "ft_wordbreaker.h"


long ft_newhighest = 0L;
long *ft_newmsgs = NULL;
int ft_num_msgs = 0;
int ft_num_alloc = 0;


void ft_index_msg(long msgnum, void *userdata) {

	if ((msgnum > CitControl.MMfulltext) && (msgnum <= ft_newhighest)) {
		++ft_num_msgs;
		if (ft_num_msgs > ft_num_alloc) {
			ft_num_alloc += 1024;
			ft_newmsgs = realloc(ft_newmsgs, (ft_num_alloc * sizeof(long)));
		}
		ft_newmsgs[ft_num_msgs - 1] = msgnum;
	}

}

/*
 * Scan a room for messages to index.
 */
void ft_index_room(struct ctdlroom *qrbuf, void *data)
{
	getroom(&CC->room, qrbuf->QRname);
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, ft_index_msg, NULL);
}


/*
 * Compare function
 */
int longcmp(const void *rec1, const void *rec2) {
	long i1, i2;

	i1 = *(const long *)rec1;
	i2 = *(const long *)rec2;

	if (i1 > i2) return(1);
	if (i1 < i2) return(-1);
	return(0);
}



void do_fulltext_indexing(void) {
	int i;

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
	 * Make sure we don't run the indexer too frequently.
	 * FIXME write this...
	 */

	/*
	 * If we've switched wordbreaker modules, burn the index and start
	 * over.  FIXME write this...
	 */

	/*
	 * Now go through each room and find messages to index.
	 */
	ft_newhighest = CitControl.MMhighest;
	ForEachRoom(ft_index_room, NULL);				/* merge ptrs */

	if (ft_num_msgs > 0) {
		qsort(ft_newmsgs, ft_num_msgs, sizeof(long), longcmp);	/* sort */
		if (i>1) for (i=0; i<(ft_num_msgs-1); ++i) {		/* purge dups */
			if (ft_newmsgs[i] == ft_newmsgs[i+1]) {
				memmove(&ft_newmsgs[i], &ft_newmsgs[i+1], ((ft_num_msgs - i)*sizeof(long)));
				--ft_num_msgs;
			}
		}

		/* Here it is ... do each message! */
		for (i=0; i<ft_num_msgs; ++i) {
			lprintf(CTDL_DEBUG, "FIXME INDEX %ld\n", ft_newmsgs[i]);
		}

		free(ft_newmsgs);
		ft_num_msgs = 0;
		ft_num_alloc = 0;
	}

	lprintf(CTDL_DEBUG, "do_fulltext_indexing() finished\n");
	return;
}


/*****************************************************************************/

char *serv_fulltext_init(void)
{
	CtdlRegisterSessionHook(do_fulltext_indexing, EVT_TIMER);
	return "$Id$";
}
