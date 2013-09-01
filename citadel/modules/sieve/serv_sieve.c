/*
 * This module glues libSieve to the Citadel server in order to implement
 * the Sieve mailbox filtering language (RFC 3028).
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
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
#include <ctype.h>
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
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "ctdl_module.h"
#include "serv_sieve.h"

struct RoomProcList *sieve_list = NULL;
char *msiv_extensions = NULL;
int SieveDebugEnable = 0;

#define DBGLOG(LEVEL) if ((LEVEL != LOG_DEBUG) || (SieveDebugEnable != 0))

#define SV_syslog(LEVEL, FORMAT, ...)				\
	DBGLOG(LEVEL) syslog(LEVEL,				\
			     "Sieve: " FORMAT, __VA_ARGS__)

#define SVM_syslog(LEVEL, FORMAT)		\
	DBGLOG(LEVEL) syslog(LEVEL,		\
			     "Sieve: " FORMAT);


/*
 * Callback function to send libSieve trace messages to Citadel log facility
 */
int ctdl_debug(sieve2_context_t *s, void *my)
{
	SV_syslog(LOG_DEBUG, "%s", sieve2_getvalue_string(s, "message"));
	return SIEVE2_OK;
}


/*
 * Callback function to log script parsing errors
 */
int ctdl_errparse(sieve2_context_t *s, void *my)
{
	SV_syslog(LOG_WARNING, "Error in script, line %d: %s",
		  sieve2_getvalue_int(s, "lineno"),
		  sieve2_getvalue_string(s, "message")
	);
	return SIEVE2_OK;
}


/*
 * Callback function to log script execution errors
 */
int ctdl_errexec(sieve2_context_t *s, void *my)
{
	SV_syslog(LOG_WARNING, "Error executing script: %s",
		  sieve2_getvalue_string(s, "message")
		);
	return SIEVE2_OK;
}


/*
 * Callback function to redirect a message to a different folder
 */
int ctdl_redirect(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	struct CtdlMessage *msg = NULL;
	struct recptypes *valid = NULL;
	char recp[256];

	safestrncpy(recp, sieve2_getvalue_string(s, "address"), sizeof recp);

	SV_syslog(LOG_DEBUG, "Action is REDIRECT, recipient <%s>", recp);

	valid = validate_recipients(recp, NULL, 0);
	if (valid == NULL) {
		SV_syslog(LOG_WARNING, "REDIRECT failed: bad recipient <%s>", recp);
		return SIEVE2_ERROR_BADARGS;
	}
	if (valid->num_error > 0) {
		SV_syslog(LOG_WARNING, "REDIRECT failed: bad recipient <%s>", recp);
		free_recipients(valid);
		return SIEVE2_ERROR_BADARGS;
	}

	msg = CtdlFetchMessage(cs->msgnum, 1);
	if (msg == NULL) {
		SV_syslog(LOG_WARNING, "REDIRECT failed: unable to fetch msg %ld", cs->msgnum);
		free_recipients(valid);
		return SIEVE2_ERROR_BADARGS;
	}

	CtdlSubmitMsg(msg, valid, NULL, 0);
	cs->cancel_implicit_keep = 1;
	free_recipients(valid);
	CM_Free(msg);
	return SIEVE2_OK;
}


/*
 * Callback function to indicate that a message *will* be kept in the inbox
 */
int ctdl_keep(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	
	SVM_syslog(LOG_DEBUG, "Action is KEEP");

	cs->keep = 1;
	cs->cancel_implicit_keep = 1;
	return SIEVE2_OK;
}


/*
 * Callback function to file a message into a different mailbox
 */
int ctdl_fileinto(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	const char *dest_folder = sieve2_getvalue_string(s, "mailbox");
	int c;
	char foldername[256];
	char original_room_name[ROOMNAMELEN];

	SV_syslog(LOG_DEBUG, "Action is FILEINTO, destination is <%s>", dest_folder);

	/* FILEINTO 'INBOX' is the same thing as KEEP */
	if ( (!strcasecmp(dest_folder, "INBOX")) || (!strcasecmp(dest_folder, MAILROOM)) ) {
		cs->keep = 1;
		cs->cancel_implicit_keep = 1;
		return SIEVE2_OK;
	}

	/* Remember what room we came from */
	safestrncpy(original_room_name, CC->room.QRname, sizeof original_room_name);

	/* First try a mailbox name match (check personal mail folders first) */
	snprintf(foldername, sizeof foldername, "%010ld.%s", cs->usernum, dest_folder);
	c = CtdlGetRoom(&CC->room, foldername);

	/* Then a regular room name match (public and private rooms) */
	if (c != 0) {
		safestrncpy(foldername, dest_folder, sizeof foldername);
		c = CtdlGetRoom(&CC->room, foldername);
	}

	if (c != 0) {
		SV_syslog(LOG_WARNING, "FILEINTO failed: target <%s> does not exist", dest_folder);
		return SIEVE2_ERROR_BADARGS;
	}

	/* Yes, we actually have to go there */
	CtdlUserGoto(NULL, 0, 0, NULL, NULL);

	c = CtdlSaveMsgPointersInRoom(NULL, &cs->msgnum, 1, 0, NULL, 0);

	/* Go back to the room we came from */
	if (strcasecmp(original_room_name, CC->room.QRname)) {
		CtdlUserGoto(original_room_name, 0, 0, NULL, NULL);
	}

	if (c == 0) {
		cs->cancel_implicit_keep = 1;
		return SIEVE2_OK;
	}
	else {
		return SIEVE2_ERROR_BADARGS;
	}
}


