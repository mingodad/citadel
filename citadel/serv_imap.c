/*
 * $Id$ 
 *
 * IMAP server for the Citadel/UX system
 * Copyright (C) 2000-2001 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * WARNING: this is an incomplete implementation.  It is now good enough to
 * be usable with much of the popular IMAP client software available, but it
 * is by no means perfect.  Some commands (particularly SEARCH and RENAME)
 * are implemented either incompletely or not at all.
 *
 * WARNING: Mark Crispin is an idiot.  IMAP is the most brain-damaged protocol
 * you will ever have the profound lack of pleasure to encounter.
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
#include "dynloader.h"
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
#include "imap_misc.h"


long SYM_IMAP;


/*
 * If there is a message ID map in memory, free it
 */
void imap_free_msgids(void) {
	if (IMAP->msgids != NULL) {
		phree(IMAP->msgids);
		IMAP->msgids = NULL;
		IMAP->num_msgs = 0;
	}
	if (IMAP->flags != NULL) {
		phree(IMAP->flags);
		IMAP->flags = NULL;
	}
}


/*
 * If there is a transmitted message in memory, free it
 */
void imap_free_transmitted_message(void) {
	if (IMAP->transmitted_message != NULL) {
		phree(IMAP->transmitted_message);
		IMAP->transmitted_message = NULL;
		IMAP->transmitted_length = 0;
	}
}


/*
 * Set the \\Seen flag for messages which aren't new
 */
void imap_set_seen_flags(void) {
	struct visit vbuf;
	int i;

	CtdlGetRelationship(&vbuf, &CC->usersupp, &CC->quickroom);
	if (IMAP->num_msgs > 0) {
		for (i=0; i<IMAP->num_msgs; ++i) {
			if (is_msg_in_mset(vbuf.v_seen, IMAP->msgids[i])) {
				IMAP->flags[i] |= IMAP_SEEN;
			}
		}
	}
}




/*
 * Back end for imap_load_msgids()
 *
 * Optimization: instead of calling realloc() to add each message, we
 * allocate space in the list for REALLOC_INCREMENT messages at a time.  This
 * allows the mapping to proceed much faster.
 */
void imap_add_single_msgid(long msgnum, void *userdata) {
	
	IMAP->num_msgs = IMAP->num_msgs + 1;
	if (IMAP->msgids == NULL) {
		IMAP->msgids = mallok(IMAP->num_msgs * sizeof(long)
					* REALLOC_INCREMENT);
	}
	else if (IMAP->num_msgs % REALLOC_INCREMENT == 0) {
		IMAP->msgids = reallok(IMAP->msgids,
			(IMAP->num_msgs + REALLOC_INCREMENT) * sizeof(long));
	}
	if (IMAP->flags == NULL) {
		IMAP->flags = mallok(IMAP->num_msgs * sizeof(long)
					* REALLOC_INCREMENT);
	}
	else if (IMAP->num_msgs % REALLOC_INCREMENT == 0) {
		IMAP->flags = reallok(IMAP->flags,
			(IMAP->num_msgs + REALLOC_INCREMENT) * sizeof(long));
	}
	IMAP->msgids[IMAP->num_msgs - 1] = msgnum;
	IMAP->flags[IMAP->num_msgs - 1] = 0;
}



/*
 * Set up a message ID map for the current room (folder)
 */
void imap_load_msgids(void) {
	 
	if (IMAP->selected == 0) {
		lprintf(5, "imap_load_msgids() can't run; no room selected\n");
		return;
	}

	imap_free_msgids();	/* If there was already a map, free it */

	CtdlForEachMessage(MSGS_ALL, 0L, (-63), NULL, NULL,
		imap_add_single_msgid, NULL);

	imap_set_seen_flags();
	lprintf(9, "imap_load_msgids() mapped %d messages\n", IMAP->num_msgs);
}


/*
 * Re-scan the selected room (folder) and see if it's been changed at all
 */
