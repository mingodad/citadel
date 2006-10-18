/*
 * $Id: serv_sieve.c 3850 2005-09-13 14:00:24Z ajc $
 *
 * This module glues libSieve to the Citadel server in order to implement
 * the Sieve mailbox filtering language (RFC 3028).
 *
 * This code is released under the terms of the GNU General Public License. 
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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "tools.h"

#ifdef HAVE_LIBSIEVE

#include "serv_sieve.h"

struct RoomProcList *sieve_list = NULL;
char *msiv_extensions = NULL;


/*
 * Callback function to send libSieve trace messages to Citadel log facility
 * Set ctdl_libsieve_debug=1 to see extremely verbose libSieve trace
 */
int ctdl_debug(sieve2_context_t *s, void *my)
{
	static int ctdl_libsieve_debug = 0;

	if (ctdl_libsieve_debug) {
		lprintf(CTDL_DEBUG, "Sieve: level [%d] module [%s] file [%s] function [%s]\n",
			sieve2_getvalue_int(s, "level"),
			sieve2_getvalue_string(s, "module"),
			sieve2_getvalue_string(s, "file"),
			sieve2_getvalue_string(s, "function"));
		lprintf(CTDL_DEBUG, "       message [%s]\n",
			sieve2_getvalue_string(s, "message"));
	}
	return SIEVE2_OK;
}


/*
 * Callback function to log script parsing errors
 */
