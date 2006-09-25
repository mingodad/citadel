/*
 * $Id: serv_test.c 3850 2005-09-13 14:00:24Z ajc $
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
	lprintf(CTDL_DEBUG, "Performing sieve processing on msg <%ld>\n", msgnum);
	return;
}


/*
 * Perform sieve processing for a single room
 * FIXME ... actually do this instead of just talking about it
 */
void sieve_do_room(char *roomname) {

	/* FIXME check to see if this room has any sieve scripts to run */

	lprintf(CTDL_DEBUG, "Performing Sieve processing for <%s>\n", roomname);

	if (getroom(&CC->room, roomname) != 0) {
		lprintf(CTDL_CRIT, "ERROR: cannot load <%s>\n", roomname);
		return;
	}

	/* Do something useful */
	/* CtdlForEachMessage(MSGS_GT, sc.lastsent, NULL, NULL, NULL, */
	/* FIXME figure out which messages haven't yet been processed by sieve */
	CtdlForEachMessage(MSGS_LAST, 1, NULL, NULL, NULL,
		sieve_do_msg,
		NULL		/* data for callback could be the script? */
	);
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
