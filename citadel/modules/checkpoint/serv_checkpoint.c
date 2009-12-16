/*
 * $Id$
 *
 * checkpointing module for the database
 *
 * Copyright (c) 1987-2009 by the citadel.org team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
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

#include <libcitadel.h>

#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "database.h"
#include "msgbase.h"
#include "sysdep_decls.h"
#include "config.h"
#include "threads.h"

#include "ctdl_module.h"
#include "context.h"
 
/*
 * Main loop for the checkpoint thread.
 */
void *checkpoint_thread(void *arg) {
	struct CitContext checkpointCC;

	CtdlLogPrintf(CTDL_DEBUG, "checkpoint_thread() initializing\n");

	CtdlFillSystemContext(&checkpointCC, "checkpoint");
	citthread_setspecific(MyConKey, (void *)&checkpointCC );

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
	return "$Id$";
}
