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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
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

void artv_export_users_backend(struct usersupp *usbuf, void *data) {
	cprintf("user\n");
	cprintf("%d\n", usbuf->version);
	cprintf("%d\n", usbuf->uid);
	cprintf("%s\n", usbuf->password);
	cprintf("%u\n", usbuf->flags);
	cprintf("%ld\n", usbuf->timescalled);
	cprintf("%ld\n", usbuf->posted);
	cprintf("%d\n", usbuf->axlevel);
	cprintf("%ld\n", usbuf->usernum);
	cprintf("%ld\n", usbuf->lastcall);
	cprintf("%d\n", usbuf->USuserpurge);
	cprintf("%s\n", usbuf->fullname);
	cprintf("%d\n", usbuf->USscreenwidth);
	cprintf("%d\n", usbuf->USscreenheight);
	cprintf("%d\n", usbuf->moderation_filter);
}


void artv_export_users(void) {
	ForEachUser(artv_export_users_backend, NULL);
}


void artv_export_room_msg(long msgnum, void *userdata) {
	cprintf("%ld\n", msgnum);
	fprintf(artv_global_message_list, "%ld\n", msgnum);
}


void artv_export_rooms_backend(struct quickroom *qrbuf, void *data) {
	cprintf("room\n");
	cprintf("%s\n", qrbuf->QRname);
	cprintf("%s\n", qrbuf->QRpasswd);
	cprintf("%ld\n", qrbuf->QRroomaide);
	cprintf("%ld\n", qrbuf->QRhighest);
	cprintf("%ld\n", qrbuf->QRgen);
	cprintf("%u\n", qrbuf->QRflags);
	cprintf("%s\n", qrbuf->QRdirname);
	cprintf("%ld\n", qrbuf->QRinfo);
	cprintf("%d\n", qrbuf->QRfloor);
	cprintf("%ld\n", qrbuf->QRmtime);
	cprintf("%d\n", qrbuf->QRep.expire_mode);
	cprintf("%d\n", qrbuf->QRep.expire_value);
	cprintf("%ld\n", qrbuf->QRnumber);
	cprintf("%d\n", qrbuf->QRorder);

	getroom(&CC->quickroom, qrbuf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, (-127), NULL, NULL,
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
	sprintf(cmd, "sort <%s >%s", artv_tempfilename1, artv_tempfilename2);
	system(cmd);
	sprintf(cmd, "uniq <%s >%s", artv_tempfilename2, artv_tempfilename1);
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

	cdb_begin_transaction();
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
		cprintf("%ld\n", vbuf.v_lastseen);
		cprintf("%u\n", vbuf.v_flags);
	}
	cdb_end_transaction();
}