/*
 * Callback function to indicate that a message should be discarded.
 */
int ctdl_discard(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	SVM_syslog(LOG_DEBUG, "Action is DISCARD");

	/* Cancel the implicit keep.  That's all there is to it. */
	cs->cancel_implicit_keep = 1;
	return SIEVE2_OK;
}



/*
 * Callback function to indicate that a message should be rejected
 */
int ctdl_reject(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	char *reject_text = NULL;

	SVM_syslog(LOG_DEBUG, "Action is REJECT");

	/* If we don't know who sent the message, do a DISCARD instead. */
	if (IsEmptyStr(cs->sender)) {
		SVM_syslog(LOG_INFO, "Unknown sender.  Doing DISCARD instead of REJECT.");
		return ctdl_discard(s, my);
	}

	/* Assemble the reject message. */
	reject_text = malloc(strlen(sieve2_getvalue_string(s, "message")) + 1024);
	if (reject_text == NULL) {
		return SIEVE2_ERROR_FAIL;
	}

	sprintf(reject_text, 
		"Content-type: text/plain\n"
		"\n"
		"The message was refused by the recipient's mail filtering program.\n"
		"The reason given was as follows:\n"
		"\n"
		"%s\n"
		"\n"
	,
		sieve2_getvalue_string(s, "message")
	);

	quickie_message(	/* This delivers the message */
		NULL,
		cs->envelope_to,
		cs->sender,
		NULL,
		reject_text,
		FMT_RFC822,
		"Delivery status notification"
	);

	free(reject_text);
	cs->cancel_implicit_keep = 1;
	return SIEVE2_OK;
}



/*
 * Callback function to indicate that a vacation message should be generated
 */
int ctdl_vacation(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	struct sdm_vacation *vptr;
	int days = 1;
	const char *message;
	char *vacamsg_text = NULL;
	char vacamsg_subject[1024];

	SVM_syslog(LOG_DEBUG, "Action is VACATION");

	message = sieve2_getvalue_string(s, "message");
	if (message == NULL) return SIEVE2_ERROR_BADARGS;

	if (sieve2_getvalue_string(s, "subject") != NULL) {
		safestrncpy(vacamsg_subject, sieve2_getvalue_string(s, "subject"), sizeof vacamsg_subject);
	}
	else {
		snprintf(vacamsg_subject, sizeof vacamsg_subject, "Re: %s", cs->subject);
	}

	days = sieve2_getvalue_int(s, "days");
	if (days < 1) days = 1;
	if (days > MAX_VACATION) days = MAX_VACATION;

	/* Check to see whether we've already alerted this sender that we're on vacation. */
	for (vptr = cs->u->first_vacation; vptr != NULL; vptr = vptr->next) {
		if (!strcasecmp(vptr->fromaddr, cs->sender)) {
			if ( (time(NULL) - vptr->timestamp) < (days * 86400) ) {
				SV_syslog(LOG_DEBUG, "Already alerted <%s> recently.", cs->sender);
				return SIEVE2_OK;
			}
		}
	}

	/* Assemble the reject message. */
	vacamsg_text = malloc(strlen(message) + 1024);
	if (vacamsg_text == NULL) {
		return SIEVE2_ERROR_FAIL;
	}

	sprintf(vacamsg_text, 
		"Content-type: text/plain charset=utf-8\n"
		"\n"
		"%s\n"
		"\n"
	,
		message
	);

	quickie_message(	/* This delivers the message */
		NULL,
		cs->envelope_to,
		cs->sender,
		NULL,
		vacamsg_text,
		FMT_RFC822,
		vacamsg_subject
	);

	free(vacamsg_text);

	/* Now update the list to reflect the fact that we've alerted this sender.
	 * If they're already in the list, just update the timestamp.
	 */
	for (vptr = cs->u->first_vacation; vptr != NULL; vptr = vptr->next) {
		if (!strcasecmp(vptr->fromaddr, cs->sender)) {
			vptr->timestamp = time(NULL);
			return SIEVE2_OK;
		}
	}

	/* If we get to this point, create a new record.
	 */
	vptr = malloc(sizeof(struct sdm_vacation));
	memset(vptr, 0, sizeof(struct sdm_vacation));
	vptr->timestamp = time(NULL);
	safestrncpy(vptr->fromaddr, cs->sender, sizeof vptr->fromaddr);
	vptr->next = cs->u->first_vacation;
	cs->u->first_vacation = vptr;

	return SIEVE2_OK;
}


/*
 * Callback function to parse addresses per local system convention
 * It is disabled because we don't support subaddresses.
 */
#if 0
int ctdl_getsubaddress(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	/* libSieve does not take ownership of the memory used here.  But, since we
	 * are just pointing to locations inside a struct which we are going to free
	 * later, we're ok.
	 */
	sieve2_setvalue_string(s, "user", cs->recp_user);
	sieve2_setvalue_string(s, "detail", "");
	sieve2_setvalue_string(s, "localpart", cs->recp_user);
	sieve2_setvalue_string(s, "domain", cs->recp_node);
	return SIEVE2_OK;
}
#endif


