/*
 * This module dumps and/or loads the Citadel database in XML format.
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

/*
 * Explanation of <progress> tags:
 *
 * 0%              nothing
 * 1%              finished exporting config
 * 2%              finished exporting control
 * 7%              finished exporting users
 * 12%             finished exporting openids
 * 17%             finished exporting rooms
 * 18%             finished exporting floors
 * 25%             finished exporting visits
 * 100%            finished exporting messages
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
#include <ctype.h>
#include <limits.h>
#include <expat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "msgbase.h"
#include "user_ops.h"
#include "control.h"
#include "euidindex.h"
#include "ctdl_module.h"

#define END_OF_MESSAGE	"---eom---dbd---"

char migr_tempfilename1[PATH_MAX];
char migr_tempfilename2[PATH_MAX];
FILE *migr_global_message_list;
int total_msgs = 0;

/*
 * Code which implements the export appears in this section
 */

/*
 * Output a string to the client with these characters escaped:  & < >
 */
void xml_strout(char *str) {

	char *c = str;

	if (str == NULL) {
		return;
	}

	while (*c != 0) {
		if (*c == '\"') {
			client_write("&quot;", 6);
		}
		else if (*c == '\'') {
			client_write("&apos;", 6);
		}
		else if (*c == '<') {
			client_write("&lt;", 4);
		}
		else if (*c == '>') {
			client_write("&gt;", 4);
		}
		else if (*c == '&') {
			client_write("&amp;", 5);
		}
		else {
			client_write(c, 1);
		}
		++c;
	}
}


/*
 * Export a user record as XML
 */
void migr_export_users_backend(struct ctdluser *buf, void *data) {
	client_write("<user>\n", 7);
	cprintf("<u_version>%d</u_version>\n", buf->version);
	cprintf("<u_uid>%ld</u_uid>\n", (long)buf->uid);
	client_write("<u_password>", 12);	xml_strout(buf->password);		client_write("</u_password>\n", 14);
	cprintf("<u_flags>%u</u_flags>\n", buf->flags);
	cprintf("<u_timescalled>%ld</u_timescalled>\n", buf->timescalled);
	cprintf("<u_posted>%ld</u_posted>\n", buf->posted);
	cprintf("<u_axlevel>%d</u_axlevel>\n", buf->axlevel);
	cprintf("<u_usernum>%ld</u_usernum>\n", buf->usernum);
	cprintf("<u_lastcall>%ld</u_lastcall>\n", (long)buf->lastcall);
	cprintf("<u_USuserpurge>%d</u_USuserpurge>\n", buf->USuserpurge);
	client_write("<u_fullname>", 12);	xml_strout(buf->fullname);		client_write("</u_fullname>\n", 14);
	client_write("</user>\n", 8);
}


void migr_export_users(void) {
	ForEachUser(migr_export_users_backend, NULL);
}


void migr_export_room_msg(long msgnum, void *userdata) {
	cprintf("%ld\n", msgnum);
	fprintf(migr_global_message_list, "%ld\n", msgnum);
}


void migr_export_rooms_backend(struct ctdlroom *buf, void *data) {
	client_write("<room>\n", 7);
	client_write("<QRname>", 8);	xml_strout(buf->QRname);	client_write("</QRname>\n", 10);
	client_write("<QRpasswd>", 10);	xml_strout(buf->QRpasswd);	client_write("</QRpasswd>\n", 12);
	cprintf("<QRroomaide>%ld</QRroomaide>\n", buf->QRroomaide);
	cprintf("<QRhighest>%ld</QRhighest>\n", buf->QRhighest);
	cprintf("<QRgen>%ld</QRgen>\n", (long)buf->QRgen);
	cprintf("<QRflags>%u</QRflags>\n", buf->QRflags);
	if (buf->QRflags & QR_DIRECTORY) {
		client_write("<QRdirname>", 11);
		xml_strout(buf->QRdirname);
		client_write("</QRdirname>\n", 13);
	}
	cprintf("<QRinfo>%ld</QRinfo>\n", buf->QRinfo);
	cprintf("<QRfloor>%d</QRfloor>\n", buf->QRfloor);
	cprintf("<QRmtime>%ld</QRmtime>\n", (long)buf->QRmtime);
	cprintf("<QRexpire_mode>%d</QRexpire_mode>\n", buf->QRep.expire_mode);
	cprintf("<QRexpire_value>%d</QRexpire_value>\n", buf->QRep.expire_value);
	cprintf("<QRnumber>%ld</QRnumber>\n", buf->QRnumber);
	cprintf("<QRorder>%d</QRorder>\n", buf->QRorder);
	cprintf("<QRflags2>%u</QRflags2>\n", buf->QRflags2);
	cprintf("<QRdefaultview>%d</QRdefaultview>\n", buf->QRdefaultview);
	client_write("</room>\n", 8);

	/* message list goes inside this tag */

	CtdlGetRoom(&CC->room, buf->QRname);
	client_write("<room_messages>", 15);
	client_write("<FRname>", 8);	xml_strout(CC->room.QRname);	client_write("</FRname>\n", 10);
	client_write("<FRmsglist>", 11);
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, migr_export_room_msg, NULL);
	client_write("</FRmsglist>\n", 13);
	client_write("</room_messages>\n", 17);


}