void imap_rescan_msgids(void) {

	int original_num_msgs = 0;
	long original_highest = 0L;
	int i;
	int count;

	if (IMAP->selected == 0) {
		lprintf(5, "imap_load_msgids() can't run; no room selected\n");
		return;
	}


	/*
	 * Check to see if any of the messages we know about have been expunged
	 */
	if (IMAP->num_msgs > 0)
	 for (i=0; i<IMAP->num_msgs; ++i) {

		count = CtdlForEachMessage(MSGS_EQ, IMAP->msgids[i],
			(-63), NULL, NULL, NULL, NULL);

		if (count == 0) {
			cprintf("* %d EXPUNGE\r\n", i+1);

			/* Here's some nice stupid nonsense.  When a message
			 * is expunged, we have to slide all the existing
			 * messages up in the message array.
			 */
			--IMAP->num_msgs;
			memcpy(&IMAP->msgids[i], &IMAP->msgids[i+1],
				(sizeof(long)*(IMAP->num_msgs-i)) );
			memcpy(&IMAP->flags[i], &IMAP->flags[i+1],
				(sizeof(long)*(IMAP->num_msgs-i)) );

			--i;
		}

	}

	/*
	 * Remember how many messages were here before we re-scanned.
	 */
	original_num_msgs = IMAP->num_msgs;
	if (IMAP->num_msgs > 0) {
		original_highest = IMAP->msgids[IMAP->num_msgs - 1];
	}
	else {
		original_highest = 0L;
	}

	/*
	 * Now peruse the room for *new* messages only.
	 */
	CtdlForEachMessage(MSGS_GT, original_highest, (-63), NULL, NULL,
		imap_add_single_msgid, NULL);

	imap_set_seen_flags();

	/*
	 * If new messages have arrived, tell the client about them.
	 */
	if (IMAP->num_msgs > original_num_msgs) {
		cprintf("* %d EXISTS\r\n", IMAP->num_msgs);
	}

}







/*
 * This cleanup function blows away the temporary memory and files used by
 * the IMAP server.
 */
void imap_cleanup_function(void) {

	/* Don't do this stuff if this is not a IMAP session! */
	if (CC->h_command_function != imap_command_loop) return;

	lprintf(9, "Performing IMAP cleanup hook\n");
	imap_free_msgids();
	imap_free_transmitted_message();
	lprintf(9, "Finished IMAP cleanup hook\n");
}



/*
 * Here's where our IMAP session begins its happy day.
 */
void imap_greeting(void) {

	strcpy(CC->cs_clientname, "IMAP session");
	CtdlAllocUserData(SYM_IMAP, sizeof(struct citimap));
	IMAP->authstate = imap_as_normal;

	cprintf("* OK %s Citadel/UX IMAP4rev1 server ready\r\n",
		config.c_fqdn);
}


/*
 * implements the LOGIN command (ordinary username/password login)
 */
void imap_login(int num_parms, char *parms[]) {
	if (CtdlLoginExistingUser(parms[2]) == login_ok) {
		if (CtdlTryPassword(parms[3]) == pass_ok) {
			cprintf("%s OK login successful\r\n", parms[0]);
			return;
		}
        }

	cprintf("%s BAD Login incorrect\r\n", parms[0]);
}


/*
 * Implements the AUTHENTICATE command
 */
void imap_authenticate(int num_parms, char *parms[]) {
	char buf[SIZ];

	if (num_parms != 3) {
		cprintf("%s BAD incorrect number of parameters\r\n", parms[0]);
		return;
	}

	if (!strcasecmp(parms[2], "LOGIN")) {
		encode_base64(buf, "Username:");
		cprintf("+ %s\r\n", buf);
		IMAP->authstate = imap_as_expecting_username;
		strcpy(IMAP->authseq, parms[0]);
		return;
	}

	else {
		cprintf("%s NO AUTHENTICATE %s failed\r\n",
			parms[0], parms[1]);
	}
}

void imap_auth_login_user(char *cmd) {
	char buf[SIZ];

	decode_base64(buf, cmd, SIZ);
	CtdlLoginExistingUser(buf);
	encode_base64(buf, "Password:");
	cprintf("+ %s\r\n", buf);
	IMAP->authstate = imap_as_expecting_password;
	return;
}