/*
 * Callback function to parse message envelope
 */
int ctdl_getenvelope(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	SVM_syslog(LOG_DEBUG, "Action is GETENVELOPE");
	SV_syslog(LOG_DEBUG, "EnvFrom: %s", cs->envelope_from);
	SV_syslog(LOG_DEBUG, "EnvTo: %s", cs->envelope_to);

	if (cs->envelope_from != NULL) {
		if ((cs->envelope_from[0] != '@')&&(cs->envelope_from[strlen(cs->envelope_from)-1] != '@')) {
			sieve2_setvalue_string(s, "from", cs->envelope_from);
		}
		else {
			sieve2_setvalue_string(s, "from", "invalid_envelope_from@example.org");
		}
	}
	else {
		sieve2_setvalue_string(s, "from", "null_envelope_from@example.org");
	}


	if (cs->envelope_to != NULL) {
		if ((cs->envelope_to[0] != '@') && (cs->envelope_to[strlen(cs->envelope_to)-1] != '@')) {
			sieve2_setvalue_string(s, "to", cs->envelope_to);
		}
		else {
			sieve2_setvalue_string(s, "to", "invalid_envelope_to@example.org");
		}
	}
	else {
		sieve2_setvalue_string(s, "to", "null_envelope_to@example.org");
	}

	return SIEVE2_OK;
}


/*
 * Callback function to fetch message body
 * (Uncomment the code if we implement this extension)
 *
int ctdl_getbody(sieve2_context_t *s, void *my)
{
	return SIEVE2_ERROR_UNSUPPORTED;
}
 *
 */


/*
 * Callback function to fetch message size
 */
int ctdl_getsize(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	struct MetaData smi;

	GetMetaData(&smi, cs->msgnum);
	
	if (smi.meta_rfc822_length > 0L) {
		sieve2_setvalue_int(s, "size", (int)smi.meta_rfc822_length);
		return SIEVE2_OK;
	}

	return SIEVE2_ERROR_UNSUPPORTED;
}


/*
 * Return a pointer to the active Sieve script.
 * (Caller does NOT own the memory and should not free the returned pointer.)
 */
char *get_active_script(struct sdm_userdata *u) {
	struct sdm_script *sptr;

	for (sptr=u->first_script; sptr!=NULL; sptr=sptr->next) {
		if (sptr->script_active > 0) {
			SV_syslog(LOG_DEBUG, "get_active_script() is using script '%s'", sptr->script_name);
			return(sptr->script_content);
		}
	}

	SVM_syslog(LOG_DEBUG, "get_active_script() found no active script");
	return(NULL);
}


/*
 * Callback function to retrieve the sieve script
 */
int ctdl_getscript(sieve2_context_t *s, void *my) {
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	char *active_script = get_active_script(cs->u);
	if (active_script != NULL) {
		sieve2_setvalue_string(s, "script", active_script);
		return SIEVE2_OK;
	}

	return SIEVE2_ERROR_GETSCRIPT;
}

/*
 * Callback function to retrieve message headers
 */
int ctdl_getheaders(sieve2_context_t *s, void *my) {

	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	SVM_syslog(LOG_DEBUG, "ctdl_getheaders() was called");
	sieve2_setvalue_string(s, "allheaders", cs->rfc822headers);
	return SIEVE2_OK;
}



/*
 * Add a room to the list of those rooms which potentially require sieve processing
 */
void sieve_queue_room(struct ctdlroom *which_room) {
	struct RoomProcList *ptr;

	ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
	if (ptr == NULL) return;

	safestrncpy(ptr->name, which_room->QRname, sizeof ptr->name);
	begin_critical_section(S_SIEVELIST);
	ptr->next = sieve_list;
	sieve_list = ptr;
	end_critical_section(S_SIEVELIST);
	SV_syslog(LOG_DEBUG, "<%s> queued for Sieve processing", which_room->QRname);
}



/*
 * Perform sieve processing for one message (called by sieve_do_room() for each message)
 */