void migr_export_rooms(void) {
	char cmd[SIZ];
	migr_global_message_list = fopen(migr_tempfilename1, "w");
	if (migr_global_message_list != NULL) {
		CtdlForEachRoom(migr_export_rooms_backend, NULL);
		fclose(migr_global_message_list);
	}

	/*
	 * Process the 'global' message list.  (Sort it and remove dups.
	 * Dups are ok because a message may be in more than one room, but
	 * this will be handled by exporting the reference count, not by
	 * exporting the message multiple times.)
	 */
	snprintf(cmd, sizeof cmd, "sort -n <%s >%s", migr_tempfilename1, migr_tempfilename2);
	if (system(cmd) != 0) syslog(LOG_ALERT, "Error %d\n", errno);
	snprintf(cmd, sizeof cmd, "uniq <%s >%s", migr_tempfilename2, migr_tempfilename1);
	if (system(cmd) != 0) syslog(LOG_ALERT, "Error %d\n", errno);


	snprintf(cmd, sizeof cmd, "wc -l %s", migr_tempfilename1);
	FILE *fp = popen(cmd, "r");
	if (fp) {
		fgets(cmd, sizeof cmd, fp);
		pclose(fp);
		total_msgs = atoi(cmd);
	}
	else {
		total_msgs = 1;	// any nonzero just to keep it from barfing
	}
	syslog(LOG_DEBUG, "Total messages to be exported: %d", total_msgs);
}


void migr_export_floors(void) {
        struct floor qfbuf, *buf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		client_write("<floor>\n", 8);
		cprintf("<f_num>%d</f_num>\n", i);
                CtdlGetFloor(&qfbuf, i);
		buf = &qfbuf;
		cprintf("<f_flags>%u</f_flags>\n", buf->f_flags);
		client_write("<f_name>", 8); xml_strout(buf->f_name); client_write("</f_name>\n", 10);
		cprintf("<f_ref_count>%d</f_ref_count>\n", buf->f_ref_count);
		cprintf("<f_ep_expire_mode>%d</f_ep_expire_mode>\n", buf->f_ep.expire_mode);
		cprintf("<f_ep_expire_value>%d</f_ep_expire_value>\n", buf->f_ep.expire_value);
		client_write("</floor>\n", 9);
	}
}


/*
 * Return nonzero if the supplied string contains only characters which are valid in a sequence set.
 */
int is_sequence_set(char *s) {
	if (!s) return(0);

	char *c = s;
	char ch;
	while (ch = *c++, ch) {
		if (!strchr("0123456789*,:", ch)) {
			return(0);
		}
	}
	return(1);
}



/* 
 *  Traverse the visits file...
 */
