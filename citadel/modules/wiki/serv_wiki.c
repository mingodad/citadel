/*
 * Server-side module for Wiki rooms.  This handles things like version control. 
 * 
 * Copyright (c) 2009-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
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
#include "room_ops.h"
#include "database.h"
#include "msgbase.h"
#include "euidindex.h"
#include "ctdl_module.h"

/*
 * Data passed back and forth between wiki_rev() and its MIME parser callback
 */
struct HistoryEraserCallBackData {
	char *tempfilename;		/* name of temp file being patched */
	char *stop_when;		/* stop when we hit this uuid */
	int done;			/* set to nonzero when we're done patching */
};

/*
 * Name of the temporary room we create to store old revisions when someone requests them.
 * We put it in an invalid namespace so the DAP cleans up after us later.
 */
char *wwm = "9999999999.WikiWaybackMachine";

/*
 * Before allowing a wiki page save to execute, we have to perform version control.
 * This involves fetching the old version of the page if it exists.
 */
int wiki_upload_beforesave(struct CtdlMessage *msg, recptypes *recp) {
	struct CitContext *CCC = CC;
	long old_msgnum = (-1L);
	struct CtdlMessage *old_msg = NULL;
	long history_msgnum = (-1L);
	struct CtdlMessage *history_msg = NULL;
	char diff_old_filename[PATH_MAX];
	char diff_new_filename[PATH_MAX];
	char diff_out_filename[PATH_MAX];
	char diff_cmd[PATH_MAX];
	FILE *fp;
	int rv;
	char history_page[1024];
	long history_page_len;
	char boundary[256];
	char prefixed_boundary[258];
	char buf[1024];
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

	/* If there's no EUID we can't do this.  Reject the post. */
	if (CM_IsEmpty(msg, eExclusiveID)) return(1);

	history_page_len = snprintf(history_page, sizeof history_page,
				    "%s_HISTORY_", msg->cm_fields[eExclusiveID]);

	/* Make sure we're saving a real wiki page rather than a wiki history page.
	 * This is important in order to avoid recursing infinitely into this hook.
	 */
	if (	(msg->cm_lengths[eExclusiveID] >= 9)
		&& (!strcasecmp(&msg->cm_fields[eExclusiveID][msg->cm_lengths[eExclusiveID]-9], "_HISTORY_"))
	) {
		syslog(LOG_DEBUG, "History page not being historied\n");
		return(0);
	}

	/* If there's no message text, obviously this is all b0rken and shouldn't happen at all */
	if (CM_IsEmpty(msg, eMesageText)) return(0);

	/* Set the message subject identical to the page name */
	CM_CopyField(msg, eMsgSubject, eExclusiveID);

	/* See if we can retrieve the previous version. */
	old_msgnum = CtdlLocateMessageByEuid(msg->cm_fields[eExclusiveID], &CCC->room);
	if (old_msgnum > 0L) {
		old_msg = CtdlFetchMessage(old_msgnum, 1);
	}
	else {
		old_msg = NULL;
	}

	if ((old_msg != NULL) && (CM_IsEmpty(old_msg, eMesageText))) {	/* old version is corrupt? */
		CM_Free(old_msg);
		old_msg = NULL;
	}
	
	/* If no changes were made, don't bother saving it again */
	if ((old_msg != NULL) && (!strcmp(msg->cm_fields[eMesageText], old_msg->cm_fields[eMesageText]))) {
		CM_Free(old_msg);
		return(1);
	}

	/*
	 * Generate diffs
	 */
	CtdlMakeTempFileName(diff_old_filename, sizeof diff_old_filename);
	CtdlMakeTempFileName(diff_new_filename, sizeof diff_new_filename);
	CtdlMakeTempFileName(diff_out_filename, sizeof diff_out_filename);

	if (old_msg != NULL) {
		fp = fopen(diff_old_filename, "w");
		rv = fwrite(old_msg->cm_fields[eMesageText], old_msg->cm_lengths[eMesageText], 1, fp);
		fclose(fp);
		CM_Free(old_msg);
	}

	fp = fopen(diff_new_filename, "w");
	rv = fwrite(msg->cm_fields[eMesageText], msg->cm_lengths[eMesageText], 1, fp);
	fclose(fp);

	snprintf(diff_cmd, sizeof diff_cmd,
		DIFF " -u %s %s >%s",
		diff_new_filename,
		((old_msg != NULL) ? diff_old_filename : "/dev/null"),
		diff_out_filename
	);
	syslog(LOG_DEBUG, "diff cmd: %s", diff_cmd);
	rv = system(diff_cmd);
	syslog(LOG_DEBUG, "diff cmd returned %d", rv);

	diffbuf_len = 0;
	diffbuf = NULL;
	fp = fopen(diff_out_filename, "r");
	if (fp == NULL) {
		fp = fopen("/dev/null", "r");
	}
	if (fp != NULL) {
		fseek(fp, 0L, SEEK_END);
		diffbuf_len = ftell(fp);
		fseek(fp, 0L, SEEK_SET);
		diffbuf = malloc(diffbuf_len + 1);
		fread(diffbuf, diffbuf_len, 1, fp);
		diffbuf[diffbuf_len] = '\0';
		fclose(fp);
	}

	syslog(LOG_DEBUG, "diff length is "SIZE_T_FMT" bytes", diffbuf_len);

	unlink(diff_old_filename);
	unlink(diff_new_filename);
	unlink(diff_out_filename);

	/* Determine whether this was a bogus (empty) edit */
	if ((diffbuf_len = 0) && (diffbuf != NULL)) {
		free(diffbuf);
		diffbuf = NULL;
	}
	if (diffbuf == NULL) {
		return(1);		/* No changes at all?  Abandon the post entirely! */
	}

	/* Now look for the existing edit history */

	history_msgnum = CtdlLocateMessageByEuid(history_page, &CCC->room);
	history_msg = NULL;
	if (history_msgnum > 0L) {
		history_msg = CtdlFetchMessage(history_msgnum, 1);
	}

	/* Create a new history message if necessary */
	if (history_msg == NULL) {
		char *buf;
		long len;

		history_msg = malloc(sizeof(struct CtdlMessage));
		memset(history_msg, 0, sizeof(struct CtdlMessage));
		history_msg->cm_magic = CTDLMESSAGE_MAGIC;
		history_msg->cm_anon_type = MES_NORMAL;
		history_msg->cm_format_type = FMT_RFC822;
		CM_SetField(history_msg, eAuthor, HKEY("Citadel"));
		CM_SetField(history_msg, eRecipient, CCC->room.QRname, strlen(CCC->room.QRname));
		CM_SetField(history_msg, eExclusiveID, history_page, history_page_len);
		CM_SetField(history_msg, eMsgSubject, history_page, history_page_len);
		CM_SetField(history_msg, eSuppressIdx, HKEY("1")); /* suppress full text indexing */
		snprintf(boundary, sizeof boundary, "Citadel--Multipart--%04x--%08lx", getpid(), time(NULL));
		buf = (char*) malloc(1024);
		len = snprintf(buf, 1024,
			       "Content-type: multipart/mixed; boundary=\"%s\"\n\n"
			       "This is a Citadel wiki history encoded as multipart MIME.\n"
			       "Each part is comprised of a diff script representing one change set.\n"
			       "\n"
			       "--%s--\n",
			       boundary, boundary
		);
		CM_SetAsField(history_msg, eMesageText, &buf, len);
	}

	/* Update the history message (regardless of whether it's new or existing) */

	/* Remove the Message-ID from the old version of the history message.  This will cause a brand
	 * new one to be generated, avoiding an uninitentional hit of the loop zapper when we replicate.
	 */
	CM_FlushField(history_msg, emessageId);

	/* Figure out the boundary string.  We do this even when we generated the
	 * boundary string in the above code, just to be safe and consistent.
	 */
	*boundary = '\0';

	ptr = history_msg->cm_fields[eMesageText];
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

	/*
	 * Now look for the first boundary.  That is where we need to insert our fun.
	 */
	if (!IsEmptyStr(boundary)) {
		char *MsgText;
		long MsgTextLen;
		time_t Now = time(NULL);

		snprintf(prefixed_boundary, sizeof(prefixed_boundary), "--%s", boundary);
		
		CM_GetAsField(history_msg, eMesageText, &MsgText, &MsgTextLen);

		ptr = bmstrcasestr(MsgText, prefixed_boundary);
		if (ptr != NULL) {
			StrBuf *NewMsgText;
			char uuid[64];
			char memo[512];
			long memolen;
			char encoded_memo[1024];
			
			NewMsgText = NewStrBufPlain(NULL, MsgTextLen + diffbuf_len + 1024);

			generate_uuid(uuid);
			memolen = snprintf(memo, sizeof(memo), "%s|%ld|%s|%s", 
					   uuid,
					   Now,
					   CCC->user.fullname,
					   config.c_nodename);

			memolen = CtdlEncodeBase64(encoded_memo, memo, memolen, 0);

			StrBufAppendBufPlain(NewMsgText, HKEY("--"), 0);
			StrBufAppendBufPlain(NewMsgText, boundary, -1, 0);
			StrBufAppendBufPlain(
				NewMsgText, 
				HKEY("\n"
				     "Content-type: text/plain\n"
				     "Content-Disposition: inline; filename=\""), 0);

			StrBufAppendBufPlain(NewMsgText, encoded_memo, memolen, 0);

			StrBufAppendBufPlain(
				NewMsgText, 
				HKEY("\"\n"
				     "Content-Transfer-Encoding: 8bit\n"
				     "\n"), 0);

			StrBufAppendBufPlain(NewMsgText, diffbuf, diffbuf_len, 0);
			StrBufAppendBufPlain(NewMsgText, HKEY("\n"), 0);

			StrBufAppendBufPlain(NewMsgText, ptr, MsgTextLen - (ptr - MsgText), 0);
			free(MsgText);
			CM_SetAsFieldSB(history_msg, eMesageText, &NewMsgText); 
		}
		else
		{
			CM_SetAsField(history_msg, eMesageText, &MsgText, MsgTextLen); 
		}

		CM_SetFieldLONG(history_msg, eTimestamp, Now);
	
		CtdlSubmitMsg(history_msg, NULL, "", 0);
	}
	else {
		syslog(LOG_ALERT, "Empty boundary string in history message.  No history!\n");
	}

	free(diffbuf);
	CM_Free(history_msg);
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
	msgnum = CtdlLocateMessageByEuid(history_page_name, &CC->room);
	if (msgnum > 0L) {
		msg = CtdlFetchMessage(msgnum, 1);
	}
	else {
		msg = NULL;
	}

	if ((msg != NULL) && CM_IsEmpty(msg, eMesageText)) {
		CM_Free(msg);
		msg = NULL;
	}

	if (msg == NULL) {
		cprintf("%d Revision history for '%s' was not found.\n", ERROR+MESSAGE_NOT_FOUND, pagename);
		return;
	}

	
	cprintf("%d Revision history for '%s'\n", LISTING_FOLLOWS, pagename);
	mime_parser(msg->cm_fields[eMesageText], NULL, *wiki_history_callback, NULL, NULL, NULL, 0);
	cprintf("000\n");

	CM_Free(msg);
	return;
}

