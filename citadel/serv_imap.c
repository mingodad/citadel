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

	/*    FIXME    ...   implement commands requiring login here   */

	else {
		cprintf("%s BAD command unrecognized\r\n", tag);
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