void migr_export_visits(void) {
	visit vbuf;
	struct cdbdata *cdbv;

	cdb_rewind(CDB_VISIT);

	while (cdbv = cdb_next_item(CDB_VISIT), cdbv != NULL) {
		memset(&vbuf, 0, sizeof(visit));
		memcpy(&vbuf, cdbv->ptr,
		       ((cdbv->len > sizeof(visit)) ?
			sizeof(visit) : cdbv->len));
		cdb_free(cdbv);

		client_write("<visit>\n", 8);
		cprintf("<v_roomnum>%ld</v_roomnum>\n", vbuf.v_roomnum);
		cprintf("<v_roomgen>%ld</v_roomgen>\n", vbuf.v_roomgen);
		cprintf("<v_usernum>%ld</v_usernum>\n", vbuf.v_usernum);

		client_write("<v_seen>", 8);
		if ( (!IsEmptyStr(vbuf.v_seen)) && (is_sequence_set(vbuf.v_seen)) ) {
			xml_strout(vbuf.v_seen);
		}
		else {
			cprintf("%ld", vbuf.v_lastseen);
		}
		client_write("</v_seen>", 9);

		if ( (!IsEmptyStr(vbuf.v_answered)) && (is_sequence_set(vbuf.v_answered)) ) {
			client_write("<v_answered>", 12);
			xml_strout(vbuf.v_answered);
			client_write("</v_answered>\n", 14);
		}

		cprintf("<v_flags>%u</v_flags>\n", vbuf.v_flags);
		cprintf("<v_view>%d</v_view>\n", vbuf.v_view);
		client_write("</visit>\n", 9);
	}
}


void migr_export_message(long msgnum) {
	struct MetaData smi;
	struct CtdlMessage *msg;
	struct ser_ret smr;

	/* We can use a static buffer here because there will never be more than
	 * one of this operation happening at any given time, and it's really best
	 * to just keep it allocated once instead of torturing malloc/free.
	 * Call this function with msgnum "-1" to free the buffer when finished.
	 */
	static int encoded_alloc = 0;
	static char *encoded_msg = NULL;

	if (msgnum < 0) {
		if ((encoded_alloc == 0) && (encoded_msg != NULL)) {
			free(encoded_msg);
			encoded_alloc = 0;
			encoded_msg = NULL;
		}
		return;
	}

	/* Ok, here we go ... */

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;	/* fail silently */

	client_write("<message>\n", 10);
	GetMetaData(&smi, msgnum);
	cprintf("<msg_msgnum>%ld</msg_msgnum>\n", msgnum);
	cprintf("<msg_meta_refcount>%d</msg_meta_refcount>\n", smi.meta_refcount);
	client_write("<msg_meta_content_type>", 23); xml_strout(smi.meta_content_type); client_write("</msg_meta_content_type>\n", 25);

	client_write("<msg_text>", 10);
	CtdlSerializeMessage(&smr, msg);
	CM_Free(msg);

	/* Predict the buffer size we need.  Expand the buffer if necessary. */
	int encoded_len = smr.len * 15 / 10 ;
	if (encoded_len > encoded_alloc) {
		encoded_alloc = encoded_len;
		encoded_msg = realloc(encoded_msg, encoded_alloc);
	}

	if (encoded_msg == NULL) {
		/* Questionable hack that hopes it'll work next time and we only lose one message */
		encoded_alloc = 0;
	}
	else {
		/* Once we do the encoding we know the exact size */
		encoded_len = CtdlEncodeBase64(encoded_msg, (char *)smr.ser, smr.len, 1);
		client_write(encoded_msg, encoded_len);
	}

	free(smr.ser);

	client_write("</msg_text>\n", 12);
	client_write("</message>\n", 11);
}



void migr_export_openids(void) {
	struct cdbdata *cdboi;
	long usernum;
	char url[512];

	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			client_write("<openid>\n", 9);
			memcpy(&usernum, cdboi->ptr, sizeof(long));
			snprintf(url, sizeof url, "%s", (cdboi->ptr)+sizeof(long) );
			client_write("<oid_url>", 9);
			xml_strout(url);
			client_write("</oid_url>\n", 11);
			cprintf("<oid_usernum>%ld</oid_usernum>\n", usernum);
			client_write("</openid>\n", 10);
		}
		cdb_free(cdboi);
	}
}


void migr_export_configs(void) {
	struct cdbdata *cdbcfg;
	int keylen = 0;
	char *key = NULL;
	char *value = NULL;

	cdb_rewind(CDB_CONFIG);
	while (cdbcfg = cdb_next_item(CDB_CONFIG), cdbcfg != NULL) {

		keylen = strlen(cdbcfg->ptr);
		key = cdbcfg->ptr;
		value = cdbcfg->ptr + keylen + 1;

		client_write("<config key=\"", 13);
		xml_strout(key);
		client_write("\">", 2);
		xml_strout(value);
		client_write("</config>\n", 10);
		cdb_free(cdbcfg);
	}
}




