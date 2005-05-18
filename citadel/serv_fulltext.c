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




/*
 * Index or de-index a message.  (op == 1 to index, 0 to de-index)
 */
void ft_index_message(long msgnum, int op) {
	struct CtdlMessage *msg;
	int num_tokens = 0;
	int *tokens = NULL;
	int i, j;
	struct cdbdata *cdb_bucket;
	int num_msgs;
	long *msgs;

	lprintf(CTDL_DEBUG, "ft_index_message() %s msg %ld\n",
		(op ? "adding" : "removing") , msgnum
	);

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;

	if (msg->cm_fields['M'] != NULL) {
		wordbreaker(msg->cm_fields['M'], &num_tokens, &tokens);
	}
	CtdlFreeMessage(msg);

	if (num_tokens > 0) {
		for (i=0; i<num_tokens; ++i) {

			/* Add the message to the relevant token bucket */

			/* FIXME lock the file */
			cdb_bucket = cdb_fetch(CDB_FULLTEXT, &tokens[i], sizeof(int));
			if (cdb_bucket == NULL) {
				cdb_bucket = malloc(sizeof(struct cdbdata));
				cdb_bucket->len = 0;
				cdb_bucket->ptr = NULL;
				num_msgs = 0;
			}
			else {
				num_msgs = cdb_bucket->len / sizeof(long);
			}

			if (op == 1) {	/* add to index */
				++num_msgs;
				cdb_bucket->ptr = realloc(cdb_bucket->ptr, num_msgs*sizeof(long) );
				msgs = (long *) cdb_bucket->ptr;
				msgs[num_msgs - 1] = msgnum;
			}

			if (op == 0) {	/* remove from index */
				if (num_msgs >= 1) {
				msgs = (long *) cdb_bucket->ptr;
					for (j=0; j<num_msgs; ++j) {
						if (msgs[j] == msgnum) {
							memmove(&msgs[j], &msgs[j+1],
								((num_msgs - j - 1)*sizeof(long)));
							--num_msgs;
						}
					}
				}
			}

			/* sort and purge dups */
			if ( (op == 1) && (num_msgs > 1) ) {
				msgs = (long *) cdb_bucket->ptr;
				qsort(msgs, num_msgs, sizeof(long), longcmp);
				for (j=0; j<(num_msgs-1); ++j) {
					if (msgs[j] == msgs[j+1]) {
						memmove(&msgs[j], &msgs[j+1],
							((num_msgs - j - 1)*sizeof(long)));
						--num_msgs;
					}
				}
			}

			cdb_store(CDB_FULLTEXT, &tokens[i], sizeof(int),
				msgs, (num_msgs*sizeof(long)) );

			cdb_free(cdb_bucket);

			/* FIXME unlock the file */
		}

		free(tokens);
	}
}



/*
 * Add a message to the list of those to be indexed.
 */
