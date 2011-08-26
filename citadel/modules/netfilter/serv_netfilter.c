/*
 * A server-side module for Citadel designed to filter idiots off the network.
 * 
 * Copyright (c) 2002-2009 by the citadel.org team
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
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "database.h"
#include "msgbase.h"
#include "serv_network.h"	/* Needed for defenition of FilterList */


#include "ctdl_module.h"


/*
 * This handler detects whether an incoming network message is from some
 * moron user who the site operator has elected to filter out.  If a match
 * is found, the message is rejected.
 */
int filter_the_idiots(struct CtdlMessage *msg, char *target_room) {
	FilterList *fptr;
	int zap_user = 0;
	int zap_room = 0;
	int zap_node = 0;

	if ( (msg == NULL) || (filterlist == NULL) ) {
		return(0);
	}

	for (fptr = filterlist; fptr != NULL; fptr = fptr->next) {

		zap_user = 0;
		zap_room = 0;
		zap_node = 0;

		if (msg->cm_fields['A'] != NULL) {
			if ( (!strcasecmp(msg->cm_fields['A'], fptr->fl_user))
			   || (fptr->fl_user[0] == 0) ) {
				zap_user = 1;
			}
		}

		if (msg->cm_fields['C'] != NULL) {
			if ( (!strcasecmp(msg->cm_fields['C'], fptr->fl_room))
			   || (fptr->fl_room[0] == 0) ) {
				zap_room = 1;
			}
		}

		if (msg->cm_fields['O'] != NULL) {
			if ( (!strcasecmp(msg->cm_fields['O'], fptr->fl_room))
			   || (fptr->fl_room[0] == 0) ) {
				zap_room = 1;
			}
		}

		if (msg->cm_fields['N'] != NULL) {
			if ( (!strcasecmp(msg->cm_fields['N'], fptr->fl_node))
			   || (fptr->fl_node[0] == 0) ) {
				zap_node = 1;
			}
		}
	
		if (zap_user + zap_room + zap_node == 3) return(1);

	}

	return(0);
}


CTDL_MODULE_INIT(netfilter)
{
	if (!threading)
	{
		CtdlRegisterNetprocHook(filter_the_idiots);
	}
	
	/* return our module name for the log */
	return "netfilter";
}