void migr_export_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;
	int progress = 0;
	int prev_progress = 0;
	CitContext *Ctx;

	Ctx = CC;
	migr_global_message_list = fopen(migr_tempfilename1, "r");
	if (migr_global_message_list != NULL) {
		syslog(LOG_INFO, "Opened %s\n", migr_tempfilename1);
		while ((Ctx->kill_me == 0) && 
		       (fgets(buf, sizeof(buf), migr_global_message_list) != NULL)) {
			msgnum = atol(buf);
			if (msgnum > 0L) {
				migr_export_message(msgnum);
				++count;
			}
			progress = (count * 74 / total_msgs) + 25 ;
			if ((progress > prev_progress) && (progress < 100)) {
				cprintf("<progress>%d</progress>\n", progress);
			}
			prev_progress = progress;
		}
		fclose(migr_global_message_list);
	}
	if (Ctx->kill_me == 0)
		syslog(LOG_INFO, "Exported %d messages.\n", count);
	else
		syslog(LOG_ERR, "Export aborted due to client disconnect! \n");

	migr_export_message(-1L);	/* This frees the encoding buffer */
}



void migr_do_export(void) {
	CitContext *Ctx;

	Ctx = CC;
	cprintf("%d Exporting all Citadel databases.\n", LISTING_FOLLOWS);
	Ctx->dont_term = 1;

	client_write("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n", 40);
	client_write("<citadel_migrate_data>\n", 23);
	cprintf("<version>%d</version>\n", REV_LEVEL);
	cprintf("<progress>%d</progress>\n", 0);

	/* export the config file (this is done using x-macros) */
	migr_export_configs();
	cprintf("<progress>%d</progress>\n", 1);
	
	/* Export the control file */
	get_control();
	client_write("<control>\n", 10);
	cprintf("<control_highest>%ld</control_highest>\n", CitControl.MMhighest);
	cprintf("<control_flags>%u</control_flags>\n", CitControl.MMflags);
	cprintf("<control_nextuser>%ld</control_nextuser>\n", CitControl.MMnextuser);
	cprintf("<control_nextroom>%ld</control_nextroom>\n", CitControl.MMnextroom);
	cprintf("<control_version>%d</control_version>\n", CitControl.MM_hosted_upgrade_level);
	client_write("</control>\n", 11);
	cprintf("<progress>%d</progress>\n", 2);

	if (Ctx->kill_me == 0)	migr_export_users();
	cprintf("<progress>%d</progress>\n", 7);
	if (Ctx->kill_me == 0)	migr_export_openids();
	cprintf("<progress>%d</progress>\n", 12);
	if (Ctx->kill_me == 0)	migr_export_rooms();
	cprintf("<progress>%d</progress>\n", 17);
	if (Ctx->kill_me == 0)	migr_export_floors();
	cprintf("<progress>%d</progress>\n", 18);
	if (Ctx->kill_me == 0)	migr_export_visits();
	cprintf("<progress>%d</progress>\n", 25);
	if (Ctx->kill_me == 0)	migr_export_messages();
	client_write("</citadel_migrate_data>\n", 24);
	cprintf("<progress>%d</progress>\n", 100);
	client_write("000\n", 4);
	Ctx->dont_term = 0;
}




	
/*
 * Here's the code that implements the import side.  It's going to end up being
 * one big loop with lots of global variables.  I don't care.  You wouldn't run
 * multiple concurrent imports anyway.  If this offends your delicate sensibilities
 * then go rewrite it in Ruby on Rails or something.
 */


int citadel_migrate_data = 0;		/* Are we inside a <citadel_migrate_data> tag pair? */
StrBuf *migr_chardata = NULL;
StrBuf *migr_MsgData = NULL;
struct ctdluser usbuf;
struct ctdlroom qrbuf;
char openid_url[512];
long openid_usernum = 0;
char FRname[ROOMNAMELEN];
struct floor flbuf;
int floornum = 0;
visit vbuf;
struct MetaData smi;
long import_msgnum = 0;

/*
 * This callback stores up the data which appears in between tags.
 */
void migr_xml_chardata(void *data, const XML_Char *s, int len)
{
	StrBufAppendBufPlain(migr_chardata, s, len, 0);
}