void artv_export_message(long msgnum) {
	struct SuppMsgInfo smi;
	struct CtdlMessage *msg;
	struct ser_ret smr;
	FILE *fp;
	char buf[SIZ];
	char tempfile[SIZ];

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) return;	/* fail silently */

	cprintf("message\n");
	GetSuppMsgInfo(&smi, msgnum);
	cprintf("%ld\n", msgnum);
	cprintf("%d\n", smi.smi_refcount);
	cprintf("%s\n", smi.smi_content_type);
	cprintf("%d\n", smi.smi_mod);

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	/* write it in base64 */
	strcpy(tempfile, tmpnam(NULL));
	sprintf(buf, "./base64 -e >%s", tempfile);
	fp = popen(buf, "w");
	fwrite(smr.ser, smr.len, 1, fp);
	fclose(fp);

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
	lprintf(7, "Exported %ld messages.\n", count);
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
	cprintf("%d\n", config.c_bbsuid);
	cprintf("%d\n", config.c_creataide);
	cprintf("%d\n", config.c_sleeping);
	cprintf("%d\n", config.c_initax);
	cprintf("%d\n", config.c_regiscall);
	cprintf("%d\n", config.c_twitdetect);
	cprintf("%s\n", config.c_twitroom);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("%d\n", config.c_restrict);
	cprintf("%ld\n", config.c_msgbase);
	cprintf("%s\n", config.c_bbs_city);
	cprintf("%s\n", config.c_sysadm);
	cprintf("%s\n", config.c_bucket_dir);
	cprintf("%d\n", config.c_setup_level);
	cprintf("%d\n", config.c_maxsessions);
	cprintf("%s\n", config.c_net_password);
	cprintf("%d\n", config.c_port_number);
	cprintf("%d\n", config.c_ipgm_secret);
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
	cprintf("%d\n", config.c_default_filter);

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
	char *ibuf;

	lprintf(9, "Importing config file\n");
	client_gets(&ibuf); strcpy(config.c_nodename, ibuf);
	lprintf(9, "c_nodename = %s\n", config.c_nodename);
	client_gets(&ibuf); strcpy (config.c_fqdn, ibuf);
	client_gets(&ibuf); strcpy (config.c_humannode, ibuf);
	client_gets(&ibuf); strcpy (config.c_phonenum, ibuf);
	client_gets(&ibuf);	config.c_bbsuid = atoi(ibuf);
	client_gets(&ibuf);	config.c_creataide = atoi(ibuf);
	client_gets(&ibuf);	config.c_sleeping = atoi(ibuf);
	client_gets(&ibuf);	config.c_initax = atoi(ibuf);
	client_gets(&ibuf);	config.c_regiscall = atoi(ibuf);
	client_gets(&ibuf);	config.c_twitdetect = atoi(ibuf);
	client_gets(&ibuf); strcpy(config.c_twitroom, ibuf);
	client_gets(&ibuf); strcpy(config.c_moreprompt, ibuf);
	client_gets(&ibuf);	config.c_restrict = atoi(ibuf);
	client_gets(&ibuf);	config.c_msgbase = atol(ibuf);
	client_gets(&ibuf); strcpy(config.c_bbs_city, ibuf);
	client_gets(&ibuf); strcpy(config.c_sysadm, ibuf);
	lprintf(9, "c_sysadm = %s\n", config.c_sysadm);
	client_gets(&ibuf); strcpy(config.c_bucket_dir, ibuf);
	client_gets(&ibuf);	config.c_setup_level = atoi(ibuf);
	client_gets(&ibuf);	config.c_maxsessions = atoi(ibuf);
	client_gets(&ibuf); strcpy(config.c_net_password, ibuf);
	client_gets(&ibuf);	config.c_port_number = atoi(ibuf);
	client_gets(&ibuf);	config.c_ipgm_secret = atoi(ibuf);
	client_gets(&ibuf);	config.c_ep.expire_mode = atoi(ibuf);
	client_gets(&ibuf);	config.c_ep.expire_value = atoi(ibuf);
	client_gets(&ibuf);	config.c_userpurge = atoi(ibuf);
	client_gets(&ibuf);	config.c_roompurge = atoi(ibuf);
	client_gets(&ibuf); strcpy(config.c_logpages, ibuf);
	client_gets(&ibuf);	config.c_createax = atoi(ibuf);
	client_gets(&ibuf);	config.c_maxmsglen = atol(ibuf);
	client_gets(&ibuf);	config.c_min_workers = atoi(ibuf);
	client_gets(&ibuf);	config.c_max_workers = atoi(ibuf);
	client_gets(&ibuf);	config.c_pop3_port = atoi(ibuf);
	client_gets(&ibuf);	config.c_smtp_port = atoi(ibuf);
	client_gets(&ibuf);	config.c_default_filter = atoi(ibuf);
	put_config();
	lprintf(7, "Imported config file\n");
}



