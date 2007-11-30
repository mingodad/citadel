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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "msgbase.h"
#include "control.h"
#include "room_ops.h"
#include "serv_fulltext.h"
#include "ft_wordbreaker.h"


#include "ctdl_module.h"



long ft_newhighest = 0L;
long *ft_newmsgs = NULL;
int ft_num_msgs = 0;
int ft_num_alloc = 0;


int ftc_num_msgs[65536];
long *ftc_msgs[65536];


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
 * Flush our index cache out to disk.
 */
void ft_flush_cache(void) {
	int i;
	time_t last_update = 0;

	for (i=0; i<65536; ++i) {
		if ((time(NULL) - last_update) >= 10) {
			lprintf(CTDL_INFO,
				"Flushing index cache to disk (%d%% complete)\n",
				(i * 100 / 65536)
			);
			last_update = time(NULL);
		}
		if (ftc_msgs[i] != NULL) {
			cdb_store(CDB_FULLTEXT, &i, sizeof(int), ftc_msgs[i],
				(ftc_num_msgs[i] * sizeof(long)));
			ftc_num_msgs[i] = 0;
			free(ftc_msgs[i]);
			ftc_msgs[i] = NULL;
		}
	}
	lprintf(CTDL_INFO, "Flushed index cache to disk (100%% complete)\n");
}


/*
 * Index or de-index a message.  (op == 1 to index, 0 to de-index)
 */
