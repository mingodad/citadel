/*
 * $Id$
 *
 * This is the "Art Vandelay" module.  It is an importer/exporter.
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
#include <ctype.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "user_ops.h"
#include "room_ops.h"
#include "control.h"

#define END_OF_MESSAGE	"---eom---dbd---"

char artv_tempfilename1[PATH_MAX];
char artv_tempfilename2[PATH_MAX];
FILE *artv_global_message_list;

void artv_export_users_backend(struct ctdluser *usbuf, void *data) {
	cprintf("user\n");
	cprintf("%d\n", usbuf->version);
	cprintf("%ld\n", (long)usbuf->uid);
	cprintf("%s\n", usbuf->password);
	cprintf("%u\n", usbuf->flags);
	cprintf("%ld\n", usbuf->timescalled);
	cprintf("%ld\n", usbuf->posted);
	cprintf("%d\n", usbuf->axlevel);
	cprintf("%ld\n", usbuf->usernum);
	cprintf("%ld\n", (long)usbuf->lastcall);
	cprintf("%d\n", usbuf->USuserpurge);
	cprintf("%s\n", usbuf->fullname);
	cprintf("%d\n", usbuf->USscreenwidth);
	cprintf("%d\n", usbuf->USscreenheight);
}


void artv_export_users(void) {
	ForEachUser(artv_export_users_backend, NULL);
}


void artv_export_room_msg(long msgnum, void *userdata) {
	cprintf("%ld\n", msgnum);
	fprintf(artv_global_message_list, "%ld\n", msgnum);
}


void artv_export_rooms_backend(struct ctdlroom *qrbuf, void *data) {
	cprintf("room\n");
	cprintf("%s\n", qrbuf->QRname);
	cprintf("%s\n", qrbuf->QRpasswd);
	cprintf("%ld\n", qrbuf->QRroomaide);
	cprintf("%ld\n", qrbuf->QRhighest);
	cprintf("%ld\n", (long)qrbuf->QRgen);
	cprintf("%u\n", qrbuf->QRflags);
	cprintf("%s\n", qrbuf->QRdirname);
	cprintf("%ld\n", qrbuf->QRinfo);
	cprintf("%d\n", qrbuf->QRfloor);
	cprintf("%ld\n", (long)qrbuf->QRmtime);
	cprintf("%d\n", qrbuf->QRep.expire_mode);
	cprintf("%d\n", qrbuf->QRep.expire_value);
	cprintf("%ld\n", qrbuf->QRnumber);
	cprintf("%d\n", qrbuf->QRorder);
	cprintf("%u\n", qrbuf->QRflags2);
	cprintf("%d\n", qrbuf->QRdefaultview);

	getroom(&CC->room, qrbuf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL,
		artv_export_room_msg, NULL);
	cprintf("0\n");

}



void artv_export_rooms(void) {
	char cmd[SIZ];
	artv_global_message_list = fopen(artv_tempfilename1, "w");
	ForEachRoom(artv_export_rooms_backend, NULL);
	fclose(artv_global_message_list);

	/*
	 * Process the 'global' message list.  (Sort it and remove dups.
	 * Dups are ok because a message may be in more than one room, but
	 * this will be handled by exporting the reference count, not by
	 * exporting the message multiple times.)
	 */
	snprintf(cmd, sizeof cmd, "sort <%s >%s", artv_tempfilename1, artv_tempfilename2);
	system(cmd);
	snprintf(cmd, sizeof cmd, "uniq <%s >%s", artv_tempfilename2, artv_tempfilename1);
	system(cmd);
}


void artv_export_floors(void) {
        struct floor flbuf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		cprintf("floor\n");
		cprintf("%d\n", i);
                getfloor(&flbuf, i);
		cprintf("%u\n", flbuf.f_flags);
		cprintf("%s\n", flbuf.f_name);
		cprintf("%d\n", flbuf.f_ref_count);
		cprintf("%d\n", flbuf.f_ep.expire_mode);
		cprintf("%d\n", flbuf.f_ep.expire_value);
	}
}





