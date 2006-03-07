/*
 * $Id$
 *
 * Implements IMAP's gratuitously complex SEARCH command.
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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_search.h"
#include "genstamp.h"
#include "serv_fulltext.h"


/*
 * imap_do_search() calls imap_do_search_msg() to search an individual
 * message after it has been fetched from the disk.  This function returns
 * nonzero if there is a match.
 *
 * supplied_msg MAY be used to pass a pointer to the message in memory,
 * if for some reason it's already been loaded.  If not, the message will
 * be loaded only if one or more search criteria require it.
 */
int imap_do_search_msg(int seq, struct CtdlMessage *supplied_msg,
			int num_items, char **itemlist, int is_uid) {

	int match = 0;
	int is_not = 0;
	int is_or = 0;
	int pos = 0;
	int i;
	char *fieldptr;
	struct CtdlMessage *msg = NULL;
	int need_to_free_msg = 0;

	if (num_items == 0) {
		return(0);
	}
	msg = supplied_msg;

	/* Initially we start at the beginning. */
	pos = 0;

	/* Check for the dreaded NOT criterion. */
	if (!strcasecmp(itemlist[0], "NOT")) {
		is_not = 1;
		pos = 1;
	}

	/* Check for the dreaded OR criterion. */
	if (!strcasecmp(itemlist[0], "OR")) {
		is_or = 1;
		pos = 1;
	}

	/* Now look for criteria. */
	if (!strcasecmp(itemlist[pos], "ALL")) {
		match = 1;
		++pos;
	}
	
	else if (!strcasecmp(itemlist[pos], "ANSWERED")) {
		if (IMAP->flags[seq-1] & IMAP_ANSWERED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "BCC")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Bcc");
		if (fieldptr != NULL) {
			if (bmstrcasestr(fieldptr, itemlist[pos+1])) {
				match = 1;
			}
			free(fieldptr);
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "BEFORE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) < 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "BODY")) {

		/* If fulltext indexing is active, on this server,
		 *  all messages have already been qualified.
		 */
		if (config.c_enable_fulltext) {
			match = 1;
		}

		/* Otherwise, we have to do a slow search. */
		else {
			if (msg == NULL) {
				msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
				need_to_free_msg = 1;
			}
			if (bmstrcasestr(msg->cm_fields['M'], itemlist[pos+1])) {
				match = 1;
			}
		}

		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "CC")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		fieldptr = msg->cm_fields['Y'];
		if (fieldptr != NULL) {
			if (bmstrcasestr(fieldptr, itemlist[pos+1])) {
				match = 1;
			}
		}
		else {
			fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Cc");
			if (fieldptr != NULL) {
				if (bmstrcasestr(fieldptr, itemlist[pos+1])) {
					match = 1;
				}
				free(fieldptr);
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "DELETED")) {
		if (IMAP->flags[seq-1] & IMAP_DELETED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "DRAFT")) {
		if (IMAP->flags[seq-1] & IMAP_DRAFT) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "FLAGGED")) {
		if (IMAP->flags[seq-1] & IMAP_FLAGGED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "FROM")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (bmstrcasestr(msg->cm_fields['A'], itemlist[pos+1])) {
			match = 1;
		}
		if (bmstrcasestr(msg->cm_fields['F'], itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "HEADER")) {

		/* We've got to do a slow search for this because the client
		 * might be asking for an RFC822 header field that has not been
		 * converted into a Citadel header field.  That requires
		 * examining the message body.
		 */
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}

		CC->redirect_buffer = malloc(SIZ);
		CC->redirect_len = 0;
		CC->redirect_alloc = SIZ;
		CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ONLY, 0, 1);

		fieldptr = rfc822_fetch_field(CC->redirect_buffer, itemlist[pos+1]);
		if (fieldptr != NULL) {
			if (bmstrcasestr(fieldptr, itemlist[pos+2])) {
				match = 1;
			}
			free(fieldptr);
		}

		free(CC->redirect_buffer);
		CC->redirect_buffer = NULL;
		CC->redirect_len = 0;
		CC->redirect_alloc = 0;

		pos += 3;	/* Yes, three */
	}

	else if (!strcasecmp(itemlist[pos], "KEYWORD")) {
		/* FIXME */
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "LARGER")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (strlen(msg->cm_fields['M']) > atoi(itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "NEW")) {
		if ( (IMAP->flags[seq-1] & IMAP_RECENT) && (!(IMAP->flags[seq-1] & IMAP_SEEN))) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "OLD")) {
		if (!(IMAP->flags[seq-1] & IMAP_RECENT)) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "ON")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) == 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "RECENT")) {
		if (IMAP->flags[seq-1] & IMAP_RECENT) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "SEEN")) {
		if (IMAP->flags[seq-1] & IMAP_SEEN) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "SENTBEFORE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) < 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SENTON")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) == 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SENTSINCE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) >= 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SINCE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) >= 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SMALLER")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (strlen(msg->cm_fields['M']) < atoi(itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SUBJECT")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (bmstrcasestr(msg->cm_fields['U'], itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "TEXT")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		for (i='A'; i<='Z'; ++i) {
			if (bmstrcasestr(msg->cm_fields[i], itemlist[pos+1])) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "TO")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(IMAP->msgids[seq-1], 1);
			need_to_free_msg = 1;
		}
		if (bmstrcasestr(msg->cm_fields['R'], itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "UID")) {
		if (is_msg_in_sequence_set(itemlist[pos+1], IMAP->msgids[seq-1])) {
			match = 1;
		}
		pos += 2;
	}

	/* Now here come the 'UN' criteria.  Why oh why do we have to
	 * implement *both* the 'UN' criteria *and* the 'NOT' keyword?  Why
	 * can't there be *one* way to do things?  Answer: the design of
	 * IMAP suffers from gratuitous complexity.
	 */

	else if (!strcasecmp(itemlist[pos], "UNANSWERED")) {
		if ((IMAP->flags[seq-1] & IMAP_ANSWERED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "UNDELETED")) {
		if ((IMAP->flags[seq-1] & IMAP_DELETED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "UNDRAFT")) {
		if ((IMAP->flags[seq-1] & IMAP_DRAFT) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "UNFLAGGED")) {
		if ((IMAP->flags[seq-1] & IMAP_FLAGGED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "UNKEYWORD")) {
		/* FIXME */
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "UNSEEN")) {
		if ((IMAP->flags[seq-1] & IMAP_SEEN) == 0) {
			match = 1;
		}
		++pos;
	}

	/* Remember to negate if we were told to */
	if (is_not) {
		match = !match;
	}

	/* Keep going if there are more criteria! */
	if (pos < num_items) {

		if (is_or) {
			match = (match || imap_do_search_msg(seq, msg,
				num_items - pos, &itemlist[pos], is_uid));
		}
		else {
			match = (match && imap_do_search_msg(seq, msg,
				num_items - pos, &itemlist[pos], is_uid));
		}

	}

	if (need_to_free_msg) {
		CtdlFreeMessage(msg);
	}
	return(match);
}


/*
 * imap_search() calls imap_do_search() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_search(int num_items, char **itemlist, int is_uid) {
	int i, j, k;
	int fts_num_msgs = 0;
	long *fts_msgs = NULL;
	int is_in_list = 0;
	int num_results = 0;

	/* If there is a BODY search criterion in the query, use our full
	 * text index to disqualify messages that don't have any chance of
	 * matching.  (Only do this if the index is enabled!!)
	 */
	if (config.c_enable_fulltext) for (i=0; i<(num_items-1); ++i) {
		if (!strcasecmp(itemlist[i], "BODY")) {
			ft_search(&fts_num_msgs, &fts_msgs, itemlist[i+1]);
			if (fts_num_msgs > 0) {
				for (j=0; j < IMAP->num_msgs; ++j) {
					if (IMAP->flags[j] & IMAP_SELECTED) {
						is_in_list = 0;
						for (k=0; k<fts_num_msgs; ++k) {
							if (IMAP->msgids[j] == fts_msgs[k]) {
								++is_in_list;
							}
						}
					}
					if (!is_in_list) {
						IMAP->flags[j] = IMAP->flags[j] & ~IMAP_SELECTED;
					}
				}
			}
			else {		/* no hits on the index; disqualify every message */
				for (j=0; j < IMAP->num_msgs; ++j) {
					IMAP->flags[j] = IMAP->flags[j] & ~IMAP_SELECTED;
				}
			}
			if (fts_msgs) {
				free(fts_msgs);
			}
		}
	}

	/* Now go through the messages and apply all search criteria. */
	buffer_output();
	cprintf("* SEARCH ");
	if (IMAP->num_msgs > 0)
	 for (i = 0; i < IMAP->num_msgs; ++i)
	  if (IMAP->flags[i] & IMAP_SELECTED) {
		if (imap_do_search_msg(i+1, NULL, num_items, itemlist, is_uid)) {
			if (num_results != 0) {
				cprintf(" ");
			}
			if (is_uid) {
				cprintf("%ld", IMAP->msgids[i]);
			}
			else {
				cprintf("%d", i+1);
			}
			++num_results;
		}
	}
	cprintf("\r\n");
	unbuffer_output();
}


/*
 * This function is called by the main command loop.
 */
void imap_search(int num_parms, char *parms[]) {
	int i;

	if (num_parms < 3) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	for (i = 0; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] |= IMAP_SELECTED;
	}

	for (i=1; i<num_parms; ++i) {
		if (imap_is_message_set(parms[i])) {
			imap_pick_range(parms[i], 0);
		}
	}

	imap_do_search(num_parms-2, &parms[2], 0);
	cprintf("%s OK SEARCH completed\r\n", parms[0]);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidsearch(int num_parms, char *parms[]) {
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	for (i = 0; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] |= IMAP_SELECTED;
	}

	for (i=1; i<num_parms; ++i) {
		if (imap_is_message_set(parms[i])) {
			imap_pick_range(parms[i], 1);
		}
	}

	imap_do_search(num_parms-3, &parms[3], 1);
	cprintf("%s OK UID SEARCH completed\r\n", parms[0]);
}