void sieve_do_msg(long msgnum, void *userdata) {
	struct sdm_userdata *u = (struct sdm_userdata *) userdata;
	sieve2_context_t *sieve2_context;
	struct ctdl_sieve my;
	int res;
	struct CtdlMessage *msg;
	int i;
	size_t headers_len = 0;
	int len = 0;

	if (u == NULL)
	{
		SV_syslog(LOG_EMERG, "Can't process message <%ld> without userdata!", msgnum);
		return;
	}

	sieve2_context = u->sieve2_context;

	SV_syslog(LOG_DEBUG, "Performing sieve processing on msg <%ld>", msgnum);

	/*
	 * Make sure you include message body so you can get those second-level headers ;)
	 */
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;

	/*
	 * Grab the message headers so we can feed them to libSieve.
	 * Use HEADERS_ONLY rather than HEADERS_FAST in order to include second-level headers.
	 */
	CC->redirect_buffer = NewStrBufPlain(NULL, SIZ);
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ONLY, 0, 1, 0);
	headers_len = StrLength(CC->redirect_buffer);
	my.rfc822headers = SmashStrBuf(&CC->redirect_buffer);

	/*
	 * libSieve clobbers the stack if it encounters badly formed
	 * headers.  Sanitize our headers by stripping nonprintable
	 * characters.
	 */
	for (i=0; i<headers_len; ++i) {
		if (!isascii(my.rfc822headers[i])) {
			my.rfc822headers[i] = '_';
		}
	}

	my.keep = 0;				/* Set to 1 to declare an *explicit* keep */
	my.cancel_implicit_keep = 0;		/* Some actions will cancel the implicit keep */
	my.usernum = atol(CC->room.QRname);	/* Keep track of the owner of the room's namespace */
	my.msgnum = msgnum;			/* Keep track of the message number in our local store */
	my.u = u;				/* Hand off a pointer to the rest of this info */

	/* Keep track of the recipient so we can do handling based on it later */
	process_rfc822_addr(msg->cm_fields[eRecipient], my.recp_user, my.recp_node, my.recp_name);

	/* Keep track of the sender so we can use it for REJECT and VACATION responses */
	if (!CM_IsEmpty(msg, erFc822Addr)) {
		safestrncpy(my.sender, msg->cm_fields[erFc822Addr], sizeof my.sender);
	}
	else if ( (!CM_IsEmpty(msg, eAuthor)) && (!CM_IsEmpty(msg, eNodeName)) ) {
		snprintf(my.sender, sizeof my.sender, "%s@%s", msg->cm_fields[eAuthor], msg->cm_fields[eNodeName]);
	}
	else if (!CM_IsEmpty(msg, eAuthor)) {
		safestrncpy(my.sender, msg->cm_fields[eAuthor], sizeof my.sender);
	}
	else {
		strcpy(my.sender, "");
	}

	/* Keep track of the subject so we can use it for VACATION responses */
	if (!CM_IsEmpty(msg, eMsgSubject)) {
		safestrncpy(my.subject, msg->cm_fields[eMsgSubject], sizeof my.subject);
	}
	else {
		strcpy(my.subject, "");
	}

	/* Keep track of the envelope-from address (use body-from if not found) */
	if (!CM_IsEmpty(msg, eMessagePath)) {
		safestrncpy(my.envelope_from, msg->cm_fields[eMessagePath], sizeof my.envelope_from);
		stripallbut(my.envelope_from, '<', '>');
	}
	else if (!CM_IsEmpty(msg, erFc822Addr)) {
		safestrncpy(my.envelope_from, msg->cm_fields[erFc822Addr], sizeof my.envelope_from);
		stripallbut(my.envelope_from, '<', '>');
	}
	else {
		strcpy(my.envelope_from, "");
	}

	len = strlen(my.envelope_from);
	for (i=0; i<len; ++i) {
		if (isspace(my.envelope_from[i])) my.envelope_from[i] = '_';
	}
	if (haschar(my.envelope_from, '@') == 0) {
		strcat(my.envelope_from, "@");
		strcat(my.envelope_from, config.c_fqdn);
	}

	/* Keep track of the envelope-to address (use body-to if not found) */
	if (!CM_IsEmpty(msg, eenVelopeTo)) {
		safestrncpy(my.envelope_to, msg->cm_fields[eenVelopeTo], sizeof my.envelope_to);
		stripallbut(my.envelope_to, '<', '>');
	}
	else if (!CM_IsEmpty(msg, eRecipient)) {
		safestrncpy(my.envelope_to, msg->cm_fields[eRecipient], sizeof my.envelope_to);
		if (!CM_IsEmpty(msg, eDestination)) {
			strcat(my.envelope_to, "@");
			strcat(my.envelope_to, msg->cm_fields[eDestination]);
		}
		stripallbut(my.envelope_to, '<', '>');
	}
	else {
		strcpy(my.envelope_to, "");
	}

	len = strlen(my.envelope_to);
	for (i=0; i<len; ++i) {
		if (isspace(my.envelope_to[i])) my.envelope_to[i] = '_';
	}
	if (haschar(my.envelope_to, '@') == 0) {
		strcat(my.envelope_to, "@");
		strcat(my.envelope_to, config.c_fqdn);
	}

	CM_Free(msg);
	
	SVM_syslog(LOG_DEBUG, "Calling sieve2_execute()");
	res = sieve2_execute(sieve2_context, &my);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_execute() returned %d: %s", res, sieve2_errstr(res));
	}

	free(my.rfc822headers);
	my.rfc822headers = NULL;

	/*
	 * Delete the message from the inbox unless either we were told not to, or
	 * if no other action was successfully taken.
	 */
	if ( (!my.keep) && (my.cancel_implicit_keep) ) {
		SVM_syslog(LOG_DEBUG, "keep is 0 -- deleting message from inbox");
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
	}

	SV_syslog(LOG_DEBUG, "Completed sieve processing on msg <%ld>", msgnum);
	u->lastproc = msgnum;

	return;
}



/*
 * Given the on-disk representation of our Sieve config, load
 * it into an in-memory data structure.
 */
