/*
 * $Id$
 *
 * Implements the SEARCH command in IMAP.
 * This command is way too convoluted.  Marc Crispin is a fscking idiot.
 *
 * NOTE: this is a partial implementation.  It is NOT FINISHED.
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



/*
 * imap_do_search() calls imap_do_search_msg() to search an individual
 * message after it has been fetched from the disk.  This function returns
 * nonzero if there is a match.
 */
int imap_do_search_msg(int seq, struct CtdlMessage *msg,
			int num_items, char **itemlist, int is_uid) {

	int match = 0;
	int is_not = 0;
	int is_or = 0;
	int pos = 0;
	int i;
	char *fieldptr;

	if (num_items == 0) return(0);

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
		fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Bcc");
		if (fieldptr != NULL) {
			if (bmstrstr(fieldptr, itemlist[pos+1], strncasecmp)) {
				match = 1;
			}
			free(fieldptr);
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "BEFORE")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) < 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "BODY")) {
		if (bmstrstr(msg->cm_fields['M'], itemlist[pos+1], strncasecmp)) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "CC")) {
		fieldptr = rfc822_fetch_field(msg->cm_fields['M'], "Cc");
		if (fieldptr != NULL) {
			if (bmstrstr(fieldptr, itemlist[pos+1], strncasecmp)) {
				match = 1;
			}
			free(fieldptr);
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
		if (bmstrstr(msg->cm_fields['A'], itemlist[pos+1], strncasecmp)) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "HEADER")) {
		/* FIXME */
		pos += 3;	/* Yes, three */
	}

	else if (!strcasecmp(itemlist[pos], "KEYWORD")) {
		/* FIXME */
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "LARGER")) {
		if (strlen(msg->cm_fields['M']) > atoi(itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "NEW")) {
		/* FIXME */
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "OLD")) {
		/* FIXME */
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "ON")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) == 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "RECENT")) {
		/* FIXME */
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "SEEN")) {
		if (IMAP->flags[seq-1] & IMAP_SEEN) {
			match = 1;
		}
		++pos;
	}

	else if (!strcasecmp(itemlist[pos], "SENTBEFORE")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) < 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SENTON")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) == 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SENTSINCE")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) >= 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SINCE")) {
		if (msg->cm_fields['T'] != NULL) {
			if (imap_datecmp(itemlist[pos+1],
					atol(msg->cm_fields['T'])) >= 0) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SMALLER")) {
		if (strlen(msg->cm_fields['M']) < atoi(itemlist[pos+1])) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "SUBJECT")) {
		if (bmstrstr(msg->cm_fields['U'], itemlist[pos+1], strncasecmp)) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "TEXT")) {
		for (i='A'; i<='Z'; ++i) {
			if (bmstrstr(msg->cm_fields[i], itemlist[pos+1], strncasecmp)) {
				match = 1;
			}
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "TO")) {
		if (bmstrstr(msg->cm_fields['R'], itemlist[pos+1], strncasecmp)) {
			match = 1;
		}
		pos += 2;
	}

	else if (!strcasecmp(itemlist[pos], "UID")) {
		if (is_msg_in_mset(itemlist[pos+1], IMAP->msgids[seq-1])) {
			match = 1;
		}
		pos += 2;
	}

	/* Now here come the 'UN' criteria.  Why oh why do we have to
	 * implement *both* the 'UN' criteria *and* the 'NOT' keyword?  Why
	 * can't there be *one* way to do things?  Answer: because Mark
	 * Crispin is an idiot.
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

	return(match);
}


/*
 * imap_search() calls imap_do_search() to do its actual work, once it's
 * validated and boiled down the request a bit.
 */
void imap_do_search(int num_items, char **itemlist, int is_uid) {
	int i;
	struct CtdlMessage *msg;

	cprintf("* SEARCH ");
	if (IMAP->num_msgs > 0)
	 for (i = 0; i < IMAP->num_msgs; ++i)
	  if (IMAP->flags[i] && IMAP_SELECTED) {
		msg = CtdlFetchMessage(IMAP->msgids[i], 1);
		if (msg != NULL) {
			if (imap_do_search_msg(i+1, msg, num_items,
			   itemlist, is_uid)) {
				if (is_uid) {
					cprintf("%ld ", IMAP->msgids[i]);
				}
				else {
					cprintf("%d ", i+1);
				}
			}
			CtdlFreeMessage(msg);
		}
		else {
			lprintf(CTDL_ERR, "SEARCH internal error\n");
		}
	}
	cprintf("\r\n");
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

	for (i=1; i<num_parms; ++i) {
		if (imap_is_message_set(parms[i])) {
			imap_pick_range(parms[i], 1);
		}
	}

	imap_do_search(num_parms-3, &parms[3], 1);
	cprintf("%s OK UID SEARCH completed\r\n", parms[0]);
}