void ft_index_message(long msgnum, int op) {
	int num_tokens = 0;
	int *tokens = NULL;
	int i, j;
	struct cdbdata *cdb_bucket;
	char *msgtext;
	int tok;

	lprintf(CTDL_DEBUG, "ft_index_message() %s msg %ld\n",
		(op ? "adding" : "removing") , msgnum
	);

	/* Output the message as text before indexing it, so we don't end up
	 * indexing a bunch of encoded base64, etc.
	 */
	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputMsg(msgnum, MT_CITADEL, HEADERS_ALL, 0, 1, NULL);
	msgtext = CC->redirect_buffer;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;
	lprintf(CTDL_DEBUG, "Wordbreaking message %ld...\n", msgnum);
	wordbreaker(msgtext, &num_tokens, &tokens);
	free(msgtext);

	lprintf(CTDL_DEBUG, "Indexing message %ld [%d tokens]\n", msgnum, num_tokens);
	if (num_tokens > 0) {
		for (i=0; i<num_tokens; ++i) {

			/* Add the message to the relevant token bucket */

			/* search for tokens[i] */
			tok = tokens[i];

			if ( (tok >= 0) && (tok <= 65535) ) {
				/* fetch the bucket, Liza */
				if (ftc_msgs[tok] == NULL) {
					cdb_bucket = cdb_fetch(CDB_FULLTEXT, &tok, sizeof(int));
					if (cdb_bucket != NULL) {
						ftc_num_msgs[tok] = cdb_bucket->len / sizeof(long);
						ftc_msgs[tok] = (long *)cdb_bucket->ptr;
						cdb_bucket->ptr = NULL;
						cdb_free(cdb_bucket);
					}
					else {
						ftc_num_msgs[tok] = 0;
						ftc_msgs[tok] = malloc(sizeof(long));
					}
				}
	
	
				if (op == 1) {	/* add to index */
					++ftc_num_msgs[tok];
					ftc_msgs[tok] = realloc(ftc_msgs[tok],
								ftc_num_msgs[tok]*sizeof(long));
					ftc_msgs[tok][ftc_num_msgs[tok] - 1] = msgnum;
				}
	
				if (op == 0) {	/* remove from index */
					if (ftc_num_msgs[tok] >= 1) {
						for (j=0; j<ftc_num_msgs[tok]; ++j) {
							if (ftc_msgs[tok][j] == msgnum) {
								memmove(&ftc_msgs[tok][j], &ftc_msgs[tok][j+1], ((ftc_num_msgs[tok] - j - 1)*sizeof(long)));
								--ftc_num_msgs[tok];
								--j;
							}
						}
					}
				}
			}
			else {
				lprintf(CTDL_ALERT, "Invalid token %d !!\n", tok);
			}
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
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, ft_index_msg, NULL);
}


/*
 * Begin the fulltext indexing process.  (Called as an EVT_TIMER event)
 */
void do_fulltext_indexing(void) {
	int i;
	static time_t last_index = 0L;
	static time_t last_progress = 0L;
	time_t run_time = 0L;
	time_t end_time = 0L;
	
	CT_PUSH();
	
	/*
	 * Don't do this if the site doesn't have it enabled.
	 */
	if (!config.c_enable_fulltext) {
		return;
	}

	/*
	 * Make sure we don't run the indexer too frequently.
	 * FIXME move the setting into config
	 */
/*
 * The thread sleeps for 300 seconds so we don't need this here any more
 
	if ( (time(NULL) - last_index) < 300L) {
		return;
	}
*/

	/*
	 * Check to see whether the fulltext index is up to date; if there
	 * are no messages to index, don't waste any more time trying.
	 */
	if (CitControl.MMfulltext >= CitControl.MMhighest) {
		return;		/* nothing to do! */
	}
	
	run_time = time(NULL);
	lprintf(CTDL_DEBUG, "do_fulltext_indexing() started (%ld)\n", run_time);
	
	/*
	 * If we've switched wordbreaker modules, burn the index and start
	 * over.
	 */
	begin_critical_section(S_CONTROL);
	if (CitControl.fulltext_wordbreaker != FT_WORDBREAKER_ID) {
		lprintf(CTDL_DEBUG, "wb ver on disk = %d, code ver = %d\n",
			CitControl.fulltext_wordbreaker, FT_WORDBREAKER_ID);
		lprintf(CTDL_INFO, "(re)initializing full text index\n");
		cdb_trunc(CDB_FULLTEXT);
		CitControl.MMfulltext = 0L;
		put_control();
	}
	end_critical_section(S_CONTROL);

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
				--i;
			}
		}

		/* Here it is ... do each message! */
		for (i=0; i<ft_num_msgs; ++i) {
			if (time(NULL) != last_progress) {
				lprintf(CTDL_DEBUG,
					"Indexed %d of %d messages (%d%%)\n",
						i, ft_num_msgs,
						((i*100) / ft_num_msgs)
				);
				last_progress = time(NULL);
			}
			ft_index_message(ft_newmsgs[i], 1);

			/* Check to see if we need to quit early */
			if (CtdlThreadCheckStop(CT)) {
				lprintf(CTDL_DEBUG, "Indexer quitting early\n");
				ft_newhighest = ft_newmsgs[i];
				break;
			}

			/* Check to see if we have to maybe flush to disk */
			if (i >= FT_MAX_CACHE) {
				lprintf(CTDL_DEBUG, "Time to flush.\n");
				ft_newhighest = ft_newmsgs[i];
				break;
			}

		}

		free(ft_newmsgs);
		ft_num_msgs = 0;
		ft_num_alloc = 0;
		ft_newmsgs = NULL;
	}
	end_time = time(NULL);
	lprintf(CTDL_DEBUG, "do_fulltext_indexing() duration (%ld)\n", end_time - run_time);

	/* Save our place so we don't have to do this again */
	ft_flush_cache();
	begin_critical_section(S_CONTROL);
	CitControl.MMfulltext = ft_newhighest;
	CitControl.fulltext_wordbreaker = FT_WORDBREAKER_ID;
	put_control();
	end_critical_section(S_CONTROL);
	last_index = time(NULL);

	lprintf(CTDL_DEBUG, "do_fulltext_indexing() finished\n");
	return;
}

/*
 * Main loop for the indexer thread.
 */