/* 
 *  Traverse the visits file...
 */
void artv_export_visits(void) {
	struct visit vbuf;
	struct cdbdata *cdbv;

	cdb_rewind(CDB_VISIT);

	while (cdbv = cdb_next_item(CDB_VISIT), cdbv != NULL) {
		memset(&vbuf, 0, sizeof(struct visit));
		memcpy(&vbuf, cdbv->ptr,
		       ((cdbv->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbv->len));
		cdb_free(cdbv);

		cprintf("visit\n");
		cprintf("%ld\n", vbuf.v_roomnum);
		cprintf("%ld\n", vbuf.v_roomgen);
		cprintf("%ld\n", vbuf.v_usernum);

		if (strlen(vbuf.v_seen) > 0) {
			cprintf("%s\n", vbuf.v_seen);
		}
		else {
			cprintf("%ld\n", vbuf.v_lastseen);
		}

		cprintf("%s\n", vbuf.v_answered);
		cprintf("%u\n", vbuf.v_flags);
		cprintf("%d\n", vbuf.v_view);
	}
}


void artv_export_message(long msgnum) {
	struct MetaData smi;
	struct CtdlMessage *msg;
	struct ser_ret smr;
	FILE *fp;
	char buf[SIZ];
	char tempfile[SIZ];

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) return;	/* fail silently */

	cprintf("message\n");
	GetMetaData(&smi, msgnum);
	cprintf("%ld\n", msgnum);
	cprintf("%d\n", smi.meta_refcount);
	cprintf("%s\n", smi.meta_content_type);

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	/* write it in base64 */
	strcpy(tempfile, tmpnam(NULL));
	snprintf(buf, sizeof buf, "./base64 -e >%s", tempfile);
	fp = popen(buf, "w");
	fwrite(smr.ser, smr.len, 1, fp);
	pclose(fp);

	phree(smr.ser);

	fp = fopen(tempfile, "r");
	unlink(tempfile);
	while (fgets(buf, sizeof(buf), fp) != NULL) {
		buf[strlen(buf)-1] = 0;
		cprintf("%s\n", buf);
	}
	fclose(fp);
	cprintf("%s\n", END_OF_MESSAGE);
}



void artv_export_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;

	artv_global_message_list = fopen(artv_tempfilename1, "r");
	lprintf(7, "Opened %s\n", artv_tempfilename1);
	while (fgets(buf, sizeof(buf), artv_global_message_list) != NULL) {
		msgnum = atol(buf);
		if (msgnum > 0L) {
			artv_export_message(msgnum);
			++count;
		}
	}
	fclose(artv_global_message_list);
	lprintf(7, "Exported %d messages.\n", count);
}




void artv_do_export(void) {
	cprintf("%d Exporting all Citadel databases.\n", LISTING_FOLLOWS);

	cprintf("version\n%d\n", REV_LEVEL);

	/* export the config file */
	cprintf("config\n");
	cprintf("%s\n", config.c_nodename);
	cprintf("%s\n", config.c_fqdn);
	cprintf("%s\n", config.c_humannode);
	cprintf("%s\n", config.c_phonenum);
	cprintf("%ld\n", (long)config.c_bbsuid);
	cprintf("%d\n", config.c_creataide);
	cprintf("%d\n", config.c_sleeping);
	cprintf("%d\n", config.c_initax);
	cprintf("%d\n", config.c_regiscall);
	cprintf("%d\n", config.c_twitdetect);
	cprintf("%s\n", config.c_twitroom);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("%d\n", config.c_restrict);
	cprintf("%s\n", config.c_bbs_city);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%d\n", config.c_setup_level);
	cprintf("%d\n", config.c_maxsessions);
	cprintf("%d\n", config.c_port_number);
	cprintf("%d\n", config.c_ep.expire_mode);
	cprintf("%d\n", config.c_ep.expire_value);
	cprintf("%d\n", config.c_userpurge);
	cprintf("%d\n", config.c_roompurge);
	cprintf("%s\n", config.c_logpages);
	cprintf("%d\n", config.c_createax);
	cprintf("%ld\n", config.c_maxmsglen);
	cprintf("%d\n", config.c_min_workers);
	cprintf("%d\n", config.c_max_workers);
	cprintf("%d\n", config.c_pop3_port);
	cprintf("%d\n", config.c_smtp_port);
	cprintf("%d\n", config.c_purge_hour);
	cprintf("%d\n", config.c_mbxep.expire_mode);
	cprintf("%d\n", config.c_mbxep.expire_value);
	cprintf("%s\n", config.c_ldap_host);
	cprintf("%d\n", config.c_ldap_port);
	cprintf("%s\n", config.c_ldap_base_dn);
	cprintf("%s\n", config.c_ldap_bind_dn);
	cprintf("%s\n", config.c_ldap_bind_pw);

	/* Export the control file */
	get_control();
	cprintf("control\n");
	cprintf("%ld\n", CitControl.MMhighest);
	cprintf("%u\n", CitControl.MMflags);
	cprintf("%ld\n", CitControl.MMnextuser);
	cprintf("%ld\n", CitControl.MMnextroom);
	cprintf("%d\n", CitControl.version);

	artv_export_users();
	artv_export_rooms();
	artv_export_floors();
	artv_export_visits();
	artv_export_messages();

	cprintf("000\n");
}