void parse_sieve_config(char *conf, struct sdm_userdata *u) {
	char *ptr;
	char *c, *vacrec;
	char keyword[256];
	struct sdm_script *sptr;
	struct sdm_vacation *vptr;

	ptr = conf;
	while (c = ptr, ptr = bmstrcasestr(ptr, CTDLSIEVECONFIGSEPARATOR), ptr != NULL) {
		*ptr = 0;
		ptr += strlen(CTDLSIEVECONFIGSEPARATOR);

		extract_token(keyword, c, 0, '|', sizeof keyword);

		if (!strcasecmp(keyword, "lastproc")) {
			u->lastproc = extract_long(c, 1);
		}

		else if (!strcasecmp(keyword, "script")) {
			sptr = malloc(sizeof(struct sdm_script));
			extract_token(sptr->script_name, c, 1, '|', sizeof sptr->script_name);
			sptr->script_active = extract_int(c, 2);
			remove_token(c, 0, '|');
			remove_token(c, 0, '|');
			remove_token(c, 0, '|');
			sptr->script_content = strdup(c);
			sptr->next = u->first_script;
			u->first_script = sptr;
		}

		else if (!strcasecmp(keyword, "vacation")) {

			if (c != NULL) while (vacrec=c, c=strchr(c, '\n'), (c != NULL)) {

				*c = 0;
				++c;

				if (strncasecmp(vacrec, "vacation|", 9)) {
					vptr = malloc(sizeof(struct sdm_vacation));
					extract_token(vptr->fromaddr, vacrec, 0, '|', sizeof vptr->fromaddr);
					vptr->timestamp = extract_long(vacrec, 1);
					vptr->next = u->first_vacation;
					u->first_vacation = vptr;
				}
			}
		}

		/* ignore unknown keywords */
	}
}

/*
 * We found the Sieve configuration for this user.
 * Now do something with it.
 */
void get_sieve_config_backend(long msgnum, void *userdata) {
	struct sdm_userdata *u = (struct sdm_userdata *) userdata;
	struct CtdlMessage *msg;
	char *conf;
	long conflen;

	u->config_msgnum = msgnum;
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		u->config_msgnum = (-1) ;
		return;
	}

	CM_GetAsField(msg, eMesageText, &conf, &conflen);

	CM_Free(msg);

	if (conf != NULL) {
		parse_sieve_config(conf, u);
		free(conf);
	}

}


/* 
 * Write our citadel sieve config back to disk
 * 
 * (Set yes_write_to_disk to nonzero to make it actually write the config;
 * otherwise it just frees the data structures.)
 */
void rewrite_ctdl_sieve_config(struct sdm_userdata *u, int yes_write_to_disk) {
	StrBuf *text;
	struct sdm_script *sptr;
	struct sdm_vacation *vptr;
	
	text = NewStrBufPlain(NULL, SIZ);
	StrBufPrintf(text,
		     "Content-type: application/x-citadel-sieve-config\n"
		     "\n"
		     CTDLSIEVECONFIGSEPARATOR
		     "lastproc|%ld"
		     CTDLSIEVECONFIGSEPARATOR
		     ,
		     u->lastproc
		);

	while (u->first_script != NULL) {
		StrBufAppendPrintf(text,
				   "script|%s|%d|%s" CTDLSIEVECONFIGSEPARATOR,
				   u->first_script->script_name,
				   u->first_script->script_active,
				   u->first_script->script_content
			);
		sptr = u->first_script;
		u->first_script = u->first_script->next;
		free(sptr->script_content);
		free(sptr);
	}

	if (u->first_vacation != NULL) {

		StrBufAppendPrintf(text, "vacation|\n");
		while (u->first_vacation != NULL) {
			if ( (time(NULL) - u->first_vacation->timestamp) < (MAX_VACATION * 86400)) {
				StrBufAppendPrintf(text, "%s|%ld\n",
						   u->first_vacation->fromaddr,
						   u->first_vacation->timestamp
					);
			}
			vptr = u->first_vacation;
			u->first_vacation = u->first_vacation->next;
			free(vptr);
		}
		StrBufAppendPrintf(text, CTDLSIEVECONFIGSEPARATOR);
	}

	if (yes_write_to_disk)
	{
		/* Save the config */
		quickie_message("Citadel", NULL, NULL, u->config_roomname,
				ChrPtr(text),
				4,
				"Sieve configuration"
		);
		
		/* And delete the old one */
		if (u->config_msgnum > 0) {
			CtdlDeleteMessages(u->config_roomname, &u->config_msgnum, 1, "");
		}
	}

	FreeStrBuf (&text);

}


/*
 * This is our callback registration table for libSieve.
 */
sieve2_callback_t ctdl_sieve_callbacks[] = {
	{ SIEVE2_ACTION_REJECT,		ctdl_reject		},
	{ SIEVE2_ACTION_VACATION,	ctdl_vacation		},
	{ SIEVE2_ERRCALL_PARSE,		ctdl_errparse		},
	{ SIEVE2_ERRCALL_RUNTIME,	ctdl_errexec		},
	{ SIEVE2_ACTION_FILEINTO,	ctdl_fileinto		},
	{ SIEVE2_ACTION_REDIRECT,	ctdl_redirect		},
	{ SIEVE2_ACTION_DISCARD,	ctdl_discard		},
	{ SIEVE2_ACTION_KEEP,		ctdl_keep		},
	{ SIEVE2_SCRIPT_GETSCRIPT,	ctdl_getscript		},
	{ SIEVE2_DEBUG_TRACE,		ctdl_debug		},
	{ SIEVE2_MESSAGE_GETALLHEADERS,	ctdl_getheaders		},
	{ SIEVE2_MESSAGE_GETSIZE,	ctdl_getsize		},
	{ SIEVE2_MESSAGE_GETENVELOPE,	ctdl_getenvelope	},
/*
 * These actions are unsupported by Citadel so we don't declare them.
 *
	{ SIEVE2_ACTION_NOTIFY,		ctdl_notify		},
	{ SIEVE2_MESSAGE_GETSUBADDRESS,	ctdl_getsubaddress	},
	{ SIEVE2_MESSAGE_GETBODY,	ctdl_getbody		},
 *
 */
	{ 0 }
};


