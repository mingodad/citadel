/*
 * Transparently handle the upgrading of server data formats.
 *
 * Copyright (c) 1987-2015 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version 3.
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
	if (CtdlGetUser(&usbuf, "Citadel") == 0)
	{
		rename_user("Citadel", "SYS_Citadel");
	}

	while (CtdlGetUserByNumber(&usbuf, 0) == 0)
	{
		/* delete user with number 0 and no name */
		if (IsEmptyStr(usbuf.fullname)) {
			cdb_delete(CDB_USERS, "", 0);
		}
		else {
			/* temporarily set this user to -1 */
			usbuf.usernum = -1;
			CtdlPutUser(&usbuf);
		}
	}

	/* Make sure user SYS_* is user 0 */
	while (CtdlGetUserByNumber(&usbuf, -1) == 0)
	{
		if (strncmp(usbuf.fullname, "SYS_", 4))
		{	/* Delete any user 0 that doesn't start with SYS_ */
			makeuserkey(usernamekey, usbuf.fullname, cutuserkey(usbuf.fullname));
			cdb_delete(CDB_USERS, usernamekey, strlen(usernamekey));
		}
		else {
			usbuf.usernum = 0;
			CtdlPutUser(&usbuf);
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

	/* Lazy programming here.  Call this function as a CtdlForEachRoom backend
	 * in order to queue up the room names, or call it with a null room
	 * to make it do the processing.
	 */
	if (qrbuf != NULL) {
		ptr = (struct RoomProcList *) malloc(sizeof (struct RoomProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
		ptr->next = rplist;
		rplist = ptr;
		return;
	}

	while (rplist != NULL) {

		if (CtdlGetRoomLock(&qr, rplist->name) == 0) {
			syslog(LOG_DEBUG, "Processing <%s>...", rplist->name);
			if ( (qr.QRflags & QR_MAILBOX) == 0) {
				syslog(LOG_DEBUG, "  -- not a mailbox");
			}
			else {

				qr.QRgen = time(NULL);
				syslog(LOG_DEBUG, "  -- fixed!");
			}
			CtdlPutRoomLock(&qr);
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
	syslog(LOG_WARNING, "Applying security fix to mailbox rooms");
	CtdlForEachRoom(cmd_bmbx_backend, NULL);
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

		if (CtdlGetUserLock(&us, uplist->user) == 0) {
			syslog(LOG_DEBUG, "Processing <%s>...", uplist->user);
			if (us.uid == CTDLUID) {
				us.uid = (-1);
			}
			CtdlPutUserLock(&us);
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
	syslog(LOG_WARNING, "Applying uid changes");
	ForEachUser(cbtm_backend, NULL);
	cbtm_backend(NULL, NULL);
	return;
}



/*
 * These accounts may have been created by code that ran between mid 2008 and early 2011.
 * If present they are no longer in use and may be deleted.
 */
void remove_thread_users(void) {
	char *deleteusers[] = {
		"SYS_checkpoint",
		"SYS_extnotify",
		"SYS_IGnet Queue",
		"SYS_indexer",
		"SYS_network",
		"SYS_popclient",
		"SYS_purger",
		"SYS_rssclient",
		"SYS_select_on_master",
		"SYS_SMTP Send"
	};

	int i;
	struct ctdluser usbuf;
	for (i=0; i<(sizeof(deleteusers)/sizeof(char *)); ++i) {
		if (CtdlGetUser(&usbuf, deleteusers[i]) == 0) {
			usbuf.axlevel = 0;
			strcpy(usbuf.password, "deleteme");
			CtdlPutUser(&usbuf);
			syslog(LOG_INFO,
				"System user account <%s> is no longer in use and will be deleted.",
				deleteusers[i]
			);
		}
	}
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
			syslog(LOG_INFO, "Configuring timezone: %s", config.c_default_cal_zone);
		}
		fclose(fp);
	}
}


/*
 * Perform any upgrades that can be done automatically based on our knowledge of the previous
 * version of Citadel server that was running here.
 *
 * Note that if the previous version was 0 then this is a new installation running for the first time.
 */
void update_config(void) {
	get_config();

	if (CitControl.MM_hosted_upgrade_level < 606) {
		config.c_rfc822_strict_from = 0;
	}

	if (CitControl.MM_hosted_upgrade_level < 609) {
		config.c_purge_hour = 3;
	}

	if (CitControl.MM_hosted_upgrade_level < 615) {
		config.c_ldap_port = 389;
	}

	if (CitControl.MM_hosted_upgrade_level < 623) {
		strcpy(config.c_ip_addr, "*");
	}

	if (CitControl.MM_hosted_upgrade_level < 650) {
		config.c_enable_fulltext = 1;
	}

	if (CitControl.MM_hosted_upgrade_level < 652) {
		config.c_auto_cull = 1;
	}

	if (CitControl.MM_hosted_upgrade_level < 725) {
		config.c_xmpp_c2s_port = 5222;
		config.c_xmpp_s2s_port = 5269;
	}

	if (CitControl.MM_hosted_upgrade_level < 830) {
		config.c_nntp_port = 119;
		config.c_nntps_port = 563;
	}

	if (IsEmptyStr(config.c_default_cal_zone)) {
		guess_time_zone();
	}

	put_config();
}



/*
 * Based on the server version number reported by the existing database,
 * run in-place data format upgrades until everything is up to date.
 */
void check_server_upgrades(void) {

	get_control();
	syslog(LOG_INFO, "Existing database version on disk is %d.%02d",
		(CitControl.MM_hosted_upgrade_level / 100),
		(CitControl.MM_hosted_upgrade_level % 100)
	);

	if (CitControl.MM_hosted_upgrade_level < REV_LEVEL) {
		syslog(LOG_WARNING,
			"Server hosted updates need to be processed at this time.  Please wait..."
		);
	}
	else {
		return;
	}

	update_config();

	if ((CitControl.MM_hosted_upgrade_level > 000) && (CitControl.MM_hosted_upgrade_level < 555)) {
		syslog(LOG_EMERG, "This database is too old to be upgraded.  Citadel server will exit.");
		exit(EXIT_FAILURE);
	}
	if ((CitControl.MM_hosted_upgrade_level > 000) && (CitControl.MM_hosted_upgrade_level < 591)) {
		bump_mailbox_generation_numbers();
	}
	if ((CitControl.MM_hosted_upgrade_level > 000) && (CitControl.MM_hosted_upgrade_level < 608)) {
		convert_ctdluid_to_minusone();
	}
	if ((CitControl.MM_hosted_upgrade_level > 000) && (CitControl.MM_hosted_upgrade_level < 659)) {
		rebuild_euid_index();
	}
	if (CitControl.MM_hosted_upgrade_level < 735) {
		fix_sys_user_name();
	}
	if (CitControl.MM_hosted_upgrade_level < 736) {
		rebuild_usersbynumber();
	}
	if (CitControl.MM_hosted_upgrade_level < 790) {
		remove_thread_users();
	}
	if (CitControl.MM_hosted_upgrade_level < 810) {
		struct ctdlroom QRoom;
		if (!CtdlGetRoom(&QRoom, SMTP_SPOOLOUT_ROOM)) {
			QRoom.QRdefaultview = VIEW_QUEUE;
			CtdlPutRoom(&QRoom);
		}
		if (!CtdlGetRoom(&QRoom, FNBL_QUEUE_ROOM)) {
			QRoom.QRdefaultview = VIEW_QUEUE;
			CtdlPutRoom(&QRoom);
		}
	}

	CitControl.MM_hosted_upgrade_level = REV_LEVEL;

	/*
	 * Negative values for maxsessions are not allowed.
	 */
	if (config.c_maxsessions < 0) {
		config.c_maxsessions = 0;
	}

	/* We need a system default message expiry policy, because this is
	 * the top level and there's no 'higher' policy to fall back on.
	 * By default, do not expire messages at all.
	 */
	if (config.c_ep.expire_mode == 0) {
		config.c_ep.expire_mode = EXPIRE_MANUAL;
		config.c_ep.expire_value = 0;
	}

	put_control();
}


CTDL_MODULE_UPGRADE(upgrade)
{
	check_server_upgrades();
	
	/* return our module id for the Log */
	return "upgrade";
}