void artv_import_control(void) {
	char *ibuf;

	lprintf(9, "Importing control file\n");
	client_gets(&ibuf);	CitControl.MMhighest = atol(ibuf);
	client_gets(&ibuf);	CitControl.MMflags = atoi(ibuf);
	client_gets(&ibuf);	CitControl.MMnextuser = atol(ibuf);
	client_gets(&ibuf);	CitControl.MMnextroom = atol(ibuf);
	client_gets(&ibuf);	CitControl.version = atoi(ibuf);
	put_control();
	lprintf(7, "Imported control file\n");
}


void artv_import_user(void) {
	char *ibuf;
	struct usersupp usbuf;

	client_gets(&ibuf);	usbuf.version = atoi(ibuf);
	client_gets(&ibuf);	usbuf.uid = atoi(ibuf);
	client_gets(&ibuf); strcpy(usbuf.password, ibuf);
	client_gets(&ibuf);	usbuf.flags = atoi(ibuf);
	client_gets(&ibuf);	usbuf.timescalled = atol(ibuf);
	client_gets(&ibuf);	usbuf.posted = atol(ibuf);
	client_gets(&ibuf);	usbuf.axlevel = atoi(ibuf);
	client_gets(&ibuf);	usbuf.usernum = atol(ibuf);
	client_gets(&ibuf);	usbuf.lastcall = atol(ibuf);
	client_gets(&ibuf);	usbuf.USuserpurge = atoi(ibuf);
	client_gets(&ibuf); strcpy(usbuf.fullname, ibuf);
	client_gets(&ibuf);	usbuf.USscreenwidth = atoi(ibuf);
	client_gets(&ibuf);	usbuf.USscreenheight = atoi(ibuf);
	client_gets(&ibuf);	usbuf.moderation_filter = atoi(ibuf);
	putuser(&usbuf);
}