void imap_auth_login_pass(char *cmd) {
	char buf[SIZ];

	decode_base64(buf, cmd, SIZ);
	if (CtdlTryPassword(buf) == pass_ok) {
		cprintf("%s OK authentication succeeded\r\n", IMAP->authseq);
	}
	else {
		cprintf("%s NO authentication failed\r\n", IMAP->authseq);
	}
	IMAP->authstate = imap_as_normal;
	return;
}



/*
 * implements the CAPABILITY command
 */
void imap_capability(int num_parms, char *parms[]) {
	cprintf("* CAPABILITY IMAP4 IMAP4REV1 AUTH=LOGIN\r\n");
	cprintf("%s OK CAPABILITY completed\r\n", parms[0]);
}





/*
 * implements the SELECT command
 */
void imap_select(int num_parms, char *parms[]) {
	char towhere[SIZ];
	char augmented_roomname[ROOMNAMELEN];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct quickroom QRscratch;
	int msgs, new;
	int floornum;
	int roomflags;
	int i;

	/* Convert the supplied folder name to a roomname */
	i = imap_roomname(towhere, sizeof towhere, parms[2]);
	if (i < 0) {
		cprintf("%s NO Invalid mailbox name.\r\n", parms[0]);
		IMAP->selected = 0;
		return;
	}
	floornum = (i & 0x00ff);
	roomflags = (i & 0xff00);

        /* First try a regular match */
        c = getroom(&QRscratch, towhere);

        /* Then try a mailbox name match */
        if (c != 0) {
                MailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->usersupp, towhere);
                c = getroom(&QRscratch, augmented_roomname);
                if (c == 0)
                        strcpy(towhere, augmented_roomname);
        }

	/* If the room exists, check security/access */
        if (c == 0) {
                /* See if there is an existing user/room relationship */
                ra = CtdlRoomAccess(&QRscratch, &CC->usersupp);

                /* normal clients have to pass through security */
                if (ra & UA_KNOWN) {
                        ok = 1;
		}
	}

	/* Fail here if no such room */
	if (!ok) {
		cprintf("%s NO ... no such room, or access denied\r\n",
			parms[0]);
		IMAP->selected = 0;
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.
	 */
	usergoto(QRscratch.QRname, 0, &msgs, &new);
	IMAP->selected = 1;

	if (!strcasecmp(parms[1], "EXAMINE")) {
		IMAP->readonly = 1;
	}
	else {
		IMAP->readonly = 0;
	}

	imap_load_msgids();

	/* FIXME ... much more info needs to be supplied here */
	cprintf("* %d EXISTS\r\n", msgs);
	cprintf("* %d RECENT\r\n", new);
	cprintf("* FLAGS (\\Deleted \\Seen)\r\n");
	cprintf("* OK [PERMANENTFLAGS (\\Deleted \\Seen)] permanent flags\r\n");
	cprintf("* OK [UIDVALIDITY 0] UIDs valid\r\n");
	cprintf("%s OK [%s] %s completed\r\n",
		parms[0],
		(IMAP->readonly ? "READ-ONLY" : "READ-WRITE"),
		parms[1]);
}



/*
 * does the real work for expunge
 */
int imap_do_expunge(void) {
	int i;
	int num_expunged = 0;

	if (IMAP->num_msgs > 0) for (i=0; i<IMAP->num_msgs; ++i) {
		if (IMAP->flags[i] & IMAP_DELETED) {
			CtdlDeleteMessages(CC->quickroom.QRname,
					IMAP->msgids[i], "");
			++num_expunged;
		}
	}

	if (num_expunged > 0) {
		imap_rescan_msgids();
	}

	return(num_expunged);
}


/*
 * implements the EXPUNGE command syntax
 */
void imap_expunge(int num_parms, char *parms[]) {
	int num_expunged = 0;
	imap_do_expunge();
	cprintf("%s OK expunged %d messages.\r\n", parms[0], num_expunged);
}


/*
 * implements the CLOSE command
 */
void imap_close(int num_parms, char *parms[]) {

	/* Yes, we always expunge on close. */
	imap_do_expunge();

	IMAP->selected = 0;
	IMAP->readonly = 0;
	imap_free_msgids();
	cprintf("%s OK CLOSE completed\r\n", parms[0]);
}




