/* $Id$ */
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
#include "policy.h"
#include "database.h"
#include "msgbase.h"


/*
 * Here's where our SMTP session begins its happy day.
 */
void smtp_greeting(void) {

	strcpy(CC->cs_clientname, "Citadel SMTP");

	cprintf("220 %s Citadel/UX SMTP server ready\n",
		config.c_fqdn);
}


/* 
 * Main command loop for SMTP sessions.
 */
void smtp_command_loop(void) {
	char cmdbuf[256];

	time(&CC->lastcmd);
	memset(cmdbuf, 0, sizeof cmdbuf); /* Clear it, just in case */
	if (client_gets(cmdbuf) < 1) {
		lprintf(3, "SMTP socket is broken.  Ending session.\n");
		CC->kill_me = 1;
		return;
	}
	lprintf(5, "citserver[%3d]: %s\n", CC->cs_pid, cmdbuf);

	if (!strncasecmp(cmdbuf,"QUIT",4)) {
		cprintf("221 Later, dude!  Microsoft sucks!!\n");
		CC->kill_me = 1;
		return;
		}

	else {
		cprintf("500 I'm afraid I can't do that, Dave.\n");
	}

}



char *Dynamic_Module_Init(void)
{
	CtdlRegisterServiceHook(2525,
				smtp_greeting,
				smtp_command_loop);
	return "$Id$";
}