/*
 * Perform sieve processing for a single room
 */
void sieve_do_room(char *roomname) {
	
	struct sdm_userdata u;
	sieve2_context_t *sieve2_context = NULL;	/* Context for sieve parser */
	int res;					/* Return code from libsieve calls */
	long orig_lastproc = 0;

	memset(&u, 0, sizeof u);

	/* See if the user who owns this 'mailbox' has any Sieve scripts that
	 * require execution.
	 */
	snprintf(u.config_roomname, sizeof u.config_roomname, "%010ld.%s", atol(roomname), USERCONFIGROOM);
	if (CtdlGetRoom(&CC->room, u.config_roomname) != 0) {
		SV_syslog(LOG_DEBUG, "<%s> does not exist.  No processing is required.", u.config_roomname);
		return;
	}

	/*
	 * Find the sieve scripts and control record and do something
	 */
	u.config_msgnum = (-1);
	CtdlForEachMessage(MSGS_LAST, 1, NULL, SIEVECONFIG, NULL,
		get_sieve_config_backend, (void *)&u );

	if (u.config_msgnum < 0) {
		SVM_syslog(LOG_DEBUG, "No Sieve rules exist.  No processing is required.");
		return;
	}

	/*
	 * Check to see whether the script is empty and should not be processed.
	 * A script is considered non-empty if it contains at least one semicolon.
	 */
	if (
		(get_active_script(&u) == NULL)
		|| (strchr(get_active_script(&u), ';') == NULL)
	) {
		SVM_syslog(LOG_DEBUG, "Sieve script is empty.  No processing is required.");
		return;
	}

	SV_syslog(LOG_DEBUG, "Rules found.  Performing Sieve processing for <%s>", roomname);

	if (CtdlGetRoom(&CC->room, roomname) != 0) {
		SV_syslog(LOG_CRIT, "ERROR: cannot load <%s>", roomname);
		return;
	}

	/* Initialize the Sieve parser */
	
	res = sieve2_alloc(&sieve2_context);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_alloc() returned %d: %s", res, sieve2_errstr(res));
		return;
	}

	res = sieve2_callbacks(sieve2_context, ctdl_sieve_callbacks);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_callbacks() returned %d: %s", res, sieve2_errstr(res));
		goto BAIL;
	}

	/* Validate the script */

	struct ctdl_sieve my;		/* dummy ctdl_sieve struct just to pass "u" slong */
	memset(&my, 0, sizeof my);
	my.u = &u;
	res = sieve2_validate(sieve2_context, &my);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_validate() returned %d: %s", res, sieve2_errstr(res));
		goto BAIL;
	}

	/* Do something useful */
	u.sieve2_context = sieve2_context;
	orig_lastproc = u.lastproc;
	CtdlForEachMessage(MSGS_GT, u.lastproc, NULL, NULL, NULL,
		sieve_do_msg,
		(void *) &u
	);

BAIL:
	res = sieve2_free(&sieve2_context);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_free() returned %d: %s", res, sieve2_errstr(res));
	}

	/* Rewrite the config if we have to */
	rewrite_ctdl_sieve_config(&u, (u.lastproc > orig_lastproc) ) ;
}


/*
 * Perform sieve processing for all rooms which require it
 */
void perform_sieve_processing(void) {
	struct RoomProcList *ptr = NULL;

	if (sieve_list != NULL) {
		SVM_syslog(LOG_DEBUG, "Begin Sieve processing");
		while (sieve_list != NULL) {
			char spoolroomname[ROOMNAMELEN];
			safestrncpy(spoolroomname, sieve_list->name, sizeof spoolroomname);
			begin_critical_section(S_SIEVELIST);

			/* pop this record off the list */
			ptr = sieve_list;
			sieve_list = sieve_list->next;
			free(ptr);

			/* invalidate any duplicate entries to prevent double processing */
			for (ptr=sieve_list; ptr!=NULL; ptr=ptr->next) {
				if (!strcasecmp(ptr->name, spoolroomname)) {
					ptr->name[0] = 0;
				}
			}

			end_critical_section(S_SIEVELIST);
			if (spoolroomname[0] != 0) {
				sieve_do_room(spoolroomname);
			}
		}
	}
}


