/*
 * $Id$ 
 *
 * IMAP server for the Citadel/UX system
 * Copyright (C) 2000 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 * Current status of standards conformance:
 *
 *               ***  ABSOLUTELY NOTHING WORKS  ***
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
 * This cleanup function blows away the temporary memory and files used by
 * the IMAP server.
 */
void imap_cleanup_function(void) {

	/* Don't do this stuff if this is not a IMAP session! */
	if (CC->h_command_function != imap_command_loop) return;

	lprintf(9, "Performing IMAP cleanup hook\n");


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
	cprintf("%s OK CLOSE completed\r\n", parms[0]);
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

	else if (IMAP->selected == 0) {
		cprintf("%s BAD command unrecognized\r\n", parms[0]);
	}

	/* commands requiring the SELECT state */

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
	CtdlRegisterServiceHook(2243,	/* FIXME put in config setup */
				NULL,
				imap_greeting,
				imap_command_loop);
	CtdlRegisterSessionHook(imap_cleanup_function, EVT_STOP);
	return "$Id$";
}
