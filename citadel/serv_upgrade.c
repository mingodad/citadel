/*
 * $Id$
 *
 * Transparently handle the upgrading of server data formats.
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
#include "control.h"
#include "serv_extensions.h"
#include "database.h"
#include "room_ops.h"
#include "user_ops.h"
#include "msgbase.h"
#include "tools.h"
#include "serv_upgrade.h"
#include "euidindex.h"



/* 
 * Back end processing function for cmd_bmbx
 */
void cmd_bmbx_backend(struct ctdlroom *qrbuf, void *data) {
	static struct RoomProcList *rplist = NULL;
	struct RoomProcList *ptr;
	struct ctdlroom qr;

	/* Lazy programming here.  Call this function as a ForEachRoom backend
	 * in order to queue up the room names, or call it with a null room
	 * to make it do the processing.
	 */
	if (qrbuf != NULL) {
		ptr = (struct RoomProcList *)
			malloc(sizeof (struct RoomProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
		ptr->next = rplist;
		rplist = ptr;
		return;
	}

	while (rplist != NULL) {

		if (lgetroom(&qr, rplist->name) == 0) {
			lprintf(CTDL_DEBUG, "Processing <%s>...\n", rplist->name);
			if ( (qr.QRflags & QR_MAILBOX) == 0) {
				lprintf(CTDL_DEBUG, "  -- not a mailbox\n");
			}
			else {

				qr.QRgen = time(NULL);
				lprintf(CTDL_DEBUG, "  -- fixed!\n");
			}
			lputroom(&qr);
		}

		ptr = rplist;
		rplist = rplist->next;
		free(ptr);
	}
}

/*
 * quick fix to bump mailbox generation numbers
 */
void bump_mailbox_generation_numbers(void) {
	lprintf(CTDL_WARNING, "Applying security fix to mailbox rooms\n");
	ForEachRoom(cmd_bmbx_backend, NULL);
	cmd_bmbx_backend(NULL, NULL);
	return;
}


/* 
 * Back end processing function for convert_ctdluid_to_minusone()
 */
void cbtm_backend(struct ctdluser *usbuf, void *data) {
	static struct UserProcList *uplist = NULL;
	struct UserProcList *ptr;
	struct ctdluser us;

	/* Lazy programming here.  Call this function as a ForEachUser backend
	 * in order to queue up the room names, or call it with a null user
	 * to make it do the processing.
	 */
	if (usbuf != NULL) {
		ptr = (struct UserProcList *)
			malloc(sizeof (struct UserProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->user, usbuf->fullname, sizeof ptr->user);
		ptr->next = uplist;
		uplist = ptr;
		return;
	}

	while (uplist != NULL) {

		if (lgetuser(&us, uplist->user) == 0) {
			lprintf(CTDL_DEBUG, "Processing <%s>...\n", uplist->user);
			if (us.uid == CTDLUID) {
				us.uid = (-1);
			}
			lputuser(&us);
		}

		ptr = uplist;
		uplist = uplist->next;
		free(ptr);
	}
}

/*
 * quick fix to change all CTDLUID users to (-1)
 */
void convert_ctdluid_to_minusone(void) {
	lprintf(CTDL_WARNING, "Applying uid changes\n");
	ForEachUser(cbtm_backend, NULL);
	cbtm_backend(NULL, NULL);
	return;
}

/*
 * Do various things to our configuration file
 */
void update_config(void) {
	get_config();

	if (CitControl.version < 606) {
		config.c_rfc822_strict_from = 0;
	}

	if (CitControl.version < 609) {
		config.c_purge_hour = 3;
	}

	if (CitControl.version < 615) {
		config.c_ldap_port = 389;
	}

	if (CitControl.version < 623) {
		strcpy(config.c_ip_addr, "0.0.0.0");
	}

	if (CitControl.version < 650) {
		config.c_enable_fulltext = 0;
	}

	if (CitControl.version < 652) {
		config.c_auto_cull = 1;
	}

	put_config();
}




void check_server_upgrades(void) {

	get_control();
	lprintf(CTDL_INFO, "Server-hosted upgrade level is %d.%02d\n",
		(CitControl.version / 100),
		(CitControl.version % 100) );

	if (CitControl.version < REV_LEVEL) {
		lprintf(CTDL_WARNING,
			"Server hosted updates need to be processed at "
			"this time.  Please wait...\n");
	}
	else {
		return;
	}

	update_config();

	if ((CitControl.version > 000) && (CitControl.version < 555)) {
		lprintf(CTDL_EMERG,
			"Your data files are from a version of Citadel\n"
			"that is too old to be upgraded.  Sorry.\n");
		exit(EXIT_FAILURE);
	}
	if ((CitControl.version > 000) && (CitControl.version < 591)) {
		bump_mailbox_generation_numbers();
	}
	if ((CitControl.version > 000) && (CitControl.version < 608)) {
		convert_ctdluid_to_minusone();
	}
	if ((CitControl.version > 000) && (CitControl.version < 659)) {
		rebuild_euid_index();
	}

	CitControl.version = REV_LEVEL;
	put_control();
}


char *serv_upgrade_init(void)
{
	check_server_upgrades();

	/* return our Subversion id for the Log */
	return "$Id$";
}
