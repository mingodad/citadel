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
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "user_ops.h"
#include "room_ops.h"
#include "control.h"
#include "euidindex.h"


#include "ctdl_module.h"




#define END_OF_MESSAGE	"---eom---dbd---"

char artv_tempfilename1[PATH_MAX];
char artv_tempfilename2[PATH_MAX];
FILE *artv_global_message_list;

void artv_export_users_backend(struct ctdluser *buf, void *data) {
	cprintf("user\n");
/*
#include "artv_serialize.h"
#include "dtds/user-defs.h"
#include "undef_data.h"
*/
	cprintf("%d\n", buf->version);
	cprintf("%ld\n", (long)buf->uid);
	cprintf("%s\n", buf->password);
	cprintf("%u\n", buf->flags);
	cprintf("%ld\n", buf->timescalled);
	cprintf("%ld\n", buf->posted);
	cprintf("%d\n", buf->axlevel);
	cprintf("%ld\n", buf->usernum);
	cprintf("%ld\n", (long)buf->lastcall);
	cprintf("%d\n", buf->USuserpurge);
	cprintf("%s\n", buf->fullname);
	cprintf("%d\n", buf->USscreenwidth);
	cprintf("%d\n", buf->USscreenheight);
}


void artv_export_users(void) {
	ForEachUser(artv_export_users_backend, NULL);
}


void artv_export_room_msg(long msgnum, void *userdata) {
	cprintf("%ld\n", msgnum);
	fprintf(artv_global_message_list, "%ld\n", msgnum);
}


void artv_export_rooms_backend(struct ctdlroom *buf, void *data) {
	cprintf("room\n");
/*
#include "artv_serialize.h"
#include "dtds/room-defs.h"
#include "undef_data.h"
*/
	cprintf("%s\n", buf->QRname);
	cprintf("%s\n", buf->QRpasswd);
	cprintf("%ld\n", buf->QRroomaide);
	cprintf("%ld\n", buf->QRhighest);
	cprintf("%ld\n", (long)buf->QRgen);
	cprintf("%u\n", buf->QRflags);
	cprintf("%s\n", buf->QRdirname);
	cprintf("%ld\n", buf->QRinfo);
	cprintf("%d\n", buf->QRfloor);
	cprintf("%ld\n", (long)buf->QRmtime);
	cprintf("%d\n", buf->QRep.expire_mode);
	cprintf("%d\n", buf->QRep.expire_value);
	cprintf("%ld\n", buf->QRnumber);
	cprintf("%d\n", buf->QRorder);
	cprintf("%u\n", buf->QRflags2);
	cprintf("%d\n", buf->QRdefaultview);

	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
//*/
	getroom(&CC->room, buf->QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
		artv_export_room_msg, NULL);
	cprintf("0\n");

}