void migr_xml_start(void *data, const char *el, const char **attr) {
	int i;

	/*** GENERAL STUFF ***/

	/* Reset chardata_len to zero and init buffer */
	FlushStrBuf(migr_chardata);
	FlushStrBuf(migr_MsgData);

	if (!strcasecmp(el, "citadel_migrate_data")) {
		++citadel_migrate_data;

		/* As soon as it looks like the input data is a genuine Citadel XML export,
		 * whack the existing database on disk to make room for the new one.
		 */
		if (citadel_migrate_data == 1) {
			for (i = 0; i < MAXCDB; ++i) {
				cdb_trunc(i);
			}
		}
		return;
	}

	if (citadel_migrate_data != 1) {
		syslog(LOG_ALERT, "Out-of-sequence tag <%s> detected.  Warning: ODD-DATA!\n", el);
		return;
	}

	/* When we begin receiving XML for one of these record types, clear out the associated
	 * buffer so we don't accidentally carry over any data from a previous record.
	 */
	if (!strcasecmp(el, "user"))			memset(&usbuf, 0, sizeof (struct ctdluser));
	else if (!strcasecmp(el, "openid"))		memset(openid_url, 0, sizeof openid_url);
	else if (!strcasecmp(el, "room"))		memset(&qrbuf, 0, sizeof (struct ctdlroom));
	else if (!strcasecmp(el, "room_messages"))	memset(FRname, 0, sizeof FRname);
	else if (!strcasecmp(el, "floor"))		memset(&flbuf, 0, sizeof (struct floor));
	else if (!strcasecmp(el, "visit"))		memset(&vbuf, 0, sizeof (visit));

	else if (!strcasecmp(el, "message")) {
		memset(&smi, 0, sizeof (struct MetaData));
		import_msgnum = 0;
	}
	else if (!strcasecmp(el, "config")) {
		syslog(LOG_DEBUG, "\033[31m IMPORT OF CONFIG START ELEMENT FIXME\033\0m");
	}

}



int migr_controlrecord(void *data, const char *el)
{
	if (!strcasecmp(el, "control_highest"))		CitControl.MMhighest = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "control_flags"))		CitControl.MMflags = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "control_nextuser"))		CitControl.MMnextuser = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "control_nextroom"))		CitControl.MMnextroom = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "control_version"))		CitControl.MM_hosted_upgrade_level = atoi(ChrPtr(migr_chardata));

	else if (!strcasecmp(el, "control")) {
		CitControl.MMfulltext = (-1L);	/* always flush */
		put_control();
		syslog(LOG_INFO, "Completed import of control record\n");
	}
	else return 0;
	return 1;

}


int migr_userrecord(void *data, const char *el)
{
	if (!strcasecmp(el, "u_version"))			usbuf.version = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_uid"))			usbuf.uid = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_password"))			safestrncpy(usbuf.password, ChrPtr(migr_chardata), sizeof usbuf.password);
	else if (!strcasecmp(el, "u_flags"))			usbuf.flags = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_timescalled"))		usbuf.timescalled = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_posted"))			usbuf.posted = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_axlevel"))			usbuf.axlevel = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_usernum"))			usbuf.usernum = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_lastcall"))			usbuf.lastcall = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_USuserpurge"))		usbuf.USuserpurge = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "u_fullname"))			safestrncpy(usbuf.fullname, ChrPtr(migr_chardata), sizeof usbuf.fullname);
	else return 0;
	return 1;
}

int migr_roomrecord(void *data, const char *el)
{
	if (!strcasecmp(el, "QRname"))			safestrncpy(qrbuf.QRname, ChrPtr(migr_chardata), sizeof qrbuf.QRname);
	else if (!strcasecmp(el, "QRpasswd"))			safestrncpy(qrbuf.QRpasswd, ChrPtr(migr_chardata), sizeof qrbuf.QRpasswd);
	else if (!strcasecmp(el, "QRroomaide"))			qrbuf.QRroomaide = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRhighest"))			qrbuf.QRhighest = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRgen"))			qrbuf.QRgen = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRflags"))			qrbuf.QRflags = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRdirname"))			safestrncpy(qrbuf.QRdirname, ChrPtr(migr_chardata), sizeof qrbuf.QRdirname);
	else if (!strcasecmp(el, "QRinfo"))			qrbuf.QRinfo = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRfloor"))			qrbuf.QRfloor = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRmtime"))			qrbuf.QRmtime = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRexpire_mode"))		qrbuf.QRep.expire_mode = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRexpire_value"))		qrbuf.QRep.expire_value = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRnumber"))			qrbuf.QRnumber = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRorder"))			qrbuf.QRorder = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRflags2"))			qrbuf.QRflags2 = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "QRdefaultview"))		qrbuf.QRdefaultview = atoi(ChrPtr(migr_chardata));
	else return 0;
	return 1;
}