void ft_index_msg(long msgnum, void *userdata) {

	if ((msgnum > CitControl.MMfulltext) && (msgnum <= ft_newhighest)) {
		++ft_num_msgs;
		if (ft_num_msgs > ft_num_alloc) {
			ft_num_alloc += 1024;
			ft_newmsgs = realloc(ft_newmsgs,
				(ft_num_alloc * sizeof(long)));
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
 * Begin the fulltext indexing process.  (Called as an EVT_TIMER event)
 */
void do_fulltext_indexing(void) {
	int i;
	static time_t last_index = 0L;

	/*
	 * Make sure we don't run the indexer too frequently.
	 * FIXME move the setting into config
	 */
	if ( (time(NULL) - last_index) < 300L) {
		return;
	}

	/*
	 * Check to see whether the fulltext index is up to date; if there
	 * are no messages to index, don't waste any more time trying.
	 */
	lprintf(CTDL_DEBUG, "CitControl.MMhighest  = %ld\n", CitControl.MMhighest);
	lprintf(CTDL_DEBUG, "CitControl.MMfulltext = %ld\n", CitControl.MMfulltext);
	if (CitControl.MMfulltext >= CitControl.MMhighest) {
		/* nothing to do! */
		return;
	}

	lprintf(CTDL_DEBUG, "do_fulltext_indexing() started\n");
	
	/*
	 * If we've switched wordbreaker modules, burn the index and start
	 * over.  FIXME write this...
	 */
	if (CitControl.fulltext_wordbreaker != FT_WORDBREAKER_ID) {
		lprintf(CTDL_INFO, "(re)initializing full text index\n");
		cdb_trunc(CDB_FULLTEXT);
		CitControl.MMfulltext = 0L;
		put_control();
	}

	/*
	 * Now go through each room and find messages to index.
	 */
	ft_newhighest = CitControl.MMhighest;
	ForEachRoom(ft_index_room, NULL);	/* load all msg pointers */

	if (ft_num_msgs > 0) {
		qsort(ft_newmsgs, ft_num_msgs, sizeof(long), longcmp);
		for (i=0; i<(ft_num_msgs-1); ++i) { /* purge dups */
			if (ft_newmsgs[i] == ft_newmsgs[i+1]) {
				memmove(&ft_newmsgs[i], &ft_newmsgs[i+1],
					((ft_num_msgs - i - 1)*sizeof(long)));
				--ft_num_msgs;
			}
		}

		/* Here it is ... do each message! */
		for (i=0; i<ft_num_msgs; ++i) {
			ft_index_message(ft_newmsgs[i], 1);
		}

		free(ft_newmsgs);
		ft_num_msgs = 0;
		ft_num_alloc = 0;
		ft_newmsgs = NULL;
	}

	/* Save our place so we don't have to do this again */
	CitControl.MMfulltext = ft_newhighest;
	CitControl.fulltext_wordbreaker = FT_WORDBREAKER_ID;
	put_control();
	last_index = time(NULL);

	lprintf(CTDL_DEBUG, "do_fulltext_indexing() finished\n");
	return;
}


/*
 * API call to perform searches.
 * (This one does the "all of these words" search.)
 * Caller is responsible for freeing the message list.
 */
void ft_search(int *fts_num_msgs, long **fts_msgs, char *search_string) {
	int num_tokens = 0;
	int *tokens = NULL;
	int i, j;
	struct cdbdata *cdb_bucket;
	int num_msgs;
	long *msgs;
	int num_all_msgs = 0;
	long *all_msgs = NULL;
	int num_ret_msgs = 0;
	int num_ret_alloc = 0;
	long *ret_msgs = NULL;

	wordbreaker(search_string, &num_tokens, &tokens);
	if (num_tokens > 0) {
		for (i=0; i<num_tokens; ++i) {

			/* search for tokens[i] */
			cdb_bucket = cdb_fetch(CDB_FULLTEXT, &tokens[i], sizeof(int));
			if (cdb_bucket != NULL) {
				num_msgs = cdb_bucket->len / sizeof(long);
				msgs = (long *)cdb_bucket->ptr;

				num_all_msgs += num_msgs;
				if (num_all_msgs > 0) {
					all_msgs = realloc(all_msgs, num_all_msgs*sizeof(long) );
					memcpy(&all_msgs[num_all_msgs - num_msgs], msgs,
						num_msgs*sizeof(long) );
				}

				cdb_free(cdb_bucket);
			}

		}
		free(tokens);
		qsort(all_msgs, num_all_msgs, sizeof(long), longcmp);

		/*
		 * At this point, if a message appears num_tokens times in the
		 * list, then it contains all of the search tokens.
		 */
		if (num_all_msgs >= num_tokens) for (j=0; j<(num_all_msgs-num_tokens+1); ++j) {
			if (all_msgs[j] == all_msgs[j+num_tokens-1]) {

				++num_ret_msgs;
				if (num_ret_msgs > num_ret_alloc) {
					num_ret_alloc += 64;
					ret_msgs = realloc(ret_msgs, (num_ret_alloc*sizeof(long)) );
				}
				ret_msgs[num_ret_msgs - 1] = all_msgs[j];

			}
		}

		free(all_msgs);
	}
	
	*fts_num_msgs = num_ret_msgs;
	*fts_msgs = ret_msgs;
}


/*
 * Tentative form of a search command
 */
void cmd_srch(char *argbuf) {
	int num_msgs = 0;
	long *msgs = NULL;
	int i;
	char search_string[256];

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(search_string, argbuf, 0, '|', sizeof search_string);
	ft_search(&num_msgs, &msgs, search_string);

	cprintf("%d %d msgs match all search words:\n",
		LISTING_FOLLOWS, num_msgs);
	if (num_msgs > 0) {
		for (i=0; i<num_msgs; ++i) {
			cprintf("%ld\n", msgs[i]);
		}
	}
	if (msgs != NULL) free(msgs);
	cprintf("000\n");
}


/*****************************************************************************/

char *serv_fulltext_init(void)
{
	CtdlRegisterSessionHook(do_fulltext_indexing, EVT_TIMER);
        CtdlRegisterProtoHook(cmd_srch, "SRCH", "Full text search");
	return "$Id$";
}
