/*
 * $Id$
 *
 * Copyright (c) 2000-2009 by the citadel.org development team
 *
 * This module dumps and/or loads the Citadel database in XML format.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
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




/*
 * Code which implements the export appears in this section
 */



/*
 * Output a string to the client with these characters escaped:  & < >
 */
void xml_strout(char *str) {

	char *c = str;

	while (*c != 0) {
		if (*c == '&') {
			client_write("&amp;", 5);
		}
		else if (*c == '<') {
			client_write("&lt;", 4);
		}
		else if (*c == '>') {
			client_write("&gt;", 4);
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
	client_write("<QRdirname>", 11);	xml_strout(buf->QRdirname);	client_write("</QRdirname>\n", 13);
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
	snprintf(cmd, sizeof cmd, "sort <%s >%s", migr_tempfilename1, migr_tempfilename2);
	if (system(cmd) != 0) CtdlLogPrintf(CTDL_ALERT, "Error %d\n", errno);
	snprintf(cmd, sizeof cmd, "uniq <%s >%s", migr_tempfilename2, migr_tempfilename1);
	if (system(cmd) != 0) CtdlLogPrintf(CTDL_ALERT, "Error %d\n", errno);
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
 *  Traverse the visits file...
 */
void migr_export_visits(void) {
	struct visit vbuf;
	struct cdbdata *cdbv;

	cdb_rewind(CDB_VISIT);

	while (cdbv = cdb_next_item(CDB_VISIT), cdbv != NULL) {
		memset(&vbuf, 0, sizeof(struct visit));
		memcpy(&vbuf, cdbv->ptr,
		       ((cdbv->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbv->len));
		cdb_free(cdbv);

		client_write("<visit>\n", 8);
		cprintf("<v_roomnum>%ld</v_roomnum>\n", vbuf.v_roomnum);
		cprintf("<v_roomgen>%ld</v_roomgen>\n", vbuf.v_roomgen);
		cprintf("<v_usernum>%ld</v_usernum>\n", vbuf.v_usernum);

		client_write("<v_seen>", 8);
		if (!IsEmptyStr(vbuf.v_seen)) {
			xml_strout(vbuf.v_seen);
		}
		else {
			cprintf("%ld", vbuf.v_lastseen);
		}
		client_write("</v_seen>", 9);

		client_write("<v_answered>", 12); xml_strout(vbuf.v_answered); client_write("</v_answered>\n", 14);
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
	serialize_message(&smr, msg);
	CtdlFreeMessage(msg);

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




void migr_export_messages(void) {
	char buf[SIZ];
	long msgnum;
	int count = 0;
	CitContext *Ctx;

	Ctx = CC;
	migr_global_message_list = fopen(migr_tempfilename1, "r");
	if (migr_global_message_list != NULL) {
		CtdlLogPrintf(CTDL_INFO, "Opened %s\n", migr_tempfilename1);
		while ((Ctx->kill_me != 1) && 
		       (fgets(buf, sizeof(buf), migr_global_message_list) != NULL)) {
			msgnum = atol(buf);
			if (msgnum > 0L) {
				migr_export_message(msgnum);
				++count;
			}
		}
		fclose(migr_global_message_list);
	}
	if (Ctx->kill_me != 1)
		CtdlLogPrintf(CTDL_INFO, "Exported %d messages.\n", count);
	else
		CtdlLogPrintf(CTDL_ERR, "Export aborted due to client disconnect! \n");

	migr_export_message(-1L);	/* This frees the encoding buffer */
}



void migr_do_export(void) {
	struct config *buf;
	buf = &config;
	CitContext *Ctx;

	Ctx = CC;
	cprintf("%d Exporting all Citadel databases.\n", LISTING_FOLLOWS);
	Ctx->dont_term = 1;

	client_write("<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n", 40);
	client_write("<citadel_migrate_data>\n", 23);
	cprintf("<version>%d</version>\n", REV_LEVEL);

	/* export the config file (this is done using x-macros) */
	client_write("<config>\n", 9);
	client_write("<c_nodename>", 12);	xml_strout(config.c_nodename);		client_write("</c_nodename>\n", 14);
	client_write("<c_fqdn>", 8);		xml_strout(config.c_fqdn);		client_write("</c_fqdn>\n", 10);
	client_write("<c_humannode>", 13);	xml_strout(config.c_humannode);		client_write("</c_humannode>\n", 15);
	client_write("<c_phonenum>", 12);	xml_strout(config.c_phonenum);		client_write("</c_phonenum>\n", 14);
	cprintf("<c_ctdluid>%d</c_ctdluid>\n", config.c_ctdluid);
	cprintf("<c_creataide>%d</c_creataide>\n", config.c_creataide);
	cprintf("<c_sleeping>%d</c_sleeping>\n", config.c_sleeping);
	cprintf("<c_initax>%d</c_initax>\n", config.c_initax);
	cprintf("<c_regiscall>%d</c_regiscall>\n", config.c_regiscall);
	cprintf("<c_twitdetect>%d</c_twitdetect>\n", config.c_twitdetect);
	client_write("<c_twitroom>", 12);	xml_strout(config.c_twitroom);		client_write("</c_twitroom>\n", 14);
	client_write("<c_moreprompt>", 14);	xml_strout(config.c_moreprompt);	client_write("</c_moreprompt>\n", 16);
	cprintf("<c_restrict>%d</c_restrict>\n", config.c_restrict);
	client_write("<c_site_location>", 17);	xml_strout(config.c_site_location);	client_write("</c_site_location>\n", 19);
	client_write("<c_sysadm>", 10);		xml_strout(config.c_sysadm);		client_write("</c_sysadm>\n", 12);
	cprintf("<c_setup_level>%d</c_setup_level>\n", config.c_setup_level);
	cprintf("<c_maxsessions>%d</c_maxsessions>\n", config.c_maxsessions);
	client_write("<c_ip_addr>", 11);	xml_strout(config.c_ip_addr);		client_write("</c_ip_addr>\n", 13);
	cprintf("<c_port_number>%d</c_port_number>\n", config.c_port_number);
	cprintf("<c_ep_expire_mode>%d</c_ep_expire_mode>\n", config.c_ep.expire_mode);
	cprintf("<c_ep_expire_value>%d</c_ep_expire_value>\n", config.c_ep.expire_value);
	cprintf("<c_userpurge>%d</c_userpurge>\n", config.c_userpurge);
	cprintf("<c_roompurge>%d</c_roompurge>\n", config.c_roompurge);
	client_write("<c_logpages>", 12);	xml_strout(config.c_logpages);		client_write("</c_logpages>\n", 14);
	cprintf("<c_createax>%d</c_createax>\n", config.c_createax);
	cprintf("<c_maxmsglen>%ld</c_maxmsglen>\n", config.c_maxmsglen);
	cprintf("<c_min_workers>%d</c_min_workers>\n", config.c_min_workers);
	cprintf("<c_max_workers>%d</c_max_workers>\n", config.c_max_workers);
	cprintf("<c_pop3_port>%d</c_pop3_port>\n", config.c_pop3_port);
	cprintf("<c_smtp_port>%d</c_smtp_port>\n", config.c_smtp_port);
	cprintf("<c_rfc822_strict_from>%d</c_rfc822_strict_from>\n", config.c_rfc822_strict_from);
	cprintf("<c_aide_zap>%d</c_aide_zap>\n", config.c_aide_zap);
	cprintf("<c_imap_port>%d</c_imap_port>\n", config.c_imap_port);
	cprintf("<c_net_freq>%ld</c_net_freq>\n", config.c_net_freq);
	cprintf("<c_disable_newu>%d</c_disable_newu>\n", config.c_disable_newu);
	cprintf("<c_enable_fulltext>%d</c_enable_fulltext>\n", config.c_enable_fulltext);
	client_write("<c_baseroom>", 12);	xml_strout(config.c_baseroom);		client_write("</c_baseroom>\n", 14);
	client_write("<c_aideroom>", 12);	xml_strout(config.c_aideroom);		client_write("</c_aideroom>\n", 14);
	cprintf("<c_purge_hour>%d</c_purge_hour>\n", config.c_purge_hour);
	cprintf("<c_mbxep_expire_mode>%d</c_mbxep_expire_mode>\n", config.c_mbxep.expire_mode);
	cprintf("<c_mbxep_expire_value>%d</c_mbxep_expire_value>\n", config.c_mbxep.expire_value);
	client_write("<c_ldap_host>", 13);	xml_strout(config.c_ldap_host);		client_write("</c_ldap_host>\n", 15);
	cprintf("<c_ldap_port>%d</c_ldap_port>\n", config.c_ldap_port);
	client_write("<c_ldap_base_dn>", 16);	xml_strout(config.c_ldap_base_dn);	client_write("</c_ldap_base_dn>\n", 18);
	client_write("<c_ldap_bind_dn>", 16);	xml_strout(config.c_ldap_bind_dn);	client_write("</c_ldap_bind_dn>\n", 18);
	client_write("<c_ldap_bind_pw>", 16);	xml_strout(config.c_ldap_bind_pw);	client_write("</c_ldap_bind_pw>\n", 18);
	cprintf("<c_msa_port>%d</c_msa_port>\n", config.c_msa_port);
	cprintf("<c_imaps_port>%d</c_imaps_port>\n", config.c_imaps_port);
	cprintf("<c_pop3s_port>%d</c_pop3s_port>\n", config.c_pop3s_port);
	cprintf("<c_smtps_port>%d</c_smtps_port>\n", config.c_smtps_port);
	cprintf("<c_auto_cull>%d</c_auto_cull>\n", config.c_auto_cull);
	cprintf("<c_instant_expunge>%d</c_instant_expunge>\n", config.c_instant_expunge);
	cprintf("<c_allow_spoofing>%d</c_allow_spoofing>\n", config.c_allow_spoofing);
	cprintf("<c_journal_email>%d</c_journal_email>\n", config.c_journal_email);
	cprintf("<c_journal_pubmsgs>%d</c_journal_pubmsgs>\n", config.c_journal_pubmsgs);
	client_write("<c_journal_dest>", 16);	xml_strout(config.c_journal_dest);	client_write("</c_journal_dest>\n", 18);
	client_write("<c_default_cal_zone>", 20);	xml_strout(config.c_default_cal_zone);	client_write("</c_default_cal_zone>\n", 22);
	cprintf("<c_pftcpdict_port>%d</c_pftcpdict_port>\n", config.c_pftcpdict_port);
	cprintf("<c_managesieve_port>%d</c_managesieve_port>\n", config.c_managesieve_port);
	cprintf("<c_auth_mode>%d</c_auth_mode>\n", config.c_auth_mode);
	client_write("<c_funambol_host>", 17);	xml_strout(config.c_funambol_host);	client_write("</c_funambol_host>\n", 19);
	cprintf("<c_funambol_port>%d</c_funambol_port>\n", config.c_funambol_port);
	client_write("<c_funambol_source>", 19);	xml_strout(config.c_funambol_source);	client_write("</c_funambol_source>\n", 21);
	client_write("<c_funambol_auth>", 17);	xml_strout(config.c_funambol_auth);	client_write("</c_funambol_auth>\n", 19);
	cprintf("<c_rbl_at_greeting>%d</c_rbl_at_greeting>\n", config.c_rbl_at_greeting);
	client_write("<c_master_user>", 15);	xml_strout(config.c_master_user);		client_write("</c_master_user>\n", 17);
	client_write("<c_master_pass>", 15);	xml_strout(config.c_master_pass);		client_write("</c_master_pass>\n", 17);
	client_write("<c_pager_program>", 17);	xml_strout(config.c_pager_program);		client_write("</c_pager_program>\n", 19);
	cprintf("<c_imap_keep_from>%d</c_imap_keep_from>\n", config.c_imap_keep_from);
	cprintf("<c_xmpp_c2s_port>%d</c_xmpp_c2s_port>\n", config.c_xmpp_c2s_port);
	cprintf("<c_xmpp_s2s_port>%d</c_xmpp_s2s_port>\n", config.c_xmpp_s2s_port);
	cprintf("<c_pop3_fetch>%ld</c_pop3_fetch>\n", config.c_pop3_fetch);
	cprintf("<c_pop3_fastest>%ld</c_pop3_fastest>\n", config.c_pop3_fastest);
	cprintf("<c_spam_flag_only>%d</c_spam_flag_only>\n", config.c_spam_flag_only);
	client_write("</config>\n", 10);
	
	/* Export the control file */
	get_control();
	client_write("<control>\n", 10);
	cprintf("<control_highest>%ld</control_highest>\n", CitControl.MMhighest);
	cprintf("<control_flags>%u</control_flags>\n", CitControl.MMflags);
	cprintf("<control_nextuser>%ld</control_nextuser>\n", CitControl.MMnextuser);
	cprintf("<control_nextroom>%ld</control_nextroom>\n", CitControl.MMnextroom);
	cprintf("<control_version>%d</control_version>\n", CitControl.version);
	client_write("</control>\n", 11);

	if (Ctx->kill_me != 1)	migr_export_users();
	if (Ctx->kill_me != 1)	migr_export_openids();
	if (Ctx->kill_me != 1)	migr_export_rooms();
	if (Ctx->kill_me != 1)	migr_export_floors();
	if (Ctx->kill_me != 1)	migr_export_visits();
	if (Ctx->kill_me != 1)	migr_export_messages();
	client_write("</citadel_migrate_data>\n", 24);
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
char *migr_chardata = NULL;
int migr_chardata_len = 0;
struct ctdluser usbuf;
struct ctdlroom qrbuf;
char openid_url[512];
long openid_usernum = 0;
char FRname[ROOMNAMELEN];
struct floor flbuf;
int floornum = 0;
struct visit vbuf;
struct MetaData smi;
long import_msgnum = 0;
char *decoded_msg = NULL;

/*
 * This callback stores up the data which appears in between tags.
 */
void migr_xml_chardata(void *data, const XML_Char *s, int len) {
	int old_len;
	int new_len;
	char *new_buffer;

	old_len = migr_chardata_len;
	new_len = old_len + len;
	new_buffer = realloc(migr_chardata, new_len + 1);
	if (new_buffer != NULL) {
		memcpy(&new_buffer[old_len], s, len);
		new_buffer[new_len] = 0;
		migr_chardata = new_buffer;
		migr_chardata_len = new_len;
	}
}


void migr_xml_start(void *data, const char *el, const char **attr) {
	int i;

	/*** GENERAL STUFF ***/

	/* Throw away any existing chardata */
	if (migr_chardata != NULL) {
		free(migr_chardata);
		migr_chardata = NULL;
		migr_chardata_len = 0;
	}

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
		CtdlLogPrintf(CTDL_ALERT, "Out-of-sequence tag <%s> detected.  Warning: ODD-DATA!\n");
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
	else if (!strcasecmp(el, "visit"))		memset(&vbuf, 0, sizeof (struct visit));

	else if (!strcasecmp(el, "message")) {
		memset(&smi, 0, sizeof (struct MetaData));
		import_msgnum = 0;
		if (decoded_msg != NULL) {
			free(decoded_msg);
			decoded_msg = NULL;
		}
	}

}


void migr_xml_end(void *data, const char *el) {
	char *ptr;
	int msgcount = 0;
	long msgnum = 0L;
	long *msglist = NULL;
	int msglist_alloc = 0;
	int i;
	int is_textual_seen = 0;
	long msglen;
	int len;

	/*** GENERAL STUFF ***/

	if (!strcasecmp(el, "citadel_migrate_data")) {
		--citadel_migrate_data;
		return;
	}

	if (citadel_migrate_data != 1) {
		CtdlLogPrintf(CTDL_ALERT, "Out-of-sequence tag <%s> detected.  Warning: ODD-DATA!\n");
		return;
	}

	if (migr_chardata == NULL) {		/* If NULL chardata, substitute an empty string instead. */
		migr_chardata = strdup("");
		migr_chardata_len = 0;
	}

	// CtdlLogPrintf(CTDL_DEBUG, "END TAG: <%s> DATA: <%s>\n", el, (migr_chardata_len ? migr_chardata : ""));

	/*** CONFIG ***/

	if (!strcasecmp(el, "config")) {
		config.c_enable_fulltext = 0;	/* always disable */
		put_config();
		CtdlLogPrintf(CTDL_INFO, "Completed import of server configuration\n");
	}

	else if (!strcasecmp(el, "c_nodename"))			safestrncpy(config.c_nodename, migr_chardata, sizeof config.c_nodename);
	else if (!strcasecmp(el, "c_fqdn"))			safestrncpy(config.c_fqdn, migr_chardata, sizeof config.c_fqdn);
	else if (!strcasecmp(el, "c_humannode"))		safestrncpy(config.c_humannode, migr_chardata, sizeof config.c_humannode);
	else if (!strcasecmp(el, "c_phonenum"))			safestrncpy(config.c_phonenum, migr_chardata, sizeof config.c_phonenum);
	else if (!strcasecmp(el, "c_ctdluid"))			config.c_ctdluid = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_creataide"))		config.c_creataide = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_sleeping"))			config.c_sleeping = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_initax"))			config.c_initax = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_regiscall"))		config.c_regiscall = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_twitdetect"))		config.c_twitdetect = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_twitroom"))			safestrncpy(config.c_twitroom, migr_chardata, sizeof config.c_twitroom);
	else if (!strcasecmp(el, "c_moreprompt"))		safestrncpy(config.c_moreprompt, migr_chardata, sizeof config.c_moreprompt);
	else if (!strcasecmp(el, "c_restrict"))			config.c_restrict = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_site_location"))		safestrncpy(config.c_site_location, migr_chardata, sizeof config.c_site_location);
	else if (!strcasecmp(el, "c_sysadm"))			safestrncpy(config.c_sysadm, migr_chardata, sizeof config.c_sysadm);
	else if (!strcasecmp(el, "c_setup_level"))		config.c_setup_level = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_maxsessions"))		config.c_maxsessions = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_ip_addr"))			safestrncpy(config.c_ip_addr, migr_chardata, sizeof config.c_ip_addr);
	else if (!strcasecmp(el, "c_port_number"))		config.c_port_number = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_ep_expire_mode"))		config.c_ep.expire_mode = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_ep_expire_value"))		config.c_ep.expire_value = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_userpurge"))		config.c_userpurge = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_roompurge"))		config.c_roompurge = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_logpages"))			safestrncpy(config.c_logpages, migr_chardata, sizeof config.c_logpages);
	else if (!strcasecmp(el, "c_createax"))			config.c_createax = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_maxmsglen"))		config.c_maxmsglen = atol(migr_chardata);
	else if (!strcasecmp(el, "c_min_workers"))		config.c_min_workers = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_max_workers"))		config.c_max_workers = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_pop3_port"))		config.c_pop3_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_smtp_port"))		config.c_smtp_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_rfc822_strict_from"))	config.c_rfc822_strict_from = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_aide_zap"))			config.c_aide_zap = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_imap_port"))		config.c_imap_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_net_freq"))			config.c_net_freq = atol(migr_chardata);
	else if (!strcasecmp(el, "c_disable_newu"))		config.c_disable_newu = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_enable_fulltext"))		config.c_enable_fulltext = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_baseroom"))			safestrncpy(config.c_baseroom, migr_chardata, sizeof config.c_baseroom);
	else if (!strcasecmp(el, "c_aideroom"))			safestrncpy(config.c_aideroom, migr_chardata, sizeof config.c_aideroom);
	else if (!strcasecmp(el, "c_purge_hour"))		config.c_purge_hour = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_mbxep_expire_mode"))	config.c_mbxep.expire_mode = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_mbxep_expire_value"))	config.c_mbxep.expire_value = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_ldap_host"))		safestrncpy(config.c_ldap_host, migr_chardata, sizeof config.c_ldap_host);
	else if (!strcasecmp(el, "c_ldap_port"))		config.c_ldap_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_ldap_base_dn"))		safestrncpy(config.c_ldap_base_dn, migr_chardata, sizeof config.c_ldap_base_dn);
	else if (!strcasecmp(el, "c_ldap_bind_dn"))		safestrncpy(config.c_ldap_bind_dn, migr_chardata, sizeof config.c_ldap_bind_dn);
	else if (!strcasecmp(el, "c_ldap_bind_pw"))		safestrncpy(config.c_ldap_bind_pw, migr_chardata, sizeof config.c_ldap_bind_pw);
	else if (!strcasecmp(el, "c_msa_port"))			config.c_msa_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_imaps_port"))		config.c_imaps_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_pop3s_port"))		config.c_pop3s_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_smtps_port"))		config.c_smtps_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_auto_cull"))		config.c_auto_cull = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_instant_expunge"))		config.c_instant_expunge = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_allow_spoofing"))		config.c_allow_spoofing = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_journal_email"))		config.c_journal_email = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_journal_pubmsgs"))		config.c_journal_pubmsgs = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_journal_dest"))		safestrncpy(config.c_journal_dest, migr_chardata, sizeof config.c_journal_dest);
	else if (!strcasecmp(el, "c_default_cal_zone"))		safestrncpy(config.c_default_cal_zone, migr_chardata, sizeof config.c_default_cal_zone);
	else if (!strcasecmp(el, "c_pftcpdict_port"))		config.c_pftcpdict_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_managesieve_port"))		config.c_managesieve_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_auth_mode"))		config.c_auth_mode = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_funambol_host"))		safestrncpy(config.c_funambol_host, migr_chardata, sizeof config.c_funambol_host);
	else if (!strcasecmp(el, "c_funambol_port"))		config.c_funambol_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_funambol_source"))		safestrncpy(config.c_funambol_source, migr_chardata, sizeof config.c_funambol_source);
	else if (!strcasecmp(el, "c_funambol_auth"))		safestrncpy(config.c_funambol_auth, migr_chardata, sizeof config.c_funambol_auth);
	else if (!strcasecmp(el, "c_rbl_at_greeting"))		config.c_rbl_at_greeting = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_master_user"))		safestrncpy(config.c_master_user, migr_chardata, sizeof config.c_master_user);
	else if (!strcasecmp(el, "c_master_pass"))		safestrncpy(config.c_master_pass, migr_chardata, sizeof config.c_master_pass);
	else if (!strcasecmp(el, "c_pager_program"))		safestrncpy(config.c_pager_program, migr_chardata, sizeof config.c_pager_program);
	else if (!strcasecmp(el, "c_imap_keep_from"))		config.c_imap_keep_from = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_xmpp_c2s_port"))		config.c_xmpp_c2s_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_xmpp_s2s_port"))		config.c_xmpp_s2s_port = atoi(migr_chardata);
	else if (!strcasecmp(el, "c_pop3_fetch"))		config.c_pop3_fetch = atol(migr_chardata);
	else if (!strcasecmp(el, "c_pop3_fastest"))		config.c_pop3_fastest = atol(migr_chardata);
	else if (!strcasecmp(el, "c_spam_flag_only"))		config.c_spam_flag_only = atoi(migr_chardata);

	/*** CONTROL ***/

	else if (!strcasecmp(el, "control_highest"))		CitControl.MMhighest = atol(migr_chardata);
	else if (!strcasecmp(el, "control_flags"))		CitControl.MMflags = atoi(migr_chardata);
	else if (!strcasecmp(el, "control_nextuser"))		CitControl.MMnextuser = atol(migr_chardata);
	else if (!strcasecmp(el, "control_nextroom"))		CitControl.MMnextroom = atol(migr_chardata);
	else if (!strcasecmp(el, "control_version"))		CitControl.version = atoi(migr_chardata);

	else if (!strcasecmp(el, "control")) {
		CitControl.MMfulltext = (-1L);	/* always flush */
		put_control();
		CtdlLogPrintf(CTDL_INFO, "Completed import of control record\n");
	}

	/*** USER ***/

	else if (!strcasecmp(el, "u_version"))			usbuf.version = atoi(migr_chardata);
	else if (!strcasecmp(el, "u_uid"))			usbuf.uid = atol(migr_chardata);
	else if (!strcasecmp(el, "u_password"))			safestrncpy(usbuf.password, migr_chardata, sizeof usbuf.password);
	else if (!strcasecmp(el, "u_flags"))			usbuf.flags = atoi(migr_chardata);
	else if (!strcasecmp(el, "u_timescalled"))		usbuf.timescalled = atol(migr_chardata);
	else if (!strcasecmp(el, "u_posted"))			usbuf.posted = atol(migr_chardata);
	else if (!strcasecmp(el, "u_axlevel"))			usbuf.axlevel = atoi(migr_chardata);
	else if (!strcasecmp(el, "u_usernum"))			usbuf.usernum = atol(migr_chardata);
	else if (!strcasecmp(el, "u_lastcall"))			usbuf.lastcall = atol(migr_chardata);
	else if (!strcasecmp(el, "u_USuserpurge"))		usbuf.USuserpurge = atoi(migr_chardata);
	else if (!strcasecmp(el, "u_fullname"))			safestrncpy(usbuf.fullname, migr_chardata, sizeof usbuf.fullname);

	else if (!strcasecmp(el, "user")) {
		CtdlPutUser(&usbuf);
		CtdlLogPrintf(CTDL_INFO, "Imported user: %s\n", usbuf.fullname);
	}

	/*** OPENID ***/

	else if (!strcasecmp(el, "oid_url"))			safestrncpy(openid_url, migr_chardata, sizeof openid_url);
	else if (!strcasecmp(el, "oid_usernum"))		openid_usernum = atol(migr_chardata);

	else if (!strcasecmp(el, "openid")) {			/* see serv_openid_rp.c for a description of the record format */
		char *oid_data;
		int oid_data_len;
		oid_data_len = sizeof(long) + strlen(openid_url) + 1;
		oid_data = malloc(oid_data_len);
		memcpy(oid_data, &openid_usernum, sizeof(long));
		memcpy(&oid_data[sizeof(long)], openid_url, strlen(openid_url) + 1);
		cdb_store(CDB_OPENID, openid_url, strlen(openid_url), oid_data, oid_data_len);
		free(oid_data);
		CtdlLogPrintf(CTDL_INFO, "Imported OpenID: %s (%ld)\n", openid_url, openid_usernum);
	}

	/*** ROOM ***/

	else if (!strcasecmp(el, "QRname"))			safestrncpy(qrbuf.QRname, migr_chardata, sizeof qrbuf.QRname);
	else if (!strcasecmp(el, "QRpasswd"))			safestrncpy(qrbuf.QRpasswd, migr_chardata, sizeof qrbuf.QRpasswd);
	else if (!strcasecmp(el, "QRroomaide"))			qrbuf.QRroomaide = atol(migr_chardata);
	else if (!strcasecmp(el, "QRhighest"))			qrbuf.QRhighest = atol(migr_chardata);
	else if (!strcasecmp(el, "QRgen"))			qrbuf.QRgen = atol(migr_chardata);
	else if (!strcasecmp(el, "QRflags"))			qrbuf.QRflags = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRdirname"))			safestrncpy(qrbuf.QRdirname, migr_chardata, sizeof qrbuf.QRdirname);
	else if (!strcasecmp(el, "QRinfo"))			qrbuf.QRinfo = atol(migr_chardata);
	else if (!strcasecmp(el, "QRfloor"))			qrbuf.QRfloor = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRmtime"))			qrbuf.QRmtime = atol(migr_chardata);
	else if (!strcasecmp(el, "QRexpire_mode"))		qrbuf.QRep.expire_mode = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRexpire_value"))		qrbuf.QRep.expire_value = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRnumber"))			qrbuf.QRnumber = atol(migr_chardata);
	else if (!strcasecmp(el, "QRorder"))			qrbuf.QRorder = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRflags2"))			qrbuf.QRflags2 = atoi(migr_chardata);
	else if (!strcasecmp(el, "QRdefaultview"))		qrbuf.QRdefaultview = atoi(migr_chardata);

	else if (!strcasecmp(el, "room")) {
		CtdlPutRoom(&qrbuf);
		CtdlLogPrintf(CTDL_INFO, "Imported room: %s\n", qrbuf.QRname);
	}

	/*** ROOM MESSAGE POINTERS ***/

	else if (!strcasecmp(el, "FRname"))			safestrncpy(FRname, migr_chardata, sizeof FRname);

	else if (!strcasecmp(el, "FRmsglist")) {
		if (!IsEmptyStr(FRname)) {
			msgcount = 0;
			msglist_alloc = 1000;
			msglist = malloc(sizeof(long) * msglist_alloc);

			CtdlLogPrintf(CTDL_DEBUG, "Message list for: %s\n", FRname);

			ptr = migr_chardata;
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
				CtdlSaveMsgPointersInRoom(FRname, msglist, msgcount, 0, NULL);
			}
			free(msglist);
			msglist = NULL;
			msglist_alloc = 0;
			CtdlLogPrintf(CTDL_DEBUG, "Imported %d messages.\n", msgcount);
			if (CtdlThreadCheckStop()) {
				return;
		}
	}

	/*** FLOORS ***/

	else if (!strcasecmp(el, "f_num"))			floornum = atoi(migr_chardata);
	else if (!strcasecmp(el, "f_flags"))			flbuf.f_flags = atoi(migr_chardata);
	else if (!strcasecmp(el, "f_name"))			safestrncpy(flbuf.f_name, migr_chardata, sizeof flbuf.f_name);
	else if (!strcasecmp(el, "f_ref_count"))		flbuf.f_ref_count = atoi(migr_chardata);
	else if (!strcasecmp(el, "f_ep_expire_mode"))		flbuf.f_ep.expire_mode = atoi(migr_chardata);
	else if (!strcasecmp(el, "f_ep_expire_value"))		flbuf.f_ep.expire_value = atoi(migr_chardata);

	else if (!strcasecmp(el, "floor")) {
		CtdlPutFloor(&flbuf, floornum);
		CtdlLogPrintf(CTDL_INFO, "Imported floor #%d (%s)\n", floornum, flbuf.f_name);
	}

	/*** VISITS ***/

	else if (!strcasecmp(el, "v_roomnum"))			vbuf.v_roomnum = atol(migr_chardata);
	else if (!strcasecmp(el, "v_roomgen"))			vbuf.v_roomgen = atol(migr_chardata);
	else if (!strcasecmp(el, "v_usernum"))			vbuf.v_usernum = atol(migr_chardata);

	else if (!strcasecmp(el, "v_seen")) {
		vbuf.v_lastseen = atol(migr_chardata);
		is_textual_seen = 0;
		for (i=0; migr_chardata[i]; ++i) if (!isdigit(migr_chardata[i])) is_textual_seen = 1;
		if (is_textual_seen)				safestrncpy(vbuf.v_seen, migr_chardata, sizeof vbuf.v_seen);
	}

	else if (!strcasecmp(el, "v_answered"))			safestrncpy(vbuf.v_answered, migr_chardata, sizeof vbuf.v_answered);
	else if (!strcasecmp(el, "v_flags"))			vbuf.v_flags = atoi(migr_chardata);
	else if (!strcasecmp(el, "v_view"))			vbuf.v_view = atoi(migr_chardata);

	else if (!strcasecmp(el, "visit")) {
		put_visit(&vbuf);
		CtdlLogPrintf(CTDL_INFO, "Imported visit: %ld/%ld/%ld\n", vbuf.v_roomnum, vbuf.v_roomgen, vbuf.v_usernum);
	}

	/*** MESSAGES ***/

	else if (!strcasecmp(el, "msg_msgnum"))			import_msgnum = atol(migr_chardata);
	else if (!strcasecmp(el, "msg_meta_refcount"))		smi.meta_refcount = atoi(migr_chardata);
	else if (!strcasecmp(el, "msg_meta_content_type"))	safestrncpy(smi.meta_content_type, migr_chardata, sizeof smi.meta_content_type);

	else if (!strcasecmp(el, "msg_text")) {
		if (decoded_msg != NULL) {
			free(decoded_msg);
		}
		len = strlen(migr_chardata);
		decoded_msg = malloc(len);
		msglen = CtdlDecodeBase64(decoded_msg, migr_chardata, len);
		cdb_store(CDB_MSGMAIN, &import_msgnum, sizeof(long), decoded_msg, msglen);
		free(decoded_msg);
		decoded_msg = NULL;
		PutMetaData(&smi);
		CtdlLogPrintf(CTDL_INFO, "Imported message #%ld, size=%ld, refcount=%d, content-type: %s\n",
			import_msgnum, msglen, smi.meta_refcount, smi.meta_content_type);
	}

	/*** MORE GENERAL STUFF ***/

	if (migr_chardata != NULL) {
		free(migr_chardata);
		migr_chardata = NULL;
		migr_chardata_len = 0;
	}
}




/*
 * Import begins here
 */
void migr_do_import(void) {
	char buf[SIZ];
	XML_Parser xp;
	int linelen;
	
	unbuffer_output();

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

	while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000")) {
		linelen = strlen(buf);
		strcpy(&buf[linelen++], "\n");

		if (CtdlThreadCheckStop())
			break;	// Should we break or return?
		
		if (buf[0] == '\0')
			continue;

		XML_Parse(xp, buf, linelen, 0);
	}

	XML_Parse(xp, "", 0, 1);
	XML_ParserFree(xp);
	
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
		CtdlRegisterProtoHook(cmd_migr, "ARTV", "Across-the-wire migration (legacy syntax)");
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}
