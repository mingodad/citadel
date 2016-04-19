/*
 * Transparently handle the upgrading of server data formats.  If we see
 * an existing version number of our database, we can make some intelligent
 * guesses about what kind of data format changes need to be applied, and
 * we apply them transparently.
 *
 * Copyright (c) 1987-2016 by the citadel.org team
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
			CtdlSetConfigStr("c_default_cal_zone", buf);
			syslog(LOG_INFO, "Configuring timezone: %s", buf);
		}
		fclose(fp);
	}
}


/*
 * Per-room callback function for ingest_old_roominfo_and_roompic_files()
 *
 * This is the second pass, where we process the list of rooms with info or pic files.
 */
void iorarf_oneroom(char *roomname, char *infofile, char *picfile)
{
	FILE *fp;
	long data_length;
	char *unencoded_data;
	char *encoded_data;
	long info_msgnum = 0;
	long pic_msgnum = 0;
	char subject[SIZ];

	// Test for the presence of a legacy "room info file"
	if (!IsEmptyStr(infofile)) {
		fp = fopen(infofile, "r");
	}
	else {
		fp = NULL;
	}
	if (fp) {
		fseek(fp, 0, SEEK_END);
		data_length = ftell(fp);

		if (data_length >= 1) {
			rewind(fp);
			unencoded_data = malloc(data_length);
			if (unencoded_data) {
				fread(unencoded_data, data_length, 1, fp);
				encoded_data = malloc((data_length * 2) + 100);
				if (encoded_data) {
					sprintf(encoded_data, "Content-type: text/plain\nContent-transfer-encoding: base64\n\n");
					CtdlEncodeBase64(&encoded_data[strlen(encoded_data)], unencoded_data, data_length, 1);
					snprintf(subject, sizeof subject, "Imported room banner for %s", roomname);
					info_msgnum = quickie_message("Citadel", NULL, NULL, SYSCONFIGROOM, encoded_data, FMT_RFC822, subject);
					free(encoded_data);
				}
				free(unencoded_data);
			}
		}
		fclose(fp);
		if (info_msgnum > 0) unlink(infofile);
	}

	// Test for the presence of a legacy "room picture file" and import it.
	if (!IsEmptyStr(picfile)) {
		fp = fopen(picfile, "r");
	}
	else {
		fp = NULL;
	}
	if (fp) {
		fseek(fp, 0, SEEK_END);
		data_length = ftell(fp);

		if (data_length >= 1) {
			rewind(fp);
			unencoded_data = malloc(data_length);
			if (unencoded_data) {
				fread(unencoded_data, data_length, 1, fp);
				encoded_data = malloc((data_length * 2) + 100);
				if (encoded_data) {
					sprintf(encoded_data, "Content-type: image/gif\nContent-transfer-encoding: base64\n\n");
					CtdlEncodeBase64(&encoded_data[strlen(encoded_data)], unencoded_data, data_length, 1);
					snprintf(subject, sizeof subject, "Imported room icon for %s", roomname);
					pic_msgnum = quickie_message("Citadel", NULL, NULL, SYSCONFIGROOM, encoded_data, FMT_RFC822, subject);
					free(encoded_data);
				}
				free(unencoded_data);
			}
		}
		fclose(fp);
		if (pic_msgnum > 0) unlink(picfile);
	}

	// Now we have the message numbers of our new banner and icon.  Record them in the room record.
	// NOTE: we are not deleting the old msgnum_info because that position in the record was previously
	// a pointer to the highest message number which existed in the room when the info file was saved,
	// and we don't want to delete messages that are not *actually* old banners.
	struct ctdlroom qrbuf;
	if (CtdlGetRoomLock(&qrbuf, roomname) == 0) {
		qrbuf.msgnum_info = info_msgnum;
		qrbuf.msgnum_pic = pic_msgnum;
		CtdlPutRoomLock(&qrbuf);
	}

}


struct iorarf_list {
	struct iorarf_list *next;
	char name[ROOMNAMELEN];
	char info[PATH_MAX];
	char pic[PATH_MAX];
};


/*
 * Per-room callback function for ingest_old_roominfo_and_roompic_files()
 *
 * This is the first pass, where the list of qualifying rooms is gathered.
 */
void iorarf_backend(struct ctdlroom *qrbuf, void *data)
{
	FILE *fp;
	struct iorarf_list **iorarf_list = (struct iorarf_list **)data;

	struct iorarf_list *i = malloc(sizeof(struct iorarf_list));
	i->next = *iorarf_list;
	strcpy(i->name, qrbuf->QRname);
	strcpy(i->info, "");
	strcpy(i->pic, "");

	// Test for the presence of a legacy "room info file"
	assoc_file_name(i->info, sizeof i->info, qrbuf, ctdl_info_dir);
	fp = fopen(i->info, "r");
	if (fp) {
		fclose(fp);
	}
	else {
		i->info[0] = 0;
	}

	// Test for the presence of a legacy "room picture file"
	assoc_file_name(i->pic, sizeof i->pic, qrbuf, ctdl_image_dir);
	fp = fopen(i->pic, "r");
	if (fp) {
		fclose(fp);
	}
	else {
		i->pic[0] = 0;
	}

	if ( (!IsEmptyStr(i->info)) || (!IsEmptyStr(i->pic)) ) {
		*iorarf_list = i;
	}
	else {
		free(i);
	}
}