void artv_export_rooms(void) {
	char cmd[SIZ];
	artv_global_message_list = fopen(artv_tempfilename1, "w");
	if (artv_global_message_list != NULL) {
		ForEachRoom(artv_export_rooms_backend, NULL);
		fclose(artv_global_message_list);
	}

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
        struct floor qfbuf, *buf;
        int i;

        for (i=0; i < MAXFLOORS; ++i) {
		cprintf("floor\n");
		cprintf("%d\n", i);
                getfloor(&qfbuf, i);
		buf = &qfbuf;
/*
#include "artv_serialize.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
/*/
		cprintf("%u\n", buf->f_flags);
		cprintf("%s\n", buf->f_name);
		cprintf("%d\n", buf->f_ref_count);
		cprintf("%d\n", buf->f_ep.expire_mode);
		cprintf("%d\n", buf->f_ep.expire_value);
//*/
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

		if (!IsEmptyStr(vbuf.v_seen)) {
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
	char tempfile[PATH_MAX];

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;	/* fail silently */

	cprintf("message\n");
	GetMetaData(&smi, msgnum);
	cprintf("%ld\n", msgnum);
	cprintf("%d\n", smi.meta_refcount);
	cprintf("%s\n", smi.meta_content_type);

	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

	/* write it in base64 */
	CtdlMakeTempFileName(tempfile, sizeof tempfile);
	snprintf(buf, sizeof buf, "%s -e >%s", file_base64, tempfile);
	fp = popen(buf, "w");
	fwrite(smr.ser, smr.len, 1, fp);
	pclose(fp);

	free(smr.ser);

	fp = fopen(tempfile, "r");
	unlink(tempfile);
	if (fp != NULL) {
		while (fgets(buf, sizeof(buf), fp) != NULL) {
			buf[strlen(buf)-1] = 0;
			cprintf("%s\n", buf);
		}
		fclose(fp);
	}
	cprintf("%s\n", END_OF_MESSAGE);
}



void artv_export_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;

	artv_global_message_list = fopen(artv_tempfilename1, "r");
	if (artv_global_message_list != NULL) {
		lprintf(CTDL_INFO, "Opened %s\n", artv_tempfilename1);
		while (fgets(buf, sizeof(buf),
		      artv_global_message_list) != NULL) {
			msgnum = atol(buf);
			if (msgnum > 0L) {
				artv_export_message(msgnum);
				++count;
			}
		}
		fclose(artv_global_message_list);
	}
	lprintf(CTDL_INFO, "Exported %d messages.\n", count);
}




void artv_do_export(void) {
	struct config *buf;
	buf = &config;
	cprintf("%d Exporting all Citadel databases.\n", LISTING_FOLLOWS);

	cprintf("version\n%d\n", REV_LEVEL);

	/* export the config file */
	cprintf("config\n");

#include "artv_serialize.h"
#include "dtds/config-defs.h"
#include "undef_data.h"

/*
	cprintf("%s\n", config.c_nodename);
	cprintf("%s\n", config.c_fqdn);
	cprintf("%s\n", config.c_humannode);
	cprintf("%s\n", config.c_phonenum);
	cprintf("%ld\n", (long)config.c_ctdluid);
	cprintf("%d\n", config.c_creataide);
	cprintf("%d\n", config.c_sleeping);
	cprintf("%d\n", config.c_initax);
	cprintf("%d\n", config.c_regiscall);
	cprintf("%d\n", config.c_twitdetect);
	cprintf("%s\n", config.c_twitroom);
	cprintf("%s\n", config.c_moreprompt);
	cprintf("%d\n", config.c_restrict);
	cprintf("%s\n", config.c_site_location);
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
	cprintf("%s\n", config.c_ip_addr);
	cprintf("%d\n", config.c_msa_port);
	cprintf("%d\n", config.c_imaps_port);
	cprintf("%d\n", config.c_pop3s_port);
	cprintf("%d\n", config.c_smtps_port);
	cprintf("%d\n", config.c_rfc822_strict_from);
	cprintf("%d\n", config.c_aide_zap);
	cprintf("%d\n", config.c_imap_port);
	cprintf("%ld\n", config.c_net_freq);
	cprintf("%d\n", config.c_disable_newu);
	cprintf("%s\n", config.c_baseroom);
	cprintf("%s\n", config.c_aideroom);
	cprintf("%d\n", config.c_auto_cull);
	cprintf("%d\n", config.c_instant_expunge);
	cprintf("%d\n", config.c_allow_spoofing);
	cprintf("%d\n", config.c_journal_email);
	cprintf("%d\n", config.c_journal_pubmsgs);
	cprintf("%s\n", config.c_journal_dest);
	cprintf("%s\n", config.c_default_cal_zone);
	cprintf("%d\n", config.c_pftcpdict_port);
	cprintf("%d\n", config.c_managesieve_port);
	cprintf("%d\n", config.c_auth_mode);
	cprintf("%s\n", config.c_funambol_host);
	cprintf("%d\n", config.c_funambol_port);
	cprintf("%s\n", config.c_funambol_source);
	cprintf("%s\n", config.c_funambol_auth);
	cprintf("%d\n", config.c_rbl_at_greeting);
*/
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
	char cbuf[SIZ];
	struct config *buf;
	buf = &config;

	lprintf(CTDL_DEBUG, "Importing config file\n");

#include "artv_deserialize.h"
#include "dtds/config-defs.h"
#include "undef_data.h"

/*
	client_getln(config.c_nodename, sizeof config.c_nodename);
	client_getln(config.c_fqdn, sizeof config.c_fqdn);
	client_getln(config.c_humannode, sizeof config.c_humannode);
	client_getln(config.c_phonenum, sizeof config.c_phonenum);
	client_getln(buf, sizeof buf);	config.c_ctdluid = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_creataide = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_sleeping = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_initax = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_regiscall = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_twitdetect = atoi(buf);
	client_getln(config.c_twitroom, sizeof config.c_twitroom);
	client_getln(config.c_moreprompt, sizeof config.c_moreprompt);
	client_getln(buf, sizeof buf);	config.c_restrict = atoi(buf);
	client_getln(config.c_site_location, sizeof config.c_site_location);
	client_getln(config.c_sysadm, sizeof config.c_sysadm);
	client_getln(buf, sizeof buf);	config.c_setup_level = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_maxsessions = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_port_number = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_ep.expire_mode = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_ep.expire_value = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_userpurge = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_roompurge = atoi(buf);
	client_getln(config.c_logpages, sizeof config.c_logpages);
	client_getln(buf, sizeof buf);	config.c_createax = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_maxmsglen = atol(buf);
	client_getln(buf, sizeof buf);	config.c_min_workers = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_max_workers = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_pop3_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_smtp_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_purge_hour = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_mbxep.expire_mode = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_mbxep.expire_value = atoi(buf);
	client_getln(config.c_ldap_host, sizeof config.c_ldap_host);
	client_getln(buf, sizeof buf);	config.c_ldap_port = atoi(buf);
	client_getln(config.c_ldap_base_dn, sizeof config.c_ldap_base_dn);
	client_getln(config.c_ldap_bind_dn, sizeof config.c_ldap_bind_dn);
	client_getln(config.c_ldap_bind_pw, sizeof config.c_ldap_bind_pw);
	client_getln(config.c_ip_addr, sizeof config.c_ip_addr);
	client_getln(buf, sizeof buf);	config.c_msa_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_imaps_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_pop3s_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_smtps_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_rfc822_strict_from = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_aide_zap = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_imap_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_net_freq = atol(buf);
	client_getln(buf, sizeof buf);	config.c_disable_newu = atoi(buf);
	client_getln(config.c_baseroom, sizeof config.c_baseroom);
	client_getln(config.c_aideroom, sizeof config.c_aideroom);
	client_getln(buf, sizeof buf);	config.c_auto_cull = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_instant_expunge = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_allow_spoofing = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_journal_email = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_journal_pubmsgs = atoi(buf);
	client_getln(config.c_journal_dest, sizeof config.c_journal_dest);
	client_getln(config.c_default_cal_zone, sizeof config.c_default_cal_zone);
	client_getln(buf, sizeof buf);	config.c_pftcpdict_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_managesieve_port = atoi(buf);
	client_getln(buf, sizeof buf);	config.c_auth_mode = atoi(buf);
	client_getln(config.c_funambol_host, sizeof config.c_funambol_host);
	client_getln(buf, sizeof buf); config.c_funambol_port = atoi(buf);
	client_getln(config.c_funambol_source, sizeof config.c_funambol_source);
	client_getln(config.c_funambol_auth, sizeof config.c_funambol_auth);
	client_getln(buf, sizeof buf);	config.c_rbl_at_greeting = atoi(buf);
*/
	config.c_enable_fulltext = 0;	/* always disable */
	put_config();
	lprintf(CTDL_INFO, "Imported config file\n");
}


void artv_import_control(void) {
	char buf[SIZ];

	lprintf(CTDL_DEBUG, "Importing control file\n");
	client_getln(buf, sizeof buf);	CitControl.MMhighest = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.MMflags = atoi(buf);
	client_getln(buf, sizeof buf);	CitControl.MMnextuser = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.MMnextroom = atol(buf);
	client_getln(buf, sizeof buf);	CitControl.version = atoi(buf);
	CitControl.MMfulltext = (-1L);	/* always flush */
	put_control();
	lprintf(CTDL_INFO, "Imported control file\n");
}


void artv_import_user(void) {
	char cbuf[SIZ];
	struct ctdluser usbuf, *buf;
	buf = &usbuf;
/*
#include "artv_deserialize.h"
#include "dtds/user-defs.h"
#include "undef_data.h"

/*/
	client_getln(cbuf, sizeof cbuf);	buf->version = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->uid = atoi(cbuf);
	client_getln(buf->password, sizeof buf->password);
	client_getln(cbuf, sizeof cbuf);	buf->flags = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->timescalled = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->posted = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->axlevel = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->usernum = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->lastcall = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->USuserpurge = atoi(cbuf);
	client_getln(buf->fullname, sizeof buf->fullname);
	client_getln(cbuf, sizeof cbuf);	buf->USscreenwidth = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->USscreenheight = atoi(cbuf);
//*/
	putuser(buf);
}


void artv_import_room(void) {
	char cbuf[SIZ];
	struct ctdlroom qrbuf, *buf;
	long msgnum;
	int msgcount = 0;

	buf = &qrbuf;
/*
#include "artv_deserialize.h"
#include "dtds/room-defs.h"
#include "undef_data.h"

/*/
	client_getln(buf->QRname, sizeof buf->QRname);
	client_getln(buf->QRpasswd, sizeof buf->QRpasswd);
	client_getln(cbuf, sizeof cbuf);	buf->QRroomaide = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRhighest = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRgen = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRflags = atoi(cbuf);
	client_getln(buf->QRdirname, sizeof buf->QRdirname);
	client_getln(cbuf, sizeof cbuf);	buf->QRinfo = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRfloor = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRmtime = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRep.expire_mode = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRep.expire_value = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRnumber = atol(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRorder = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRflags2 = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->QRdefaultview = atoi(cbuf);
//*/
	putroom(buf);
	lprintf(CTDL_INFO, "Imported room <%s>\n", qrbuf.QRname);
	/* format of message list export is all message numbers output
	 * one per line terminated by a 0.
	 */
	while (client_getln(cbuf, sizeof cbuf), msgnum = atol(cbuf), msgnum > 0) {
		CtdlSaveMsgPointerInRoom(qrbuf.QRname, msgnum, 0, NULL);
		++msgcount;
	}
	lprintf(CTDL_INFO, "(%d messages)\n", msgcount);
}


void artv_import_floor(void) {
        struct floor flbuf, *buf;
        int i;
	char cbuf[SIZ];

	buf = & flbuf;
	memset(buf, 0, sizeof(buf));
	client_getln(cbuf, sizeof cbuf);	i = atoi(cbuf);
/*
#include "artv_deserialize.h"
#include "dtds/floor-defs.h"
#include "undef_data.h"
/*/
	client_getln(cbuf, sizeof cbuf);	buf->f_flags = atoi(cbuf);
	client_getln(buf->f_name, sizeof buf->f_name);
	client_getln(cbuf, sizeof cbuf);	buf->f_ref_count = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->f_ep.expire_mode = atoi(cbuf);
	client_getln(cbuf, sizeof cbuf);	buf->f_ep.expire_value = atoi(cbuf);
//*/
	putfloor(buf, i);
	lprintf(CTDL_INFO, "Imported floor #%d (%s)\n", i, flbuf.f_name);
}


/* 
 */
void artv_import_visit(void) {
	struct visit vbuf;
	char buf[SIZ];
	int i;
	int is_textual_seen = 0;

	client_getln(buf, sizeof buf);	vbuf.v_roomnum = atol(buf);
	client_getln(buf, sizeof buf);	vbuf.v_roomgen = atol(buf);
	client_getln(buf, sizeof buf);	vbuf.v_usernum = atol(buf);

	client_getln(buf, sizeof buf);
	vbuf.v_lastseen = atol(buf);
	for (i=0; !IsEmptyStr(&buf[i]); ++i) if (!isdigit(buf[i])) is_textual_seen = 1;
	if (is_textual_seen)	strcpy(vbuf.v_seen, buf);

	client_getln(vbuf.v_answered, sizeof vbuf.v_answered);
	client_getln(buf, sizeof buf);	vbuf.v_flags = atoi(buf);
	client_getln(buf, sizeof buf);	vbuf.v_view = atoi(buf);
	put_visit(&vbuf);
	lprintf(CTDL_INFO, "Imported visit %ld/%ld/%ld\n",
		vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
}



void artv_import_message(void) {
	struct MetaData smi;
	long msgnum;
	long msglen;
	FILE *fp;
	char buf[SIZ];
	char tempfile[PATH_MAX];
	char *mbuf;

	memset(&smi, 0, sizeof(struct MetaData));
	client_getln(buf, sizeof buf);	msgnum = atol(buf);
				smi.meta_msgnum = msgnum;
	client_getln(buf, sizeof buf);	smi.meta_refcount = atoi(buf);
	client_getln(smi.meta_content_type, sizeof smi.meta_content_type);

	lprintf(CTDL_INFO, "message #%ld\n", msgnum);

	/* decode base64 message text */
	CtdlMakeTempFileName(tempfile, sizeof tempfile);
	snprintf(buf, sizeof buf, "%s -d >%s", file_base64, tempfile);
	fp = popen(buf, "w");
	while (client_getln(buf, sizeof buf), strcasecmp(buf, END_OF_MESSAGE)) {
		fprintf(fp, "%s\n", buf);
	}
	pclose(fp);
	fp = fopen(tempfile, "rb");
	fseek(fp, 0L, SEEK_END);
	msglen = ftell(fp);
	fclose(fp);
	lprintf(CTDL_DEBUG, "msglen = %ld\n", msglen);

	mbuf = malloc(msglen);
	fp = fopen(tempfile, "rb");
	fread(mbuf, msglen, 1, fp);
	fclose(fp);

        cdb_store(CDB_MSGMAIN, &msgnum, sizeof(long), mbuf, msglen);

	free(mbuf);
	unlink(tempfile);

	PutMetaData(&smi);
	lprintf(CTDL_INFO, "Imported message %ld\n", msgnum);
}




void artv_do_import(void) {
	char buf[SIZ];
	char abuf[SIZ];
	char s_version[SIZ];
	int version;
	long iterations;

	unbuffer_output();

	cprintf("%d sock it to me\n", SEND_LISTING);
	abuf[0] = '\0';
	unbuffer_output();
	iterations = 0;
	while (client_getln(buf, sizeof buf), strcmp(buf, "000")) {

		lprintf(CTDL_DEBUG, "import keyword: <%s>\n", buf);
		if ((abuf[0] == '\0') || (strcasecmp(buf, abuf))) {
			cprintf ("\n\nImporting datatype %s\n", buf);
			strncpy (abuf, buf, SIZ);	
			iterations = 0;
		}
		else {
			cprintf(".");
			if (iterations % 64 == 0)
				cprintf("\n");
			
		}
		
		if (!strcasecmp(buf, "version")) {
			client_getln(s_version, sizeof s_version);
			version = atoi(s_version);
			if ((version<EXPORT_REV_MIN) || (version>REV_LEVEL)) {
				lprintf(CTDL_ERR, "Version mismatch in ARTV import; aborting\n");
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
		iterations ++;
	}
	lprintf(CTDL_INFO, "Invalid keyword <%s>.  Flushing input.\n", buf);
	while (client_getln(buf, sizeof buf), strcmp(buf, "000"))  ;;
	rebuild_euid_index();
}



void cmd_artv(char *cmdbuf) {
	char cmd[32];
	static int is_running = 0;

	if (CtdlAccessCheck(ac_internal)) return;
	if (is_running) {
		cprintf("%d The importer/exporter is already running.\n",
			ERROR + RESOURCE_BUSY);
		return;
	}
	is_running = 1;

	CtdlMakeTempFileName(artv_tempfilename1, sizeof artv_tempfilename1);
	CtdlMakeTempFileName(artv_tempfilename2, sizeof artv_tempfilename2);

	extract_token(cmd, cmdbuf, 0, '|', sizeof cmd);
	if (!strcasecmp(cmd, "export")) artv_do_export();
	else if (!strcasecmp(cmd, "import")) artv_do_import();
	else cprintf("%d illegal command\n", ERROR + ILLEGAL_VALUE);

	unlink(artv_tempfilename1);
	unlink(artv_tempfilename2);

	is_running = 0;
}




CTDL_MODULE_INIT(vandelay)
{
	CtdlRegisterProtoHook(cmd_artv, "ARTV", "import/export data store");

	/* return our Subversion id for the Log */
	return "$Id$";
}
