/*
 * Implements the STORE command in IMAP.
 *
 * Copyright (c) 2001-2009 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "ctdl_module.h"

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
#include "room_ops.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_imap.h"
#include "imap_tools.h"
#include "imap_fetch.h"
#include "imap_store.h"
#include "genstamp.h"


/*
 * imap_do_store() calls imap_do_store_msg() to tweak the settings of
 * an individual message.
 *
 * We also implement the ".SILENT" protocol option here.  :(
 */
void imap_do_store_msg(int seq, const char *oper, unsigned int bits_to_twiddle) {
	citimap *Imap = IMAP;

	if (!strncasecmp(oper, "FLAGS", 5)) {
		Imap->flags[seq] &= IMAP_MASK_SYSTEM;
		Imap->flags[seq] |= bits_to_twiddle;
	}
	else if (!strncasecmp(oper, "+FLAGS", 6)) {
		Imap->flags[seq] |= bits_to_twiddle;
	}
	else if (!strncasecmp(oper, "-FLAGS", 6)) {
		Imap->flags[seq] &= (~bits_to_twiddle);
	}
}


/*
 * imap_store() calls imap_do_store() to perform the actual bit twiddling
 * on the flags.
 */
void imap_do_store(citimap_command *Cmd) {
	int i, j;
	unsigned int bits_to_twiddle = 0;
	const char *oper;
	char flag[32];
	char whichflags[256];
	char num_flags;
	int silent = 0;
	long *ss_msglist;
	int num_ss = 0;
	int last_item_twiddled = (-1);
	citimap *Imap = IMAP;

	if (Cmd->num_parms < 2) return;
	oper = Cmd->Params[0].Key;
	if (cbmstrcasestr(oper, ".SILENT")) {
		silent = 1;
	}

	/*
	 * ss_msglist is an array of message numbers to manipulate.  We
	 * are going to supply this array to CtdlSetSeen() later.
	 */
	ss_msglist = malloc(Imap->num_msgs * sizeof(long));
	if (ss_msglist == NULL) return;

	/*
	 * Ok, go ahead and parse the flags.
	 */
	for (i=1; i<Cmd->num_parms; ++i) {///TODO: why strcpy? 
		strcpy(whichflags, Cmd->Params[i].Key);
		if (whichflags[0]=='(') {
			safestrncpy(whichflags, &whichflags[1], 
				sizeof whichflags);
		}
		if (whichflags[strlen(whichflags)-1]==')') {
			whichflags[strlen(whichflags)-1]=0;
		}
		striplt(whichflags);

		/* A client might twiddle more than one bit at a time.
		 * Note that we check for the flag names without the leading
		 * backslash because imap_parameterize() strips them out.
		 */
		num_flags = num_tokens(whichflags, ' ');
		for (j=0; j<num_flags; ++j) {
			extract_token(flag, whichflags, j, ' ', sizeof flag);

			if ((!strcasecmp(flag, "\\Deleted"))
			   || (!strcasecmp(flag, "Deleted"))) {
				if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom()) {
					bits_to_twiddle |= IMAP_DELETED;
				}
			}
			if ((!strcasecmp(flag, "\\Seen"))
			   || (!strcasecmp(flag, "Seen"))) {
				bits_to_twiddle |= IMAP_SEEN;
			}
			if ((!strcasecmp(flag, "\\Answered")) 
			   || (!strcasecmp(flag, "Answered"))) {
				bits_to_twiddle |= IMAP_ANSWERED;
			}
		}
	}

	if (Imap->num_msgs > 0) {
		for (i = 0; i < Imap->num_msgs; ++i) {
			if (Imap->flags[i] & IMAP_SELECTED) {
				last_item_twiddled = i;

				ss_msglist[num_ss++] = Imap->msgids[i];
				imap_do_store_msg(i, oper, bits_to_twiddle);

				if (!silent) {
					IAPrintf("* %d FETCH (", i+1);
					imap_fetch_flags(i);
					IAPuts(")\r\n");
				}

			}
		}
	}

	/*
	 * Now manipulate the database -- all in one shot.
	 */
	if ( (last_item_twiddled >= 0) && (num_ss > 0) ) {

		if (bits_to_twiddle & IMAP_SEEN) {
			CtdlSetSeen(ss_msglist, num_ss,
				((Imap->flags[last_item_twiddled] & IMAP_SEEN) ? 1 : 0),
				ctdlsetseen_seen,
				NULL, NULL
			);
		}

		if (bits_to_twiddle & IMAP_ANSWERED) {
			CtdlSetSeen(ss_msglist, num_ss,
				((Imap->flags[last_item_twiddled] & IMAP_ANSWERED) ? 1 : 0),
				ctdlsetseen_answered,
				NULL, NULL
			);
		}

	}

	free(ss_msglist);
	imap_do_expunge();		// Citadel always expunges immediately.
	imap_rescan_msgids();
}


/*
 * This function is called by the main command loop.
 */
void imap_store(int num_parms, ConstStr *Params) {
	citimap_command Cmd;
	int num_items;

	if (num_parms < 3) {
		IReply("BAD invalid parameters");
		return;
	}

	if (imap_is_message_set(Params[2].Key)) {
		imap_pick_range(Params[2].Key, 0);
	}
	else {
		IReply("BAD invalid parameters");
		return;
	}

	memset(&Cmd, 0, sizeof(citimap_command));
	Cmd.CmdBuf = NewStrBufPlain(NULL, StrLength(IMAP->Cmd.CmdBuf));
	MakeStringOf(Cmd.CmdBuf, 3);

	num_items = imap_extract_data_items(&Cmd);
	if (num_items < 1) {
		IReply("BAD invalid data item list");
		FreeStrBuf(&Cmd.CmdBuf);
		free(Cmd.Params);
		return;
	}

	imap_do_store(&Cmd);
	IReply("OK STORE completed");
	FreeStrBuf(&Cmd.CmdBuf);
	free(Cmd.Params);
}

/*
 * This function is called by the main command loop.
 */
void imap_uidstore(int num_parms, ConstStr *Params) {
	citimap_command Cmd;
	int num_items;

	if (num_parms < 4) {
		IReply("BAD invalid parameters");
		return;
	}

	if (imap_is_message_set(Params[3].Key)) {
		imap_pick_range(Params[3].Key, 1);
	}
	else {
		IReply("BAD invalid parameters");
		return;
	}

	memset(&Cmd, 0, sizeof(citimap_command));
	Cmd.CmdBuf = NewStrBufPlain(NULL, StrLength(IMAP->Cmd.CmdBuf));
	MakeStringOf(Cmd.CmdBuf, 4);

	num_items = imap_extract_data_items(&Cmd);
	if (num_items < 1) {
		IReply("BAD invalid data item list");
		FreeStrBuf(&Cmd.CmdBuf);
		free(Cmd.Params);
		return;
	}

	imap_do_store(&Cmd);
	IReply("OK UID STORE completed");
	FreeStrBuf(&Cmd.CmdBuf);
	free(Cmd.Params);
}


