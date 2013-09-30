/*
 * Implements the LIST and LSUB commands.
 *
 * Copyright (c) 2000-2009 by Art Cancro and others.
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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
#include "imap_store.h"
#include "imap_acl.h"
#include "imap_misc.h"
#include "imap_list.h"
#include "ctdl_module.h"


typedef struct __ImapRoomListFilter {
	char verb[16];
	int subscribed_rooms_only;
	int return_subscribed;
	int return_children;

	int num_patterns;
	int num_patterns_avail;
	StrBuf **patterns;
}ImapRoomListFilter;

/*
 * Used by LIST and LSUB to show the floors in the listing
 */
void imap_list_floors(char *verb, int num_patterns, StrBuf **patterns)
{
	int i;
	struct floor *fl;
	int j = 0;
	int match = 0;

	for (i = 0; i < MAXFLOORS; ++i) {
		fl = CtdlGetCachedFloor(i);
		if (fl->f_flags & F_INUSE) {
			match = 0;
			for (j=0; j<num_patterns; ++j) {
				if (imap_mailbox_matches_pattern (ChrPtr(patterns[j]), fl->f_name)) {
					match = 1;
				}
			}
			if (match) {
				IAPrintf("* %s (\\NoSelect \\HasChildren) \"/\" ", verb);
				IPutStr(fl->f_name, (fl->f_name)?strlen(fl->f_name):0);
				IAPuts("\r\n");
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
#define SUBSCRIBED_STR "\\Subscribed"
#define HASCHILD_STR "\\HasChildren"
	char MailboxName[SIZ];
	char return_options[256];
	int ra;
	int yes_output_this_room;
	ImapRoomListFilter *ImapFilter;
	int i = 0;
	int match = 0;
	int ROLen;

	/* Here's how we break down the array of pointers passed to us */
	ImapFilter = (ImapRoomListFilter*)data;

	/* Only list rooms to which the user has access!! */
	yes_output_this_room = 0;
	*return_options = '\0';
	ROLen = 0;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, NULL);

	if (ImapFilter->return_subscribed) {
		if (ra & UA_KNOWN) {
			memcpy(return_options, HKEY(SUBSCRIBED_STR) + 1);
			ROLen += sizeof(SUBSCRIBED_STR) - 1;
		}
	}

	/* Warning: ugly hack.
	 * We don't have any way to determine the presence of child mailboxes
	 * without refactoring this entire module.  So we're just going to return
	 * the \HasChildren attribute for every room.
	 * We'll fix this later when we have time.
	 */
	if (ImapFilter->return_children) {
		if (!IsEmptyStr(return_options)) {
			memcpy(return_options + ROLen, HKEY(" "));
			ROLen ++;
		}
		memcpy(return_options + ROLen, HKEY(SUBSCRIBED_STR) + 1);
	}

	if (ImapFilter->subscribed_rooms_only) {
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
		long len;
		len = imap_mailboxname(MailboxName, sizeof MailboxName, qrbuf);
		match = 0;
		for (i=0; i<ImapFilter->num_patterns; ++i) {
			if (imap_mailbox_matches_pattern(ChrPtr(ImapFilter->patterns[i]), MailboxName)) {
				match = 1;
			}
		}
		if (match) {
			IAPrintf("* %s (%s) \"/\" ", ImapFilter->verb, return_options);
			IPutStr(MailboxName, len);
			IAPuts("\r\n");
		}
	}
}


/*
 * Implements the LIST and LSUB commands
 */
void imap_list(int num_parms, ConstStr *Params)
{
	struct CitContext *CCC = CC;
	citimap *Imap = CCCIMAP;
	int i, j, paren_nest;
	ImapRoomListFilter ImapFilter;
	int selection_left = (-1);
	int selection_right = (-1);
	int return_left = (-1);
	int root_pos = 2;
	int patterns_left = 3;
	int patterns_right = 3;
	int extended_list_in_use = 0;

	if (num_parms < 4) {
		IReply("BAD arguments invalid");
		return;
	}

	ImapFilter.num_patterns = 1;
	ImapFilter.return_subscribed = 0;
	ImapFilter.return_children = 0;
	ImapFilter.subscribed_rooms_only = 0;
	

	/* parms[1] is the IMAP verb being used (e.g. LIST or LSUB)
	 * This tells us how to behave, and what verb to return back to the caller
	 */
	safestrncpy(ImapFilter.verb, Params[1].Key, sizeof ImapFilter.verb);
	j = Params[1].len;
	for (i=0; i<j; ++i) {
		ImapFilter.verb[i] = toupper(ImapFilter.verb[i]);
	}

	if (!strcasecmp(ImapFilter.verb, "LSUB")) {
		ImapFilter.subscribed_rooms_only = 1;
	}

	/*
	 * Partial implementation of LIST-EXTENDED (which will not get used because
	 * we don't advertise it in our capabilities string).  Several requirements:
	 *
	 * Extraction of selection options:
	 *	SUBSCRIBED option: done
	 *	RECURSIVEMATCH option: not done yet
	 *	REMOTE: safe to silently ignore
	 *
	 * Extraction of return options:
	 *	SUBSCRIBED option: done
	 *	CHILDREN option: done, but needs a non-ugly rewrite
	 *
	 * Multiple match patterns: done
	 */

	/*
	 * If parameter 2 begins with a '(' character, the client is specifying
	 * selection options.  Extract their exact position, and then modify our
	 * expectation of where the root folder will be specified.
	 */
	if (Params[2].Key[0] == '(') {
		extended_list_in_use = 1;
		selection_left = 2;
		paren_nest = 0;
		for (i=2; i<num_parms; ++i) {
			for (j=0; Params[i].Key[j]; ++j) {
				if (Params[i].Key[j] == '(') ++paren_nest;
				if (Params[i].Key[j] == ')') --paren_nest;
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
		if (Params[selection_left].Key[0] == '(') {
			TokenCutLeft(&Imap->Cmd, 
				     &Params[selection_left], 
				     1);
		}
		if (Params[selection_right].Key[Params[selection_right].len-1] == ')') {
			TokenCutRight(&Imap->Cmd, 
				      &Params[selection_right], 
				      1);
		}

		for (i=selection_left; i<=selection_right; ++i) {

			if (!strcasecmp(Params[i].Key, "SUBSCRIBED")) {
				ImapFilter.subscribed_rooms_only = 1;
			}

			else if (!strcasecmp(Params[i].Key, "RECURSIVEMATCH")) {
				/* FIXME - do this! */
			}

		}

	}

	/* The folder root appears immediately after the selection options,
	 * or in position 2 if no selection options were specified.
	 */
	ImapFilter.num_patterns_avail = num_parms + 1;
	ImapFilter.patterns = malloc(ImapFilter.num_patterns_avail * sizeof(StrBuf*));
	memset(ImapFilter.patterns, 0, ImapFilter.num_patterns_avail * sizeof(StrBuf*));

	patterns_left = root_pos + 1;
	patterns_right = root_pos + 1;

	if (Params[patterns_left].Key[0] == '(') {
		extended_list_in_use = 1;
		paren_nest = 0;
		for (i=patterns_left; i<num_parms; ++i) {
			for (j=0; &Params[i].Key[j]; ++j) {
				if (Params[i].Key[j] == '(') ++paren_nest;
				if (Params[i].Key[j] == ')') --paren_nest;
			}
			if (paren_nest == 0) {
				patterns_right = i;	/* found end of patterns */
				i = num_parms + 1;	/* break out of the loop */
			}
		}
		ImapFilter.num_patterns = patterns_right - patterns_left + 1;
		for (i=0; i<ImapFilter.num_patterns; ++i) {
			if (i < MAX_PATTERNS) {
				ImapFilter.patterns[i] = NewStrBufPlain(NULL, 
									Params[root_pos].len + 
									Params[patterns_left+i].len);
				if (i == 0) {
					if (Params[root_pos].len > 1)
						StrBufAppendBufPlain(ImapFilter.patterns[i], 
								     1 + CKEY(Params[root_pos]) - 1, 0);
				}
				else
					StrBufAppendBufPlain(ImapFilter.patterns[i], 
							     CKEY(Params[root_pos]), 0);

				if (i == ImapFilter.num_patterns-1) {
					if (Params[patterns_left+i].len > 1)
						StrBufAppendBufPlain(ImapFilter.patterns[i], 
								     CKEY(Params[patterns_left+i]) - 1, 0);
				}
				else StrBufAppendBufPlain(ImapFilter.patterns[i], 
							  CKEY(Params[patterns_left+i]), 0);

			}

		}
	}
	else {
		ImapFilter.num_patterns = 1;
		ImapFilter.patterns[0] = NewStrBufPlain(NULL, 
							Params[root_pos].len + 
							Params[patterns_left].len);
		StrBufAppendBufPlain(ImapFilter.patterns[0], 
				     CKEY(Params[root_pos]), 0);
		StrBufAppendBufPlain(ImapFilter.patterns[0], 
				     CKEY(Params[patterns_left]), 0);
	}

	/* If the word "RETURN" appears after the folder pattern list, then the client
	 * is specifying return options.
	 */
	if (num_parms - patterns_right > 2) if (!strcasecmp(Params[patterns_right+1].Key, "RETURN")) {
		return_left = patterns_right + 2;
		extended_list_in_use = 1;
		paren_nest = 0;
		for (i=return_left; i<num_parms; ++i) {
			for (j=0;   Params[i].Key[j]; ++j) {
				if (Params[i].Key[j] == '(') ++paren_nest;
				if (Params[i].Key[j] == ')') --paren_nest;
			}

			/* Might as well look for these while we're in here... */
			if (Params[i].Key[0] == '(') 
				TokenCutLeft(&Imap->Cmd, 
					     &Params[i], 
					     1);
			if (Params[i].Key[Params[i].len-1] == ')')
			    TokenCutRight(&Imap->Cmd, 
					  &Params[i], 
					  1);

			IMAP_syslog(LOG_DEBUG, "evaluating <%s>", Params[i].Key);

			if (!strcasecmp(Params[i].Key, "SUBSCRIBED")) {
				ImapFilter.return_subscribed = 1;
			}

			else if (!strcasecmp(Params[i].Key, "CHILDREN")) {
				ImapFilter.return_children = 1;
			}

			if (paren_nest == 0) {
				i = num_parms + 1;	/* break out of the loop */
			}
		}
	}

	/* Now start setting up the data we're going to send to the CtdlForEachRoom() callback.
	 */
	
	/* The non-extended LIST command is required to treat an empty
	 * ("" string) mailbox name argument as a special request to return the
	 * hierarchy delimiter and the root name of the name given in the
	 * reference parameter.
	 */
	if ( (StrLength(ImapFilter.patterns[0]) == 0) && (extended_list_in_use == 0) ) {
		IAPrintf("* %s (\\Noselect) \"/\" \"\"\r\n", ImapFilter.verb);
	}

	/* Non-empty mailbox names, and any form of the extended LIST command,
	 * is handled by this loop.
	 */
	else {
		imap_list_floors(ImapFilter.verb, 
				 ImapFilter.num_patterns, 
				 ImapFilter.patterns);
		CtdlForEachRoom(imap_listroom, (char**)&ImapFilter);
	}

	/* 
	 * Free the pattern buffers we allocated above.
	 */
	for (i=0; i<ImapFilter.num_patterns; ++i) {
		FreeStrBuf(&ImapFilter.patterns[i]);
	}
	free(ImapFilter.patterns);

	IReplyPrintf("OK %s completed", ImapFilter.verb);
}