int ctdl_errparse(sieve2_context_t *s, void *my)
{
	lprintf(CTDL_WARNING, "Error in script, line %d: %s\n",
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
	lprintf(CTDL_WARNING, "Error executing script: %s\n",
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

	lprintf(CTDL_DEBUG, "Action is REDIRECT, recipient <%s>\n", recp);

	valid = validate_recipients(recp);
	if (valid == NULL) {
		lprintf(CTDL_WARNING, "REDIRECT failed: bad recipient <%s>\n", recp);
		return SIEVE2_ERROR_BADARGS;
	}
	if (valid->num_error > 0) {
		lprintf(CTDL_WARNING, "REDIRECT failed: bad recipient <%s>\n", recp);
		free(valid);
		return SIEVE2_ERROR_BADARGS;
	}

	msg = CtdlFetchMessage(cs->msgnum, 1);
	if (msg == NULL) {
		lprintf(CTDL_WARNING, "REDIRECT failed: unable to fetch msg %ld\n", cs->msgnum);
		free(valid);
		return SIEVE2_ERROR_BADARGS;
	}

	CtdlSubmitMsg(msg, valid, NULL);
	cs->actiontaken = 1;
	free(valid);
	CtdlFreeMessage(msg);
	return SIEVE2_OK;
}


/*
 * Callback function to indicate that a message *will* be kept in the inbox
 */
int ctdl_keep(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	
	lprintf(CTDL_DEBUG, "Action is KEEP\n");

	cs->keep = 1;
	cs->actiontaken = 1;
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

	lprintf(CTDL_DEBUG, "Action is FILEINTO, destination is <%s>\n", dest_folder);

	/* FILEINTO 'INBOX' is the same thing as KEEP */
	if ( (!strcasecmp(dest_folder, "INBOX")) || (!strcasecmp(dest_folder, MAILROOM)) ) {
		cs->keep = 1;
		cs->actiontaken = 1;
		return SIEVE2_OK;
	}

	/* Remember what room we came from */
	safestrncpy(original_room_name, CC->room.QRname, sizeof original_room_name);

	/* First try a mailbox name match (check personal mail folders first) */
	snprintf(foldername, sizeof foldername, "%010ld.%s", cs->usernum, dest_folder);
	c = getroom(&CC->room, foldername);

	/* Then a regular room name match (public and private rooms) */
	if (c < 0) {
		safestrncpy(foldername, dest_folder, sizeof foldername);
		c = getroom(&CC->room, foldername);
	}

	if (c < 0) {
		lprintf(CTDL_WARNING, "FILEINTO failed: target <%s> does not exist\n", dest_folder);
		return SIEVE2_ERROR_BADARGS;
	}

	/* Yes, we actually have to go there */
	usergoto(NULL, 0, 0, NULL, NULL);

	c = CtdlSaveMsgPointersInRoom(NULL, &cs->msgnum, 1, 0, NULL);

	/* Go back to the room we came from */
	if (strcasecmp(original_room_name, CC->room.QRname)) {
		usergoto(original_room_name, 0, 0, NULL, NULL);
	}

	if (c == 0) {
		cs->actiontaken = 1;
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

	lprintf(CTDL_DEBUG, "Action is DISCARD\n");

	/* Yes, this is really all there is to it.  Since we are not setting "keep" to 1,
	 * the message will be discarded because "some other action" was successfully taken.
	 */
	cs->actiontaken = 1;
	return SIEVE2_OK;
}



/*
 * Callback function to indicate that a message should be rejected
 */
int ctdl_reject(sieve2_context_t *s, void *my)
{
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;
	char *reject_text = NULL;

	lprintf(CTDL_DEBUG, "Action is REJECT\n");

	/* If we don't know who sent the message, do a DISCARD instead. */
	if (strlen(cs->sender) == 0) {
		lprintf(CTDL_INFO, "Unknown sender.  Doing DISCARD instead of REJECT.\n");
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
		"Citadel",
		cs->sender,
		NULL,
		reject_text,
		FMT_RFC822,
		"Delivery status notification"
	);

	free(reject_text);
	cs->actiontaken = 1;
	return SIEVE2_OK;
}



/*
 * Callback function to indicate that a vacation message should be generated
 * FIXME implement this
 */
int ctdl_vacation(sieve2_context_t *s, void *my)
{
	lprintf(CTDL_DEBUG, "Action is VACATION\n");
	return SIEVE2_ERROR_UNSUPPORTED;
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
 * FIXME implement this
 */
int ctdl_getenvelope(sieve2_context_t *s, void *my)
{
	return SIEVE2_ERROR_UNSUPPORTED;
}


/*
 * Callback function to fetch message body
 * FIXME implement this
 */
int ctdl_getbody(sieve2_context_t *s, void *my)
{
	return SIEVE2_ERROR_UNSUPPORTED;
}


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
 * Callback function to retrieve the sieve script
 */
int ctdl_getscript(sieve2_context_t *s, void *my) {
	struct sdm_script *sptr;
	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	for (sptr=cs->u->first_script; sptr!=NULL; sptr=sptr->next) {
		if (sptr->script_active > 0) {
			lprintf(CTDL_DEBUG, "ctdl_getscript() is using script '%s'\n", sptr->script_name);
			sieve2_setvalue_string(s, "script", sptr->script_content);
			return SIEVE2_OK;
		}
	}
		
	lprintf(CTDL_DEBUG, "ctdl_getscript() found no active script\n");
	return SIEVE2_ERROR_GETSCRIPT;
}

/*
 * Callback function to retrieve message headers
 */
int ctdl_getheaders(sieve2_context_t *s, void *my) {

	struct ctdl_sieve *cs = (struct ctdl_sieve *)my;

	lprintf(CTDL_DEBUG, "ctdl_getheaders() was called\n");

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
}



/*
 * Perform sieve processing for one message (called by sieve_do_room() for each message)
 */
void sieve_do_msg(long msgnum, void *userdata) {
	struct sdm_userdata *u = (struct sdm_userdata *) userdata;
	sieve2_context_t *sieve2_context = u->sieve2_context;
	struct ctdl_sieve my;
	int res;
	struct CtdlMessage *msg;

	lprintf(CTDL_DEBUG, "Performing sieve processing on msg <%ld>\n", msgnum);

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) return;

	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ONLY, 0, 1);
	my.rfc822headers = CC->redirect_buffer;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	my.keep = 0;		/* Don't keep a copy in the inbox unless a callback tells us to do so */
	my.actiontaken = 0;	/* Keep track of whether any actions were successfully taken */
	my.usernum = atol(CC->room.QRname);	/* Keep track of the owner of the room's namespace */
	my.msgnum = msgnum;	/* Keep track of the message number in our local store */
	my.u = u;		/* Hand off a pointer to the rest of this info */

	/* Keep track of the recipient so we can do handling based on it later */
	process_rfc822_addr(msg->cm_fields['R'], my.recp_user, my.recp_node, my.recp_name);

	/* Keep track of the sender so we can use it for REJECT and VACATION responses */
	if (msg->cm_fields['F'] != NULL) {
		safestrncpy(my.sender, msg->cm_fields['F'], sizeof my.sender);
	}
	else if ( (msg->cm_fields['A'] != NULL) && (msg->cm_fields['N'] != NULL) ) {
		snprintf(my.sender, sizeof my.sender, "%s@%s", msg->cm_fields['A'], msg->cm_fields['N']);
	}
	else if (msg->cm_fields['A'] != NULL) {
		safestrncpy(my.sender, msg->cm_fields['A'], sizeof my.sender);
	}
	else {
		strcpy(my.sender, "");
	}

	free(msg);

	sieve2_setvalue_string(sieve2_context, "allheaders", my.rfc822headers);
	
	lprintf(CTDL_DEBUG, "Calling sieve2_execute()\n");
	res = sieve2_execute(sieve2_context, &my);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_execute() returned %d: %s\n", res, sieve2_errstr(res));
	}

	free(my.rfc822headers);
	my.rfc822headers = NULL;

	/*
	 * Delete the message from the inbox unless either we were told not to, or
	 * if no other action was successfully taken.
	 */
	if ( (!my.keep) && (my.actiontaken) ) {
		lprintf(CTDL_DEBUG, "keep is 0 -- deleting message from inbox\n");
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "", 0);
	}

	lprintf(CTDL_DEBUG, "Completed sieve processing on msg <%ld>\n", msgnum);
	u->lastproc = msgnum;

	return;
}



