/*
 * $Id$
 *
 * This module handles self-service subscription/unsubscription to mail lists.
 *
 * Copyright (C) 2002-2005 by Art Cancro and others.
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
#include "serv_extensions.h"
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
	sprintf(sourcebuf, "%lx",
		(long) (++seq + getpid() + time(NULL))
	);

	/* Convert it to base64 so it looks cool */	
	CtdlEncodeBase64(buf, sourcebuf, strlen(sourcebuf));
}


/*
 * Enter a subscription request
 */
void do_subscribe(char *room, char *email, char *subtype, char *webpage) {
	struct ctdlroom qrbuf;
	FILE *ncfp;
	char filename[256];
	char token[256];
	char confirmation_request[2048];
	char buf[512];
	char urlroom[ROOMNAMELEN];
	char scancmd[64];
	char scanemail[256];
	int found_sub = 0;

	if (getroom(&qrbuf, room) != 0) {
		cprintf("%d There is no list called '%s'\n", ERROR + ROOM_NOT_FOUND, room);
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	listsub_generate_token(token);

	assoc_file_name(filename, sizeof filename, &qrbuf, ctdl_netcfg_dir);

	/* 
	 * Make sure the requested address isn't already subscribed
	 */
	begin_critical_section(S_NETCONFIGS);
	ncfp = fopen(filename, "r");
	if (ncfp != NULL) {
		while (fgets(buf, sizeof buf, ncfp) != NULL) {
			buf[strlen(buf)-1] = 0;
			extract_token(scancmd, buf, 0, '|', sizeof scancmd);
			extract_token(scanemail, buf, 1, '|', sizeof scanemail);
			if ((!strcasecmp(scancmd, "listrecp"))
			   || (!strcasecmp(scancmd, "digestrecp"))) {
				if (!strcasecmp(scanemail, email)) {
					++found_sub;
				}
			}
		}
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	if (found_sub != 0) {
		cprintf("%d '%s' is already subscribed to '%s'.\n",
			ERROR + ALREADY_EXISTS,
			email, qrbuf.QRname);
		return;
	}

	/*
	 * Now add it to the file
	 */	
	begin_critical_section(S_NETCONFIGS);
	ncfp = fopen(filename, "a");
	if (ncfp != NULL) {
		fprintf(ncfp, "subpending|%s|%s|%s|%ld|%s\n",
			email,
			subtype,
			token,
			time(NULL),
			webpage
		);
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	/* Generate and send the confirmation request */

	urlesc(urlroom, qrbuf.QRname);

	snprintf(confirmation_request, sizeof confirmation_request,
		"Content-type: text/html\nMIME-Version: 1.0\n\n"
		"<HTML><BODY>"
		"Someone (probably you) has submitted a request to subscribe\n"
		"&lt;%s&gt; to the <B>%s</B> mailing list.<BR><BR>\n"
		"Please click here to confirm this request:<BR>\n"
		"<A HREF=\"%s?room=%s&token=%s&cmd=confirm\">"
		"%s?room=%s&token=%s&cmd=confirm</A><BR><BR>\n"
		"If this request has been submitted in error and you do not\n"
		"wish to receive the '%s' mailing list, simply do nothing,\n"
		"and you will not receive any further mailings.\n"
		"</BODY></HTML>\n",

		email, qrbuf.QRname,
		webpage, urlroom, token,
		webpage, urlroom, token,
		qrbuf.QRname
	);

	quickie_message(	/* This delivers the message */
		"Citadel",
		email,
		NULL,
		confirmation_request,
		FMT_RFC822,
		"Please confirm your list subscription"
	);

	cprintf("%d Subscription entered; confirmation request sent\n", CIT_OK);
}


/*
 * Enter an unsubscription request
 */
void do_unsubscribe(char *room, char *email, char *webpage) {
	struct ctdlroom qrbuf;
	FILE *ncfp;
	char filename[256];
	char token[256];
	char buf[512];
	char confirmation_request[2048];
	char urlroom[ROOMNAMELEN];
	char scancmd[256];
	char scanemail[256];
	int found_sub = 0;

	if (getroom(&qrbuf, room) != 0) {
		cprintf("%d There is no list called '%s'\n",
			ERROR + ROOM_NOT_FOUND, room);
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	listsub_generate_token(token);

	assoc_file_name(filename, sizeof filename, &qrbuf, ctdl_netcfg_dir);

	/* 
	 * Make sure there's actually a subscription there to remove
	 */
	begin_critical_section(S_NETCONFIGS);
	ncfp = fopen(filename, "r");
	if (ncfp != NULL) {
		while (fgets(buf, sizeof buf, ncfp) != NULL) {
			buf[strlen(buf)-1] = 0;
			extract_token(scancmd, buf, 0, '|', sizeof scancmd);
			extract_token(scanemail, buf, 1, '|', sizeof scanemail);
			if ((!strcasecmp(scancmd, "listrecp"))
			   || (!strcasecmp(scancmd, "digestrecp"))) {
				if (!strcasecmp(scanemail, email)) {
					++found_sub;
				}
			}
		}
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	if (found_sub == 0) {
		cprintf("%d '%s' is not subscribed to '%s'.\n",
			ERROR + NO_SUCH_USER,
			email, qrbuf.QRname);
		return;
	}
	
	/* 
	 * Ok, now enter the unsubscribe-pending entry.
	 */
	begin_critical_section(S_NETCONFIGS);
	ncfp = fopen(filename, "a");
	if (ncfp != NULL) {
		fprintf(ncfp, "unsubpending|%s|%s|%ld|%s\n",
			email,
			token,
			time(NULL),
			webpage
		);
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	/* Generate and send the confirmation request */

	urlesc(urlroom, qrbuf.QRname);

	snprintf(confirmation_request, sizeof confirmation_request,
		"Content-type: text/html\nMIME-Version: 1.0\n\n"
		"<HTML><BODY>"
		"Someone (probably you) has submitted a request "
		"to unsubscribe\n"
		"&lt;%s&gt; from the <B>%s</B> mailing list.<BR><BR>\n"
		"Please click here to confirm this request:<BR>\n"
		"<A HREF=\"%s?room=%s&token=%s&cmd=confirm\">"
		"%s?room=%s&token=%s&cmd=confirm</A><BR><BR>\n"
		"If this request has been submitted in error and you do\n"
		"<i>not</i> wish to unsubscribe from the "
		"'%s' mailing list, simply do nothing,\n"
		"and you will remain subscribed to the list.\n"
		"</BODY></HTML>\n",

		email, qrbuf.QRname,
		webpage, urlroom, token,
		webpage, urlroom, token,
		qrbuf.QRname
	);

	quickie_message(	/* This delivers the message */
		"Citadel",
		email,
		NULL,
		confirmation_request,
		FMT_RFC822,
		"Please confirm your unsubscribe request"
	);

	cprintf("%d Unubscription noted; confirmation request sent\n", CIT_OK);
}


/*
 * Confirm a subscribe/unsubscribe request.
 */
void do_confirm(char *room, char *token) {
	struct ctdlroom qrbuf;
	FILE *ncfp;
	char filename[256];
	char line_token[256];
	long line_offset;
	int line_length;
	char buf[512];
	char cmd[256];
	char email[256];
	char subtype[128];
	int success = 0;
	char address_to_unsubscribe[256];
	char scancmd[256];
	char scanemail[256];
	char *holdbuf = NULL;
	int linelen = 0;
	int buflen = 0;

	strcpy(address_to_unsubscribe, "");

	if (getroom(&qrbuf, room) != 0) {
		cprintf("%d There is no list called '%s'\n",
			ERROR + ROOM_NOT_FOUND, room);
		return;
	}

	if ((qrbuf.QRflags2 & QR2_SELFLIST) == 0) {
		cprintf("%d '%s' "
			"does not accept subscribe/unsubscribe requests.\n",
			ERROR + HIGHER_ACCESS_REQUIRED, qrbuf.QRname);
		return;
	}

	/*
	 * Now start scanning this room's netconfig file for the
	 * specified token.
	 */
	assoc_file_name(filename, sizeof filename, &qrbuf, ctdl_netcfg_dir);
	begin_critical_section(S_NETCONFIGS);
	ncfp = fopen(filename, "r+");
	if (ncfp != NULL) {
		while (line_offset = ftell(ncfp),
		      (fgets(buf, sizeof buf, ncfp) != NULL) ) {
			buf[strlen(buf)-1] = 0;
			line_length = strlen(buf);
			extract_token(cmd, buf, 0, '|', sizeof cmd);
			if (!strcasecmp(cmd, "subpending")) {
				extract_token(email, buf, 1, '|', sizeof email);
				extract_token(subtype, buf, 2, '|', sizeof subtype);
				extract_token(line_token, buf, 3, '|', sizeof line_token);
				if (!strcasecmp(token, line_token)) {
					if (!strcasecmp(subtype, "digest")) {
						safestrncpy(buf, "digestrecp|", sizeof buf);
					}
					else {
						safestrncpy(buf, "listrecp|", sizeof buf);
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
			if (!strcasecmp(cmd, "unsubpending")) {
				extract_token(line_token, buf, 2, '|', sizeof line_token);
				if (!strcasecmp(token, line_token)) {
					extract_token(address_to_unsubscribe, buf, 1, '|',
						sizeof address_to_unsubscribe);
				}
			}
		}
		fclose(ncfp);
	}
	end_critical_section(S_NETCONFIGS);

	/*
	 * If "address_to_unsubscribe" contains something, then we have to
	 * make another pass at the file, stripping out lines referring to
	 * that address.
	 */
	if (strlen(address_to_unsubscribe) > 0) {
		holdbuf = malloc(SIZ);
		begin_critical_section(S_NETCONFIGS);
		ncfp = fopen(filename, "r+");
		if (ncfp != NULL) {
			while (line_offset = ftell(ncfp),
			      (fgets(buf, sizeof buf, ncfp) != NULL) ) {
				buf[strlen(buf)-1]=0;
				extract_token(scancmd, buf, 0, '|', sizeof scancmd);
				extract_token(scanemail, buf, 1, '|', sizeof scanemail);
				if ( (!strcasecmp(scancmd, "listrecp"))
				   && (!strcasecmp(scanemail,
						address_to_unsubscribe)) ) {
					++success;
				}
				else if ( (!strcasecmp(scancmd, "digestrecp"))
				   && (!strcasecmp(scanemail,
						address_to_unsubscribe)) ) {
					++success;
				}
				else if ( (!strcasecmp(scancmd, "subpending"))
				   && (!strcasecmp(scanemail,
						address_to_unsubscribe)) ) {
					++success;
				}
				else if ( (!strcasecmp(scancmd, "unsubpending"))
				   && (!strcasecmp(scanemail,
						address_to_unsubscribe)) ) {
					++success;
				}
				else {	/* Not relevant, so *keep* it! */
					linelen = strlen(buf);
					holdbuf = realloc(holdbuf,
						(buflen + linelen + 2) );
					strcpy(&holdbuf[buflen], buf);
					buflen += linelen;
					strcpy(&holdbuf[buflen], "\n");
					buflen += 1;
				}
			}
			fclose(ncfp);
		}
		ncfp = fopen(filename, "w");
		if (ncfp != NULL) {
			fwrite(holdbuf, buflen+1, 1, ncfp);
			fclose(ncfp);
		}
		end_critical_section(S_NETCONFIGS);
		free(holdbuf);
	}

	/*
	 * Did we do anything useful today?
	 */
	if (success) {
		cprintf("%d %d operation(s) confirmed.\n", CIT_OK, success);
	}
	else {
		cprintf("%d Invalid token.\n", ERROR + ILLEGAL_VALUE);
	}

}



/* 
 * process subscribe/unsubscribe requests and confirmations
 */
void cmd_subs(char *cmdbuf) {

	char opr[256];
	char room[ROOMNAMELEN];
	char email[256];
	char subtype[256];
	char token[256];
	char webpage[256];

	extract_token(opr, cmdbuf, 0, '|', sizeof opr);
	if (!strcasecmp(opr, "subscribe")) {
		extract_token(subtype, cmdbuf, 3, '|', sizeof subtype);
		if ( (strcasecmp(subtype, "list"))
		   && (strcasecmp(subtype, "digest")) ) {
			cprintf("%d Invalid subscription type '%s'\n",
				ERROR + ILLEGAL_VALUE, subtype);
		}
		else {
			extract_token(room, cmdbuf, 1, '|', sizeof room);
			extract_token(email, cmdbuf, 2, '|', sizeof email);
			extract_token(webpage, cmdbuf, 4, '|', sizeof webpage);
			do_subscribe(room, email, subtype, webpage);
		}
	}
	else if (!strcasecmp(opr, "unsubscribe")) {
		extract_token(room, cmdbuf, 1, '|', sizeof room);
		extract_token(email, cmdbuf, 2, '|', sizeof email);
		extract_token(webpage, cmdbuf, 3, '|', sizeof webpage);
		do_unsubscribe(room, email, webpage);
	}
	else if (!strcasecmp(opr, "confirm")) {
		extract_token(room, cmdbuf, 1, '|', sizeof room);
		extract_token(token, cmdbuf, 2, '|', sizeof token);
		do_confirm(room, token);
	}
	else {
		cprintf("%d Invalid command\n", ERROR + ILLEGAL_VALUE);
	}
}


/*
 * Module entry point
 */
char *serv_listsub_init(void)
{
	CtdlRegisterProtoHook(cmd_subs, "SUBS", "List subscribe/unsubscribe");
	return "$Id$";
}
