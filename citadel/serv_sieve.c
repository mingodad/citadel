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
#include "tools.h"

#ifdef HAVE_LIBSIEVE

#include <sieve2.h>
#include <sieve2_error.h>
#include "serv_sieve.h"

struct RoomProcList *sieve_list = NULL;

struct ctdl_sieve {
	char *rfc822headers;
	int actiontaken;		/* Set to 1 if the message was successfully acted upon */
	int keep;			/* Set to 1 to suppress message deletion from the inbox */
	long usernum;			/* Owner of the mailbox we're processing */
	long msgnum;			/* Message base ID of the message being processed */
};


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

	lprintf(CTDL_DEBUG, "REDIRECT to <%s> (FIXME complete this)\n",
		sieve2_getvalue_string(s, "address"));

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
 * Callback function to retrieve the sieve script
 */
int ctdl_getscript(sieve2_context_t *s, void *my) {

	lprintf(CTDL_DEBUG, "ctdl_getscript() was called\n");

	sieve2_setvalue_string(s, "script",
		
		"require \"fileinto\";						\n"
		"    if header :contains [\"From\"]  [\"coyote\"] {		\n"
		"        redirect \"acm@frobnitzm.edu\";			\n"
		"    } elsif header :contains \"Subject\" \"JIHAD\" {		\n"
		"        fileinto \"jihad\";					\n"
		"    } else {							\n"
		"        keep; 							\n"
		"    }								\n"

	);

        return SIEVE2_OK;
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
	sieve2_context_t *sieve2_context = (sieve2_context_t *) userdata;
	struct ctdl_sieve my;
	int res;

	lprintf(CTDL_DEBUG, "Performing sieve processing on msg <%ld>\n", msgnum);

	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputMsg(msgnum, MT_RFC822, HEADERS_ONLY, 0, 1, NULL);
	my.rfc822headers = CC->redirect_buffer;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	my.keep = 0;		/* Don't keep a copy in the inbox unless a callback tells us to do so */
	my.actiontaken = 0;	/* Keep track of whether any actions were successfully taken */
	my.usernum = atol(CC->room.QRname);	/* Keep track of the owner of the room's namespace */
	my.msgnum = msgnum;	/* Keep track of the message number in our local store */

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

	return;
}


/*
 * Perform sieve processing for a single room
 * FIXME ... actually do this instead of just talking about it
 */
void sieve_do_room(char *roomname) {
	
	sieve2_context_t *sieve2_context = NULL;	/* Context for sieve parser */
	int res;					/* Return code from libsieve calls */

	/*
	 * CALLBACK REGISTRATION TABLE
	 */
	sieve2_callback_t ctdl_sieve_callbacks[] = {
		{ SIEVE2_DEBUG_TRACE,           ctdl_debug       },
/*
		{ SIEVE2_ACTION_REJECT,         my_reject        },
		{ SIEVE2_ACTION_NOTIFY,         my_notify        },
		{ SIEVE2_ACTION_VACATION,       my_vacation      },
*/
		{ SIEVE2_ERRCALL_PARSE,         ctdl_errparse    },
		{ SIEVE2_ERRCALL_RUNTIME,       ctdl_errexec     },
		{ SIEVE2_ACTION_FILEINTO,       ctdl_fileinto    },
		{ SIEVE2_ACTION_REDIRECT,       ctdl_redirect    },
		{ SIEVE2_ACTION_DISCARD,        ctdl_discard     },
		{ SIEVE2_ACTION_KEEP,           ctdl_keep        },
		{ SIEVE2_SCRIPT_GETSCRIPT,      ctdl_getscript   },
		{ SIEVE2_MESSAGE_GETHEADER,     NULL             },	/* We don't support one header at a time. */
		{ SIEVE2_MESSAGE_GETALLHEADERS, ctdl_getheaders  },	/* libSieve can parse headers itself, so we'll use that. */
/*
		{ SIEVE2_MESSAGE_GETSUBADDRESS, my_getsubaddress },
		{ SIEVE2_MESSAGE_GETENVELOPE,   my_getenvelope   },
		{ SIEVE2_MESSAGE_GETBODY,       my_getbody       },
		{ SIEVE2_MESSAGE_GETSIZE,       my_getsize       },
*/
		{ 0 }
	};

	/* FIXME check to see if this room has any sieve scripts to run */

	lprintf(CTDL_DEBUG, "Performing Sieve processing for <%s>\n", roomname);

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

	res = sieve2_validate(sieve2_context, NULL);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_validate() returned %d: %s\n", res, sieve2_errstr(res));
		goto BAIL;
	}

	/* Do something useful */
	/* CtdlForEachMessage(MSGS_GT, sc.lastsent, NULL, NULL, NULL, */
	/* FIXME figure out which messages haven't yet been processed by sieve */
	CtdlForEachMessage(MSGS_LAST, 1, NULL, NULL, NULL,
		sieve_do_msg,
		(void *) sieve2_context
	);

BAIL:
	res = sieve2_free(&sieve2_context);
	if (res != SIEVE2_OK) {
		lprintf(CTDL_CRIT, "sieve2_free() returned %d: %s\n", res, sieve2_errstr(res));
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



/**
 *	We don't really care about dumping the entire credits to the log
 *	every time the server is initialized.  The documentation will suffice
 *	for that purpose.  We are making a call to sieve2_credits() in order
 *	to demonstrate that we have successfully linked in to libsieve.
 */
void log_the_sieve2_credits(void) {
	char *cred = NULL;

	cred = strdup(sieve2_credits());
	if (cred == NULL) return;

	if (strlen(cred) > 60) {
		strcpy(&cred[55], "...");
	}

	lprintf(CTDL_INFO, "%s\n",cred);
	free(cred);
}


char *serv_sieve_init(void)
{
	log_the_sieve2_credits();
	return "$Id: serv_sieve.c 3850 2005-09-13 14:00:24Z ajc $";
}

#else	/* HAVE_LIBSIEVE */

char *serv_sieve_init(void)
{
	lprintf(CTDL_INFO, "This server is missing libsieve.  Mailbox filtering will be disabled.\n");
	return "$Id: serv_sieve.c 3850 2005-09-13 14:00:24Z ajc $";
}

#endif	/* HAVE_LIBSIEVE */
