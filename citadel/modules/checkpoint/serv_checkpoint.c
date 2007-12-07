/*
 * $Id: serv_checkpoint.c 5756 2007-11-16 17:15:22Z ajc $
 *
 * checkpointing module for the database
 */
 
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#ifdef HAVE_DB_H
#include <db.h>
#elif defined(HAVE_DB4_DB_H)
#include <db4/db.h>
#else
#error Neither <db.h> nor <db4/db.h> was found by configure. Install db4-devel.
#endif


#if DB_VERSION_MAJOR < 4 || DB_VERSION_MINOR < 1
#error Citadel requires Berkeley DB v4.1 or newer.  Please upgrade.
#endif

#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "database.h"
#include "msgbase.h"
#include "sysdep_decls.h"
#include "config.h"

#include "ctdl_module.h"
 
/*
 * Main loop for the checkpoint thread.
 */
void *checkpoint_thread(void *arg) {
	struct CitContext checkpointCC;

	CtdlLogPrintf(CTDL_DEBUG, "checkpoint_thread() initializing\n");

	memset(&checkpointCC, 0, sizeof(struct CitContext));
	checkpointCC.internal_pgm = 1;
	checkpointCC.cs_pid = 0;
	pthread_setspecific(MyConKey, (void *)&checkpointCC );

	while (!CtdlThreadCheckStop()) {
		cdb_checkpoint();
		CtdlThreadSleep(60);
	}

	CtdlLogPrintf(CTDL_DEBUG, "checkpoint_thread() exiting\n");
	return NULL;
}


CTDL_MODULE_INIT(checkpoint) {
	if (threading)
	{
		CtdlThreadCreate ("checkpoint", CTDLTHREAD_BIGSTACK, checkpoint_thread, NULL);
	}
	/* return our Subversion id for the Log */
	return "$Id: serv_autocompletion.c 5756 2007-11-16 17:15:22Z ajc $";
}
