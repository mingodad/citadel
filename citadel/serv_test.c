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

symtab *My_Symtab = NULL;	/* dyn */

extern struct CitContext *ContextList;

#define MODULE_NAME 	"Dummy test module"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	1

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

void Dynamic_Module_Init(struct DLModule_Info *info)
{
   strncpy(info->module_name, MODULE_NAME, 30);
   strncpy(info->module_author, MODULE_AUTHOR, 30);
   strncpy(info->module_author_email, MODULE_EMAIL, 30);
   info->major_version = MAJOR_VERSION;
   info->minor_version = MINOR_VERSION;

   CtdlRegisterCleanupHook(CleanupTest);
   CtdlRegisterNewRoomHook(NewRoomTest);
   CtdlRegisterSessionHook(SessionStartTest, 1);
   CtdlRegisterSessionHook(SessionStopTest, 0);
   CtdlRegisterLoginHook(LoginTest);

}

void Get_Symtab(symtab **the_symtab)
{
   (*the_symtab) = My_Symtab;
   return;
}