/*
 * Given the on-disk representation of our Sieve config, load
 * it into an in-memory data structure.
 */
void parse_sieve_config(char *conf, struct sdm_userdata *u) {
	char *ptr;
	char *c;
	char keyword[256];
	struct sdm_script *sptr;

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

	u->config_msgnum = msgnum;
	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		u->config_msgnum = (-1) ;
		return;
	}

	conf = msg->cm_fields['M'];
	msg->cm_fields['M'] = NULL;
	CtdlFreeMessage(msg);

	if (conf != NULL) {
		parse_sieve_config(conf, u);
		free(conf);
	}

}


/* 
 * Write our citadel sieve config back to disk
 */
void rewrite_ctdl_sieve_config(struct sdm_userdata *u) {
	char *text;
	struct sdm_script *sptr;


	text = malloc(1024);
	snprintf(text, 1024,
		"Content-type: application/x-citadel-sieve-config\n"
		"\n"
		CTDLSIEVECONFIGSEPARATOR
		"lastproc|%ld"
		CTDLSIEVECONFIGSEPARATOR
	,
		u->lastproc
	);

	while (u->first_script != NULL) {
		text = realloc(text, strlen(text) + strlen(u->first_script->script_content) + 256);
		sprintf(&text[strlen(text)], "script|%s|%d|%s" CTDLSIEVECONFIGSEPARATOR,
			u->first_script->script_name,
			u->first_script->script_active,
			u->first_script->script_content
		);
		sptr = u->first_script;
		u->first_script = u->first_script->next;
		free(sptr->script_content);
		free(sptr);
	}

	/* Save the config */
	quickie_message("Citadel", NULL, u->config_roomname,
			text,
			4,
			"Sieve configuration"
	);

	/* And delete the old one */
	if (u->config_msgnum > 0) {
		CtdlDeleteMessages(u->config_roomname, &u->config_msgnum, 1, "", 0);
	}

}


/*
 * This is our callback registration table for libSieve.
 */
sieve2_callback_t ctdl_sieve_callbacks[] = {
	{ SIEVE2_ACTION_REJECT,		ctdl_reject				},
	{ SIEVE2_ACTION_NOTIFY,		NULL	/* ctdl_notify */		},
	{ SIEVE2_ACTION_VACATION,	ctdl_vacation				},
	{ SIEVE2_ERRCALL_PARSE,		ctdl_errparse				},
	{ SIEVE2_ERRCALL_RUNTIME,	ctdl_errexec				},
	{ SIEVE2_ACTION_FILEINTO,	ctdl_fileinto				},
	{ SIEVE2_ACTION_REDIRECT,	ctdl_redirect				},
	{ SIEVE2_ACTION_DISCARD,	ctdl_discard				},
	{ SIEVE2_ACTION_KEEP,		ctdl_keep				},
	{ SIEVE2_SCRIPT_GETSCRIPT,	ctdl_getscript				},
	{ SIEVE2_DEBUG_TRACE,		ctdl_debug				},
	{ SIEVE2_MESSAGE_GETALLHEADERS,	ctdl_getheaders				},
	{ SIEVE2_MESSAGE_GETSUBADDRESS,	NULL	/* ctdl_getsubaddress */	},
	{ SIEVE2_MESSAGE_GETENVELOPE,	ctdl_getenvelope			},
	{ SIEVE2_MESSAGE_GETBODY,	ctdl_getbody				},
	{ SIEVE2_MESSAGE_GETSIZE,	ctdl_getsize				},
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
	snprintf(u.config_roomname, sizeof u.config_roomname, "%010ld.%s", atol(roomname), SIEVERULES);
	if (getroom(&CC->room, u.config_roomname) != 0) {
		lprintf(CTDL_DEBUG, "<%s> does not exist.  No processing is required.\n", u.config_roomname);
		return;
	}

	/*
	 * Find the sieve scripts and control record and do something
	 */
	u.config_msgnum = (-1);
	CtdlForEachMessage(MSGS_LAST, 1, NULL, SIEVECONFIG, NULL,
		get_sieve_config_backend, (void *)&u );

	if (u.config_msgnum < 0) {
		lprintf(CTDL_DEBUG, "No Sieve rules exist.  No processing is required.\n");
		return;
	}

	lprintf(CTDL_DEBUG, "Rules found.  Performing Sieve processing for <%s>\n", roomname);

	if (getroom(&CC->room, roomname) != 0) {
		lprintf(CTDL_CRIT, "ERROR: cannot load <%s>\n", roomname);
		return;
	}

	/* Initialize the Sieve parser */
	
	res = sieve2_alloc(&sieve2_context);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_alloc() returned %d: %s\n", res, sieve2_errstr(res));
		return;
	}

	res = sieve2_callbacks(sieve2_context, ctdl_sieve_callbacks);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_callbacks() returned %d: %s\n", res, sieve2_errstr(res));
		goto BAIL;
	}

	/* Validate the script */

	struct ctdl_sieve my;		/* dummy ctdl_sieve struct just to pass "u" slong */
	memset(&my, 0, sizeof my);
	my.u = &u;
	res = sieve2_validate(sieve2_context, &my);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_validate() returned %d: %s\n", res, sieve2_errstr(res));
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
		lprintf(CTDL_CRIT, "sieve2_free() returned %d: %s\n", res, sieve2_errstr(res));
	}

	/* Rewrite the config if we have to */
	if (u.lastproc > orig_lastproc) {
		rewrite_ctdl_sieve_config(&u);
	}
}