void artv_import_room(void) {
	char *ibuf;
	struct quickroom qrbuf;
	long msgnum;
	int msgcount = 0;

	client_gets(&ibuf); strcpy(qrbuf.QRname, ibuf);
	client_gets(&ibuf); strcpy(qrbuf.QRpasswd, ibuf);
	client_gets(&ibuf);	qrbuf.QRroomaide = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRhighest = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRgen = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRflags = atoi(ibuf);
	client_gets(&ibuf); strcpy(qrbuf.QRdirname, ibuf);
	client_gets(&ibuf);	qrbuf.QRinfo = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRfloor = atoi(ibuf);
	client_gets(&ibuf);	qrbuf.QRmtime = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRep.expire_mode = atoi(ibuf);
	client_gets(&ibuf);	qrbuf.QRep.expire_value = atoi(ibuf);
	client_gets(&ibuf);	qrbuf.QRnumber = atol(ibuf);
	client_gets(&ibuf);	qrbuf.QRorder = atoi(ibuf);
	putroom(&qrbuf);
	lprintf(7, "Imported room <%s>\n", qrbuf.QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	while (client_gets(&ibuf), msgnum = atol(ibuf), msgnum > 0) {
		CtdlSaveMsgPointerInRoom(qrbuf.QRname, msgnum, 0);
		++msgcount;
	}
	lprintf(7, "(%d messages)\n", msgcount);
}


void artv_import_floor(void) {
        struct floor flbuf;
        int i;
	char *ibuf;

	client_gets(&ibuf);		i = atoi(ibuf);
	client_gets(&ibuf);		flbuf.f_flags = atoi(ibuf);
	client_gets(&ibuf);     strcpy(flbuf.f_name, ibuf);
	client_gets(&ibuf);		flbuf.f_ref_count = atoi(ibuf);
	client_gets(&ibuf);		flbuf.f_ep.expire_mode = atoi(ibuf);
	client_gets(&ibuf);		flbuf.f_ep.expire_value = atoi(ibuf);
	putfloor(&flbuf, i);
	lprintf(7, "Imported floor #%d (%s)\n", i, flbuf.f_name);
}


/* 
 */
void artv_import_visit(void) {
	struct visit vbuf;
	char *ibuf;

	client_gets(&ibuf);	vbuf.v_roomnum = atol(ibuf);
	client_gets(&ibuf);	vbuf.v_roomgen = atol(ibuf);
	client_gets(&ibuf);	vbuf.v_usernum = atol(ibuf);
	client_gets(&ibuf);	vbuf.v_lastseen = atol(ibuf);
	client_gets(&ibuf);	vbuf.v_flags = atoi(ibuf);
	put_visit(&vbuf);
	lprintf(7, "Imported visit %ld/%ld/%ld\n",
		vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
}



void artv_import_message(void) {
	struct SuppMsgInfo smi;
	long msgnum;
	int msglen;
	FILE *fp;
	char *ibuf;
	char buf[SIZ];
	char tempfile[SIZ];
	char *mbuf;

	memset(&smi, 0, sizeof(struct SuppMsgInfo));
	client_gets(&ibuf);	msgnum = atol(ibuf);
				smi.smi_msgnum = msgnum;
	client_gets(&ibuf);	smi.smi_refcount = atoi(ibuf);
	client_gets(&ibuf); strcpy(smi.smi_content_type, ibuf);
	client_gets(&ibuf);	smi.smi_mod = atoi(ibuf);

	lprintf(7, "message #%ld\n", msgnum);

	/* decode base64 message text */
	strcpy(tempfile, tmpnam(NULL));
	sprintf(buf, "./base64 -d >%s", tempfile);
	fp = popen(buf, "w");
	while (client_gets(&ibuf), strcasecmp(ibuf, END_OF_MESSAGE)) {
		fprintf(fp, "%s\n", buf);
	}
	fclose(fp);
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

	PutSuppMsgInfo(&smi);
	lprintf(7, "Imported message %ld\n", msgnum);
}




void artv_do_import(void) {
	char *ibuf;
	char *is_version;
	int version;

	cprintf("%d sock it to me\n", SEND_LISTING);
	while (client_gets(&ibuf), strcmp(ibuf, "000")) {

		lprintf(9, "import keyword: <%s>\n", ibuf); 

		if (!strcasecmp(ibuf, "version")) {
			client_gets(&is_version);
			version = atoi(is_version);
			if ((version < REV_MIN) || (version > REV_LEVEL)) {
				lprintf(7, "Version mismatch - aborting\n");
				break;
			}
		}
		else if (!strcasecmp(ibuf, "config")) artv_import_config();
		else if (!strcasecmp(ibuf, "control")) artv_import_control();
		else if (!strcasecmp(ibuf, "user")) artv_import_user();
		else if (!strcasecmp(ibuf, "room")) artv_import_room();
		else if (!strcasecmp(ibuf, "floor")) artv_import_floor();
		else if (!strcasecmp(ibuf, "visit")) artv_import_visit();
		else if (!strcasecmp(ibuf, "message")) artv_import_message();
		else break;

	}
	lprintf(7, "Invalid keyword <%s>.  Flushing input.\n", ibuf);
	while (client_gets(&ibuf), strcmp(ibuf, "000"))  ;;
}



void cmd_artv(char *cmdbuf) {
	char cmd[SIZ]; 
	static int is_running = 0;

	if (CtdlAccessCheck(ac_aide)) return;	/* FIXME should be intpgm */
	if (is_running) {
		cprintf("%d The importer/exporter is already running.\n",
			ERROR);
		return;
	}
	is_running = 1;

	strcpy(artv_tempfilename1, tmpnam(NULL));
	strcpy(artv_tempfilename2, tmpnam(NULL));

	extract(cmd, cmdbuf, 0); /* this is limited to SIZ */
	if (!strcasecmp(cmd, "export")) artv_do_export();
	else if (!strcasecmp(cmd, "import")) artv_do_import();
	else cprintf("%d illegal command\n", ERROR);

	unlink(artv_tempfilename1);
	unlink(artv_tempfilename2);

	is_running = 0;
}




char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");
	return "$Id$";
}