int migr_floorrecord(void *data, const char *el)
{
	if (!strcasecmp(el, "f_num"))			floornum = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "f_flags"))			flbuf.f_flags = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "f_name"))			safestrncpy(flbuf.f_name, ChrPtr(migr_chardata), sizeof flbuf.f_name);
	else if (!strcasecmp(el, "f_ref_count"))		flbuf.f_ref_count = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "f_ep_expire_mode"))		flbuf.f_ep.expire_mode = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "f_ep_expire_value"))		flbuf.f_ep.expire_value = atoi(ChrPtr(migr_chardata));
	else return 0;
	return 1;
}

int migr_visitrecord(void *data, const char *el)
{
	if (!strcasecmp(el, "v_roomnum"))			vbuf.v_roomnum = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "v_roomgen"))			vbuf.v_roomgen = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "v_usernum"))			vbuf.v_usernum = atol(ChrPtr(migr_chardata));

	else if (!strcasecmp(el, "v_seen")) {
		int is_textual_seen = 0;
		int i;
		int max = StrLength(migr_chardata);

		vbuf.v_lastseen = atol(ChrPtr(migr_chardata));
		is_textual_seen = 0;
		for (i=0; i < max; ++i) 
			if (!isdigit(ChrPtr(migr_chardata)[i]))
				is_textual_seen = 1;
		if (is_textual_seen)
			safestrncpy(vbuf.v_seen, ChrPtr(migr_chardata), sizeof vbuf.v_seen);
	}

	else if (!strcasecmp(el, "v_answered"))			safestrncpy(vbuf.v_answered, ChrPtr(migr_chardata), sizeof vbuf.v_answered);
	else if (!strcasecmp(el, "v_flags"))			vbuf.v_flags = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "v_view"))			vbuf.v_view = atoi(ChrPtr(migr_chardata));
	else return 0;
	return 1;
}