void msiv_load(struct sdm_userdata *u) {
	char hold_rm[ROOMNAMELEN];

	strcpy(hold_rm, CC->room.QRname);       /* save current room */

	/* Take a spin through the user's personal address book */
	if (CtdlGetRoom(&CC->room, USERCONFIGROOM) == 0) {
	
		u->config_msgnum = (-1);
		strcpy(u->config_roomname, CC->room.QRname);
		CtdlForEachMessage(MSGS_LAST, 1, NULL, SIEVECONFIG, NULL,
			get_sieve_config_backend, (void *)u );

	}

	if (strcmp(CC->room.QRname, hold_rm)) {
		CtdlGetRoom(&CC->room, hold_rm);    /* return to saved room */
	}
}

void msiv_store(struct sdm_userdata *u, int yes_write_to_disk) {
/*
 * Initialise the sieve configs last processed message number.
 * We don't need to get the highest message number for the users inbox since the systems
 * highest message number will be higher than that and loer than this scripts message number
 * This prevents this new script from processing any old messages in the inbox.
 * Most importantly it will prevent vacation messages being sent to lots of old messages
 * in the inbox.
 */
	u->lastproc = CtdlGetCurrentMessageNumber();
	rewrite_ctdl_sieve_config(u, yes_write_to_disk);
}


/*
 * Select the active script.
 * (Set script_name to an empty string to disable all scripts)
 * 
 * Returns 0 on success or nonzero for error.
 */
int msiv_setactive(struct sdm_userdata *u, char *script_name) {
	int ok = 0;
	struct sdm_script *s;

	/* First see if the supplied value is ok */

	if (IsEmptyStr(script_name)) {
		ok = 1;
	}
	else {
		for (s=u->first_script; s!=NULL; s=s->next) {
			if (!strcasecmp(s->script_name, script_name)) {
				ok = 1;
			}
		}
	}

	if (!ok) return(-1);

	/* Now set the active script */
	for (s=u->first_script; s!=NULL; s=s->next) {
		if (!strcasecmp(s->script_name, script_name)) {
			s->script_active = 1;
		}
		else {
			s->script_active = 0;
		}
	}
	
	return(0);
}


/*
 * Fetch a script by name.
 *
 * Returns NULL if the named script was not found, or a pointer to the script
 * if it was found.   NOTE: the caller does *not* own the memory returned by
 * this function.  Copy it if you need to keep it.
 */
char *msiv_getscript(struct sdm_userdata *u, char *script_name) {
	struct sdm_script *s;

	for (s=u->first_script; s!=NULL; s=s->next) {
		if (!strcasecmp(s->script_name, script_name)) {
			if (s->script_content != NULL) {
				return (s->script_content);
			}
		}
	}

	return(NULL);
}


/*
 * Delete a script by name.
 *
 * Returns 0 if the script was deleted.
 *	 1 if the script was not found.
 *	 2 if the script cannot be deleted because it is active.
 */
int msiv_deletescript(struct sdm_userdata *u, char *script_name) {
	struct sdm_script *s = NULL;
	struct sdm_script *script_to_delete = NULL;

	for (s=u->first_script; s!=NULL; s=s->next) {
		if (!strcasecmp(s->script_name, script_name)) {
			script_to_delete = s;
			if (s->script_active) {
				return(2);
			}
		}
	}

	if (script_to_delete == NULL) return(1);

	if (u->first_script == script_to_delete) {
		u->first_script = u->first_script->next;
	}
	else for (s=u->first_script; s!=NULL; s=s->next) {
		if (s->next == script_to_delete) {
			s->next = s->next->next;
		}
	}

	free(script_to_delete->script_content);
	free(script_to_delete);
	return(0);
}


/*
 * Add or replace a new script.  
 * NOTE: after this function returns, "u" owns the memory that "script_content"
 * was pointing to.
 */
void msiv_putscript(struct sdm_userdata *u, char *script_name, char *script_content) {
	int replaced = 0;
	struct sdm_script *s, *sptr;

	for (s=u->first_script; s!=NULL; s=s->next) {
		if (!strcasecmp(s->script_name, script_name)) {
			if (s->script_content != NULL) {
				free(s->script_content);
			}
			s->script_content = script_content;
			replaced = 1;
		}
	}

	if (replaced == 0) {
		sptr = malloc(sizeof(struct sdm_script));
		safestrncpy(sptr->script_name, script_name, sizeof sptr->script_name);
		sptr->script_content = script_content;
		sptr->script_active = 0;
		sptr->next = u->first_script;
		u->first_script = sptr;
	}
}



/*
 * Citadel protocol to manage sieve scripts.
 * This is basically a simplified (read: doesn't resemble IMAP) version
 * of the 'managesieve' protocol.
 */