void *indexer_thread(void *arg) {
	struct CitContext indexerCC;

	CT_PUSH();
	
	lprintf(CTDL_DEBUG, "indexer_thread() initializing\n");

	memset(&indexerCC, 0, sizeof(struct CitContext));
	indexerCC.internal_pgm = 1;
	indexerCC.cs_pid = 0;
	pthread_setspecific(MyConKey, (void *)&indexerCC );

	cdb_allocate_tsd();

	while (!CtdlThreadCheckStop(CT)) {
		do_fulltext_indexing();
		CtdlThreadSleep(300);
	}

	lprintf(CTDL_DEBUG, "indexer_thread() exiting\n");
	return NULL;
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
	int num_all_msgs = 0;
	long *all_msgs = NULL;
	int num_ret_msgs = 0;
	int num_ret_alloc = 0;
	long *ret_msgs = NULL;
	int tok;

	wordbreaker(search_string, &num_tokens, &tokens);
	if (num_tokens > 0) {
		for (i=0; i<num_tokens; ++i) {

			/* search for tokens[i] */
			tok = tokens[i];

			/* fetch the bucket, Liza */
			if (ftc_msgs[tok] == NULL) {
				cdb_bucket = cdb_fetch(CDB_FULLTEXT, &tok, sizeof(int));
				if (cdb_bucket != NULL) {
					ftc_num_msgs[tok] = cdb_bucket->len / sizeof(long);
					ftc_msgs[tok] = (long *)cdb_bucket->ptr;
					cdb_bucket->ptr = NULL;
					cdb_free(cdb_bucket);
				}
				else {
					ftc_num_msgs[tok] = 0;
					ftc_msgs[tok] = malloc(sizeof(long));
				}
			}

			num_all_msgs += ftc_num_msgs[tok];
			if (num_all_msgs > 0) {
				all_msgs = realloc(all_msgs, num_all_msgs*sizeof(long) );
				memcpy(&all_msgs[num_all_msgs-ftc_num_msgs[tok]],
					ftc_msgs[tok], ftc_num_msgs[tok]*sizeof(long) );
			}

		}
		free(tokens);
		qsort(all_msgs, num_all_msgs, sizeof(long), longcmp);

		/*
		 * At this point, if a message appears num_tokens times in the
		 * list, then it contains all of the search tokens.
		 */
		if (num_all_msgs >= num_tokens)
		   for (j=0; j<(num_all_msgs-num_tokens+1); ++j) {
			if (all_msgs[j] == all_msgs[j+num_tokens-1]) {

				++num_ret_msgs;
				if (num_ret_msgs > num_ret_alloc) {
					num_ret_alloc += 64;
					ret_msgs = realloc(ret_msgs,
						(num_ret_alloc*sizeof(long)) );
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
 * This search command is for diagnostic purposes and may be removed or replaced.
 */
void cmd_srch(char *argbuf) {
	int num_msgs = 0;
	long *msgs = NULL;
	int i;
	char search_string[256];

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (!config.c_enable_fulltext) {
		cprintf("%d Full text index is not enabled on this server.\n",
			ERROR + CMD_NOT_SUPPORTED);
		return;
	}

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

/*
 * Zero out our index cache.
 */
void initialize_ft_cache(void) {
	memset(ftc_num_msgs, 0, (65536 * sizeof(int)));
	memset(ftc_msgs, 0, (65536 * sizeof(long *)));
}


void ft_delete_remove(char *room, long msgnum)
{
	if (room) return;
	
	/* Remove from fulltext index */
	if (config.c_enable_fulltext) {
		ft_index_message(msgnum, 0);
	}
}

/*****************************************************************************/

CTDL_MODULE_INIT(fulltext)
{
	if (!threading)
	{
		initialize_ft_cache();
		CtdlRegisterProtoHook(cmd_srch, "SRCH", "Full text search");
		CtdlRegisterDeleteHook(ft_delete_remove);
		CtdlRegisterSearchFuncHook(ft_search, "fulltext");
	}
	else
	{
		CtdlThreadCreate("indexer", CTDLTHREAD_BIGSTACK, indexer_thread, NULL);
	}
	/* return our Subversion id for the Log */
	return "$Id$";
}
