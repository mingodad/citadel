/*
 * represent messages to the citadel clients
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <libcitadel.h>

#include "citserver.h"
#include "ctdl_module.h"
#include "internet_addressing.h"
#include "user_ops.h"
#include "room_ops.h"
#include "config.h"

extern char *msgkeys[];


/*
 * Back end for the MSGS command: output message number only.
 */
void simple_listing(long msgnum, void *userdata)
{
	cprintf("%ld\n", msgnum);
}


/*
 * Back end for the MSGS command: output header summary.
 */
void headers_listing(long msgnum, void *userdata)
{
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) {
		cprintf("%ld|0|||||\n", msgnum);
		return;
	}

	cprintf("%ld|%s|%s|%s|%s|%s|\n",
		msgnum,
		(!CM_IsEmpty(msg, eTimestamp) ? msg->cm_fields[eTimestamp] : "0"),
		(!CM_IsEmpty(msg, eAuthor) ? msg->cm_fields[eAuthor] : ""),
		(!CM_IsEmpty(msg, eNodeName) ? msg->cm_fields[eNodeName] : ""),
		(!CM_IsEmpty(msg, erFc822Addr) ? msg->cm_fields[erFc822Addr] : ""),
		(!CM_IsEmpty(msg, eMsgSubject) ? msg->cm_fields[eMsgSubject] : "")
	);
	CM_Free(msg);
}

/*
 * Back end for the MSGS command: output EUID header.
 */
void headers_euid(long msgnum, void *userdata)
{
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) {
		cprintf("%ld||\n", msgnum);
		return;
	}

	cprintf("%ld|%s|%s\n", 
		msgnum, 
		(!CM_IsEmpty(msg, eExclusiveID) ? msg->cm_fields[eExclusiveID] : ""),
		(!CM_IsEmpty(msg, eTimestamp) ? msg->cm_fields[eTimestamp] : "0"));
	CM_Free(msg);
}



/*
 * cmd_msgs()  -  get list of message #'s in this room
 *		implements the MSGS server command using CtdlForEachMessage()
 */
void cmd_msgs(char *cmdbuf)
{
	int mode = 0;
	char which[16];
	char buf[256];
	char tfield[256];
	char tvalue[256];
	int cm_ref = 0;
	int i;
	int with_template = 0;
	struct CtdlMessage *template = NULL;
	char search_string[1024];
	ForEachMsgCallback CallBack;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	extract_token(which, cmdbuf, 0, '|', sizeof which);
	cm_ref = extract_int(cmdbuf, 1);
	extract_token(search_string, cmdbuf, 1, '|', sizeof search_string);
	with_template = extract_int(cmdbuf, 2);
	switch (extract_int(cmdbuf, 3))
	{
	default:
	case MSG_HDRS_BRIEF:
		CallBack = simple_listing;
		break;
	case MSG_HDRS_ALL:
		CallBack = headers_listing;
		break;
	case MSG_HDRS_EUID:
		CallBack = headers_euid;
		break;
	}

	strcat(which, "   ");
	if (!strncasecmp(which, "OLD", 3))
		mode = MSGS_OLD;
	else if (!strncasecmp(which, "NEW", 3))
		mode = MSGS_NEW;
	else if (!strncasecmp(which, "FIRST", 5))
		mode = MSGS_FIRST;
	else if (!strncasecmp(which, "LAST", 4))
		mode = MSGS_LAST;
	else if (!strncasecmp(which, "GT", 2))
		mode = MSGS_GT;
	else if (!strncasecmp(which, "LT", 2))
		mode = MSGS_LT;
	else if (!strncasecmp(which, "SEARCH", 6))
		mode = MSGS_SEARCH;
	else
		mode = MSGS_ALL;

	if ( (mode == MSGS_SEARCH) && (!CtdlGetConfigInt("c_enable_fulltext")) ) {
		cprintf("%d Full text index is not enabled on this server.\n",
			ERROR + CMD_NOT_SUPPORTED);
		return;
	}

	if (with_template) {
		unbuffer_output();
		cprintf("%d Send template then receive message list\n",
			START_CHAT_MODE);
		template = (struct CtdlMessage *)
			malloc(sizeof(struct CtdlMessage));
		memset(template, 0, sizeof(struct CtdlMessage));
		template->cm_magic = CTDLMESSAGE_MAGIC;
		template->cm_anon_type = MES_NORMAL;

		while(client_getln(buf, sizeof buf) >= 0 && strcmp(buf,"000")) {
			long tValueLen;
			extract_token(tfield, buf, 0, '|', sizeof tfield);
			tValueLen = extract_token(tvalue, buf, 1, '|', sizeof tvalue);
			for (i='A'; i<='Z'; ++i) if (msgkeys[i]!=NULL) {
				if (!strcasecmp(tfield, msgkeys[i])) {
					CM_SetField(template, i, tvalue, tValueLen);
				}
			}
		}
		buffer_output();
	}
	else {
		cprintf("%d  \n", LISTING_FOLLOWS);
	}

	CtdlForEachMessage(mode,
			   ( (mode == MSGS_SEARCH) ? 0 : cm_ref ),
			   ( (mode == MSGS_SEARCH) ? search_string : NULL ),
			   NULL,
			   template,
			   CallBack,
			   NULL);
	if (template != NULL) CM_Free(template);
	cprintf("000\n");
}

