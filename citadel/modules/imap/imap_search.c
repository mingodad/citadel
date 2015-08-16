/*
 * Implements IMAP's gratuitously complex SEARCH command.
 *
 * Copyright (c) 2001-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "ctdl_module.h"


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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_search.h"
#include "genstamp.h"


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
			int num_items, ConstStr *itemlist, int is_uid) {

	citimap *Imap = IMAP;
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
	if (!strcasecmp(itemlist[0].Key, "NOT")) {
		is_not = 1;
		pos = 1;
	}

	/* Check for the dreaded OR criterion. */
	if (!strcasecmp(itemlist[0].Key, "OR")) {
		is_or = 1;
		pos = 1;
	}

	/* Now look for criteria. */
	if (!strcasecmp(itemlist[pos].Key, "ALL")) {
		match = 1;
		++pos;
	}
	
	else if (!strcasecmp(itemlist[pos].Key, "ANSWERED")) {
		if (Imap->flags[seq-1] & IMAP_ANSWERED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "BCC")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Bcc");
			if (fieldptr != NULL) {
				if (bmstrcasestr(fieldptr, itemlist[pos+1].Key)) {
					match = 1;
				}
				free(fieldptr);
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "BEFORE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) < 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "BODY")) {

		/* If fulltext indexing is active, on this server,
		 *  all messages have already been qualified.
		 */
		if (CtdlGetConfigInt("c_enable_fulltext")) {
			match = 1;
		}

		/* Otherwise, we have to do a slow search. */
		else {
			if (msg == NULL) {
				msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
				need_to_free_msg = 1;
			}
			if (msg != NULL) {
				if (bmstrcasestr(msg->cm_fields[eMesageText], itemlist[pos+1].Key)) {
					match = 1;
				}
			}
		}

		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "CC")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			fieldptr = msg->cm_fields[eCarbonCopY];
			if (fieldptr != NULL) {
				if (bmstrcasestr(fieldptr, itemlist[pos+1].Key)) {
					match = 1;
				}
			}
			else {
				fieldptr = rfc822_fetch_field(msg->cm_fields[eMesageText], "Cc");
				if (fieldptr != NULL) {
					if (bmstrcasestr(fieldptr, itemlist[pos+1].Key)) {
						match = 1;
					}
					free(fieldptr);
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "DELETED")) {
		if (Imap->flags[seq-1] & IMAP_DELETED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "DRAFT")) {
		if (Imap->flags[seq-1] & IMAP_DRAFT) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "FLAGGED")) {
		if (Imap->flags[seq-1] & IMAP_FLAGGED) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "FROM")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (bmstrcasestr(msg->cm_fields[eAuthor], itemlist[pos+1].Key)) {
				match = 1;
			}
			if (bmstrcasestr(msg->cm_fields[erFc822Addr], itemlist[pos+1].Key)) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "HEADER")) {

		/* We've got to do a slow search for this because the client
		 * might be asking for an RFC822 header field that has not been
		 * converted into a Citadel header field.  That requires
		 * examining the message body.
		 */
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}

		if (msg != NULL) {
	
			CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
			CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_FAST, 0, 1, 0);
	
			fieldptr = rfc822_fetch_field(ChrPtr(CC->redirect_buffer), itemlist[pos+1].Key);
			if (fieldptr != NULL) {
				if (bmstrcasestr(fieldptr, itemlist[pos+2].Key)) {
					match = 1;
				}
				free(fieldptr);
			}
	
			FreeStrBuf(&CC->redirect_buffer);
		}

		pos += 3;	/* Yes, three */
	}

	else if (!strcasecmp(itemlist[pos].Key, "KEYWORD")) {
		/* not implemented */
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "LARGER")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (msg->cm_lengths[eMesageText] > atoi(itemlist[pos+1].Key)) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "NEW")) {
		if ( (Imap->flags[seq-1] & IMAP_RECENT) && (!(Imap->flags[seq-1] & IMAP_SEEN))) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "OLD")) {
		if (!(Imap->flags[seq-1] & IMAP_RECENT)) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "ON")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) == 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "RECENT")) {
		if (Imap->flags[seq-1] & IMAP_RECENT) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SEEN")) {
		if (Imap->flags[seq-1] & IMAP_SEEN) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SENTBEFORE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) < 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SENTON")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) == 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SENTSINCE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) >= 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SINCE")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (!CM_IsEmpty(msg, eTimestamp)) {
				if (imap_datecmp(itemlist[pos+1].Key,
						atol(msg->cm_fields[eTimestamp])) >= 0) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SMALLER")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (msg->cm_lengths[eMesageText] < atoi(itemlist[pos+1].Key)) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "SUBJECT")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (bmstrcasestr(msg->cm_fields[eMsgSubject], itemlist[pos+1].Key)) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "TEXT")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			for (i='A'; i<='Z'; ++i) {
				if (bmstrcasestr(msg->cm_fields[i], itemlist[pos+1].Key)) {
					match = 1;
				}
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "TO")) {
		if (msg == NULL) {
			msg = CtdlFetchMessage(Imap->msgids[seq-1], 1, 1);
			need_to_free_msg = 1;
		}
		if (msg != NULL) {
			if (bmstrcasestr(msg->cm_fields[eRecipient], itemlist[pos+1].Key)) {
				match = 1;
			}
		}
		pos += 2;
	}

	/* FIXME this is b0rken.  fix it. */
	else if (imap_is_message_set(itemlist[pos].Key)) {
		if (is_msg_in_sequence_set(itemlist[pos].Key, seq)) {
			match = 1;
		}
		pos += 1;
	}

	/* FIXME this is b0rken.  fix it. */
	else if (!strcasecmp(itemlist[pos].Key, "UID")) {
		if (is_msg_in_sequence_set(itemlist[pos+1].Key, Imap->msgids[seq-1])) {
			match = 1;
		}
		pos += 2;
	}

	/* Now here come the 'UN' criteria.  Why oh why do we have to
	 * implement *both* the 'UN' criteria *and* the 'NOT' keyword?  Why
	 * can't there be *one* way to do things?  More gratuitous complexity.
	 */

	else if (!strcasecmp(itemlist[pos].Key, "UNANSWERED")) {
		if ((Imap->flags[seq-1] & IMAP_ANSWERED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "UNDELETED")) {
		if ((Imap->flags[seq-1] & IMAP_DELETED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "UNDRAFT")) {
		if ((Imap->flags[seq-1] & IMAP_DRAFT) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "UNFLAGGED")) {
		if ((Imap->flags[seq-1] & IMAP_FLAGGED) == 0) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos].Key, "UNKEYWORD")) {
		/* FIXME */
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos].Key, "UNSEEN")) {
		if ((Imap->flags[seq-1] & IMAP_SEEN) == 0) {
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
		CM_Free(msg);
	}
	return(match);
}