void artv_import_config(void) {
	char buf[SIZ];

	lprintf(9, "Importing config file\n");
	client_gets(config.c_nodename);
	lprintf(9, "c_nodename = %s\n", config.c_nodename);
	client_gets(config.c_fqdn);
	client_gets(config.c_humannode);
	client_gets(config.c_phonenum);
	client_gets(buf);	config.c_bbsuid = atoi(buf);
	client_gets(buf);	config.c_creataide = atoi(buf);
	client_gets(buf);	config.c_sleeping = atoi(buf);
	client_gets(buf);	config.c_initax = atoi(buf);
	client_gets(buf);	config.c_regiscall = atoi(buf);
	client_gets(buf);	config.c_twitdetect = atoi(buf);
	client_gets(config.c_twitroom);
	client_gets(config.c_moreprompt);
	client_gets(buf);	config.c_restrict = atoi(buf);
	client_gets(config.c_bbs_city);
	client_gets(config.c_sysadm);
	lprintf(9, "c_sysadm = %s\n", config.c_sysadm);
	client_gets(buf);	config.c_setup_level = atoi(buf);
	client_gets(buf);	config.c_maxsessions = atoi(buf);
	client_gets(buf);	config.c_port_number = atoi(buf);
	client_gets(buf);	config.c_ep.expire_mode = atoi(buf);
	client_gets(buf);	config.c_ep.expire_value = atoi(buf);
	client_gets(buf);	config.c_userpurge = atoi(buf);
	client_gets(buf);	config.c_roompurge = atoi(buf);
	client_gets(config.c_logpages);
	client_gets(buf);	config.c_createax = atoi(buf);
	client_gets(buf);	config.c_maxmsglen = atol(buf);
	client_gets(buf);	config.c_min_workers = atoi(buf);
	client_gets(buf);	config.c_max_workers = atoi(buf);
	client_gets(buf);	config.c_pop3_port = atoi(buf);
	client_gets(buf);	config.c_smtp_port = atoi(buf);
	client_gets(buf);	config.c_purge_hour = atoi(buf);
	client_gets(buf);	config.c_mbxep.expire_mode = atoi(buf);
	client_gets(buf);	config.c_mbxep.expire_value = atoi(buf);
	client_gets(config.c_ldap_host);
	client_gets(buf);	config.c_ldap_port = atoi(buf);
	client_gets(config.c_ldap_base_dn);
	client_gets(config.c_ldap_bind_dn);
	client_gets(config.c_ldap_bind_pw);
	put_config();
	lprintf(7, "Imported config file\n");
}