/*
 * display a message (mode 0 - Citadel proprietary)
 */
void cmd_msg0(char *cmdbuf)
{
	long msgid;
	int headers_only = HEADERS_ALL;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_CITADEL, headers_only, 1, 0, NULL, 0, NULL, NULL, NULL);
	return;
}


/*
 * display a message (mode 2 - RFC822)
 */
void cmd_msg2(char *cmdbuf)
{
	long msgid;
	int headers_only = HEADERS_ALL;

	msgid = extract_long(cmdbuf, 0);
	headers_only = extract_int(cmdbuf, 1);

	CtdlOutputMsg(msgid, MT_RFC822, headers_only, 1, 1, NULL, 0, NULL, NULL, NULL);
}



/* 
 * display a message (mode 3 - IGnet raw format - internal programs only)
 */
void cmd_msg3(char *cmdbuf)
{
	long msgnum;
	struct CtdlMessage *msg = NULL;
	struct ser_ret smr;

	if (CC->internal_pgm == 0) {
		cprintf("%d This command is for internal programs only.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	msgnum = extract_long(cmdbuf, 0);
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n", 
			ERROR + MESSAGE_NOT_FOUND, msgnum);
		return;
	}

	CtdlSerializeMessage(&smr, msg);
	CM_Free(msg);

	if (smr.len == 0) {
		cprintf("%d Unable to serialize message\n",
			ERROR + INTERNAL_ERROR);
		return;
	}

	cprintf("%d %ld\n", BINARY_FOLLOWS, (long)smr.len);
	client_write((char *)smr.ser, (int)smr.len);
	free(smr.ser);
}



/* 
 * Display a message using MIME content types
 */
void cmd_msg4(char *cmdbuf)
{
	long msgid;
	char section[64];

	msgid = extract_long(cmdbuf, 0);
	extract_token(section, cmdbuf, 1, '|', sizeof section);
	CtdlOutputMsg(msgid, MT_MIME, 0, 1, 0, (section[0] ? section : NULL) , 0, NULL, NULL, NULL);
}



/* 
 * Client tells us its preferred message format(s)
 */
void cmd_msgp(char *cmdbuf)
{
	if (!strcasecmp(cmdbuf, "dont_decode")) {
		CC->msg4_dont_decode = 1;
		cprintf("%d MSG4 will not pre-decode messages.\n", CIT_OK);
	}
	else {
		safestrncpy(CC->preferred_formats, cmdbuf, sizeof(CC->preferred_formats));
		cprintf("%d Preferred MIME formats have been set.\n", CIT_OK);
	}
}


/*
 * Open a component of a MIME message as a download file 
 */
void cmd_opna(char *cmdbuf)
{
	long msgid;
	char desired_section[128];

	msgid = extract_long(cmdbuf, 0);
	extract_token(desired_section, cmdbuf, 1, '|', sizeof desired_section);
	safestrncpy(CC->download_desired_section, desired_section,
		sizeof CC->download_desired_section);
	CtdlOutputMsg(msgid, MT_DOWNLOAD, 0, 1, 1, NULL, 0, NULL, NULL, NULL);
}			


/*
 * Open a component of a MIME message and transmit it all at once
 */
