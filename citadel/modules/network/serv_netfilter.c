/*
 * A server-side module for Citadel designed to filter idiots off the network.
 * 
 * Copyright (c) 2002-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
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


#include "ctdl_module.h"

typedef struct FilterList FilterList;

struct FilterList {
	FilterList *next;
	char fl_user[SIZ];
	char fl_room[SIZ];
	char fl_node[SIZ];
};

struct FilterList *filterlist = NULL;

/*
 * Keep track of what messages to reject
 */
FilterList *load_filter_list(void) {
	char *serialized_list = NULL;
	int i;
	char buf[SIZ];
	FilterList *newlist = NULL;
	FilterList *nptr;

	serialized_list = CtdlGetSysConfig(FILTERLIST);
	if (serialized_list == NULL) return(NULL); /* if null, no entries */

	/* Use the string tokenizer to grab one line at a time */
	for (i=0; i<num_tokens(serialized_list, '\n'); ++i) {
		extract_token(buf, serialized_list, i, '\n', sizeof buf);
		nptr = (FilterList *) malloc(sizeof(FilterList));
		extract_token(nptr->fl_user, buf, 0, '|', sizeof nptr->fl_user);
		striplt(nptr->fl_user);
		extract_token(nptr->fl_room, buf, 1, '|', sizeof nptr->fl_room);
		striplt(nptr->fl_room);
		extract_token(nptr->fl_node, buf, 2, '|', sizeof nptr->fl_node);
		striplt(nptr->fl_node);

		/* Cowardly refuse to add an any/any/any entry that would
		 * end up filtering every single message.
		 */
		if (IsEmptyStr(nptr->fl_user) && 
		    IsEmptyStr(nptr->fl_room) &&
		    IsEmptyStr(nptr->fl_node)) {
			free(nptr);
		}
		else {
			nptr->next = newlist;
			newlist = nptr;
		}
	}

	free(serialized_list);
	return newlist;
}


void free_filter_list(FilterList *fl) {
	if (fl == NULL) return;
	free_filter_list(fl->next);
	free(fl);
}

void free_netfilter_list(void)
{
	free_filter_list(filterlist);
	filterlist = NULL;
}

void load_network_filter_list(void)
{
	filterlist = load_filter_list();
}


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
/*
  currently unsupported.
		CtdlRegisterNetprocHook(filter_the_idiots);
*/
	}
	
	/* return our module name for the log */
	return "netfilter";
}