/*
 * MIME Parser callback for wiki_rev()
 *
 * The "filename" field will contain a memo field, which includes (among other things)
 * the uuid of this revision.  After we hit the desired revision, we stop processing.
 *
 * The "content" filed will contain "diff" output suitable for applying via "patch"
 * to our temporary file.
 */
void wiki_rev_callback(char *name, char *filename, char *partnum, char *disp,
		   void *content, char *cbtype, char *cbcharset, size_t length,
		   char *encoding, char *cbid, void *cbuserdata)
{
	struct HistoryEraserCallBackData *hecbd = (struct HistoryEraserCallBackData *)cbuserdata;
	char memo[1024];
	char this_rev[256];
	FILE *fp;
	char *ptr = NULL;
	char buf[1024];

	/* Did a previous callback already indicate that we've reached our target uuid?
	 * If so, don't process anything else.
	 */
	if (hecbd->done) {
		return;
	}

	CtdlDecodeBase64(memo, filename, strlen(filename));
	extract_token(this_rev, memo, 0, '|', sizeof this_rev);
	striplt(this_rev);

	/* Perform the patch */
	fp = popen(PATCH " -f -s -p0 -r /dev/null >/dev/null 2>/dev/null", "w");
	if (fp) {
		/* Replace the filenames in the patch with the tempfilename we're actually tweaking */
		fprintf(fp, "--- %s\n", hecbd->tempfilename);
		fprintf(fp, "+++ %s\n", hecbd->tempfilename);

		ptr = (char *)content;
		int linenum = 0;
		do {
			++linenum;
			ptr = memreadline(ptr, buf, sizeof buf);
			if (*ptr != 0) {
				if (linenum <= 2) {
					/* skip the first two lines; they contain bogus filenames */
				}
				else {
					fprintf(fp, "%s\n", buf);
				}
			}
		} while ((*ptr != 0) && (ptr < ((char*)content + length)));
		if (pclose(fp) != 0) {
			syslog(LOG_ERR, "pclose() returned an error - patch failed\n");
		}
	}

	if (!strcasecmp(this_rev, hecbd->stop_when)) {
		/* Found our target rev.  Tell any subsequent callbacks to suppress processing. */
		syslog(LOG_DEBUG, "Target revision has been reached -- stop patching.\n");
		hecbd->done = 1;
	}
}


