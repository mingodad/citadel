/*
 * $Id$
 *
 * Server-side module for Wiki rooms.  This will handle things like version control. 
 * 
 * Copyright (c) 2009 by the citadel.org team
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
#include <ctype.h>
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
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "euidindex.h"
#include "ctdl_module.h"

/*
 * Before allowing a wiki page save to execute, we have to perform version control.
 * This involves fetching the old version of the page if it exists.
 */
int wiki_upload_beforesave(struct CtdlMessage *msg) {
	struct CitContext *CCC = CC;
	long old_msgnum = (-1L);
	struct CtdlMessage *old_msg = NULL;
	long history_msgnum = (-1L);
	struct CtdlMessage *history_msg = NULL;
	char diff_old_filename[PATH_MAX];
	char diff_new_filename[PATH_MAX];
	char diff_cmd[PATH_MAX];
	FILE *fp;
	int rv;
	char history_page[1024];
	char boundary[256];
	char prefixed_boundary[258];
	char buf[1024];
	int nbytes = 0;
	char *diffbuf = NULL;
	size_t diffbuf_len = 0;
	char *ptr = NULL;

	if (!CCC->logged_in) return(0);	/* Only do this if logged in. */

	/* Is this a room with a Wiki in it, don't run this hook. */
	if (CCC->room.QRdefaultview != VIEW_WIKI) {
		return(0);
	}

	/* If this isn't a MIME message, don't bother. */
	if (msg->cm_format_type != 4) return(0);

	/* If there's no EUID we can't do this. */
	if (msg->cm_fields['E'] == NULL) return(0);
	snprintf(history_page, sizeof history_page, "%s_HISTORY_", msg->cm_fields['E']);

	/* Make sure we're saving a real wiki page rather than a wiki history page.
	 * This is important in order to avoid recursing infinitely into this hook.
	 */
	if (	(strlen(msg->cm_fields['E']) >= 9)
		&& (!strcasecmp(&msg->cm_fields['E'][strlen(msg->cm_fields['E'])-9], "_HISTORY_"))
	) {
		CtdlLogPrintf(CTDL_DEBUG, "History page not being historied\n");
		return(0);
	}

	/* If there's no message text, obviously this is all b0rken and shouldn't happen at all */
	if (msg->cm_fields['M'] == NULL) return(0);

	/* See if we can retrieve the previous version. */
	old_msgnum = locate_message_by_euid(msg->cm_fields['E'], &CCC->room);
	if (old_msgnum > 0L) {
		old_msg = CtdlFetchMessage(old_msgnum, 1);
	}
	else {
		old_msg = NULL;
	}

	if ((old_msg != NULL) && (old_msg->cm_fields['M'] == NULL)) {	/* old version is corrupt? */
		CtdlFreeMessage(old_msg);
		old_msg = NULL;
	}
	
	/* If no changes were made, don't bother saving it again */
	if ((old_msg != NULL) && (!strcmp(msg->cm_fields['M'], old_msg->cm_fields['M']))) {
		CtdlFreeMessage(old_msg);
		return(1);
	}

	/*
	 * Generate diffs
	 */
	CtdlMakeTempFileName(diff_old_filename, sizeof diff_old_filename);
	CtdlMakeTempFileName(diff_new_filename, sizeof diff_new_filename);

	if (old_msg != NULL) {
		fp = fopen(diff_old_filename, "w");
		rv = fwrite(old_msg->cm_fields['M'], strlen(old_msg->cm_fields['M']), 1, fp);
		fclose(fp);
		CtdlFreeMessage(old_msg);
	}

	fp = fopen(diff_new_filename, "w");
	rv = fwrite(msg->cm_fields['M'], strlen(msg->cm_fields['M']), 1, fp);
	fclose(fp);

	diffbuf_len = 0;
	diffbuf = NULL;
	snprintf(diff_cmd, sizeof diff_cmd,
		"diff -u %s %s",
		diff_new_filename,
		((old_msg != NULL) ? diff_old_filename : "/dev/null")
	);
	fp = popen(diff_cmd, "r");
	if (fp != NULL) {
		do {
			diffbuf = realloc(diffbuf, diffbuf_len + 1025);
			nbytes = fread(&diffbuf[diffbuf_len], 1, 1024, fp);
			diffbuf_len += nbytes;
		} while (nbytes == 1024);
		diffbuf[diffbuf_len] = 0;
		pclose(fp);
	}
	CtdlLogPrintf(CTDL_DEBUG, "diff length is %d bytes\n", diffbuf_len);

	unlink(diff_old_filename);
	unlink(diff_new_filename);

	/* Determine whether this was a bogus (empty) edit */
	if ((diffbuf_len = 0) && (diffbuf != NULL)) {
		free(diffbuf);
		diffbuf = NULL;
	}
	if (diffbuf == NULL) {
		return(1);		/* No changes at all?  Abandon the post entirely! */
	}

	/* Now look for the existing edit history */

	history_msgnum = locate_message_by_euid(history_page, &CCC->room);
	history_msg = NULL;
	if (history_msgnum > 0L) {
		history_msg = CtdlFetchMessage(history_msgnum, 1);
	}

	/* Create a new history message if necessary */
	if (history_msg == NULL) {
		history_msg = malloc(sizeof(struct CtdlMessage));
		memset(history_msg, 0, sizeof(struct CtdlMessage));
		history_msg->cm_magic = CTDLMESSAGE_MAGIC;
		history_msg->cm_anon_type = MES_NORMAL;
		history_msg->cm_format_type = FMT_RFC822;
		history_msg->cm_fields['A'] = strdup("Citadel");
		history_msg->cm_fields['R'] = strdup(CCC->room.QRname);
		history_msg->cm_fields['E'] = strdup(history_page);
		history_msg->cm_fields['U'] = strdup(history_page);
		snprintf(boundary, sizeof boundary, "Citadel--Multipart--%04x--%08lx", getpid(), time(NULL));
		history_msg->cm_fields['M'] = malloc(1024);
		snprintf(history_msg->cm_fields['M'], 1024,
			"Content-type: multipart/mixed; boundary=\"%s\"\n\n"
			"This is a Citadel wiki history encoded as multipart MIME.\n"
			"Each part is comprised of a diff script representing one change set.\n"
			"\n"
			"--%s--\n"
			,
			boundary, boundary
		);
	}

	/* Update the history message (regardless of whether it's new or existing) */

	/* First, figure out the boundary string.  We do this even when we generated the
	 * boundary string in the above code, just to be safe and consistent.
	 */
	strcpy(boundary, "");

	ptr = history_msg->cm_fields['M'];
	do {
		ptr = memreadline(ptr, buf, sizeof buf);
		if (*ptr != 0) {
			striplt(buf);
			if (!IsEmptyStr(buf) && (!strncasecmp(buf, "Content-type:", 13))) {
				if (
					(bmstrcasestr(buf, "multipart") != NULL)
					&& (bmstrcasestr(buf, "boundary=") != NULL)
				) {
					safestrncpy(boundary, bmstrcasestr(buf, "\""), sizeof boundary);
					char *qu;
					qu = strchr(boundary, '\"');
					if (qu) {
						strcpy(boundary, ++qu);
					}
					qu = strchr(boundary, '\"');
					if (qu) {
						*qu = 0;
					}
				}
			}
		}
	} while ( (IsEmptyStr(boundary)) && (*ptr != 0) );

	/* Now look for the first boundary.  That is where we need to insert our fun.
	 */
	if (!IsEmptyStr(boundary)) {
		snprintf(prefixed_boundary, sizeof prefixed_boundary, "--%s", boundary);
		history_msg->cm_fields['M'] = realloc(history_msg->cm_fields['M'],
			strlen(history_msg->cm_fields['M']) + strlen(diffbuf) + 1024
		);
		ptr = bmstrcasestr(history_msg->cm_fields['M'], prefixed_boundary);
		if (ptr != NULL) {
			char *the_rest_of_it = strdup(ptr);
			char uuid[32];
			char memo[512];
			char encoded_memo[768];
			generate_uuid(uuid);
			snprintf(memo, sizeof memo, "%s|%ld|%s|%s", 
				uuid,
				time(NULL),
				CCC->user.fullname,
				config.c_nodename
				/* no longer logging CCC->cs_inet_email */
			);
			CtdlEncodeBase64(encoded_memo, memo, strlen(memo), 0);
			sprintf(ptr, "--%s\n"
					"Content-type: text/plain\n"
					"Content-Disposition: inline; filename=\"%s\"\n"
					"Content-Transfer-Encoding: 8bit\n"
					"\n"
					"%s\n"
					"%s"
					,
				boundary,
				encoded_memo,
				diffbuf,
				the_rest_of_it
			);
			free(the_rest_of_it);
		}

		history_msg->cm_fields['T'] = realloc(history_msg->cm_fields['T'], 32);
		snprintf(history_msg->cm_fields['T'], 32, "%ld", time(NULL));
	
		CtdlSubmitMsg(history_msg, NULL, "", 0);
	}
	else {
		CtdlLogPrintf(CTDL_ALERT, "Empty boundary string in history message.  No history!\n");
	}

	free(diffbuf);
	free(history_msg);
	return(0);
}