/*
 * Used by LIST and LSUB to show the floors in the listing
 */
void imap_list_floors(char *cmd, char *pattern) {
	int i;
	struct floor *fl;

	for (i=0; i<MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			if (imap_mailbox_matches_pattern(pattern, fl->f_name)) {
				cprintf("* %s (\\NoSelect) \"|\" ", cmd);
				imap_strout(fl->f_name);
				cprintf("\r\n");
			}
		}
	}
}



/*
 * Back end for imap_lsub()
 *
 * IMAP "subscribed folder" is equivocated to Citadel "known rooms."  This
 * may or may not be the desired behavior in the future.
 */
void imap_lsub_listroom(struct quickroom *qrbuf, void *data) {
	char buf[SIZ];
	int ra;
	char *pattern;

	pattern = (char *)data;

	/* Only list rooms to which the user has access!! */
	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if (ra & UA_KNOWN) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		if (imap_mailbox_matches_pattern(pattern, buf)) {
			cprintf("* LSUB () \"|\" ");
			imap_strout(buf);
			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LSUB command
 */
void imap_lsub(int num_parms, char *parms[]) {
	char pattern[SIZ];
	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}
	snprintf(pattern, sizeof pattern, "%s%s", parms[2], parms[3]);

	if (strlen(parms[3])==0) {
		cprintf("* LIST (\\Noselect) \"|\" \"\"\r\n");
	}

	else {
		imap_list_floors("LSUB", pattern);
		ForEachRoom(imap_lsub_listroom, pattern);
	}

	cprintf("%s OK LSUB completed\r\n", parms[0]);
}



/*
 * Back end for imap_list()
 */
void imap_list_listroom(struct quickroom *qrbuf, void *data) {
	char buf[SIZ];
	int ra;
	char *pattern;

	pattern = (char *)data;

	/* Only list rooms to which the user has access!! */
	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if ( (ra & UA_KNOWN) 
	  || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		if (imap_mailbox_matches_pattern(pattern, buf)) {
			cprintf("* LIST () \"|\" ");
			imap_strout(buf);
			cprintf("\r\n");
		}
	}
}


/*
 * Implements the LIST command
 */
void imap_list(int num_parms, char *parms[]) {
	char pattern[SIZ];
	if (num_parms < 4) {
		cprintf("%s BAD arguments invalid\r\n", parms[0]);
		return;
	}
	snprintf(pattern, sizeof pattern, "%s%s", parms[2], parms[3]);

	if (strlen(parms[3])==0) {
		cprintf("* LIST (\\Noselect) \"|\" \"\"\r\n");
	}

	else {
		imap_list_floors("LIST", pattern);
		ForEachRoom(imap_list_listroom, pattern);
	}

	cprintf("%s OK LIST completed\r\n", parms[0]);
}



/*
 * Implements the CREATE command
 *
 */
void imap_create(int num_parms, char *parms[]) {
	int ret;
	char roomname[ROOMNAMELEN];
	int floornum;
	int flags;
	int newroomtype;

	ret = imap_roomname(roomname, sizeof roomname, parms[2]);
	if (ret < 0) {
		cprintf("%s NO Invalid mailbox name or location\r\n",
			parms[0]);
		return;
	}
	floornum = ( ret & 0x00ff );	/* lower 8 bits = floor number */
	flags =    ( ret & 0xff00 );	/* upper 8 bits = flags        */

	if (flags & IR_MAILBOX) {
		newroomtype = 4;	/* private mailbox */
	}
	else {
		newroomtype = 0;	/* public folder */
	}

	lprintf(7, "Create new room <%s> on floor <%d> with type <%d>\n",
		roomname, floornum, newroomtype);

	ret = create_room(roomname, newroomtype, "", floornum, 1);
	if (ret == 0) {
		cprintf("%s NO Mailbox already exists, or create failed\r\n",
			parms[0]);
	}
	else {
		cprintf("%s OK CREATE completed\r\n", parms[0]);
	}
}


/*
 * Locate a room by its IMAP folder name, and check access to it
 */
int imap_grabroom(char *returned_roomname, char *foldername) {
	int ret;
	char augmented_roomname[ROOMNAMELEN];
	char roomname[ROOMNAMELEN];
	int c;
	struct quickroom QRscratch;
	int ra;
	int ok = 0;

	ret = imap_roomname(roomname, sizeof roomname, foldername);
	if (ret < 0) {
		return(1);
	}

        /* First try a regular match */
        c = getroom(&QRscratch, roomname);

        /* Then try a mailbox name match */
        if (c != 0) {
                MailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->usersupp, roomname);
                c = getroom(&QRscratch, augmented_roomname);
                if (c == 0)
                        strcpy(roomname, augmented_roomname);
        }

	/* If the room exists, check security/access */
        if (c == 0) {
                /* See if there is an existing user/room relationship */
                ra = CtdlRoomAccess(&QRscratch, &CC->usersupp);

                /* normal clients have to pass through security */
                if (ra & UA_KNOWN) {
                        ok = 1;
		}
	}

	/* Fail here if no such room */
	if (!ok) {
		strcpy(returned_roomname, "");
		return(2);
	}
	else {
		strcpy(returned_roomname, QRscratch.QRname);
		return(0);
	}
}