void migr_xml_end(void *data, const char *el)
{
	const char *ptr;
	int msgcount = 0;
	long msgnum = 0L;
	long *msglist = NULL;
	int msglist_alloc = 0;
	/*** GENERAL STUFF ***/

	if (!strcasecmp(el, "citadel_migrate_data")) {
		--citadel_migrate_data;
		return;
	}

	if (citadel_migrate_data != 1) {
		syslog(LOG_ALERT, "Out-of-sequence tag <%s> detected.  Warning: ODD-DATA!\n", el);
		return;
	}

	// syslog(LOG_DEBUG, "END TAG: <%s> DATA: <%s>\n", el, (migr_chardata_len ? migr_chardata : ""));

	/*** CONFIG ***/

	if (!strcasecmp(el, "config"))
	{
		syslog(LOG_DEBUG, "\033[31m IMPORT OF CONFIG END ELEMENT FIXME\033\0m");
		CtdlSetConfigInt("c_enable_fulltext", 0);	/* always disable FIXME put this somewhere more appropriate */
	}

	/*** CONTROL ***/
	else if ((!strncasecmp(el, HKEY("control"))) && 
		 migr_controlrecord(data, el))
		; /* Nothing to do anymore */
	/*** USER ***/
	else if ((!strncasecmp(el, HKEY("u_"))) && 
		 migr_userrecord(data, el))
		; /* Nothing to do anymore */
	else if (!strcasecmp(el, "user")) {
		CtdlPutUser(&usbuf);
		syslog(LOG_INFO, "Imported user: %s\n", usbuf.fullname);
	}

	/*** OPENID ***/

	else if (!strcasecmp(el, "oid_url"))			safestrncpy(openid_url, ChrPtr(migr_chardata), sizeof openid_url);
	else if (!strcasecmp(el, "oid_usernum"))		openid_usernum = atol(ChrPtr(migr_chardata));

	else if (!strcasecmp(el, "openid")) {			/* see serv_openid_rp.c for a description of the record format */
		char *oid_data;
		int oid_data_len;
		oid_data_len = sizeof(long) + strlen(openid_url) + 1;
		oid_data = malloc(oid_data_len);
		memcpy(oid_data, &openid_usernum, sizeof(long));
		memcpy(&oid_data[sizeof(long)], openid_url, strlen(openid_url) + 1);
		cdb_store(CDB_OPENID, openid_url, strlen(openid_url), oid_data, oid_data_len);
		free(oid_data);
		syslog(LOG_INFO, "Imported OpenID: %s (%ld)\n", openid_url, openid_usernum);
	}

	/*** ROOM ***/
	else if ((!strncasecmp(el, HKEY("QR"))) && 
		 migr_roomrecord(data, el))
		; /* Nothing to do anymore */
	else if (!strcasecmp(el, "room")) {
		CtdlPutRoom(&qrbuf);
		syslog(LOG_INFO, "Imported room: %s\n", qrbuf.QRname);
	}

	/*** ROOM MESSAGE POINTERS ***/

	else if (!strcasecmp(el, "FRname"))			safestrncpy(FRname, ChrPtr(migr_chardata), sizeof FRname);

	else if (!strcasecmp(el, "FRmsglist")) {
		if (!IsEmptyStr(FRname)) {
			msgcount = 0;
			msglist_alloc = 1000;
			msglist = malloc(sizeof(long) * msglist_alloc);

			syslog(LOG_DEBUG, "Message list for: %s\n", FRname);

			ptr = ChrPtr(migr_chardata);
			while (*ptr != 0) {
				while ((*ptr != 0) && (!isdigit(*ptr))) {
					++ptr;
				}
				if ((*ptr != 0) && (isdigit(*ptr))) {
					msgnum = atol(ptr);
					if (msgnum > 0L) {
						if (msgcount >= msglist_alloc) {
							msglist_alloc *= 2;
							msglist = realloc(msglist, sizeof(long) * msglist_alloc);
						}
						msglist[msgcount++] = msgnum;
						}
					}
					while ((*ptr != 0) && (isdigit(*ptr))) {
						++ptr;
					}
				}
			}
			if (msgcount > 0) {
				CtdlSaveMsgPointersInRoom(FRname, msglist, msgcount, 0, NULL, 1);
			}
			free(msglist);
			msglist = NULL;
			msglist_alloc = 0;
			syslog(LOG_DEBUG, "Imported %d messages.\n", msgcount);
			if (server_shutting_down) {
				return;
		}
	}

	/*** FLOORS ***/
	else if ((!strncasecmp(el, HKEY("f_"))) && 
		 migr_floorrecord(data, el))
		; /* Nothing to do anymore */

	else if (!strcasecmp(el, "floor")) {
		CtdlPutFloor(&flbuf, floornum);
		syslog(LOG_INFO, "Imported floor #%d (%s)\n", floornum, flbuf.f_name);
	}

	/*** VISITS ***/
	else if ((!strncasecmp(el, HKEY("v_"))) && 
		 migr_visitrecord(data, el))
		; /* Nothing to do anymore */
	else if (!strcasecmp(el, "visit")) {
		put_visit(&vbuf);
		syslog(LOG_INFO, "Imported visit: %ld/%ld/%ld\n", vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
	}

	/*** MESSAGES ***/
	
	else if (!strcasecmp(el, "msg_msgnum"))			import_msgnum = atol(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "msg_meta_refcount"))		smi.meta_refcount = atoi(ChrPtr(migr_chardata));
	else if (!strcasecmp(el, "msg_meta_content_type"))	safestrncpy(smi.meta_content_type, ChrPtr(migr_chardata), sizeof smi.meta_content_type);

	else if (!strcasecmp(el, "msg_text"))
	{

		FlushStrBuf(migr_MsgData);
		StrBufDecodeBase64To(migr_MsgData, migr_MsgData);

		cdb_store(CDB_MSGMAIN,
			  &import_msgnum,
			  sizeof(long),
			  ChrPtr(migr_MsgData), 
			  StrLength(migr_MsgData) + 1);

		smi.meta_msgnum = import_msgnum;
		PutMetaData(&smi);

		syslog(LOG_INFO,
		       "Imported message #%ld, size=%d, refcount=%d, content-type: %s\n",
		       import_msgnum,
		       StrLength(migr_MsgData),
		       smi.meta_refcount,
		       smi.meta_content_type);
	}

	/*** MORE GENERAL STUFF ***/

	FlushStrBuf(migr_chardata);
}