/*
 * imap_search() calls imap_do_search() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_search(int num_items, ConstStr *itemlist, int is_uid) {
	citimap *Imap = IMAP;
	int i, j, k;
	int fts_num_msgs = 0;
	long *fts_msgs = NULL;
	int is_in_list = 0;
	int num_results = 0;

	/* Strip parentheses.  We realize that this method will not work
	 * in all cases, but it seems to work with all currently available
	 * client software.  Revisit later...
	 */
	for (i=0; i<num_items; ++i) {
		if (itemlist[i].Key[0] == '(') {
			
			TokenCutLeft(&Imap->Cmd, 
				     &itemlist[i], 
				     1);
		}
		if (itemlist[i].Key[itemlist[i].len-1] == ')') {
			TokenCutRight(&Imap->Cmd, 
				      &itemlist[i], 
				      1);
		}
	}

	/* If there is a BODY search criterion in the query, use our full
	 * text index to disqualify messages that don't have any chance of
	 * matching.  (Only do this if the index is enabled!!)
	 */
	if (CtdlGetConfigInt("c_enable_fulltext")) for (i=0; i<(num_items-1); ++i) {
		if (!strcasecmp(itemlist[i].Key, "BODY")) {
			CtdlModuleDoSearch(&fts_num_msgs, &fts_msgs, itemlist[i+1].Key, "fulltext");
			if (fts_num_msgs > 0) {
				for (j=0; j < Imap->num_msgs; ++j) {
					if (Imap->flags[j] & IMAP_SELECTED) {
						is_in_list = 0;
						for (k=0; k<fts_num_msgs; ++k) {
							if (Imap->msgids[j] == fts_msgs[k]) {
								++is_in_list;
							}
						}
					}
					if (!is_in_list) {
						Imap->flags[j] = Imap->flags[j] & ~IMAP_SELECTED;
					}
				}
			}
			else {		/* no hits on the index; disqualify every message */
				for (j=0; j < Imap->num_msgs; ++j) {
					Imap->flags[j] = Imap->flags[j] & ~IMAP_SELECTED;
				}
			}
			if (fts_msgs) {
				free(fts_msgs);
			}
		}
	}

	/* Now go through the messages and apply all search criteria. */
	buffer_output();
	IAPuts("* SEARCH ");
	if (Imap->num_msgs > 0)
	 for (i = 0; i < Imap->num_msgs; ++i)
	  if (Imap->flags[i] & IMAP_SELECTED) {
		if (imap_do_search_msg(i+1, NULL, num_items, itemlist, is_uid)) {
			if (num_results != 0) {
				IAPuts(" ");
			}
			if (is_uid) {
				IAPrintf("%ld", Imap->msgids[i]);
			}
			else {
				IAPrintf("%d", i+1);
			}
			++num_results;
		}
	}
	IAPuts("\r\n");
	unbuffer_output();
}


/*
 * This function is called by the main command loop.
 */
void imap_search(int num_parms, ConstStr *Params) {
	int i;

	if (num_parms < 3) {
		IReply("BAD invalid parameters");
		return;
	}

	for (i = 0; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] |= IMAP_SELECTED;
	}

	imap_do_search(num_parms-2, &Params[2], 0);
	IReply("OK SEARCH completed");
}

/*
 * This function is called by the main command loop.
 */
void imap_uidsearch(int num_parms, ConstStr *Params) {
	int i;

	if (num_parms < 4) {
		IReply("BAD invalid parameters");
		return;
	}

	for (i = 0; i < IMAP->num_msgs; ++i) {
		IMAP->flags[i] |= IMAP_SELECTED;
	}

	imap_do_search(num_parms-3, &Params[3], 1);
	IReply("OK UID SEARCH completed");
}