/*
 * Implements the STATUS command (sort of)
 *
 */
void imap_status(int num_parms, char *parms[]) {
	int ret;
	char roomname[ROOMNAMELEN];
	char buf[SIZ];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2]);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or location, or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->quickroom.QRname);
	}
	usergoto(roomname, 0, &msgs, &new);

	/*
	 * Tell the client what it wants to know.  In fact, tell it *more* than
	 * it wants to know.  We happily IGnore the supplied status data item
	 * names and simply spew all possible data items.  It's far easier to
	 * code and probably saves us some processing time too.
	 */
	imap_mailboxname(buf, sizeof buf, &CC->quickroom);
	cprintf("* STATUS ");
	imap_strout(buf);
	cprintf(" (MESSAGES %d ", msgs);
	cprintf("RECENT 0 ");	/* FIXME we need to implement this */
	cprintf("UIDNEXT %ld ", CitControl.MMhighest + 1);
	cprintf("UNSEEN %d)\r\n", new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, &msgs, &new);
	}

	/*
	 * Oooh, look, we're done!
	 */
	cprintf("%s OK STATUS completed\r\n", parms[0]);
}



/*
 * Implements the SUBSCRIBE command
 *
 */
void imap_subscribe(int num_parms, char *parms[]) {
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2]);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or location, or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, which has the side
	 * effect of marking the room as not-zapped ... exactly the effect
	 * we're looking for.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->quickroom.QRname);
	}
	usergoto(roomname, 0, &msgs, &new);

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, &msgs, &new);
	}

	cprintf("%s OK SUBSCRIBE completed\r\n", parms[0]);
}


/*
 * Implements the UNSUBSCRIBE command
 *
 */
void imap_unsubscribe(int num_parms, char *parms[]) {
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2]);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name or location, or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room.
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->quickroom.QRname);
	}
	usergoto(roomname, 0, &msgs, &new);

	/* 
	 * Now make the API call to zap the room
	 */
	if (CtdlForgetThisRoom() == 0) {
		cprintf("%s OK UNSUBSCRIBE completed\r\n", parms[0]);
	}
	else {
		cprintf("%s NO You may not unsubscribe from this folder.\r\n",
			parms[0]);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, &msgs, &new);
	}
}



/*
 * Implements the DELETE command
 *
 */