void cmd_dlat(char *cmdbuf)
{
	long msgid;
	char desired_section[128];

	msgid = extract_long(cmdbuf, 0);
	extract_token(desired_section, cmdbuf, 1, '|', sizeof desired_section);
	safestrncpy(CC->download_desired_section, desired_section,
		sizeof CC->download_desired_section);
	CtdlOutputMsg(msgid, MT_SPEW_SECTION, 0, 1, 1, NULL, 0, NULL, NULL, NULL);
}

/*
 * message entry  -  mode 0 (normal)
 */
void cmd_ent0(char *entargs)
{
	struct CitContext *CCC = CC;
	int post = 0;
	char recp[SIZ];
	char cc[SIZ];
	char bcc[SIZ];
	char supplied_euid[128];
	int anon_flag = 0;
	int format_type = 0;
	char newusername[256];
	char newuseremail[256];
	struct CtdlMessage *msg;
	int anonymous = 0;
	char errmsg[SIZ];
	int err = 0;
	recptypes *valid = NULL;
	recptypes *valid_to = NULL;
	recptypes *valid_cc = NULL;
	recptypes *valid_bcc = NULL;
	char subject[SIZ];
	int subject_required = 0;
	int do_confirm = 0;
	long msgnum;
	int i, j;
	char buf[256];
	int newuseremail_ok = 0;
	char references[SIZ];
	char *ptr;

	unbuffer_output();

	post = extract_int(entargs, 0);
	extract_token(recp, entargs, 1, '|', sizeof recp);
	anon_flag = extract_int(entargs, 2);
	format_type = extract_int(entargs, 3);
	extract_token(subject, entargs, 4, '|', sizeof subject);
	extract_token(newusername, entargs, 5, '|', sizeof newusername);
	do_confirm = extract_int(entargs, 6);
	extract_token(cc, entargs, 7, '|', sizeof cc);
	extract_token(bcc, entargs, 8, '|', sizeof bcc);
	switch(CC->room.QRdefaultview) {
	case VIEW_NOTES:
	case VIEW_WIKI:
	case VIEW_WIKIMD:
		extract_token(supplied_euid, entargs, 9, '|', sizeof supplied_euid);
		break;
	default:
		supplied_euid[0] = 0;
		break;
	}
	extract_token(newuseremail, entargs, 10, '|', sizeof newuseremail);
	extract_token(references, entargs, 11, '|', sizeof references);
	for (ptr=references; *ptr != 0; ++ptr) {
		if (*ptr == '!') *ptr = '|';
	}

	/* first check to make sure the request is valid. */

	err = CtdlDoIHavePermissionToPostInThisRoom(
		errmsg,
		sizeof errmsg,
		NULL,
		POST_LOGGED_IN,
		(!IsEmptyStr(references))		/* is this a reply?  or a top-level post? */
		);
	if (err)
	{
		cprintf("%d %s\n", err, errmsg);
		return;
	}

	/* Check some other permission type things. */

	if (IsEmptyStr(newusername)) {
		strcpy(newusername, CCC->user.fullname);
	}
	if (  (CCC->user.axlevel < AxAideU)
	      && (strcasecmp(newusername, CCC->user.fullname))
	      && (strcasecmp(newusername, CCC->cs_inet_fn))
		) {	
		cprintf("%d You don't have permission to author messages as '%s'.\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			newusername
			);
		return;
	}


	if (IsEmptyStr(newuseremail)) {
		newuseremail_ok = 1;
	}

	if (!IsEmptyStr(newuseremail)) {
		if (!strcasecmp(newuseremail, CCC->cs_inet_email)) {
			newuseremail_ok = 1;
		}
		else if (!IsEmptyStr(CCC->cs_inet_other_emails)) {
			j = num_tokens(CCC->cs_inet_other_emails, '|');
			for (i=0; i<j; ++i) {
				extract_token(buf, CCC->cs_inet_other_emails, i, '|', sizeof buf);
				if (!strcasecmp(newuseremail, buf)) {
					newuseremail_ok = 1;
				}
			}
		}
	}

	if (!newuseremail_ok) {
		cprintf("%d You don't have permission to author messages as '%s'.\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			newuseremail
			);
		return;
	}

	CCC->cs_flags |= CS_POSTING;

	/* In mailbox rooms we have to behave a little differently --
	 * make sure the user has specified at least one recipient.  Then
	 * validate the recipient(s).  We do this for the Mail> room, as
	 * well as any room which has the "Mailbox" view set - unless it
	 * is the DRAFTS room which does not require recipients
	 */

	if ( (  ( (CCC->room.QRflags & QR_MAILBOX) && (!strcasecmp(&CCC->room.QRname[11], MAILROOM)) )
		|| ( (CCC->room.QRflags & QR_MAILBOX) && (CCC->curr_view == VIEW_MAILBOX) )
		     ) && (strcasecmp(&CCC->room.QRname[11], USERDRAFTROOM)) !=0 ) {
		if (CCC->user.axlevel < AxProbU) {
			strcpy(recp, "sysop");
			strcpy(cc, "");
			strcpy(bcc, "");
		}

		valid_to = validate_recipients(recp, NULL, 0);
		if (valid_to->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_to->errormsg);
			free_recipients(valid_to);
			return;
		}

		valid_cc = validate_recipients(cc, NULL, 0);
		if (valid_cc->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_cc->errormsg);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			return;
		}

		valid_bcc = validate_recipients(bcc, NULL, 0);
		if (valid_bcc->num_error > 0) {
			cprintf("%d %s\n", ERROR + NO_SUCH_USER, valid_bcc->errormsg);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}

		/* Recipient required, but none were specified */
		if ( (valid_to->num_error < 0) && (valid_cc->num_error < 0) && (valid_bcc->num_error < 0) ) {
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			cprintf("%d At least one recipient is required.\n", ERROR + NO_SUCH_USER);
			return;
		}

		if (valid_to->num_internet + valid_cc->num_internet + valid_bcc->num_internet > 0) {
			if (CtdlCheckInternetMailPermission(&CCC->user)==0) {
				cprintf("%d You do not have permission "
					"to send Internet mail.\n",
					ERROR + HIGHER_ACCESS_REQUIRED);
				free_recipients(valid_to);
				free_recipients(valid_cc);
				free_recipients(valid_bcc);
				return;
			}
		}

		if ( ( (valid_to->num_internet + valid_to->num_ignet + valid_cc->num_internet + valid_cc->num_ignet + valid_bcc->num_internet + valid_bcc->num_ignet) > 0)
		     && (CCC->user.axlevel < AxNetU) ) {
			cprintf("%d Higher access required for network mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}
	
		if ((RESTRICT_INTERNET == 1)
		    && (valid_to->num_internet + valid_cc->num_internet + valid_bcc->num_internet > 0)
		    && ((CCC->user.flags & US_INTERNET) == 0)
		    && (!CCC->internal_pgm)) {
			cprintf("%d You don't have access to Internet mail.\n",
				ERROR + HIGHER_ACCESS_REQUIRED);
			free_recipients(valid_to);
			free_recipients(valid_cc);
			free_recipients(valid_bcc);
			return;
		}

	}

	/* Is this a room which has anonymous-only or anonymous-option? */
	anonymous = MES_NORMAL;
	if (CCC->room.QRflags & QR_ANONONLY) {
		anonymous = MES_ANONONLY;
	}
	if (CCC->room.QRflags & QR_ANONOPT) {
		if (anon_flag == 1) {	/* only if the user requested it */
			anonymous = MES_ANONOPT;
		}
	}

	if ((CCC->room.QRflags & QR_MAILBOX) == 0) {
		recp[0] = 0;
	}

	/* Recommend to the client that the use of a message subject is
	 * strongly recommended in this room, if either the SUBJECTREQ flag
	 * is set, or if there is one or more Internet email recipients.
	 */
	if (CCC->room.QRflags2 & QR2_SUBJECTREQ) subject_required = 1;
	if ((valid_to)	&& (valid_to->num_internet > 0))	subject_required = 1;
	if ((valid_cc)	&& (valid_cc->num_internet > 0))	subject_required = 1;
	if ((valid_bcc)	&& (valid_bcc->num_internet > 0))	subject_required = 1;

	/* If we're only checking the validity of the request, return
	 * success without creating the message.
	 */
	if (post == 0) {
		cprintf("%d %s|%d\n", CIT_OK,
			((valid_to != NULL) ? valid_to->display_recp : ""), 
			subject_required);
		free_recipients(valid_to);
		free_recipients(valid_cc);
		free_recipients(valid_bcc);
		return;
	}

	/* We don't need these anymore because we'll do it differently below */
	free_recipients(valid_to);
	free_recipients(valid_cc);
	free_recipients(valid_bcc);

	/* Read in the message from the client. */
	if (do_confirm) {
		cprintf("%d send message\n", START_CHAT_MODE);
	} else {
		cprintf("%d send message\n", SEND_LISTING);
	}

	msg = CtdlMakeMessage(&CCC->user, recp, cc,
			      CCC->room.QRname, anonymous, format_type,
			      newusername, newuseremail, subject,
			      ((!IsEmptyStr(supplied_euid)) ? supplied_euid : NULL),
			      NULL, references);

	/* Put together one big recipients struct containing to/cc/bcc all in
	 * one.  This is for the envelope.
	 */
	char *all_recps = malloc(SIZ * 3);
	strcpy(all_recps, recp);
	if (!IsEmptyStr(cc)) {
		if (!IsEmptyStr(all_recps)) {
			strcat(all_recps, ",");
		}
		strcat(all_recps, cc);
	}
	if (!IsEmptyStr(bcc)) {
		if (!IsEmptyStr(all_recps)) {
			strcat(all_recps, ",");
		}
		strcat(all_recps, bcc);
	}
	if (!IsEmptyStr(all_recps)) {
		valid = validate_recipients(all_recps, NULL, 0);
	}
	else {
		valid = NULL;
	}
	free(all_recps);

	if ((valid != NULL) && (valid->num_room == 1))
	{
		/* posting into an ML room? set the envelope from 
		 * to the actual mail address so others get a valid
		 * reply-to-header.
		 */
		CM_SetField(msg, eenVelopeTo, valid->recp_orgroom, strlen(valid->recp_orgroom));
	}

	if (msg != NULL) {
		msgnum = CtdlSubmitMsg(msg, valid, "", QP_EADDR);
		if (do_confirm) {
			cprintf("%ld\n", msgnum);

			if (StrLength(CCC->StatusMessage) > 0) {
				cprintf("%s\n", ChrPtr(CCC->StatusMessage));
			}
			else if (msgnum >= 0L) {
				client_write(HKEY("Message accepted.\n"));
			}
			else {
				client_write(HKEY("Internal error.\n"));
			}

			if (!CM_IsEmpty(msg, eExclusiveID)) {
				cprintf("%s\n", msg->cm_fields[eExclusiveID]);
			} else {
				cprintf("\n");
			}
			cprintf("000\n");
		}

		CM_Free(msg);
	}
	if (valid != NULL) {
		free_recipients(valid);
	}
	return;
}

