/*
 * $Id$
 *
 *
 *
 * Copyright (c) 2001-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
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
#include "policy.h"
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
int imap_do_copy(char *destination_folder) {
	int i;
	char roomname[ROOMNAMELEN];
	struct ctdlroom qrbuf;
	long *selected_msgs = NULL;
	int num_selected = 0;

	if (IMAP->num_msgs < 1) {
		return(0);
	}

	i = imap_grabroom(roomname, destination_folder, 1);
	if (i != 0) return(i);

	/*
	 * Copy all the message pointers in one shot.
	 */
	selected_msgs = malloc(sizeof(long) * IMAP->num_msgs);
	if (selected_msgs == NULL) return(-1);

	for (i = 0; i < IMAP->num_msgs; ++i) {
		if (IMAP->flags[i] & IMAP_SELECTED) {
			selected_msgs[num_selected++] = IMAP->msgids[i];
		}
	}

	if (num_selected > 0) {
		CtdlSaveMsgPointersInRoom(roomname, selected_msgs, num_selected, 1, NULL);
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

	for (i = 0; i < IMAP->num_msgs; ++i) {
		if (IMAP->flags[i] & IMAP_SELECTED) {
			if (IMAP->flags[i] & IMAP_SEEN) {
				seen_yes[num_seen_yes++] = IMAP->msgids[i];
			}
			if ((IMAP->flags[i] & IMAP_SEEN) == 0) {
				seen_no[num_seen_no++] = IMAP->msgids[i];
			}
			if (IMAP->flags[i] & IMAP_ANSWERED) {
				answ_yes[num_answ_yes++] = IMAP->msgids[i];
			}
			if ((IMAP->flags[i] & IMAP_ANSWERED) == 0) {
				answ_no[num_answ_no++] = IMAP->msgids[i];
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
	int i;
	int num_output = 0;
  
	for (i = 0; i < IMAP->num_msgs; ++i) {
		if (IMAP->flags[i] & IMAP_SELECTED) {
			++num_output;
			if (num_output == 1) {
				cprintf("[COPYUID ");
			}
			else if (num_output > 1) {
				cprintf(",");
			}
			cprintf("%ld", IMAP->msgids[i]);
		}
	}
	if (num_output > 0) {
		cprintf("] ");
	}
}


/*
 * This function is called by the main command loop.
 */
void imap_copy(int num_parms, char *parms[]) {
	int ret;

	if (num_parms != 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[2])) {
		imap_pick_range(parms[2], 0);
	}
	else {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	ret = imap_do_copy(parms[3]);
	if (!ret) {
		cprintf("%s OK ", parms[0]);
		imap_output_copyuid_response();
		cprintf("COPY completed\r\n");
	}
	else {
		cprintf("%s NO COPY failed (error %d)\r\n", parms[0], ret);
	}
}

/*
 * This function is called by the main command loop.
 */
void imap_uidcopy(int num_parms, char *parms[]) {

	if (num_parms != 5) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_is_message_set(parms[3])) {
		imap_pick_range(parms[3], 1);
	}
	else {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	if (imap_do_copy(parms[4]) == 0) {
		cprintf("%s OK ", parms[0]);
		imap_output_copyuid_response();
		cprintf("UID COPY completed\r\n");
	}
	else {
		cprintf("%s NO UID COPY failed\r\n", parms[0]);
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
void imap_append(int num_parms, char *parms[]) {
	long literal_length;
	long bytes_transferred;
	long stripped_length = 0;
	struct CtdlMessage *msg = NULL;
	long new_msgnum = (-1L);
	int ret = 0;
	char roomname[ROOMNAMELEN];
	char buf[SIZ];
	char savedroom[ROOMNAMELEN];
	int msgs, new;
	int i;
	char new_message_flags[SIZ];
	struct citimap *Imap;

	if (num_parms < 4) {
		cprintf("%s BAD usage error\r\n", parms[0]);
		return;
	}

	if ( (parms[num_parms-1][0] != '{')
	   || (parms[num_parms-1][strlen(parms[num_parms-1])-1] != '}') )  {
		cprintf("%s BAD no message literal supplied\r\n", parms[0]);
		return;
	}

	strcpy(new_message_flags, "");
	if (num_parms >= 5) {
		for (i=3; i<num_parms; ++i) {
			strcat(new_message_flags, parms[i]);
			strcat(new_message_flags, " ");
		}
		stripallbut(new_message_flags, '(', ')');
	}

	/* This is how we'd do this if it were relevant in our data store.
	 * if (num_parms >= 6) {
	 *  new_message_internaldate = parms[4];
	 * }
	 */

	literal_length = atol(&parms[num_parms-1][1]);
	if (literal_length < 1) {
		cprintf("%s BAD Message length must be at least 1.\r\n",
			parms[0]);
		return;
	}

	Imap = IMAP;
	imap_free_transmitted_message();	/* just in case. */
	Imap->transmitted_message = malloc(literal_length + 1);
	if (Imap->transmitted_message == NULL) {
		cprintf("%s NO Cannot allocate memory.\r\n", parms[0]);
		return;
	}
	Imap->transmitted_length = literal_length;

	cprintf("+ Transmit message now.\r\n");

	bytes_transferred = 0;

	ret = client_read(Imap->transmitted_message, literal_length);
	Imap->transmitted_message[literal_length] = 0;

	if (ret != 1) {
		cprintf("%s NO Read failed.\r\n", parms[0]);
		return;
	}

	/* Client will transmit a trailing CRLF after the literal (the message
	 * text) is received.  This call to client_getln() absorbs it.
	 */
	flush_output();
	client_getln(buf, sizeof buf);

	/* Convert RFC822 newlines (CRLF) to Unix newlines (LF) */
	CtdlLogPrintf(CTDL_DEBUG, "Converting CRLF to LF\n");
	stripped_length = 0;
	for (i=0; i<literal_length; ++i) {
		if (strncmp(&Imap->transmitted_message[i], "\r\n", 2)) {
			Imap->transmitted_message[stripped_length++] =
				Imap->transmitted_message[i];
		}
	}
	literal_length = stripped_length;
	Imap->transmitted_message[literal_length] = 0;	/* reterminate it */

	CtdlLogPrintf(CTDL_DEBUG, "Converting message format\n");
	msg = convert_internet_message(Imap->transmitted_message);
	Imap->transmitted_message = NULL;
	Imap->transmitted_length = 0;

	ret = imap_grabroom(roomname, parms[2], 1);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * CtdlUserGoto() formally takes us to the desired room.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (Imap->selected) {
		strcpy(savedroom, CC->room.QRname);
	}
	CtdlUserGoto(roomname, 0, 0, &msgs, &new);

	/* If the user is locally authenticated, FORCE the From: header to
	 * show up as the real sender.  FIXME do we really want to do this?
	 * Probably should make it site-definable or even room-definable.
	 *
	 * For now, we allow "forgeries" if the room is one of the user's
	 * private mailboxes.
	 */
	if (CC->logged_in) {
	   if ( ((CC->room.QRflags & QR_MAILBOX) == 0) && (config.c_imap_keep_from == 0)) {
		if (msg->cm_fields['A'] != NULL) free(msg->cm_fields['A']);
		if (msg->cm_fields['N'] != NULL) free(msg->cm_fields['N']);
		if (msg->cm_fields['H'] != NULL) free(msg->cm_fields['H']);
		msg->cm_fields['A'] = strdup(CC->user.fullname);
		msg->cm_fields['N'] = strdup(config.c_nodename);
		msg->cm_fields['H'] = strdup(config.c_humannode);
	    }
	}

	/* 
	 * Can we post here?
	 */
	ret = CtdlDoIHavePermissionToPostInThisRoom(buf, sizeof buf, NULL, POST_LOGGED_IN);

	if (ret) {
		/* Nope ... print an error message */
		cprintf("%s NO %s\r\n", parms[0], buf);
	}

	else {
		/* Yes ... go ahead and post! */
		if (msg != NULL) {
			new_msgnum = CtdlSubmitMsg(msg, NULL, "", 0);
		}
		if (new_msgnum >= 0L) {
			cprintf("%s OK [APPENDUID %ld %ld] APPEND completed\r\n",
				parms[0], GLOBAL_UIDVALIDITY_VALUE, new_msgnum);
		}
		else {
			cprintf("%s BAD Error %ld saving message to disk.\r\n",
				parms[0], new_msgnum);
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
	CtdlFreeMessage(msg);

	if (new_message_flags != NULL) {
		imap_do_append_flags(new_msgnum, new_message_flags);
	}
}
