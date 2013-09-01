/*
 * Copyright (c) 1987-2011 by the citadel.org team
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
#include "imap_misc.h"
#include "genstamp.h"
#include "ctdl_module.h"





/*
 * imap_copy() calls imap_do_copy() to do its actual work, once it's
 * validated and boiled down the request a bit.  (returns 0 on success)
 */
int imap_do_copy(const char *destination_folder) {
	citimap *Imap = IMAP;
	int i;
	char roomname[ROOMNAMELEN];
	struct ctdlroom qrbuf;
	long *selected_msgs = NULL;
	int num_selected = 0;

	if (Imap->num_msgs < 1) {
		return(0);
	}

	i = imap_grabroom(roomname, destination_folder, 1);
	if (i != 0) return(i);

	/*
	 * Copy all the message pointers in one shot.
	 */
	selected_msgs = malloc(sizeof(long) * Imap->num_msgs);
	if (selected_msgs == NULL) return(-1);

	for (i = 0; i < Imap->num_msgs; ++i) {
		if (Imap->flags[i] & IMAP_SELECTED) {
			selected_msgs[num_selected++] = Imap->msgids[i];
		}
	}

	if (num_selected > 0) {
		CtdlSaveMsgPointersInRoom(roomname, selected_msgs, num_selected, 1, NULL, 0);
	}
	free(selected_msgs);

	/* Don't bother wasting any more time if there were no messages. */
	if (num_selected == 0) {
		return(0);
	}

	/* Enumerate lists of messages for which flags are toggled */
	long *seen_yes = NULL;
	int num_seen_yes = 0;
	long *seen_no = NULL;
	int num_seen_no = 0;
	long *answ_yes = NULL;
	int num_answ_yes = 0;
	long *answ_no = NULL;
	int num_answ_no = 0;

	seen_yes = malloc(num_selected * sizeof(long));
	seen_no = malloc(num_selected * sizeof(long));
	answ_yes = malloc(num_selected * sizeof(long));
	answ_no = malloc(num_selected * sizeof(long));

	for (i = 0; i < Imap->num_msgs; ++i) {
		if (Imap->flags[i] & IMAP_SELECTED) {
			if (Imap->flags[i] & IMAP_SEEN) {
				seen_yes[num_seen_yes++] = Imap->msgids[i];
			}
			if ((Imap->flags[i] & IMAP_SEEN) == 0) {
				seen_no[num_seen_no++] = Imap->msgids[i];
			}
			if (Imap->flags[i] & IMAP_ANSWERED) {
				answ_yes[num_answ_yes++] = Imap->msgids[i];
			}
			if ((Imap->flags[i] & IMAP_ANSWERED) == 0) {
				answ_no[num_answ_no++] = Imap->msgids[i];
			}
		}
	}

	/* Set the flags... */
	i = CtdlGetRoom(&qrbuf, roomname);
	if (i == 0) {
		CtdlSetSeen(seen_yes, num_seen_yes, 1, ctdlsetseen_seen, NULL, &qrbuf);
		CtdlSetSeen(seen_no, num_seen_no, 0, ctdlsetseen_seen, NULL, &qrbuf);
		CtdlSetSeen(answ_yes, num_answ_yes, 1, ctdlsetseen_answered, NULL, &qrbuf);
		CtdlSetSeen(answ_no, num_answ_no, 0, ctdlsetseen_answered, NULL, &qrbuf);
	}

	free(seen_yes);
	free(seen_no);
	free(answ_yes);
	free(answ_no);

	return(0);
}


/*
 * Output the [COPYUID xxx yyy] response code required by RFC2359
 * to tell the client the UID's of the messages that were copied (if any).
 * We are assuming that the IMAP_SELECTED flag is still set on any relevant
 * messages in our source room.  Since the Citadel system uses UID's that
 * are both globally unique and persistent across a room-to-room copy, we
 * can get this done quite easily.
 */
void imap_output_copyuid_response(void) {
	citimap *Imap = IMAP;
	int i;
	int num_output = 0;
  
	for (i = 0; i < Imap->num_msgs; ++i) {
		if (Imap->flags[i] & IMAP_SELECTED) {
			++num_output;
			if (num_output == 1) {
				IAPuts("[COPYUID ");
			}
			else if (num_output > 1) {
				IAPuts(",");
			}
			IAPrintf("%ld", Imap->msgids[i]);
		}
	}
	if (num_output > 0) {
		IAPuts("] ");
	}
}


/*
 * This function is called by the main command loop.
 */
