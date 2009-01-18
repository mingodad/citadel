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
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "database.h"
#include "room_ops.h"
#include "user_ops.h"
#include "msgbase.h"
#include "serv_upgrade.h"
#include "euidindex.h"


#include "ctdl_module.h"



/*
 * Fix up the name for Citadel user 0 and try to remove any extra users with number 0
 */
void fix_sys_user_name(void)
{
	struct ctdluser usbuf;
	char usernamekey[USERNAME_SIZE];

	/** If we have a user called Citadel rename them to SYS_Citadel */
	if (getuser(&usbuf, "Citadel") == 0)
	{
		rename_user("Citadel", "SYS_Citadel");
	}

	while (getuserbynumber(&usbuf, 0) == 0)
	{
		/* delete user with number 0 and no name */
		if (IsEmptyStr(usbuf.fullname))
			cdb_delete(CDB_USERS, "", 0);
		else
		{ /* temporarily set this user to -1 */
			usbuf.usernum = -1;
			putuser(&usbuf);
		}
	}

	/** Make sure user SYS_* is user 0 */
	while (getuserbynumber(&usbuf, -1) == 0)
	{
		if (strncmp(usbuf.fullname, "SYS_", 4))
		{	/** Delete any user 0 that doesn't start with SYS_ */
			makeuserkey(usernamekey, usbuf.fullname);
			cdb_delete(CDB_USERS, usernamekey, strlen(usernamekey));
		}
		else
		{
			usbuf.usernum = 0;
			putuser(&usbuf);
		}
	}
}


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
			CtdlLogPrintf(CTDL_DEBUG, "Processing <%s>...\n", rplist->name);
			if ( (qr.QRflags & QR_MAILBOX) == 0) {
				CtdlLogPrintf(CTDL_DEBUG, "  -- not a mailbox\n");
			}
			else {

				qr.QRgen = time(NULL);
				CtdlLogPrintf(CTDL_DEBUG, "  -- fixed!\n");
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
	CtdlLogPrintf(CTDL_WARNING, "Applying security fix to mailbox rooms\n");
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
			CtdlLogPrintf(CTDL_DEBUG, "Processing <%s>...\n", uplist->user);
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
	CtdlLogPrintf(CTDL_WARNING, "Applying uid changes\n");
	ForEachUser(cbtm_backend, NULL);
	cbtm_backend(NULL, NULL);
	return;
}


/*
 * Attempt to guess the name of the time zone currently in use
 * on the underlying host system.
 */
void guess_time_zone(void) {
	FILE *fp;
	char buf[PATH_MAX];

	fp = popen(file_guesstimezone, "r");
	if (fp) {
		if (fgets(buf, sizeof buf, fp) && (strlen(buf) > 2)) {
			buf[strlen(buf)-1] = 0;
			safestrncpy(config.c_default_cal_zone, buf, sizeof config.c_default_cal_zone);
			CtdlLogPrintf(CTDL_INFO, "Configuring timezone: %s\n", config.c_default_cal_zone);
		}
		fclose(fp);
	}
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

	if (CitControl.version < 725) {
		config.c_xmpp_c2s_port = 5222;
		config.c_xmpp_s2s_port = 5269;
	}

	if (IsEmptyStr(config.c_default_cal_zone)) {
		guess_time_zone();
	}

	put_config();
}




void check_server_upgrades(void) {

	get_control();
	CtdlLogPrintf(CTDL_INFO, "Server-hosted upgrade level is %d.%02d\n",
		(CitControl.version / 100),
		(CitControl.version % 100) );

	if (CitControl.version < REV_LEVEL) {
		CtdlLogPrintf(CTDL_WARNING,
			"Server hosted updates need to be processed at "
			"this time.  Please wait...\n");
	}
	else {
		return;
	}

	update_config();

	if ((CitControl.version > 000) && (CitControl.version < 555)) {
		CtdlLogPrintf(CTDL_EMERG,
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
	if (CitControl.version < 735) {
		fix_sys_user_name();
	}
	if (CitControl.version < 736) {
		rebuild_usersbynumber();
	}
	CitControl.version = REV_LEVEL;
	put_control();
}


CTDL_MODULE_UPGRADE(upgrade)
{
	check_server_upgrades();
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