/*
 * Import begins here
 */
void migr_do_import(void) {
	StrBuf *Buf;
	XML_Parser xp;
	int Finished = 0;
	
	unbuffer_output();
	migr_chardata = NewStrBufPlain(NULL, SIZ * 20);
	migr_MsgData = NewStrBufPlain(NULL, SIZ * 20);
	Buf = NewStrBufPlain(NULL, SIZ);
	xp = XML_ParserCreate(NULL);
	if (!xp) {
		cprintf("%d Failed to create XML parser instance\n", ERROR+INTERNAL_ERROR);
		return;
	}
	XML_SetElementHandler(xp, migr_xml_start, migr_xml_end);
	XML_SetCharacterDataHandler(xp, migr_xml_chardata);

	CC->dont_term = 1;

	cprintf("%d sock it to me\n", SEND_LISTING);
	unbuffer_output();

	client_set_inbound_buf(SIZ * 10);

	while (!Finished && client_read_random_blob(Buf, -1) >= 0) {
		if ((StrLength(Buf) > 4) &&
		    !strcmp(ChrPtr(Buf) + StrLength(Buf) - 4, "000\n"))
		{
			Finished = 1;
			StrBufCutAt(Buf, StrLength(Buf) - 4, NULL);
		}
		if (server_shutting_down)
			break;	// Should we break or return?
		
		if (StrLength(Buf) == 0)
			continue;

		XML_Parse(xp, ChrPtr(Buf), StrLength(Buf), 0);
		FlushStrBuf(Buf);
	}

	XML_Parse(xp, "", 0, 1);
	XML_ParserFree(xp);
	FreeStrBuf(&Buf);
	FreeStrBuf(&migr_chardata);
	FreeStrBuf(&migr_MsgData);
	rebuild_euid_index();
	rebuild_usersbynumber();
	CC->dont_term = 0;
}



/*
 * Dump out the pathnames of directories which can be copied "as is"
 */
void migr_do_listdirs(void) {
	cprintf("%d Don't forget these:\n", LISTING_FOLLOWS);
	cprintf("bio|%s\n",		ctdl_bio_dir);
	cprintf("files|%s\n",		ctdl_file_dir);
	cprintf("userpics|%s\n",	ctdl_usrpic_dir);
	cprintf("messages|%s\n",	ctdl_message_dir);
	cprintf("netconfigs|%s\n",	ctdl_netcfg_dir);
	cprintf("keys|%s\n",		ctdl_key_dir);
	cprintf("images|%s\n",		ctdl_image_dir);
	cprintf("info|%s\n",		ctdl_info_dir);
	cprintf("000\n");
}


/*
 * Common code appears in this section
 */


void cmd_migr(char *cmdbuf) {
	char cmd[32];
	
	if (CtdlAccessCheck(ac_internal)) return;
	
	if (CtdlTrySingleUser())
	{
		CtdlMakeTempFileName(migr_tempfilename1, sizeof migr_tempfilename1);
		CtdlMakeTempFileName(migr_tempfilename2, sizeof migr_tempfilename2);

		extract_token(cmd, cmdbuf, 0, '|', sizeof cmd);
		if (!strcasecmp(cmd, "export")) {
			migr_do_export();
		}
		else if (!strcasecmp(cmd, "import")) {
			migr_do_import();
		}
		else if (!strcasecmp(cmd, "listdirs")) {
			migr_do_listdirs();
		}
		else {
			cprintf("%d illegal command\n", ERROR + ILLEGAL_VALUE);
		}

		unlink(migr_tempfilename1);
		unlink(migr_tempfilename2);
		
		CtdlEndSingleUser();
	}
	else
	{
		cprintf("%d The migrator is already running.\n", ERROR + RESOURCE_BUSY);
	}
}


CTDL_MODULE_INIT(migrate)
{
	if (!threading)
	{
		CtdlRegisterProtoHook(cmd_migr, "MIGR", "Across-the-wire migration");
	}
	
	/* return our module name for the log */
	return "migrate";
}
