/*
 * $Id$
 *
 * A skeleton module to test the dynamic loader.
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
#include "policy.h"
#include "database.h"
#include "msgbase.h"

extern struct CitContext *ContextList;

void CleanupTest(void) {
	lprintf(9, "--- test of adding an unload hook --- \n");
	}

void NewRoomTest(void) {
	lprintf(9, "--- test module was told we're now in a new room ---\n");
	}

void SessionStartTest(void) {
	lprintf(9, "--- starting up session %d ---\n",
		CC->cs_pid);
	}

void SessionStopTest(void) {
	lprintf(9, "--- ending session %d ---\n", 
		CC->cs_pid);
	}

void LoginTest(void) {
	lprintf(9, "--- Hello, %s ---\n", CC->curr_user);
	}


void Ygorl(char *username, long usernum) {
	if (!strcasecmp(username, "Unsuspecting User")) {
		strcpy(username, "Flaming Asshole");
		}
	}

void LogTest(char *buf) {
	fprintf(stderr,"%c[1m%s%c[0m", 27, buf, 27);
	fflush(stderr);
	}


char *Dynamic_Module_Init(void)
{
   CtdlRegisterCleanupHook(CleanupTest);
   CtdlRegisterSessionHook(NewRoomTest, EVT_NEWROOM);
   CtdlRegisterSessionHook(SessionStartTest, EVT_START);
   CtdlRegisterSessionHook(SessionStopTest, EVT_STOP);
   CtdlRegisterSessionHook(LoginTest, EVT_LOGIN);
   CtdlRegisterUserHook(Ygorl, EVT_OUTPUTMSG);
   CtdlRegisterLogHook(LogTest, 1);
   return "$Id$";
}