/*
 * Fetch a specific revision of a wiki page.  The "operation" string may be set to "fetch" in order
 * to simply fetch the desired revision and store it in a temporary location for viewing, or "revert"
 * to revert the currently active page to that revision.
 */
void wiki_rev(char *pagename, char *rev, char *operation)
{
	struct CitContext *CCC = CC;
	int r;
	char history_page_name[270];
	long msgnum;
	char temp[PATH_MAX];
	struct CtdlMessage *msg;
	FILE *fp;
	struct HistoryEraserCallBackData hecbd;
	long len = 0L;
	int rv;

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

	if (!strcasecmp(operation, "revert")) {
		r = CtdlDoIHavePermissionToPostInThisRoom(temp, sizeof temp, NULL, POST_LOGGED_IN, 0);
		if (r != 0) {
			cprintf("%d %s\n", r, temp);
			return;
		}
	}

	/* Begin by fetching the current version of the page.  We're going to patch
	 * backwards through the diffs until we get the one we want.
	 */
	msgnum = CtdlLocateMessageByEuid(pagename, &CCC->room);
	if (msgnum > 0L) {
		msg = CtdlFetchMessage(msgnum, 1);
	}
	else {
		msg = NULL;
	}

	if ((msg != NULL) && CM_IsEmpty(msg, eMesageText)) {
		CM_Free(msg);
		msg = NULL;
	}

	if (msg == NULL) {
		cprintf("%d Page '%s' was not found.\n", ERROR+MESSAGE_NOT_FOUND, pagename);
		return;
	}

	/* Output it to a temporary file */

	CtdlMakeTempFileName(temp, sizeof temp);
	fp = fopen(temp, "w");
	if (fp != NULL) {
		r = fwrite(msg->cm_fields[eMesageText], msg->cm_lengths[eMesageText], 1, fp);
		fclose(fp);
	}
	else {
		syslog(LOG_ALERT, "Cannot open %s: %s\n", temp, strerror(errno));
	}
	CM_Free(msg);

	/* Get the revision history */

	snprintf(history_page_name, sizeof history_page_name, "%s_HISTORY_", pagename);
	msgnum = CtdlLocateMessageByEuid(history_page_name, &CCC->room);
	if (msgnum > 0L) {
		msg = CtdlFetchMessage(msgnum, 1);
	}
	else {
		msg = NULL;
	}

	if ((msg != NULL) && CM_IsEmpty(msg, eMesageText)) {
		CM_Free(msg);
		msg = NULL;
	}

	if (msg == NULL) {
		cprintf("%d Revision history for '%s' was not found.\n", ERROR+MESSAGE_NOT_FOUND, pagename);
		return;
	}

	/* Start patching backwards (newest to oldest) through the revision history until we
	 * hit the revision uuid requested by the user.  (The callback will perform each one.)
	 */

	memset(&hecbd, 0, sizeof(struct HistoryEraserCallBackData));
	hecbd.tempfilename = temp;
	hecbd.stop_when = rev;
	striplt(hecbd.stop_when);

	mime_parser(msg->cm_fields[eMesageText], NULL, *wiki_rev_callback, NULL, NULL, (void *)&hecbd, 0);
	CM_Free(msg);

	/* Were we successful? */
	if (hecbd.done == 0) {
		cprintf("%d Revision '%s' of page '%s' was not found.\n",
			ERROR + MESSAGE_NOT_FOUND, rev, pagename
		);
	}

	/* We have the desired revision on disk.  Now do something with it. */

	else if ( (!strcasecmp(operation, "fetch")) || (!strcasecmp(operation, "revert")) ) {
		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = FMT_RFC822;
		fp = fopen(temp, "r");
		if (fp) {
			char *msgbuf;
			fseek(fp, 0L, SEEK_END);
			len = ftell(fp);
			fseek(fp, 0L, SEEK_SET);
			msgbuf = malloc(len + 1);
			rv = fread(msgbuf, len, 1, fp);
			syslog(LOG_DEBUG, "did %d blocks of %ld bytes\n", rv, len);
			msgbuf[len] = '\0';
			CM_SetAsField(msg, eMesageText, &msgbuf, len);
			fclose(fp);
		}
		if (len <= 0) {
			msgnum = (-1L);
		}
		else if (!strcasecmp(operation, "fetch")) {
			CM_SetField(msg, eAuthor, HKEY("Citadel"));
			CtdlCreateRoom(wwm, 5, "", 0, 1, 1, VIEW_BBS);	/* Not an error if already exists */
			msgnum = CtdlSubmitMsg(msg, NULL, wwm, 0);	/* Store the revision here */

			/*
			 * WARNING: VILE SLEAZY HACK
			 * This will avoid the 'message xxx is not in this room' security error,
			 * but only if the client fetches the message we just generated immediately
			 * without first trying to perform other fetch operations.
			 */
			if (CCC->cached_msglist != NULL) {
				free(CCC->cached_msglist);
				CCC->cached_msglist = NULL;
				CCC->cached_num_msgs = 0;
			}
			CCC->cached_msglist = malloc(sizeof(long));
			if (CCC->cached_msglist != NULL) {
				CCC->cached_num_msgs = 1;
				CCC->cached_msglist[0] = msgnum;
			}

		}
		else if (!strcasecmp(operation, "revert")) {
			CM_SetFieldLONG(msg, eTimestamp, time(NULL));
			CM_SetField(msg, eAuthor, CCC->user.fullname, strlen(CCC->user.fullname));
			CM_SetField(msg, erFc822Addr, CCC->cs_inet_email, strlen(CCC->cs_inet_email));
			CM_SetField(msg, eOriginalRoom, CCC->room.QRname, strlen(CCC->room.QRname));
			CM_SetField(msg, eNodeName, NODENAME, strlen(NODENAME));
			CM_SetField(msg, eExclusiveID, pagename, strlen(pagename));
			msgnum = CtdlSubmitMsg(msg, NULL, "", 0);	/* Replace the current revision */
		}
		else {
			/* Theoretically it is impossible to get here, but throw an error anyway */
			msgnum = (-1L);
		}
		CM_Free(msg);
		if (msgnum >= 0L) {
			cprintf("%d %ld\n", CIT_OK, msgnum);		/* Give the client a msgnum */
		}
		else {
			cprintf("%d Error %ld has occurred.\n", ERROR+INTERNAL_ERROR, msgnum);
		}
	}

	/* We did all this work for nothing.  Express anguish to the caller. */
	else {
		cprintf("%d An unknown operation was requested.\n", ERROR+CMD_NOT_SUPPORTED);
	}

	unlink(temp);
	return;
}



/*
 * commands related to wiki management
 */
void cmd_wiki(char *argbuf) {
	char subcmd[32];
	char pagename[256];
	char rev[128];
	char operation[16];

	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);

	if (!strcasecmp(subcmd, "history")) {
		extract_token(pagename, argbuf, 1, '|', sizeof pagename);
		wiki_history(pagename);
		return;
	}

	if (!strcasecmp(subcmd, "rev")) {
		extract_token(pagename, argbuf, 1, '|', sizeof pagename);
		extract_token(rev, argbuf, 2, '|', sizeof rev);
		extract_token(operation, argbuf, 3, '|', sizeof operation);
		wiki_rev(pagename, rev, operation);
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

	/* return our module name for the log */
	return "wiki";
}