/*
 * MIME Parser callback for wiki_history()
 *
 * The "filename" field will contain a memo field.  All we have to do is decode
 * the base64 and output it.  The data is already in a delimited format suitable
 * for our client protocol.
 */
void wiki_history_callback(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	char memo[1024];

	CtdlDecodeBase64(memo, filename, strlen(filename));
	cprintf("%s\n", memo);
}


/*
 * Fetch a list of revisions for a particular wiki page
 */
void wiki_history(char *pagename) {
	int r;
	char history_page_name[270];
	long msgnum;
	struct CtdlMessage *msg;

	r = CtdlDoIHavePermissionToReadMessagesInThisRoom();
	if (r != om_ok) {
		if (r == om_not_logged_in) {
			cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		}
		else {
			cprintf("%d An unknown error has occurred.\n", ERROR);
		}
		return;
	}

	snprintf(history_page_name, sizeof history_page_name, "%s_HISTORY_", pagename);
	msgnum = locate_message_by_euid(history_page_name, &CC->room);
	if (msgnum > 0L) {
		msg = CtdlFetchMessage(msgnum, 1);
	}
	else {
		msg = NULL;
	}

	if ((msg != NULL) && (msg->cm_fields['M'] == NULL)) {
		CtdlFreeMessage(msg);
		msg = NULL;
	}

	if (msg == NULL) {
		cprintf("%d Revision history for '%s' was not found.\n", ERROR+MESSAGE_NOT_FOUND, pagename);
		return;
	}

	
	cprintf("%d Revision history for '%s'\n", LISTING_FOLLOWS, pagename);
	mime_parser(msg->cm_fields['M'], NULL, *wiki_history_callback, NULL, NULL, NULL, 0);
	cprintf("000\n");

	CtdlFreeMessage(msg);
	return;
}