/*
 * Perform sieve processing for all rooms which require it
 */
void perform_sieve_processing(void) {
	struct RoomProcList *ptr = NULL;

	if (sieve_list != NULL) {
		lprintf(CTDL_DEBUG, "Begin Sieve processing\n");
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
	if (getroom(&CC->room, SIEVERULES) == 0) {
	
		u->config_msgnum = (-1);
		strcpy(u->config_roomname, CC->room.QRname);
		CtdlForEachMessage(MSGS_LAST, 1, NULL, SIEVECONFIG, NULL,
			get_sieve_config_backend, (void *)u );

	}

	if (strcmp(CC->room.QRname, hold_rm)) {
		getroom(&CC->room, hold_rm);    /* return to saved room */
	}
}

void msiv_store(struct sdm_userdata *u) {
	rewrite_ctdl_sieve_config(u);
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

	if (strlen(script_name) == 0) {
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

	memset(&u, 0, sizeof(struct sdm_userdata));

	if (CtdlAccessCheck(ac_logged_in)) return;
	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);
	msiv_load(&u);

	if (!strcasecmp(subcmd, "putscript")) {
		extract_token(script_name, argbuf, 1, '|', sizeof script_name);
		if (strlen(script_name) > 0) {
			cprintf("%d Transmit script now\n", SEND_LISTING);
			script_content = CtdlReadMessageBody("000", config.c_maxmsglen, NULL, 0);
			msiv_putscript(&u, script_name, script_content);
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
			cprintf("%d Script:\n", SEND_LISTING);
			cprintf("%s000\n", script_content);
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

	msiv_store(&u);
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

	lprintf(CTDL_INFO, "%s\n",cred);
	free(cred);

	/* Briefly initialize a Sieve parser instance just so we can list the
	 * extensions that are available.
	 */
	res = sieve2_alloc(&sieve2_context);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_alloc() returned %d: %s\n", res, sieve2_errstr(res));
		return;
	}

	res = sieve2_callbacks(sieve2_context, ctdl_sieve_callbacks);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_callbacks() returned %d: %s\n", res, sieve2_errstr(res));
		goto BAIL;
	}

	msiv_extensions = strdup(sieve2_listextensions(sieve2_context));
	lprintf(CTDL_INFO, "Extensions: %s\n", msiv_extensions);

BAIL:	res = sieve2_free(&sieve2_context);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_free() returned %d: %s\n", res, sieve2_errstr(res));
	}

}



char *serv_sieve_init(void)
{
	ctdl_sieve_init();
	CtdlRegisterProtoHook(cmd_msiv, "MSIV", "Manage Sieve scripts");
	return "$Id: serv_sieve.c 3850 2005-09-13 14:00:24Z ajc $";
}

#else	/* HAVE_LIBSIEVE */

char *serv_sieve_init(void)
{
	lprintf(CTDL_INFO, "This server is missing libsieve.  Mailbox filtering will be disabled.\n");
	return "$Id: serv_sieve.c 3850 2005-09-13 14:00:24Z ajc $";
}

#endif	/* HAVE_LIBSIEVE */
