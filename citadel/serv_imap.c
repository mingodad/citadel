/* $Id$ 
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
void imap_login(char *tag, char *cmd, char *parms) {
	char username[256];
	char password[256];

	extract_token(username, parms, 0, ' ');
	extract_token(password, parms, 1, ' ');

	if (CtdlLoginExistingUser(username) == login_ok) {
		if (CtdlTryPassword(password) == pass_ok) {
			cprintf("%s OK login successful\r\n", tag);
			return;
		}
        }

	cprintf("%s BAD Login incorrect\r\n", tag);
}


/*
 * implements the CAPABILITY command
 */
void imap_capability(char *tag, char *cmd, char *parms) {
	cprintf("* CAPABILITY IMAP4 IMAP4REV1 AUTH=LOGIN\r\n");
	cprintf("%s OK CAPABILITY completed\r\n", tag);
}





/*
 * implements the SELECT command
 */
void imap_select(char *tag, char *cmd, char *parms) {
	char towhere[256];
	char augmented_roomname[256];
	int c = 0;
	int ok = 0;
	int ra = 0;
	struct quickroom QRscratch;
	int msgs, new;

	extract_token(towhere, parms, 0, ' ');

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
		cprintf("%s NO ... no such room, or access denied\r\n", tag);
		IMAP->selected = 0;
		return;
	}

	/*
	 * usergoto() formally takes us to the desired room, happily returning
	 * the number of messages and number of new messages.
	 */
	usergoto(QRscratch.QRname, 0, &msgs, &new);

	cprintf("* %d EXISTS\r\n", msgs);
	cprintf("* %d RECENT\r\n", new);
	cprintf("* OK [UIDVALIDITY 0] UIDs valid\r\n");
	cprintf("%s OK [FIXME] SELECT completed\r\n", tag);
}


/* 
 * Main command loop for IMAP sessions.
 */
void imap_command_loop(void) {
	char cmdbuf[256];
	char tag[256];
	char cmd[256];

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
	extract_token(tag, cmdbuf, 0, ' ');
	extract_token(cmd, cmdbuf, 1, ' ');
	remove_token(cmdbuf, 0, ' ');
	remove_token(cmdbuf, 0, ' ');
	lprintf(9, "tag=<%s> cmd=<%s> parms=<%s>\n", tag, cmd, cmdbuf);

	/* commands which may be executed in any state */

	if (!strcasecmp(cmd, "NOOP")) {
		cprintf("%s OK This command successfully did nothing.\r\n",
			tag);
	}

	else if (!strcasecmp(cmd, "LOGOUT")) {
		cprintf("* BYE %s logging out\r\n", config.c_fqdn);
		cprintf("%s OK thank you for using Citadel IMAP\r\n", tag);
		CC->kill_me = 1;
		return;
	}

	else if (!strcasecmp(cmd, "LOGIN")) {
		imap_login(tag, cmd, cmdbuf);
	}

	else if (!strcasecmp(cmd, "CAPABILITY")) {
		imap_capability(tag, cmd, cmdbuf);
	}

	else if (!CC->logged_in) {
		cprintf("%s BAD Not logged in.\r\n", tag);
	}

	/*  commands requiring the client to be logged in */

	else if (!strcasecmp(cmd, "SELECT")) {
		imap_select(tag, cmd, cmdbuf);
	}

	/* end of commands */

	else {
		cprintf("%s BAD command unrecognized\r\n", tag);
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
