/*
 * $Id$ 
 *
 * IMAP server for the Citadel/UX system
 * Copyright (C) 2000-2001 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * WARNING: this is an incomplete implementation, still in progress.  Parts of
 * it work, but it's not really usable yet from a user perspective.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <ctype.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
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
 * Back end for imap_load_msgids()
 *
 * FIXME: this should be optimized by figuring out a way to allocate memory
 * once rather than doing a reallok() for each message.
 */
void imap_add_single_msgid(long msgnum, void *userdata) {
	
	IMAP->num_msgs = IMAP->num_msgs + 1;
	if (IMAP->msgids == NULL) {
		IMAP->msgids = mallok(IMAP->num_msgs * sizeof(long));
	}
	else {
		IMAP->msgids = reallok(IMAP->msgids,
			IMAP->num_msgs * sizeof(long));
	}
	if (IMAP->flags == NULL) {
		IMAP->flags = mallok(IMAP->num_msgs * sizeof(long));
	}
	else {
		IMAP->flags = reallok(IMAP->flags,
			IMAP->num_msgs * sizeof(long));
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

	decode_base64(buf, cmd);
	CtdlLoginExistingUser(buf);
	encode_base64(buf, "Password:");
	cprintf("+ %s\r\n", buf);
	IMAP->authstate = imap_as_expecting_password;
	return;
}

void imap_auth_login_pass(char *cmd) {
	char buf[SIZ];

	decode_base64(buf, cmd);
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
                MailboxName(augmented_roomname, &CC->usersupp, towhere);
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
	cprintf("* FLAGS (\\Deleted)\r\n");
	cprintf("* OK [PERMANENTFLAGS (\\Deleted)] permanent flags\r\n");
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
void imap_list_floors(char *cmd) {
	int i;
	struct floor *fl;

	for (i=0; i<MAXFLOORS; ++i) {
		fl = cgetfloor(i);
		if (fl->f_flags & F_INUSE) {
			cprintf("* %s (\\NoSelect) \"|\" ", cmd);
			imap_strout(fl->f_name);
			cprintf("\r\n");
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

	/* Only list rooms to which the user has access!! */
	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if (ra & UA_KNOWN) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		cprintf("* LSUB () \"|\" ");
		imap_strout(buf);
		cprintf("\r\n");
	}
}


/*
 * Implements the LSUB command
 *
 * FIXME: Handle wildcards, please.
 */
void imap_lsub(int num_parms, char *parms[]) {
	imap_list_floors("LSUB");
	ForEachRoom(imap_lsub_listroom, NULL);
	cprintf("%s OK LSUB completed\r\n", parms[0]);
}



/*
 * Back end for imap_list()
 */
void imap_list_listroom(struct quickroom *qrbuf, void *data) {
	char buf[SIZ];
	int ra;

	/* Only list rooms to which the user has access!! */
	ra = CtdlRoomAccess(qrbuf, &CC->usersupp);
	if ( (ra & UA_KNOWN) 
	  || ((ra & UA_GOTOALLOWED) && (ra & UA_ZAPPED))) {
		imap_mailboxname(buf, sizeof buf, qrbuf);
		cprintf("* LIST () \"|\" ");
		imap_strout(buf);
		cprintf("\r\n");
	}
}


/*
 * Implements the LIST command
 *
 * FIXME: Handle wildcards, please.
 */
void imap_list(int num_parms, char *parms[]) {
	imap_list_floors("LIST");
	ForEachRoom(imap_list_listroom, NULL);
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
                MailboxName(augmented_roomname, &CC->usersupp, roomname);
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
	 *
	 * FIXME we need to implement RECENT and UNSEEN eventually...
	 */

	imap_mailboxname(buf, sizeof buf, &CC->quickroom);
	cprintf("* STATUS ");
	imap_strout(buf);
	cprintf(" (MESSAGES %d RECENT 0 UIDNEXT %ld "
		"UIDVALIDITY 0 UNSEEN 0)\r\n",
		msgs,
		CitControl.MMhighest + 1
	);

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

	lprintf(5, "citserver[%3d]: %s\r\n", CC->cs_pid, cmdbuf);
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


	/* Ok, at this point we're in normal command mode */

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

	else if (!strcasecmp(parms[1], "STATUS")) {
		imap_status(num_parms, parms);
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
