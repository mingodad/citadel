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
#include <pthread.h>
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"

extern struct CitContext *ContextList;

#define MODULE_NAME 	"Dummy test module"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	1

static struct DLModule_Info info = {
  MODULE_NAME,
  MODULE_AUTHOR,
  MODULE_EMAIL,
  MAJOR_VERSION,
  MINOR_VERSION
};

void CleanupTest(void) {
	lprintf(9, "--- test of adding an unload hook --- \n");
	}

void NewRoomTest(char *RoomName) {
	lprintf(9, "--- test module was told we're now in %s ---\n", RoomName);
	}

void SessionStartTest(int WhichSession) {
	lprintf(9, "--- starting up session %d ---\n", WhichSession);
	}

void SessionStopTest(int WhichSession) {
	lprintf(9, "--- ending session %d ---\n", WhichSession);
	}

void LoginTest(void) {
	lprintf(9, "--- Hello, %s ---\n", CC->curr_user);
	}

struct DLModule_Info *Dynamic_Module_Init(void)
{
   CtdlRegisterCleanupHook(CleanupTest);
   CtdlRegisterNewRoomHook(NewRoomTest);
   CtdlRegisterSessionHook(SessionStartTest, 1);
   CtdlRegisterSessionHook(SessionStopTest, 0);
   CtdlRegisterLoginHook(LoginTest);
   return &info;
}
