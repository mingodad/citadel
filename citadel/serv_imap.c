/*
 * $Id$ 
 *
 * IMAP server for the Citadel/UX system
 * Copyright (C) 2000 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * *** THIS IS UNDER DEVELOPMENT.  IT DOES NOT WORK.  DO NOT USE IT. ***
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
	IMAP->msgids[IMAP->num_msgs - 1] = msgnum;
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
	CC->internal_pgm = 1;
	CtdlAllocUserData(SYM_IMAP, sizeof(struct citimap));

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
	char towhere[256];
	char augmented_roomname[256];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct quickroom QRscratch;
	int msgs, new;

	strcpy(towhere, parms[2]);

	/* IMAP uses the reserved name "INBOX" for the user's default incoming
	 * mail folder.  Convert this to Citadel's reserved name "_MAIL_".
	 */
	if (!strcasecmp(towhere, "INBOX"))
		strcpy(towhere, MAILROOM);

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
                if (ra & UA_KNOWN)
                        ok = 1;
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
	cprintf("* OK [UIDVALIDITY 0] UIDs valid\r\n");
	cprintf("%s OK [%s] %s completed\r\n",
		parms[0],
		(IMAP->readonly ? "READ-ONLY" : "READ-WRITE"),
		parms[1]);
}



/*
 * implements the CLOSE command
 */
void imap_close(int num_parms, char *parms[]) {
	IMAP->selected = 0;
	IMAP->readonly = 0;
	imap_free_msgids();
	cprintf("%s OK CLOSE completed\r\n", parms[0]);
}





/*
 * Back end for imap_lsub()
 */
void imap_lsub_listroom(struct quickroom *qrbuf, void *data) {
	char buf[256];
	
	imap_mailboxname(buf, sizeof buf, qrbuf);
	cprintf("* LSUB () \"|\" \"%s\"\r\n", buf);
}


/*
 * Implements the LSUB command
 *
 * FIXME: Handle wildcards, please.
 * FIXME: Currently we show all rooms as subscribed folders.  Need to handle
 *        subscriptions properly.
 */
void imap_lsub(int num_parms, char *parms[]) {
	ForEachRoom(imap_lsub_listroom, NULL);
	cprintf("%s OK LSUB completed\r\n", parms[0]);
}



/*
 * Back end for imap_list()
 */
void imap_list_listroom(struct quickroom *qrbuf, void *data) {
	char buf[256];
	
	imap_mailboxname(buf, sizeof buf, qrbuf);
	cprintf("* LIST () \"|\" \"%s\"\r\n", buf);
}


/*
 * Implements the LIST command
 *
 * FIXME: Handle wildcards, please.
 */
void imap_list(int num_parms, char *parms[]) {
	ForEachRoom(imap_list_listroom, NULL);
	cprintf("%s OK LIST completed\r\n", parms[0]);
}


/*
 * Do the actual work for imap_fetch().  By the time this function is called,
 * the "lo" and "hi" sequence numbers are already validated, and the data item
 * names are, um... something.
 */
void imap_do_fetch(int lo, int hi, char *items) {
	cprintf("* lo=%d hi=%d items=<%s>\r\n", lo, hi, items);
}


/*
 * Mark Crispin is a fscking idiot.
 */
void imap_fetch(int num_parms, char *parms[]) {
	int lo = 0;
	int hi = 0;
	char lostr[1024], histr[1024], items[1024];
	int i;

	if (num_parms < 4) {
		cprintf("%s BAD invalid parameters\r\n", parms[0]);
		return;
	}

	extract_token(lostr, parms[2], 0, ':');
	lo = atoi(lostr);
	extract_token(histr, parms[2], 1, ':');
	hi = atoi(histr);

	if ( (lo < 1) || (hi < 1) || (lo > hi) || (hi > IMAP->num_msgs) ) {
		cprintf("%s BAD invalid sequence numbers %d:%d\r\n",
			parms[0], lo, hi);
		return;
	}

	strcpy(items, "");
	for (i=3; i<num_parms; ++i) {
		strcat(items, parms[i]);
		if (i < (num_parms-1)) strcat(items, " ");
	}
	for (i=0; i<strlen(items); ++i) {
		if (isspace(items[i])) items[i] = ' ';
	}

	imap_do_fetch(lo, hi, items);
	cprintf("%s OK FETCH completed\r\n", parms[0]);
}




/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void) {
	char cmdbuf[256];
	char *parms[16];
	int num_parms;
	int i;

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

	/* grab the tag */
	num_parms = imap_parameterize(parms, cmdbuf);
	for (i=0; i<num_parms; ++i) {
		lprintf(9, " parms[%d]='%s'\n", i, parms[i]);
	}

	/* commands which may be executed in any state */

	if ( (!strcasecmp(parms[1], "NOOP"))
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

	else if (!strcasecmp(parms[1], "CAPABILITY")) {
		imap_capability(num_parms, parms);
	}

	else if (!CC->logged_in) {
		cprintf("%s BAD Not logged in.\r\n", parms[0]);
	}

	/* commands requiring the client to be logged in */

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

	else if (IMAP->selected == 0) {
		cprintf("%s BAD no folder selected\r\n", parms[0]);
	}

	/* commands requiring the SELECT state */

	else if (!strcasecmp(parms[1], "FETCH")) {
		imap_fetch(num_parms, parms);
	}

	else if (!strcasecmp(parms[1], "CLOSE")) {
		imap_close(num_parms, parms);
	}

	/* end of commands */

	else {
		cprintf("%s BAD command unrecognized\r\n", parms[0]);
	}

}



char *Dynamic_Module_Init(void)
{
	SYM_IMAP = CtdlGetDynamicSymbol();
	CtdlRegisterServiceHook(143,	/* FIXME put in config setup */
				NULL,
				imap_greeting,
				imap_command_loop);
	CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP);
	return "$Id$";
}
