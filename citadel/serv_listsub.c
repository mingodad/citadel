/*
 * $Id$
 *
 * This module handles self-service subscription/unsubscription to mail lists.
 *
 * Copyright (C) 2002 by Art Cancro and others.
 * This code is released under the terms of the GNU General Public License.
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>
#include <dirent.h>
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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "serv_network.h"
#include "clientsocket.h"
#include "file_ops.h"

#ifndef HAVE_SNPRINTF
#include "snprintf.h"
#endif


/*
 * Generate a randomizationalisticized token to use for authentication of
 * a subscribe or unsubscribe request.
 */
void listsub_generate_token(char *buf) {
	char sourcebuf[SIZ];
	static int seq = 0;

	/* Theo, please sit down and shut up.  This key doesn't have to be
	 * tinfoil-hat secure, it just needs to be reasonably unguessable
	 * and unique.
	 */
	sprintf(sourcebuf, "%d%d%ld",
		++seq,
		getpid(),
		time(NULL)
	);

	/* Convert it to base64 so it looks cool */	
	encode_base64(buf, sourcebuf);
}


/*
 * Enter a subscription request
 */
void do_subscribe(char *room, char *email, char *subtype) {
	struct quickroom qrbuf;
	FILE *ncfp;
	char filename[SIZ];
	char token[SIZ];
	char confirmation_request[SIZ];

	if (getroom(&qrbuf, room) != 0) {
		cprintf("%d There is no list called '%s'\n", ERROR, room);
		return;
	}

	listsub_generate_token(token);

	begin_critical_section(S_NETCONFIGS);
	assoc_file_name(filename, sizeof filename, &qrbuf, "netconfigs");
	ncfp = fopen(filename, "a");
	if (ncfp != NULL) {
		fprintf(ncfp, "subpending|%s|%s|%s|%ld\n",
			email,
			subtype,
			token,
			time(NULL)
		);
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	/* Generate and send the confirmation request */

	snprintf(confirmation_request, sizeof confirmation_request,
		"Someone (probably you) has submitted a request to subscribe\n"
		"<%s> to the '%s' mailing list.\n\n"
		"In order to confirm this subscription request, please\n"
		"point your web browser at the following location:\n\n"
		"http://FIXME.com:FIXME/blah?room=%s&token=%s\n\n"
		"If this request has been submitted in error and you do not\n"
		"wish to receive the '%s' mailing list, simply do nothing,\n"
		"and you will not receive any further mailings.\n",

		email, qrbuf.QRname, qrbuf.QRname, token, qrbuf.QRname
	);

	quickie_message(
		"Citadel",
		email,
		qrbuf.QRname,
		confirmation_request
	);

	cprintf("%d Subscription entered; confirmation request sent\n", CIT_OK);

}


/*
 * Confirm a subscribe/unsubscribe request.
 */
void do_confirm(char *room, char *token) {
	struct quickroom qrbuf;
	FILE *ncfp;
	char filename[SIZ];
	char line_token[SIZ];
	long line_offset;
	int line_length;
	char buf[SIZ];
	char cmd[SIZ];
	char email[SIZ];
	char subtype[SIZ];
	int success = 0;

	if (getroom(&qrbuf, room) != 0) {
		cprintf("%d There is no list called '%s'\n", ERROR, room);
		return;
	}

	begin_critical_section(S_NETCONFIGS);
	assoc_file_name(filename, sizeof filename, &qrbuf, "netconfigs");
	ncfp = fopen(filename, "r+");
	if (ncfp != NULL) {
		while (line_offset = ftell(ncfp),
		      (fgets(buf, sizeof buf, ncfp) != NULL) ) {
			buf[strlen(buf)-1] = 0;
			line_length = strlen(buf);
			extract(cmd, buf, 0);
			if (!strcasecmp(cmd, "subpending")) {
				extract(email, buf, 1);
				extract(subtype, buf, 2);
				extract(line_token, buf, 3);
				if (!strcasecmp(token, line_token)) {
					if (!strcasecmp(subtype, "digest")) {
						strcpy(buf, "digestrecp|");
					}
					else {
						strcpy(buf, "listrecp|");
					}
					strcat(buf, email);
					strcat(buf, "|");
					/* SLEAZY HACK: pad the line out so
				 	 * it's the same length as the line
					 * we're replacing.
				 	 */
					while (strlen(buf) < line_length) {
						strcat(buf, " ");
					}
					fseek(ncfp, line_offset, SEEK_SET);
					fprintf(ncfp, "%s\n", buf);
					++success;
				}
			}
		}
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	if (success) {
		cprintf("%d %d operation(s) confirmed.\n", CIT_OK, success);
	}
	else {
		cprintf("%d Invalid token.\n", ERROR);
	}

}



/* 
 * process subscribe/unsubscribe requests and confirmations
 */
void cmd_subs(char *cmdbuf) {

	char opr[SIZ];
	char room[SIZ];
	char email[SIZ];
	char subtype[SIZ];
	char token[SIZ];

	extract(opr, cmdbuf, 0);
	if (!strcasecmp(opr, "subscribe")) {
		extract(subtype, cmdbuf, 3);
		if ( (strcasecmp(subtype, "list"))
		   && (strcasecmp(subtype, "digest")) ) {
			cprintf("%d Invalid subscription type.\n", ERROR);
		}
		else {
			extract(room, cmdbuf, 1);
			extract(email, cmdbuf, 2);
			do_subscribe(room, email, subtype);
		}
	}
	else if (!strcasecmp(opr, "unsubscribe")) {
		cprintf("%d not yet implemented\n", ERROR);
	}
	else if (!strcasecmp(opr, "confirm")) {
		extract(room, cmdbuf, 1);
		extract(token, cmdbuf, 2);
		do_confirm(room, token);
	}
	else {
		cprintf("%d Invalid command\n", ERROR);
	}
}


/*
 * Module entry point
 */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_subs, "SUBS", "List subscribe/unsubscribe");
	return "$Id$";
}
