/*
 * checkpointing module for the database
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
 
#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>

#include <libcitadel.h>

#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "msgbase.h"
#include "sysdep_decls.h"
#include "config.h"
#include "threads.h"

#include "ctdl_module.h"
#include "context.h"

CTDL_MODULE_INIT(checkpoint)
{
	if (threading)
	{
		CtdlRegisterSessionHook(cdb_checkpoint, EVT_TIMER, PRIO_CLEANUP + 10);
	}
	/* return our module name for the log */
	return "checkpoint";
}
