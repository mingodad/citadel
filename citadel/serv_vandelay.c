/*
 * $Id$
 *
 * This is the "Art Vandelay" module.  It is an importer/exporter.
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
#include "database.h"
#include "msgbase.h"
#include "tools.h"


void artv_do_export(void) {
	cprintf("%d command not yet implemented\n", ERROR);
}




void artv_do_import(void) {
	cprintf("%d command not yet implemented\n", ERROR);
}



void cmd_artv(char *cmdbuf) {
	char cmd[256];

	if (CtdlAccessCheck(ac_aide)) return;	/* FIXME should be intpgm */

	extract(cmd, cmdbuf, 0);
	if (!strcasecmp(cmd, "export")) artv_do_export();
	else if (!strcasecmp(cmd, "import")) artv_do_import();
	else cprintf("%d illegal command\n", ERROR);
}




char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");
	return "$Id$";
}