void artv_import_control(void) {
	char buf[SIZ];

	lprintf(9, "Importing control file\n");
	client_gets(buf);	CitControl.MMhighest = atol(buf);
	client_gets(buf);	CitControl.MMflags = atoi(buf);
	client_gets(buf);	CitControl.MMnextuser = atol(buf);
	client_gets(buf);	CitControl.MMnextroom = atol(buf);
	client_gets(buf);	CitControl.version = atoi(buf);
	put_control();
	lprintf(7, "Imported control file\n");
}


void artv_import_user(void) {
	char buf[SIZ];
	struct ctdluser usbuf;

	client_gets(buf);	usbuf.version = atoi(buf);
	client_gets(buf);	usbuf.uid = atoi(buf);
	client_gets(usbuf.password);
	client_gets(buf);	usbuf.flags = atoi(buf);
	client_gets(buf);	usbuf.timescalled = atol(buf);
	client_gets(buf);	usbuf.posted = atol(buf);
	client_gets(buf);	usbuf.axlevel = atoi(buf);
	client_gets(buf);	usbuf.usernum = atol(buf);
	client_gets(buf);	usbuf.lastcall = atol(buf);
	client_gets(buf);	usbuf.USuserpurge = atoi(buf);
	client_gets(usbuf.fullname);
	client_gets(buf);	usbuf.USscreenwidth = atoi(buf);
	client_gets(buf);	usbuf.USscreenheight = atoi(buf);
	putuser(&usbuf);
}


