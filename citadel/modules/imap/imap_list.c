/*
 * $Id$
 *
 * Implements the LIST and LSUB commands.
 *
 * Copyright (C) 2000-2007 by Art Cancro and others.
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
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
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
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_misc.h"
#include "imap_list.h"


/*
 * Used by LIST and LSUB to show the floors in the listing
 */
void imap_list_floors(char *verb, int num_patterns, char **patterns)
{
	int i;
	struct floor *fl;
	int j = 0;
	int match = 0;

	for (i = 0; i < MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			match = 0;
			for (j=0; j<num_patterns; ++j) {
				if (imap_mailbox_matches_pattern (patterns[j], fl->f_name)) {
					match = 1;
				}
			}
			if (match) {
				cprintf("* %s (\\NoSelect \\HasChildren) \"/\" ", verb);
				imap_strout(fl->f_name);
				cprintf("\r\n");
			}
		}
	}
}


/*
 * Back end for imap_list()
 *
 * Implementation note: IMAP "subscribed folder" is equivalent to Citadel "known room"
 *
 * The "user data" field is actually an array of pointers; see below for the breakdown
 *
 */
void imap_listroom(struct ctdlroom *qrbuf, void *data)
{
	char buf[SIZ];
	char return_options[256];
	int ra;
	int yes_output_this_room;

	char **data_for_callback;
	char *verb;
	int subscribed_rooms_only;
	int num_patterns;
	char **patterns;
	int return_subscribed;
	int return_children;
	int return_metadata;
	int i = 0;
	int match = 0;

	/* Here's how we break down the array of pointers passed to us */
	data_for_callback = data;
	verb = data_for_callback[0];
	subscribed_rooms_only = (int) data_for_callback[1];
	num_patterns = (int) data_for_callback[2];
	patterns = (char **) data_for_callback[3];
	return_subscribed = (int) data_for_callback[4];
	return_children = (int) data_for_callback[5];
	return_metadata = (int) data_for_callback[6];

	/* Only list rooms to which the user has access!! */
	yes_output_this_room = 0;
	strcpy(return_options, "");
	CtdlRoomAccess(qrbuf, &CC->user, &ra, NULL);

	if (return_subscribed) {
		if (ra & UA_KNOWN) {
			strcat(return_options, "\\Subscribed");
		}
	}

	/* Warning: ugly hack.
	 * We don't have any way to determine the presence of child mailboxes
	 * without refactoring this entire module.  So we're just going to return
	 * the \HasChildren attribute for every room.
	 * We'll fix this later when we have time.
	 */
	if (return_children) {
		if (!IsEmptyStr(return_options)) {
			strcat(return_options, " ");
		}
		strcat(return_options, "\\HasChildren");
	}

	if (subscribed_rooms_only) {
		if (ra & UA_KNOWN) {
			yes_output_this_room = 1;
		}
	}
	else {
		if ((ra & UA_KNOWN) || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
			yes_output_this_room = 1;
		}
	}

	if (yes_output_this_room) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		match = 0;
		for (i=0; i<num_patterns; ++i) {
			if (imap_mailbox_matches_pattern(patterns[i], buf)) {
				match = 1;
			}
		}
		if (match) {
			cprintf("* %s (%s) \"/\" ", verb, return_options);
			imap_strout(buf);

			if (return_metadata) {
				cprintf(" (METADATA ())");	/* FIXME */
			}

			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LIST and LSUB commands
 */
void imap_list(int num_parms, char *parms[])
{
	int subscribed_rooms_only = 0;
	char verb[16];
	int i, j, paren_nest;
	char *data_for_callback[7];
	int num_patterns = 1;
	char *patterns[MAX_PATTERNS];
	int selection_left = (-1);
	int selection_right = (-1);
	int return_left = (-1);
	int return_right = (-1);
	int root_pos = 2;
	int patterns_left = 3;
	int patterns_right = 3;
	int extended_list_in_use = 0;
	int return_subscribed = 0;
	int return_children = 0;
	int return_metadata = 0;
	int select_metadata_left = (-1);
	int select_metadata_right = (-1);
	int select_metadata_nest = 0;

	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}

	/* parms[1] is the IMAP verb being used (e.g. LIST or LSUB)
	 * This tells us how to behave, and what verb to return back to the caller
	 */
	safestrncpy(verb, parms[1], sizeof verb);
	j = strlen(verb);
	for (i=0; i<j; ++i) {
		verb[i] = toupper(verb[i]);
	}

	if (!strcasecmp(verb, "LSUB")) {
		subscribed_rooms_only = 1;
	}

	/*
	 * In order to implement draft-ietf-imapext-list-extensions-18
	 * ("LIST Command Extensions") we need to:
	 *
	 * 1. Extract "selection options"
	 *				(Extraction: done
	 *				SUBSCRIBED option: done
	 *				RECURSIVEMATCH option: not done yet
	 *				REMOTE: safe to silently ignore)
	 *
	 * 2. Extract "return options"
	 *				(Extraction: done
	 *				SUBSCRIBED option: done
	 *				CHILDREN option: done, but needs a non-ugly rewrite)
	 *
	 * 3. Determine whether there is more than one match pattern (done)
	 */

	/*
	 * If parameter 2 begins with a '(' character, the client is specifying
	 * selection options.  Extract their exact position, and then modify our
	 * expectation of where the root folder will be specified.
	 */
	if (parms[2][0] == '(') {
		extended_list_in_use = 1;
		selection_left = 2;
		paren_nest = 0;
		for (i=2; i<num_parms; ++i) {
			for (j=0; parms[i][j]; ++j) {
				if (parms[i][j] == '(') ++paren_nest;
				if (parms[i][j] == ')') --paren_nest;
			}
			if (paren_nest == 0) {
				selection_right = i;	/* found end of selection options */
				root_pos = i+1;		/* folder root appears after selection options */
				i = num_parms + 1;	/* break out of the loop */
			}
		}
	}

	/* If selection options were found, do something with them.
	 */
	if ((selection_left > 0) && (selection_right >= selection_left)) {

		/* Strip off the outer parentheses */
		if (parms[selection_left][0] == '(') {
			strcpy(parms[selection_left], &parms[selection_left][1]);
		}
		if (parms[selection_right][strlen(parms[selection_right])-1] == ')') {
			parms[selection_right][strlen(parms[selection_right])-1] = 0;
		}

		for (i=selection_left; i<=selection_right; ++i) {

			/* are we in the middle of a metadata select block? */
			if ((select_metadata_left >= 0) && (select_metadata_right < 0)) {
				select_metadata_nest += haschar(parms[i], '(') - haschar(parms[i], ')') ;
				if (select_metadata_nest == 0) {
					select_metadata_right = i;
				}
			}

			else if (!strcasecmp(parms[i], "METADATA")) {
				select_metadata_left = i+1;
				select_metadata_nest = 0;
			}

			else if (!strcasecmp(parms[i], "SUBSCRIBED")) {
				subscribed_rooms_only = 1;
			}

			else if (!strcasecmp(parms[i], "RECURSIVEMATCH")) {
				/* FIXME - do this! */
			}

		}

	}

	lprintf(CTDL_DEBUG, "select metadata: %d to %d\n", select_metadata_left, select_metadata_right);
	/* FIXME blah, we have to do something with this */

	/* The folder root appears immediately after the selection options,
	 * or in position 2 if no selection options were specified.
	 */
	patterns_left = root_pos + 1;
	patterns_right = root_pos + 1;

	if (parms[patterns_left][0] == '(') {
		extended_list_in_use = 1;
		paren_nest = 0;
		for (i=patterns_left; i<num_parms; ++i) {
			for (j=0; &parms[i][j]; ++j) {
				if (parms[i][j] == '(') ++paren_nest;
				if (parms[i][j] == ')') --paren_nest;
			}
			if (paren_nest == 0) {
				patterns_right = i;	/* found end of patterns */
				i = num_parms + 1;	/* break out of the loop */
			}
		}
		num_patterns = patterns_right - patterns_left + 1;
		for (i=0; i<num_patterns; ++i) {
			if (i < MAX_PATTERNS) {
				patterns[i] = malloc(512);
				snprintf(patterns[i], 512, "%s%s", parms[root_pos], parms[patterns_left+i]);
				if (i == 0) {
					strcpy(patterns[i], &patterns[i][1]);
				}
				if (i == num_patterns-1) {
					patterns[i][strlen(patterns[i])-1] = 0;
				}
			}
		}
	}
	else {
		num_patterns = 1;
		patterns[0] = malloc(512);
		snprintf(patterns[0], 512, "%s%s", parms[root_pos], parms[patterns_left]);
	}

	/* If the word "RETURN" appears after the folder pattern list, then the client
	 * is specifying return options.
	 */
	if (num_parms - patterns_right > 2) if (!strcasecmp(parms[patterns_right+1], "RETURN")) {
		return_left = patterns_right + 2;
		extended_list_in_use = 1;
		paren_nest = 0;
		for (i=return_left; i<num_parms; ++i) {
			for (j=0; parms[i][j]; ++j) {
				if (parms[i][j] == '(') ++paren_nest;
				if (parms[i][j] == ')') --paren_nest;
			}

			/* Might as well look for these while we're in here... */
			if (parms[i][0] == '(') strcpy(parms[i], &parms[i][1]);
			if (parms[i][strlen(parms[i])-1] == ')') parms[i][strlen(parms[i])-1] = 0;
			lprintf(9, "evaluating <%s>\n", parms[i]);

			if (!strcasecmp(parms[i], "SUBSCRIBED")) {
				return_subscribed = 1;
			}

			else if (!strcasecmp(parms[i], "CHILDREN")) {
				return_children = 1;
			}

			else if (!strcasecmp(parms[i], "METADATA")) {
				return_metadata = 1;
			}

			if (paren_nest == 0) {
				return_right = i;	/* found end of patterns */
				i = num_parms + 1;	/* break out of the loop */
			}
		}
	}

	/* Now start setting up the data we're going to send to the ForEachRoom() callback.
	 */
	data_for_callback[0] = (char *) verb;
	data_for_callback[1] = (char *) subscribed_rooms_only;
	data_for_callback[2] = (char *) num_patterns;
	data_for_callback[3] = (char *) patterns;
	data_for_callback[4] = (char *) return_subscribed;
	data_for_callback[5] = (char *) return_children;
	data_for_callback[6] = (char *) return_metadata;

	/* The non-extended LIST command is required to treat an empty
	 * ("" string) mailbox name argument as a special request to return the
	 * hierarchy delimiter and the root name of the name given in the
	 * reference parameter.
	 */
	if ( (IsEmptyStr(patterns[0])) && (extended_list_in_use == 0) ) {
		cprintf("* %s (\\Noselect) \"/\" \"\"\r\n", verb);
	}

	/* Non-empty mailbox names, and any form of the extended LIST command,
	 * is handled by this loop.
	 */
	else {
		imap_list_floors(verb, num_patterns, patterns);
		ForEachRoom(imap_listroom, data_for_callback);
	}

	/* 
	 * Free the pattern buffers we allocated above.
	 */
	for (i=0; i<num_patterns; ++i) {
		free(patterns[i]);
	}

	cprintf("%s OK %s completed\r\n", parms[0], verb);
}