void imap_copy(int num_parms, ConstStr *Params) {
	int ret;

	if (num_parms != 4) {
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

	ret = imap_do_copy(Params[3].Key);
	if (!ret) {
		IAPrintf("%s OK ", Params[0].Key);
		imap_output_copyuid_response();
		IAPuts("COPY completed\r\n");
	}
	else {
		IReplyPrintf("NO COPY failed (error %d)", ret);
	}
}

/*
 * This function is called by the main command loop.
 */
void imap_uidcopy(int num_parms, ConstStr *Params) {

	if (num_parms != 5) {
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

	if (imap_do_copy(Params[4].Key) == 0) {
		IAPrintf("%s OK ", Params[0].Key);
		imap_output_copyuid_response();
		IAPuts("UID COPY completed\r\n");
	}
	else {
		IReply("NO UID COPY failed");
	}
}


/*
 * imap_do_append_flags() is called by imap_append() to set any flags that
 * the client specified at append time.
 *
 * FIXME find a way to do these in bulk so we don't max out our db journal
 */
void imap_do_append_flags(long new_msgnum, char *new_message_flags) {
	char flags[32];
	char this_flag[sizeof flags];
	int i;

	if (new_message_flags == NULL) return;
	if (IsEmptyStr(new_message_flags)) return;

	safestrncpy(flags, new_message_flags, sizeof flags);

	for (i=0; i<num_tokens(flags, ' '); ++i) {
		extract_token(this_flag, flags, i, ' ', sizeof this_flag);
		if (this_flag[0] == '\\') strcpy(this_flag, &this_flag[1]);
		if (!strcasecmp(this_flag, "Seen")) {
			CtdlSetSeen(&new_msgnum, 1, 1, ctdlsetseen_seen,
				NULL, NULL);
		}
		if (!strcasecmp(this_flag, "Answered")) {
			CtdlSetSeen(&new_msgnum, 1, 1, ctdlsetseen_answered,
				NULL, NULL);
		}
	}
}


/*
 * This function is called by the main command loop.
 */
void imap_append(int num_parms, ConstStr *Params) {
	struct CitContext *CCC = CC;
	long literal_length;
	struct CtdlMessage *msg = NULL;
	long new_msgnum = (-1L);
	int ret = 0;
	char roomname[ROOMNAMELEN];
	char errbuf[SIZ];
	char dummy[SIZ];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int i;
	char new_message_flags[SIZ];
	citimap *Imap;

	if (num_parms < 4) {
		IReply("BAD usage error");
		return;
	}

	if ( (Params[num_parms-1].Key[0] != '{')
	   || (Params[num_parms-1].Key[Params[num_parms-1].len-1] != '}') )  {
		IReply("BAD no message literal supplied");
		return;
	}

	strcpy(new_message_flags, "");
	if (num_parms >= 5) {
		for (i=3; i<num_parms; ++i) {
			strcat(new_message_flags, Params[i].Key);
			strcat(new_message_flags, " ");
		}
		stripallbut(new_message_flags, '(', ')');
	}

	/* This is how we'd do this if it were relevant in our data store.
	 * if (num_parms >= 6) {
	 *  new_message_internaldate = parms[4];
	 * }
	 */

	literal_length = atol(&Params[num_parms-1].Key[1]);
	if (literal_length < 1) {
		IReply("BAD Message length must be at least 1.");
		return;
	}

	Imap = IMAP;
	imap_free_transmitted_message();	/* just in case. */

	Imap->TransmittedMessage = NewStrBufPlain(NULL, literal_length);

	if (Imap->TransmittedMessage == NULL) {
		IReply("NO Cannot allocate memory.");
		return;
	}
	
	IAPrintf("+ Transmit message now.\r\n");
	
	IUnbuffer ();

	client_read_blob(Imap->TransmittedMessage, literal_length, config.c_sleeping);

	if ((ret < 0) || (StrLength(Imap->TransmittedMessage) < literal_length)) {
		IReply("NO Read failed.");
		return;
	}

	/* Client will transmit a trailing CRLF after the literal (the message
	 * text) is received.  This call to client_getln() absorbs it.
	 */
	flush_output();
	client_getln(dummy, sizeof dummy);

	/* Convert RFC822 newlines (CRLF) to Unix newlines (LF) */
	IMAPM_syslog(LOG_DEBUG, "Converting CRLF to LF");
	StrBufToUnixLF(Imap->TransmittedMessage);

	IMAPM_syslog(LOG_DEBUG, "Converting message format");
	msg = convert_internet_message_buf(&Imap->TransmittedMessage);

	ret = imap_grabroom(roomname, Params[2].Key, 1);
	if (ret != 0) {
		IReply("NO Invalid mailbox name or access denied");
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (Imap->selected) {
		strcpy(savedroom, CCC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/* If the user is locally authenticated, FORCE the From: header to
	 * show up as the real sender.  FIXME do we really want to do this?
	 * Probably should make it site-definable or even room-definable.
	 *
	 * For now, we allow "forgeries" if the room is one of the user's
	 * private mailboxes.
	 */
	if (CCC->logged_in) {
	   if ( ((CCC->room.QRflags & QR_MAILBOX) == 0) && (config.c_imap_keep_from == 0)) {

		CM_SetField(msg, eAuthor, CCC->user.fullname, strlen(CCC->user.fullname));
		CM_SetField(msg, eNodeName, config.c_nodename, strlen(config.c_nodename));
		CM_SetField(msg, eHumanNode, config.c_humannode, strlen(config.c_humannode));
	    }
	}

	/* 
	 * Can we post here?
	 */
	ret = CtdlDoIHavePermissionToPostInThisRoom(errbuf, sizeof errbuf, NULL, POST_LOGGED_IN, 0);

	if (ret) {
		/* Nope ... print an error message */
		IReplyPrintf("NO %s", errbuf);
	}

	else {
		/* Yes ... go ahead and post! */
		if (msg != NULL) {
			new_msgnum = CtdlSubmitMsg(msg, NULL, "", 0);
		}
		if (new_msgnum >= 0L) {
			IReplyPrintf("OK [APPENDUID %ld %ld] APPEND completed",
				     GLOBAL_UIDVALIDITY_VALUE, new_msgnum);
		}
		else {
			IReplyPrintf("BAD Error %ld saving message to disk.",
				     new_msgnum);
		}
	}

	/*
	 * IMAP protocol response to client has already been sent by now.
	 *
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (Imap->selected) {
		CtdlUserGoto(savedroom, 0, 0, &msgs, &new);
	}

	/* We don't need this buffer anymore */
	CM_Free(msg);

	if (new_message_flags != NULL) {
		imap_do_append_flags(new_msgnum, new_message_flags);
	}
}