void artv_import_room(void) {
	char buf[SIZ];
	struct ctdlroom qrbuf;
	long msgnum;
	int msgcount = 0;

	client_gets(qrbuf.QRname);
	client_gets(qrbuf.QRpasswd);
	client_gets(buf);	qrbuf.QRroomaide = atol(buf);
	client_gets(buf);	qrbuf.QRhighest = atol(buf);
	client_gets(buf);	qrbuf.QRgen = atol(buf);
	client_gets(buf);	qrbuf.QRflags = atoi(buf);
	client_gets(qrbuf.QRdirname);
	client_gets(buf);	qrbuf.QRinfo = atol(buf);
	client_gets(buf);	qrbuf.QRfloor = atoi(buf);
	client_gets(buf);	qrbuf.QRmtime = atol(buf);
	client_gets(buf);	qrbuf.QRep.expire_mode = atoi(buf);
	client_gets(buf);	qrbuf.QRep.expire_value = atoi(buf);
	client_gets(buf);	qrbuf.QRnumber = atol(buf);
	client_gets(buf);	qrbuf.QRorder = atoi(buf);
	client_gets(buf);	qrbuf.QRflags2 = atoi(buf);
	client_gets(buf);	qrbuf.QRdefaultview = atoi(buf);
	putroom(&qrbuf);
	lprintf(7, "Imported room <%s>\n", qrbuf.QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	while (client_gets(buf), msgnum = atol(buf), msgnum > 0) {
		CtdlSaveMsgPointerInRoom(qrbuf.QRname, msgnum, 0);
		++msgcount;
	}
	lprintf(7, "(%d messages)\n", msgcount);
}


void artv_import_floor(void) {
        struct floor flbuf;
        int i;
	char buf[SIZ];

	client_gets(buf);		i = atoi(buf);
	client_gets(buf);		flbuf.f_flags = atoi(buf);
	client_gets(flbuf.f_name);
	client_gets(buf);		flbuf.f_ref_count = atoi(buf);
	client_gets(buf);		flbuf.f_ep.expire_mode = atoi(buf);
	client_gets(buf);		flbuf.f_ep.expire_value = atoi(buf);
	putfloor(&flbuf, i);
	lprintf(7, "Imported floor #%d (%s)\n", i, flbuf.f_name);
}


/* 
 */
void artv_import_visit(void) {
	struct visit vbuf;
	char buf[SIZ];
	int i;
	int is_textual_seen = 0;

	client_gets(buf);	vbuf.v_roomnum = atol(buf);
	client_gets(buf);	vbuf.v_roomgen = atol(buf);
	client_gets(buf);	vbuf.v_usernum = atol(buf);

	client_gets(buf);
	vbuf.v_lastseen = atol(buf);
	for (i=0; i<strlen(buf); ++i) if (!isdigit(buf[i])) is_textual_seen = 1;
	if (is_textual_seen)	strcpy(vbuf.v_seen, buf);

	client_gets(vbuf.v_answered);
	client_gets(buf);	vbuf.v_flags = atoi(buf);
	client_gets(buf);	vbuf.v_view = atoi(buf);
	put_visit(&vbuf);
	lprintf(7, "Imported visit %ld/%ld/%ld\n",
		vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
}



void artv_import_message(void) {
	struct MetaData smi;
	long msgnum;
	long msglen;
	FILE *fp;
	char buf[SIZ];
	char tempfile[SIZ];
	char *mbuf;

	memset(&smi, 0, sizeof(struct MetaData));
	client_gets(buf);	msgnum = atol(buf);
				smi.meta_msgnum = msgnum;
	client_gets(buf);	smi.meta_refcount = atoi(buf);
	client_gets(smi.meta_content_type);

	lprintf(7, "message #%ld\n", msgnum);

	/* decode base64 message text */
	strcpy(tempfile, tmpnam(NULL));
	snprintf(buf, sizeof buf, "./base64 -d >%s", tempfile);
	fp = popen(buf, "w");
	while (client_gets(buf), strcasecmp(buf, END_OF_MESSAGE)) {
		fprintf(fp, "%s\n", buf);
	}
	pclose(fp);
	fp = fopen(tempfile, "rb");
	fseek(fp, 0L, SEEK_END);
	msglen = ftell(fp);
	fclose(fp);
	lprintf(9, "msglen = %ld\n", msglen);

	mbuf = mallok(msglen);
	fp = fopen(tempfile, "rb");
	fread(mbuf, msglen, 1, fp);
	fclose(fp);

        cdb_store(CDB_MSGMAIN, &msgnum, sizeof(long), mbuf, msglen);

	phree(mbuf);
	unlink(tempfile);

	PutMetaData(&smi);
	lprintf(7, "Imported message %ld\n", msgnum);
}




void artv_do_import(void) {
	char buf[SIZ];
	char s_version[SIZ];
	int version;

	cprintf("%d sock it to me\n", SEND_LISTING);
	while (client_gets(buf), strcmp(buf, "000")) {

		lprintf(9, "import keyword: <%s>\n", buf);

		if (!strcasecmp(buf, "version")) {
			client_gets(s_version);
			version = atoi(s_version);
			if ((version<EXPORT_REV_MIN) || (version>REV_LEVEL)) {
				lprintf(7, "Version mismatch - aborting\n");
				break;
			}
		}
		else if (!strcasecmp(buf, "config")) artv_import_config();
		else if (!strcasecmp(buf, "control")) artv_import_control();
		else if (!strcasecmp(buf, "user")) artv_import_user();
		else if (!strcasecmp(buf, "room")) artv_import_room();
		else if (!strcasecmp(buf, "floor")) artv_import_floor();
		else if (!strcasecmp(buf, "visit")) artv_import_visit();
		else if (!strcasecmp(buf, "message")) artv_import_message();
		else break;

	}
	lprintf(7, "Invalid keyword <%s>.  Flushing input.\n", buf);
	while (client_gets(buf), strcmp(buf, "000"))  ;;
}



void cmd_artv(char *cmdbuf) {
	char cmd[SIZ];
	static int is_running = 0;

	if (CtdlAccessCheck(ac_internal)) return;
	if (is_running) {
		cprintf("%d The importer/exporter is already running.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}
	is_running = 1;

	strcpy(artv_tempfilename2, tmpnam(NULL));

	extract(cmd, cmdbuf, 0);
	if (!strcasecmp(cmd, "export")) artv_do_export();
	else if (!strcasecmp(cmd, "import")) artv_do_import();
	else cprintf("%d illegal command\n", ERROR + ILLEGAL_VALUE);

	unlink(artv_tempfilename1);
	unlink(artv_tempfilename2);

	is_running = 0;
}




char *serv_vandelay_init(void)
{
	CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");
	return "$Id$";
}