void cmd_msiv(char *argbuf) {
	char subcmd[256];
	struct sdm_userdata u;
	char script_name[256];
	char *script_content = NULL;
	struct sdm_script *s;
	int i;
	int changes_made = 0;

	memset(&u, 0, sizeof(struct sdm_userdata));

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);
	msiv_load(&u);

	if (!strcasecmp(subcmd, "putscript")) {
		extract_token(script_name, argbuf, 1, '|', sizeof script_name);
		if (!IsEmptyStr(script_name)) {
			cprintf("%d Transmit script now\n", SEND_LISTING);
			script_content = CtdlReadMessageBody(HKEY("000"), config.c_maxmsglen, NULL, 0, 0);
			msiv_putscript(&u, script_name, script_content);
			changes_made = 1;
		}
		else {
			cprintf("%d Invalid script name.\n", ERROR + ILLEGAL_VALUE);
		}
	}	
	
	else if (!strcasecmp(subcmd, "listscripts")) {
		cprintf("%d Scripts:\n", LISTING_FOLLOWS);
		for (s=u.first_script; s!=NULL; s=s->next) {
			if (s->script_content != NULL) {
				cprintf("%s|%d|\n", s->script_name, s->script_active);
			}
		}
		cprintf("000\n");
	}

	else if (!strcasecmp(subcmd, "setactive")) {
		extract_token(script_name, argbuf, 1, '|', sizeof script_name);
		if (msiv_setactive(&u, script_name) == 0) {
			cprintf("%d ok\n", CIT_OK);
			changes_made = 1;
		}
		else {
			cprintf("%d Script '%s' does not exist.\n",
				ERROR + ILLEGAL_VALUE,
				script_name
			);
		}
	}

	else if (!strcasecmp(subcmd, "getscript")) {
		extract_token(script_name, argbuf, 1, '|', sizeof script_name);
		script_content = msiv_getscript(&u, script_name);
		if (script_content != NULL) {
			int script_len;

			cprintf("%d Script:\n", LISTING_FOLLOWS);
			script_len = strlen(script_content);
			client_write(script_content, script_len);
			if (script_content[script_len-1] != '\n') {
				cprintf("\n");
			}
			cprintf("000\n");
		}
		else {
			cprintf("%d Invalid script name.\n", ERROR + ILLEGAL_VALUE);
		}
	}

	else if (!strcasecmp(subcmd, "deletescript")) {
		extract_token(script_name, argbuf, 1, '|', sizeof script_name);
		i = msiv_deletescript(&u, script_name);
		if (i == 0) {
			cprintf("%d ok\n", CIT_OK);
			changes_made = 1;
		}
		else if (i == 1) {
			cprintf("%d Script '%s' does not exist.\n",
				ERROR + ILLEGAL_VALUE,
				script_name
			);
		}
		else if (i == 2) {
			cprintf("%d Script '%s' is active and cannot be deleted.\n",
				ERROR + ILLEGAL_VALUE,
				script_name
			);
		}
		else {
			cprintf("%d unknown error\n", ERROR);
		}
	}

	else {
		cprintf("%d Invalid subcommand\n", ERROR + CMD_NOT_SUPPORTED);
	}

	msiv_store(&u, changes_made);
}



void ctdl_sieve_init(void) {
	char *cred = NULL;
	sieve2_context_t *sieve2_context = NULL;
	int res;

	/*
	 *	We don't really care about dumping the entire credits to the log
	 *	every time the server is initialized.  The documentation will suffice
	 *	for that purpose.  We are making a call to sieve2_credits() in order
	 *	to demonstrate that we have successfully linked in to libsieve.
	 */
	cred = strdup(sieve2_credits());
	if (cred == NULL) return;

	if (strlen(cred) > 60) {
		strcpy(&cred[55], "...");
	}

	SV_syslog(LOG_INFO, "%s",cred);
	free(cred);

	/* Briefly initialize a Sieve parser instance just so we can list the
	 * extensions that are available.
	 */
	res = sieve2_alloc(&sieve2_context);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_alloc() returned %d: %s", res, sieve2_errstr(res));
		return;
	}

	res = sieve2_callbacks(sieve2_context, ctdl_sieve_callbacks);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_callbacks() returned %d: %s", res, sieve2_errstr(res));
		goto BAIL;
	}

	msiv_extensions = strdup(sieve2_listextensions(sieve2_context));
	SV_syslog(LOG_INFO, "Extensions: %s", msiv_extensions);

BAIL:	res = sieve2_free(&sieve2_context);
	if (res != SIEVE2_OK) {
		SV_syslog(LOG_CRIT, "sieve2_free() returned %d: %s", res, sieve2_errstr(res));
	}

}

void cleanup_sieve(void)
{
        struct RoomProcList *ptr, *ptr2;

	if (msiv_extensions != NULL)
		free(msiv_extensions);
	msiv_extensions = NULL;

        begin_critical_section(S_SIEVELIST);
	ptr=sieve_list;
	while (ptr != NULL) {
		ptr2 = ptr->next;
		free(ptr);
		ptr = ptr2;
	}
        sieve_list = NULL;
        end_critical_section(S_SIEVELIST);
}

int serv_sieve_room(struct ctdlroom *room)
{
	if (!strcasecmp(&room->QRname[11], MAILROOM)) {
		sieve_queue_room(room);
	}
	return 0;
}

void LogSieveDebugEnable(const int n)
{
	SieveDebugEnable = n;
}
CTDL_MODULE_INIT(sieve)
{
	if (!threading)
	{
		CtdlRegisterDebugFlagHook(HKEY("sieve"), LogSieveDebugEnable, &SieveDebugEnable);
		ctdl_sieve_init();
		CtdlRegisterProtoHook(cmd_msiv, "MSIV", "Manage Sieve scripts");
	        CtdlRegisterRoomHook(serv_sieve_room);
        	CtdlRegisterSessionHook(perform_sieve_processing, EVT_HOUSE, PRIO_HOUSE + 10);
		CtdlRegisterCleanupHook(cleanup_sieve);
	}
	
        /* return our module name for the log */
	return "sieve";
}