void imap_delete(int num_parms, char *parms[]) {
	int ret;
	char roomname[ROOMNAMELEN];
	char savedroom[ROOMNAMELEN];
	int msgs, new;

	ret = imap_grabroom(roomname, parms[2]);
	if (ret != 0) {
		cprintf("%s NO Invalid mailbox name, or access denied\r\n",
			parms[0]);
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.  (If another
	 * folder is selected, save its name so we can return there!!!!!)
	 */
	if (IMAP->selected) {
		strcpy(savedroom, CC->quickroom.QRname);
	}
	usergoto(roomname, 0, &msgs, &new);

	/*
	 * Now delete the room.
	 */
	if (CtdlDoIHavePermissionToDeleteThisRoom(&CC->quickroom)) {
		cprintf("%s OK DELETE completed\r\n", parms[0]);
		delete_room(&CC->quickroom);
	}
	else {
		cprintf("%s NO Can't delete this folder.\r\n", parms[0]);
	}

	/*
	 * If another folder is selected, go back to that room so we can resume
	 * our happy day without violent explosions.
	 */
	if (IMAP->selected) {
		usergoto(savedroom, 0, &msgs, &new);
	}
}




/*
 * Back end function for imap_rename()
 */
void imap_rename_backend(struct quickroom *qrbuf, void *data) {
	struct irr_t { char *old_path; char *new_path; };
	struct irr_t *irr = (struct irr_t *)data;
	char foldername[SIZ];
	char newfoldername[SIZ];
	char newroomname[ROOMNAMELEN];
	int newfloor;
	int r;

	imap_mailboxname(foldername, sizeof foldername, qrbuf);

	if ( (!strncasecmp(foldername, irr->old_path, strlen(irr->old_path)) 
	   && (foldername[strlen(irr->old_path)] == '|')) ) {

		sprintf(newfoldername, "%s|%s",
			irr->new_path,
			&foldername[strlen(irr->old_path)+1]
		);

		newfloor = imap_roomname(newroomname,
				sizeof newroomname, newfoldername);

		r = CtdlRenameRoom(qrbuf->QRname, newroomname, newfloor);
		/* FIXME handle error returns */
	}
}




/*
 * Implements the RENAME command
 *
 */
void imap_rename(int num_parms, char *parms[]) {
	char old_room[ROOMNAMELEN];
	char new_room[ROOMNAMELEN];
	int oldr, newr;
	int new_floor;
	int r;

	struct irr_t { char *old_path; char *new_path; };
	struct irr_t irr = { parms[2], parms[3] };

	oldr = imap_roomname(old_room, sizeof old_room, parms[2]);
	newr = imap_roomname(new_room, sizeof new_room, parms[3]);
	new_floor = (newr & 0xFF);

	r = CtdlRenameRoom(old_room, new_room, new_floor);

	if (r == crr_room_not_found) {
		cprintf("%s NO Could not locate this folder\r\n", parms[0]);
		return;
	}
	if (r == crr_already_exists) {
		cprintf("%s '%s' already exists.\r\n", parms[0], parms[2]);
		return;
	}
	if (r == crr_noneditable) {
		cprintf("%s This folder is not editable.\r\n", parms[0]);
		return;
	}
	if (r == crr_invalid_floor) {
		cprintf("%s Folder root does not exist.\r\n", parms[0]);
		return;
	}
	if (r == crr_access_denied) {
		cprintf("%s You do not have permission to edit "
			"this folder.\r\n", parms[0]);
		return;
	}
	if (r != crr_ok) {
		cprintf("%s NO Rename failed - undefined error %d\r\n",
			parms[0], r);
		return;
	}

	/* FIXME supply something */
	ForEachRoom(imap_rename_backend, (void *)&irr );

	cprintf("%s OK RENAME completed\r\n", parms[0]);
}




/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void) {
	char cmdbuf[SIZ];
	char *parms[SIZ];
	int num_parms;

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "IMAP socket is broken.  Ending session.\r\n");
		CC->kill_me = 1;
		return;
	}

	lprintf(5, "IMAP: %s\r\n", cmdbuf);
	while (strlen(cmdbuf) < 5) strcat(cmdbuf, " ");


	/* strip off l/t whitespace and CRLF */
	if (cmdbuf[strlen(cmdbuf)-1]=='\n') cmdbuf[strlen(cmdbuf)-1]=0;
	if (cmdbuf[strlen(cmdbuf)-1]=='\r') cmdbuf[strlen(cmdbuf)-1]=0;
	striplt(cmdbuf);

	/* If we're in the middle of a multi-line command, handle that */
	if (IMAP->authstate == imap_as_expecting_username) {
		imap_auth_login_user(cmdbuf);
		return;
	}
	if (IMAP->authstate == imap_as_expecting_password) {
		imap_auth_login_pass(cmdbuf);
		return;
	}


	/* Ok, at this point we're in normal command mode.  The first thing
	 * we do is print any incoming pages (yeah! we really do!)
	 */
	imap_print_express_messages();

	/*
	 * Before processing the command that was just entered... if we happen
	 * to have a folder selected, we'd like to rescan that folder for new
	 * messages, and for deletions/changes of existing messages.  This
	 * could probably be optimized somehow, but IMAP sucks...
	 */
	if (IMAP->selected) {
		imap_rescan_msgids();
	}

	/* Now for the command set. */

	/* Grab the tag, command, and parameters.  Check syntax. */
	num_parms = imap_parameterize(parms, cmdbuf);
	if (num_parms < 2) {
		cprintf("BAD syntax error\r\n");
	}

	/* The commands below may be executed in any state */

	else if ( (!strcasecmp(parms[1], "NOOP"))
	   || (!strcasecmp(parms[1], "CHECK")) ) {
		cprintf("%s OK This command successfully did nothing.\r\n",
			parms[0]);
	}

	else if (!strcasecmp(parms[1], "LOGOUT")) {
		cprintf("* BYE %s logging out\r\n", config.c_fqdn);
		cprintf("%s OK thank you for using Citadel IMAP\r\n", parms[0]);
		CC->kill_me = 1;
		return;
	}

	else if (!strcasecmp(parms[1], "LOGIN")) {
		imap_login(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "AUTHENTICATE")) {
		imap_authenticate(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CAPABILITY")) {
		imap_capability(num_parms, parms);
	}

	else if (!CC->logged_in) {
		cprintf("%s BAD Not logged in.\r\n", parms[0]);
	}

	/* The commans below require a logged-in state */

	else if (!strcasecmp(parms[1], "SELECT")) {
		imap_select(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "EXAMINE")) {
		imap_select(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "LSUB")) {
		imap_lsub(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "LIST")) {
		imap_list(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CREATE")) {
		imap_create(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "DELETE")) {
		imap_delete(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "RENAME")) {
		imap_rename(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "STATUS")) {
		imap_status(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "SUBSCRIBE")) {
		imap_subscribe(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "UNSUBSCRIBE")) {
		imap_unsubscribe(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "APPEND")) {
		imap_append(num_parms, parms);
	}

	else if (IMAP->selected == 0) {
		cprintf("%s BAD no folder selected\r\n", parms[0]);
	}

	/* The commands below require the SELECT state on a mailbox */

	else if (!strcasecmp(parms[1], "FETCH")) {
		imap_fetch(num_parms, parms);
	}

	else if ( (!strcasecmp(parms[1], "UID"))
		&& (!strcasecmp(parms[2], "FETCH")) ) {
		imap_uidfetch(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "SEARCH")) {
		imap_search(num_parms, parms);
	}

	else if ( (!strcasecmp(parms[1], "UID"))
		&& (!strcasecmp(parms[2], "SEARCH")) ) {
		imap_uidsearch(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "STORE")) {
		imap_store(num_parms, parms);
	}

	else if ( (!strcasecmp(parms[1], "UID"))
		&& (!strcasecmp(parms[2], "STORE")) ) {
		imap_uidstore(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "COPY")) {
		imap_copy(num_parms, parms);
	}

	else if ( (!strcasecmp(parms[1], "UID"))
		&& (!strcasecmp(parms[2], "COPY")) ) {
		imap_uidcopy(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "EXPUNGE")) {
		imap_expunge(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CLOSE")) {
		imap_close(num_parms, parms);
	}

	/* End of commands.  If we get here, the command is either invalid
	 * or unimplemented.
	 */

	else {
		cprintf("%s BAD command unrecognized\r\n", parms[0]);
	}

	/* If the client transmitted a message we can free it now */
	imap_free_transmitted_message();
}



/*
 * This function is called by dynloader.c to register the IMAP module
 * with the Citadel server.
 */
char *Dynamic_Module_Init(void)
{
	SYM_IMAP = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(config.c_imap_port,
				NULL,
				imap_greeting,
				imap_command_loop);
	CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP);
	return "$Id$";
}