/*
 * Fetch a specific revision of a wiki page
 */
void wiki_rev(char *pagename, char *rev)
{
	int r;
	char history_page_name[270];
	long msgnum;
	struct CtdlMessage *msg;

	r = CtdlDoIHavePermissionToReadMessagesInThisRoom();
	if (r != om_ok) {
		if (r == om_not_logged_in) {
			cprintf("%d Not logged in.\n", ERROR + NOT_LOGGED_IN);
		}
		else {
			cprintf("%d An unknown error has occurred.\n", ERROR);
		}
		return;
	}

	snprintf(history_page_name, sizeof history_page_name, "%s_HISTORY_", pagename);
	msgnum = locate_message_by_euid(history_page_name, &CC->room);
	if (msgnum > 0L) {
		msg = CtdlFetchMessage(msgnum, 1);
	}
	else {
		msg = NULL;
	}

	if ((msg != NULL) && (msg->cm_fields['M'] == NULL)) {
		CtdlFreeMessage(msg);
		msg = NULL;
	}

	if (msg == NULL) {
		cprintf("%d Revision history for '%s' was not found.\n", ERROR+MESSAGE_NOT_FOUND, pagename);
		return;
	}

	
/*
   FIXME do something here
	mime_parser(msg->cm_fields['M'], NULL, *wiki_history_callback, NULL, NULL, NULL, 0);
 */

	CtdlFreeMessage(msg);
	cprintf("%d FIXME not finished yet, check the log to find out wtf\n", ERROR);
	return;
}



/*
 * commands related to wiki management
 */
void cmd_wiki(char *argbuf) {
	char subcmd[32];
	char pagename[256];
	char rev[128];

	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);

	if (!strcasecmp(subcmd, "history")) {
		extract_token(pagename, argbuf, 1, '|', sizeof pagename);
		wiki_history(pagename);
		return;
	}

	if (!strcasecmp(subcmd, "rev")) {
		extract_token(pagename, argbuf, 1, '|', sizeof pagename);
		extract_token(rev, argbuf, 2, '|', sizeof rev);
		wiki_rev(pagename, rev);
		return;
	}

	cprintf("%d Invalid subcommand\n", ERROR + CMD_NOT_SUPPORTED);
}



/*
 * Module initialization
 */
CTDL_MODULE_INIT(wiki)
{
	if (!threading)
	{
		CtdlRegisterMessageHook(wiki_upload_beforesave, EVT_BEFORESAVE);
	        CtdlRegisterProtoHook(cmd_wiki, "WIKI", "Commands related to Wiki management");
	}

	/* return our Subversion id for the Log */
	return "$Id$";
}