/*
 * Delete message from current room
 */
void cmd_dele(char *args)
{
	int num_deleted;
	int i;
	char msgset[SIZ];
	char msgtok[32];
	long *msgs;
	int num_msgs = 0;

	extract_token(msgset, args, 0, '|', sizeof msgset);
	num_msgs = num_tokens(msgset, ',');
	if (num_msgs < 1) {
		cprintf("%d Nothing to do.\n", CIT_OK);
		return;
	}

	if (CtdlDoIHavePermissionToDeleteMessagesFromThisRoom() == 0) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	/*
	 * Build our message set to be moved/copied
	 */
	msgs = malloc(num_msgs * sizeof(long));
	for (i=0; i<num_msgs; ++i) {
		extract_token(msgtok, msgset, i, ',', sizeof msgtok);
		msgs[i] = atol(msgtok);
	}

	num_deleted = CtdlDeleteMessages(CC->room.QRname, msgs, num_msgs, "");
	free(msgs);

	if (num_deleted) {
		cprintf("%d %d message%s deleted.\n", CIT_OK,
			num_deleted, ((num_deleted != 1) ? "s" : ""));
	} else {
		cprintf("%d Message not found.\n", ERROR + MESSAGE_NOT_FOUND);
	}
}



/*
 * move or copy a message to another room
 */