/*
 * Prior to Citadel Server version 902, room info and pictures (which comprise the
 * displayed banner for each room) were stored in the filesystem.  If we are upgrading
 * from version >000 to version >=902, ingest those files into the database.
 */
void ingest_old_roominfo_and_roompic_files(void)
{
	struct iorarf_list *il = NULL;

	CtdlForEachRoom(iorarf_backend, &il);

	struct iorarf_list *p;
	while (il) {
		iorarf_oneroom(il->name, il->info, il->pic);
		p = il->next;
		free(il);
		il = p;
	}

}


/*
 * Perform any upgrades that can be done automatically based on our knowledge of the previous
 * version of Citadel server that was running here.
 *
 * Note that if the previous version was 0 then this is a new installation running for the first time.
 */
void update_config(void) {

	int oldver = CtdlGetConfigInt("MM_hosted_upgrade_level");

	if (oldver < 606) {
		CtdlSetConfigInt("c_rfc822_strict_from", 0);
	}

	if (oldver < 609) {
		CtdlSetConfigInt("c_purge_hour", 3);
	}

	if (oldver < 615) {
		CtdlSetConfigInt("c_ldap_port", 389);
	}

	if (oldver < 623) {
		CtdlSetConfigStr("c_ip_addr", "*");
	}

	if (oldver < 650) {
		CtdlSetConfigInt("c_enable_fulltext", 1);
	}

	if (oldver < 652) {
		CtdlSetConfigInt("c_auto_cull", 1);
	}

	if (oldver < 725) {
		CtdlSetConfigInt("c_xmpp_c2s_port", 5222);
		CtdlSetConfigInt("c_xmpp_s2s_port", 5269);
	}

	if (oldver < 830) {
		CtdlSetConfigInt("c_nntp_port", 119);
		CtdlSetConfigInt("c_nntps_port", 563);
	}

	if (IsEmptyStr(CtdlGetConfigStr("c_default_cal_zone"))) {
		guess_time_zone();
	}
}



/*
 * Based on the server version number reported by the existing database,
 * run in-place data format upgrades until everything is up to date.
 */
void check_server_upgrades(void) {

	syslog(LOG_INFO, "Existing database version on disk is %d", CtdlGetConfigInt("MM_hosted_upgrade_level"));

	if (CtdlGetConfigInt("MM_hosted_upgrade_level") < REV_LEVEL) {
		syslog(LOG_WARNING, "Server hosted updates need to be processed at this time.  Please wait...");
	}
	else {
		return;
	}

	update_config();

	if ((CtdlGetConfigInt("MM_hosted_upgrade_level") > 000) && (CtdlGetConfigInt("MM_hosted_upgrade_level") < 591)) {
		syslog(LOG_EMERG, "This database is too old to be upgraded.  Citadel server will exit.");
		exit(EXIT_FAILURE);
	}
	if ((CtdlGetConfigInt("MM_hosted_upgrade_level") > 000) && (CtdlGetConfigInt("MM_hosted_upgrade_level") < 608)) {
		convert_ctdluid_to_minusone();
	}
	if ((CtdlGetConfigInt("MM_hosted_upgrade_level") > 000) && (CtdlGetConfigInt("MM_hosted_upgrade_level") < 659)) {
		rebuild_euid_index();
	}
	if (CtdlGetConfigInt("MM_hosted_upgrade_level") < 735) {
		fix_sys_user_name();
	}
	if (CtdlGetConfigInt("MM_hosted_upgrade_level") < 736) {
		rebuild_usersbynumber();
	}
	if (CtdlGetConfigInt("MM_hosted_upgrade_level") < 790) {
		remove_thread_users();
	}
	if (CtdlGetConfigInt("MM_hosted_upgrade_level") < 810) {
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

	if ((CtdlGetConfigInt("MM_hosted_upgrade_level") > 000) && (CtdlGetConfigInt("MM_hosted_upgrade_level") < 902)) {
		ingest_old_roominfo_and_roompic_files();
	}

	CtdlSetConfigInt("MM_hosted_upgrade_level", REV_LEVEL);

	/*
	 * Negative values for maxsessions are not allowed.
	 */
	if (CtdlGetConfigInt("c_maxsessions") < 0) {
		CtdlSetConfigInt("c_maxsessions", 0);
	}

	/* We need a system default message expiry policy, because this is
	 * the top level and there's no 'higher' policy to fall back on.
	 * By default, do not expire messages at all.
	 */
	if (CtdlGetConfigInt("c_ep_mode") == 0) {
		CtdlSetConfigInt("c_ep_mode", EXPIRE_MANUAL);
		CtdlSetConfigInt("c_ep_value", 0);
	}
}


CTDL_MODULE_UPGRADE(upgrade)
{
	check_server_upgrades();
	
	/* return our module id for the Log */
	return "upgrade";
}