void cmd_move(char *args)
{
	char msgset[SIZ];
	char msgtok[32];
	long *msgs;
	int num_msgs = 0;

	char targ[ROOMNAMELEN];
	struct ctdlroom qtemp;
	int err;
	int is_copy = 0;
	int ra;
	int permit = 0;
	int i;

	extract_token(msgset, args, 0, '|', sizeof msgset);
	num_msgs = num_tokens(msgset, ',');
	if (num_msgs < 1) {
		cprintf("%d Nothing to do.\n", CIT_OK);
		return;
	}

	extract_token(targ, args, 1, '|', sizeof targ);
	convert_room_name_macros(targ, sizeof targ);
	targ[ROOMNAMELEN - 1] = 0;
	is_copy = extract_int(args, 2);

	if (CtdlGetRoom(&qtemp, targ) != 0) {
		cprintf("%d '%s' does not exist.\n", ERROR + ROOM_NOT_FOUND, targ);
		return;
	}

	if (!strcasecmp(qtemp.QRname, CC->room.QRname)) {
		cprintf("%d Source and target rooms are the same.\n", ERROR + ALREADY_EXISTS);
		return;
	}

	CtdlGetUser(&CC->user, CC->curr_user);
	CtdlRoomAccess(&qtemp, &CC->user, &ra, NULL);

	/* Check for permission to perform this operation.
	 * Remember: "CC->room" is source, "qtemp" is target.
	 */
	permit = 0;

	/* Admins can move/copy */
	if (CC->user.axlevel >= AxAideU) permit = 1;

	/* Room aides can move/copy */
	if (CC->user.usernum == CC->room.QRroomaide) permit = 1;

	/* Permit move/copy from personal rooms */
	if ((CC->room.QRflags & QR_MAILBOX)
	    && (qtemp.QRflags & QR_MAILBOX)) permit = 1;

	/* Permit only copy from public to personal room */
	if ( (is_copy)
	     && (!(CC->room.QRflags & QR_MAILBOX))
	     && (qtemp.QRflags & QR_MAILBOX)) permit = 1;

	/* Permit message removal from collaborative delete rooms */
	if (CC->room.QRflags2 & QR2_COLLABDEL) permit = 1;

	/* Users allowed to post into the target room may move into it too. */
	if ((CC->room.QRflags & QR_MAILBOX) && 
	    (qtemp.QRflags & UA_POSTALLOWED))  permit = 1;

	/* User must have access to target room */
	if (!(ra & UA_KNOWN))  permit = 0;

	if (!permit) {
		cprintf("%d Higher access required.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	/*
	 * Build our message set to be moved/copied
	 */
	msgs = malloc(num_msgs * sizeof(long));
	for (i=0; i<num_msgs; ++i) {
		extract_token(msgtok, msgset, i, ',', sizeof msgtok);
		msgs[i] = atol(msgtok);
	}

	/*
	 * Do the copy
	 */
	err = CtdlSaveMsgPointersInRoom(targ, msgs, num_msgs, 1, NULL, 0);
	if (err != 0) {
		cprintf("%d Cannot store message(s) in %s: error %d\n",
			err, targ, err);
		free(msgs);
		return;
	}

	/* Now delete the message from the source room,
	 * if this is a 'move' rather than a 'copy' operation.
	 */
	if (is_copy == 0) {
		CtdlDeleteMessages(CC->room.QRname, msgs, num_msgs, "");
	}
	free(msgs);

	cprintf("%d Message(s) %s.\n", CIT_OK, (is_copy ? "copied" : "moved") );
}


/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/
CTDL_MODULE_INIT(ctdl_message)
{
	if (!threading) {

		CtdlRegisterProtoHook(cmd_msgs, "MSGS", "Output a list of messages in the current room");
		CtdlRegisterProtoHook(cmd_msg0, "MSG0", "Output a message in plain text format");
		CtdlRegisterProtoHook(cmd_msg2, "MSG2", "Output a message in RFC822 format");
		CtdlRegisterProtoHook(cmd_msg3, "MSG3", "Output a message in raw format (deprecated)");
		CtdlRegisterProtoHook(cmd_msg4, "MSG4", "Output a message in the client's preferred format");
		CtdlRegisterProtoHook(cmd_msgp, "MSGP", "Select preferred format for MSG4 output");
		CtdlRegisterProtoHook(cmd_opna, "OPNA", "Open an attachment for download");
		CtdlRegisterProtoHook(cmd_dlat, "DLAT", "Download an attachment");
		CtdlRegisterProtoHook(cmd_ent0, "ENT0", "Enter a message");
		CtdlRegisterProtoHook(cmd_dele, "DELE", "Delete a message");
		CtdlRegisterProtoHook(cmd_move, "MOVE", "Move or copy a message to another room");
	}

        /* return our Subversion id for the Log */
	return "ctdl_message";
}
