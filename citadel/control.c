/*
 * This module handles states which are global to the entire server.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <stdio.h>
#include <sys/file.h>
#include <libcitadel.h>

#include "ctdl_module.h"
#include "config.h"
#include "citserver.h"
#include "user_ops.h"

struct CitControl CitControl;
extern struct config config;
FILE *control_fp = NULL;
long control_highest_user = 0;


/*
 * lock_control  -  acquire a lock on the control record file.
 *                  This keeps multiple citservers from running concurrently.
 */
void lock_control(void)
{
#if defined(LOCK_EX) && defined(LOCK_NB)
	if (flock(fileno(control_fp), (LOCK_EX | LOCK_NB))) {
		syslog(LOG_EMERG, "citserver: unable to lock %s.\n", file_citadel_control);
		syslog(LOG_EMERG, "Is another citserver already running?\n");
		exit(CTDLEXIT_CONTROL);
	}
#endif
}

/*
 * callback to get highest room number when rebuilding control file
 */
void control_find_highest(struct ctdlroom *qrbuf, void *data)
{
	struct ctdlroom room;
	struct cdbdata *cdbfr;
	long *msglist;
	int num_msgs=0;
	int c;
	int room_fixed = 0;
	int message_fixed = 0;
	
	if (qrbuf->QRnumber > CitControl.MMnextroom)
	{
		CitControl.MMnextroom = qrbuf->QRnumber;
		room_fixed = 1;
	}
		
	CtdlGetRoom (&room, qrbuf->QRname);
	
	/* Load the message list */
	cdbfr = cdb_fetch(CDB_MSGLISTS, &room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
		msglist = (long *) cdbfr->ptr;
		num_msgs = cdbfr->len / sizeof(long);
	} else {
		return;	/* No messages at all?  No further action. */
	}

	if (num_msgs>0)
	{
		for (c=0; c<num_msgs; c++)
		{
			if (msglist[c] > CitControl.MMhighest)
			{
				CitControl.MMhighest = msglist[c];
				message_fixed = 1;
			}
		}
	}
	cdb_free(cdbfr);
	if (room_fixed)
		syslog(LOG_INFO, "Control record checking....Fixed room counter\n");
	if (message_fixed)
		syslog(LOG_INFO, "Control record checking....Fixed message count\n");
	return;
}


/*
 * Callback to get highest user number.
 */
 
void control_find_user (struct ctdluser *EachUser, void *out_data)
{
	int user_fixed = 0;
	
	if (EachUser->usernum > CitControl.MMnextuser)
	{
		CitControl.MMnextuser = EachUser->usernum;
		user_fixed = 1;
	}
	if(user_fixed)
		syslog(LOG_INFO, "Control record checking....Fixed user count\n");
}


/*
 * get_control  -  read the control record into memory.
 */
void get_control(void)
{
	static int already_have_control = 0;
	int rv = 0;

	/*
	 * If we already have the control record in memory, there's no point
	 * in reading it from disk again.
	 */
	if (already_have_control) return;

	/* Zero it out.  If the control record on disk is missing or short,
	 * the system functions with all control record fields initialized
	 * to zero.
	 */
	memset(&CitControl, 0, sizeof(struct CitControl));
	if (control_fp == NULL) {
		control_fp = fopen(file_citadel_control, "rb+");
		if (control_fp != NULL) {
			lock_control();
			rv = fchown(fileno(control_fp), config.c_ctdluid, -1);
			if (rv == -1)
				syslog(LOG_EMERG, "Failed to adjust ownership of: %s [%s]\n", 
				       file_citadel_control, strerror(errno));
			rv = fchmod(fileno(control_fp), S_IRUSR|S_IWUSR);
			if (rv == -1)
				syslog(LOG_EMERG, "Failed to adjust accessrights of: %s [%s]\n", 
				       file_citadel_control, strerror(errno));
		}
	}
	if (control_fp == NULL) {
		control_fp = fopen(file_citadel_control, "wb+");
		if (control_fp != NULL) {
			lock_control();
			memset(&CitControl, 0, sizeof(struct CitControl));

			rv = fchown(fileno(control_fp), config.c_ctdluid, -1);
			if (rv == -1)
				syslog(LOG_EMERG, "Failed to adjust ownership of: %s [%s]\n", 
				       file_citadel_control, strerror(errno));

			rv = fchmod(fileno(control_fp), S_IRUSR|S_IWUSR);
			if (rv == -1)
				syslog(LOG_EMERG, "Failed to adjust accessrights of: %s [%s]\n", 
				       file_citadel_control, strerror(errno));
			rv = fwrite(&CitControl, sizeof(struct CitControl), 1, control_fp);
			if (rv == -1)
				syslog(LOG_EMERG, "Failed to write: %s [%s]\n", 
				       file_citadel_control, strerror(errno));
			rewind(control_fp);
		}
	}
	if (control_fp == NULL) {
		syslog(LOG_ALERT, "ERROR opening %s: %s\n", file_citadel_control, strerror(errno));
		return;
	}

	rewind(control_fp);
	rv = fread(&CitControl, sizeof(struct CitControl), 1, control_fp);
	if (rv == -1)
		syslog(LOG_EMERG, "Failed to read Controlfile: %s [%s]\n", 
		       file_citadel_control, strerror(errno));
	already_have_control = 1;
	rv = chown(file_citadel_control, config.c_ctdluid, (-1));
	if (rv == -1)
		syslog(LOG_EMERG, "Failed to adjust ownership of: %s [%s]\n", 
		       file_citadel_control, strerror(errno));	
}

/*
 * put_control  -  write the control record to disk.
 */
void put_control(void)
{
	int rv = 0;

	if (control_fp != NULL) {
		rewind(control_fp);
		rv = fwrite(&CitControl, sizeof(struct CitControl), 1, control_fp);
		if (rv == -1)
			syslog(LOG_EMERG, "Failed to write: %s [%s]\n", 
			       file_citadel_control, strerror(errno));
		fflush(control_fp);
	}
}


/*
 * check_control   -  check the control record has sensible values for message, user and room numbers
 */
void check_control(void)
{
	syslog(LOG_INFO, "Checking/re-building control record\n");
	get_control();
	// Find highest room number and message number.
	CtdlForEachRoom(control_find_highest, NULL);
	ForEachUser(control_find_user, NULL);
	put_control();
}


/*
 * release_control - close our fd on exit
 */
void release_control(void)
{
	if (control_fp != NULL) {
		fclose(control_fp);
	}
	control_fp = NULL;
}

/*
 * get_new_message_number()  -  Obtain a new, unique ID to be used for a message.
 */
long get_new_message_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMhighest;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}


/*
 * CtdlGetCurrentMessageNumber()  -  Obtain the current highest message number in the system
 * This provides a quick way to initialise a variable that might be used to indicate
 * messages that should not be processed. EG. a new Sieve script will use this
 * to record determine that messages older than this should not be processed.
 */
long CtdlGetCurrentMessageNumber(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = CitControl.MMhighest;
	end_critical_section(S_CONTROL);
	return(retval);
}


/*
 * get_new_user_number()  -  Obtain a new, unique ID to be used for a user.
 */
long get_new_user_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMnextuser;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}



/*
 * get_new_room_number()  -  Obtain a new, unique ID to be used for a room.
 */
long get_new_room_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	get_control();
	retval = ++CitControl.MMnextroom;
	put_control();
	end_critical_section(S_CONTROL);
	return(retval);
}



/* 
 * Get or set global configuration options
 *
 * IF YOU ADD OR CHANGE FIELDS HERE, YOU *MUST* DOCUMENT YOUR CHANGES AT:
 * http://www.citadel.org/doku.php?id=documentation:applicationprotocol
 *
 */
void cmd_conf(char *argbuf)
{
	char cmd[16];
	char buf[256];
	int a;
	char *confptr;
	char confname[128];

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);
	if (!strcasecmp(cmd, "GET")) {
		cprintf("%d Configuration...\n", LISTING_FOLLOWS);
		cprintf("%s\n", config.c_nodename);
		cprintf("%s\n", config.c_fqdn);
		cprintf("%s\n", config.c_humannode);
		cprintf("%s\n", config.c_phonenum);
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
		cprintf("%d\n", config.c_maxsessions);
		cprintf("xxx\n"); /* placeholder -- field no longer in use */
		cprintf("%d\n", config.c_userpurge);
		cprintf("%d\n", config.c_roompurge);
		cprintf("%s\n", config.c_logpages);
		cprintf("%d\n", config.c_createax);
		cprintf("%ld\n", config.c_maxmsglen);
		cprintf("%d\n", config.c_min_workers);
		cprintf("%d\n", config.c_max_workers);
		cprintf("%d\n", config.c_pop3_port);
		cprintf("%d\n", config.c_smtp_port);
		cprintf("%d\n", config.c_rfc822_strict_from);
		cprintf("%d\n", config.c_aide_zap);
		cprintf("%d\n", config.c_imap_port);
		cprintf("%ld\n", config.c_net_freq);
		cprintf("%d\n", config.c_disable_newu);
		cprintf("1\n");	/* niu */
		cprintf("%d\n", config.c_purge_hour);
#ifdef HAVE_LDAP
		cprintf("%s\n", config.c_ldap_host);
		cprintf("%d\n", config.c_ldap_port);
		cprintf("%s\n", config.c_ldap_base_dn);
		cprintf("%s\n", config.c_ldap_bind_dn);
		cprintf("%s\n", config.c_ldap_bind_pw);
#else
		cprintf("\n");
		cprintf("0\n");
		cprintf("\n");
		cprintf("\n");
		cprintf("\n");
#endif
		cprintf("%s\n", config.c_ip_addr);
		cprintf("%d\n", config.c_msa_port);
		cprintf("%d\n", config.c_imaps_port);
		cprintf("%d\n", config.c_pop3s_port);
		cprintf("%d\n", config.c_smtps_port);
		cprintf("%d\n", config.c_enable_fulltext);
		cprintf("%d\n", config.c_auto_cull);
		cprintf("1\n");
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
		cprintf("%s\n", config.c_master_user);
		cprintf("%s\n", config.c_master_pass);
		cprintf("%s\n", config.c_pager_program);
		cprintf("%d\n", config.c_imap_keep_from);
		cprintf("%d\n", config.c_xmpp_c2s_port);
		cprintf("%d\n", config.c_xmpp_s2s_port);
		cprintf("%ld\n", config.c_pop3_fetch);
		cprintf("%ld\n", config.c_pop3_fastest);
		cprintf("%d\n", config.c_spam_flag_only);
		cprintf("%d\n", config.c_guest_logins);
		cprintf("%d\n", config.c_port_number);
		cprintf("%d\n", config.c_ctdluid);
		cprintf("000\n");
	}

	else if (!strcasecmp(cmd, "SET")) {
		unbuffer_output();
		cprintf("%d Send configuration...\n", SEND_LISTING);
		a = 0;
		while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000")) {
			switch (a) {
			case 0:
				configlen.c_nodename = safestrncpy(config.c_nodename, buf,
								   sizeof config.c_nodename);
				break;
			case 1:
				configlen.c_fqdn = safestrncpy(config.c_fqdn, buf,
							       sizeof config.c_fqdn);
				break;
			case 2:
				configlen.c_humannode = safestrncpy(config.c_humannode, buf,
								    sizeof config.c_humannode);
				break;
			case 3:
				configlen.c_phonenum = safestrncpy(config.c_phonenum, buf,
								   sizeof config.c_phonenum);
				break;
			case 4:
				config.c_creataide = atoi(buf);
				break;
			case 5:
				config.c_sleeping = atoi(buf);
				break;
			case 6:
				config.c_initax = atoi(buf);
				if (config.c_initax < 1)
					config.c_initax = 1;
				if (config.c_initax > 6)
					config.c_initax = 6;
				break;
			case 7:
				config.c_regiscall = atoi(buf);
				if (config.c_regiscall != 0)
					config.c_regiscall = 1;
				break;
			case 8:
				config.c_twitdetect = atoi(buf);
				if (config.c_twitdetect != 0)
					config.c_twitdetect = 1;
				break;
			case 9:
				configlen.c_twitroom = safestrncpy(config.c_twitroom, buf,
								   sizeof config.c_twitroom);
				break;
			case 10:
				configlen.c_moreprompt = safestrncpy(config.c_moreprompt, buf,
								     sizeof config.c_moreprompt);
				break;
			case 11:
				config.c_restrict = atoi(buf);
				if (config.c_restrict != 0)
					config.c_restrict = 1;
				break;
			case 12:
				configlen.c_site_location = safestrncpy(
					config.c_site_location, buf,
					sizeof config.c_site_location);
				break;
			case 13:
				configlen.c_sysadm = safestrncpy(config.c_sysadm, buf,
								 sizeof config.c_sysadm);
				break;
			case 14:
				config.c_maxsessions = atoi(buf);
				if (config.c_maxsessions < 0)
					config.c_maxsessions = 0;
				break;
			case 15:
				/* placeholder -- field no longer in use */
				break;
			case 16:
				config.c_userpurge = atoi(buf);
				break;
			case 17:
				config.c_roompurge = atoi(buf);
				break;
			case 18:
				configlen.c_logpages = safestrncpy(config.c_logpages, buf,
								   sizeof config.c_logpages);
				break;
			case 19:
				config.c_createax = atoi(buf);
				if (config.c_createax < 1)
					config.c_createax = 1;
				if (config.c_createax > 6)
					config.c_createax = 6;
				break;
			case 20:
				if (atoi(buf) >= 8192)
					config.c_maxmsglen = atoi(buf);
				break;
			case 21:
				if (atoi(buf) >= 2)
					config.c_min_workers = atoi(buf);
			case 22:
				if (atoi(buf) >= config.c_min_workers)
					config.c_max_workers = atoi(buf);
			case 23:
				config.c_pop3_port = atoi(buf);
				break;
			case 24:
				config.c_smtp_port = atoi(buf);
				break;
			case 25:
				config.c_rfc822_strict_from = atoi(buf);
				break;
			case 26:
				config.c_aide_zap = atoi(buf);
				if (config.c_aide_zap != 0)
					config.c_aide_zap = 1;
				break;
			case 27:
				config.c_imap_port = atoi(buf);
				break;
			case 28:
				config.c_net_freq = atol(buf);
				break;
			case 29:
				config.c_disable_newu = atoi(buf);
				if (config.c_disable_newu != 0)
					config.c_disable_newu = 1;
				break;
			case 30:
				/* niu */
				break;
			case 31:
				if ((config.c_purge_hour >= 0)
				    && (config.c_purge_hour <= 23)) {
					config.c_purge_hour = atoi(buf);
				}
				break;
#ifdef HAVE_LDAP
			case 32:
				configlen.c_ldap_host = safestrncpy(config.c_ldap_host, buf,
								    sizeof config.c_ldap_host);
				break;
			case 33:
				config.c_ldap_port = atoi(buf);
				break;
			case 34:
				configlen.c_ldap_base_dn = safestrncpy(config.c_ldap_base_dn, buf,
								       sizeof config.c_ldap_base_dn);
				break;
			case 35:
				configlen.c_ldap_bind_dn = safestrncpy(config.c_ldap_bind_dn, buf,
								       sizeof config.c_ldap_bind_dn);
				break;
			case 36:
				configlen.c_ldap_bind_pw = safestrncpy(config.c_ldap_bind_pw, buf,
								       sizeof config.c_ldap_bind_pw);
				break;
#endif
			case 37:
				configlen.c_ip_addr = safestrncpy(config.c_ip_addr, buf,
								  sizeof config.c_ip_addr);
			case 38:
				config.c_msa_port = atoi(buf);
				break;
			case 39:
				config.c_imaps_port = atoi(buf);
				break;
			case 40:
				config.c_pop3s_port = atoi(buf);
				break;
			case 41:
				config.c_smtps_port = atoi(buf);
				break;
			case 42:
				config.c_enable_fulltext = atoi(buf);
				break;
			case 43:
				config.c_auto_cull = atoi(buf);
				break;
			case 44:
				/* niu */
				break;
			case 45:
				config.c_allow_spoofing = atoi(buf);
				break;
			case 46:
				config.c_journal_email = atoi(buf);
				break;
			case 47:
				config.c_journal_pubmsgs = atoi(buf);
				break;
			case 48:
				configlen.c_journal_dest = safestrncpy(config.c_journal_dest, buf,
								       sizeof config.c_journal_dest);
			case 49:
				configlen.c_default_cal_zone = safestrncpy(
					config.c_default_cal_zone, buf,
					sizeof config.c_default_cal_zone);
				break;
			case 50:
				config.c_pftcpdict_port = atoi(buf);
				break;
			case 51:
				config.c_managesieve_port = atoi(buf);
				break;
			case 52:
				config.c_auth_mode = atoi(buf);
			case 53:
				configlen.c_funambol_host = safestrncpy(
					config.c_funambol_host, buf,
					sizeof config.c_funambol_host);
				break;
			case 54:
				config.c_funambol_port = atoi(buf);
				break;
			case 55:
				configlen.c_funambol_source = safestrncpy(
					config.c_funambol_source, buf, 
					sizeof config.c_funambol_source);
				break;
			case 56:
				configlen.c_funambol_auth = safestrncpy(
					config.c_funambol_auth, buf,
					sizeof config.c_funambol_auth);
				break;
			case 57:
				config.c_rbl_at_greeting = atoi(buf);
				break;
			case 58:
				configlen.c_master_user = safestrncpy(
					config.c_master_user,
					buf, sizeof config.c_master_user);
				break;
			case 59:
				configlen.c_master_pass = safestrncpy(
					config.c_master_pass, buf, sizeof config.c_master_pass);
				break;
			case 60:
				configlen.c_pager_program = safestrncpy(
					config.c_pager_program,	buf, sizeof config.c_pager_program);
				break;
			case 61:
				config.c_imap_keep_from = atoi(buf);
				break;
			case 62:
				config.c_xmpp_c2s_port = atoi(buf);
				break;
			case 63:
				config.c_xmpp_s2s_port = atoi(buf);
				break;
			case 64:
				config.c_pop3_fetch = atol(buf);
				break;
			case 65:
				config.c_pop3_fastest = atol(buf);
				break;
			case 66:
				config.c_spam_flag_only = atoi(buf);
				break;
			case 67:
				config.c_guest_logins = atoi(buf);
				break;
			case 68:
				config.c_port_number = atoi(buf);
				break;
			case 69:
				config.c_ctdluid = atoi(buf);
				break;
			}
			++a;
		}
		put_config();
		snprintf(buf, sizeof buf,
			"The global system configuration has been edited by %s.\n",
			 (CC->logged_in ? CC->curr_user : "an administrator")
		);
		CtdlAideMessage(buf,"Citadel Configuration Manager Message");

		if (!IsEmptyStr(config.c_logpages))
			CtdlCreateRoom(config.c_logpages, 3, "", 0, 1, 1, VIEW_BBS);

		/* If full text indexing has been disabled, invalidate the
		 * index so it doesn't try to use it later.
		 */
		if (config.c_enable_fulltext == 0) {
			CitControl.fulltext_wordbreaker = 0;
			put_control();
		}
	}

	else if (!strcasecmp(cmd, "GETSYS")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		confptr = CtdlGetSysConfig(confname);
		if (confptr != NULL) {
			long len; 

			len = strlen(confptr);
			cprintf("%d %s\n", LISTING_FOLLOWS, confname);
			client_write(confptr, len);
			if ((len > 0) && (confptr[len - 1] != 10))
				client_write("\n", 1);
			cprintf("000\n");
			free(confptr);
		} else {
			cprintf("%d No such configuration.\n",
				ERROR + ILLEGAL_VALUE);
		}
	}

	else if (!strcasecmp(cmd, "PUTSYS")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		unbuffer_output();
		cprintf("%d %s\n", SEND_LISTING, confname);
		confptr = CtdlReadMessageBody(HKEY("000"), config.c_maxmsglen, NULL, 0, 0);
		CtdlPutSysConfig(confname, confptr);
		free(confptr);
	}

	else {
		cprintf("%d Illegal option(s) specified.\n",
			ERROR + ILLEGAL_VALUE);
	}
}

typedef struct __ConfType {
	ConstStr Name;
	long Type;
}ConfType;

ConfType CfgNames[] = {
	{ {HKEY("localhost") },    0},
	{ {HKEY("directory") },    0},
	{ {HKEY("smarthost") },    2},
	{ {HKEY("fallbackhost") }, 2},
	{ {HKEY("rbl") },          3},
	{ {HKEY("spamassassin") }, 3},
	{ {HKEY("masqdomain") },   1},
	{ {HKEY("clamav") },       3},
	{ {HKEY("notify") },       3},
	{ {NULL, 0}, 0}
};

HashList *CfgNameHash = NULL;
void cmd_gvdn(char *argbuf)
{
	const ConfType *pCfg;
	char *confptr;
	long min = atol(argbuf);
	const char *Pos = NULL;
	const char *PPos = NULL;
	const char *HKey;
	long HKLen;
	StrBuf *Line;
	StrBuf *Config;
	StrBuf *Cfg;
	StrBuf *CfgToken;
	HashList *List;
	HashPos *It;
	void *vptr;
	
	List = NewHash(1, NULL);
	Cfg = NewStrBufPlain(config.c_fqdn, -1);
	Put(List, SKEY(Cfg), Cfg, HFreeStrBuf);
	Cfg = NULL;

	confptr = CtdlGetSysConfig(INTERNETCFG);
	Config = NewStrBufPlain(confptr, -1);
	free(confptr);

	Line = NewStrBufPlain(NULL, StrLength(Config));
	CfgToken = NewStrBufPlain(NULL, StrLength(Config));
	while (StrBufSipLine(Line, Config, &Pos))
	{
		if (Cfg == NULL)
			Cfg = NewStrBufPlain(NULL, StrLength(Line));
		PPos = NULL;
		StrBufExtract_NextToken(Cfg, Line, &PPos, '|');
		StrBufExtract_NextToken(CfgToken, Line, &PPos, '|');
		if (GetHash(CfgNameHash, SKEY(CfgToken), &vptr) &&
		    (vptr != NULL))
		{
			pCfg = (ConfType *) vptr;
			if (pCfg->Type <= min)
			{
				Put(List, SKEY(Cfg), Cfg, HFreeStrBuf);
				Cfg = NULL;
			}
		}
	}

	cprintf("%d Valid Domains\n", LISTING_FOLLOWS);
	It = GetNewHashPos(List, 1);
	while (GetNextHashPos(List, It, &HKLen, &HKey, &vptr))
	{
		cputbuf(vptr);
		cprintf("\n");
	}
	cprintf("000\n");

	DeleteHashPos(&It);
	DeleteHash(&List);
	FreeStrBuf(&Cfg);
	FreeStrBuf(&Line);
	FreeStrBuf(&CfgToken);
	FreeStrBuf(&Config);
}

/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

void control_cleanup(void)
{
	DeleteHash(&CfgNameHash);
}
CTDL_MODULE_INIT(control)
{
	if (!threading) {
		int i;

		CfgNameHash = NewHash(1, NULL);
		for (i = 0; CfgNames[i].Name.Key != NULL; i++)
			Put(CfgNameHash, CKEY(CfgNames[i].Name), &CfgNames[i], reference_free_handler);

		CtdlRegisterProtoHook(cmd_gvdn, "GVDN", "get valid domain names");
		CtdlRegisterProtoHook(cmd_conf, "CONF", "get/set system configuration");
		CtdlRegisterCleanupHook(control_cleanup);

	}
	/* return our id for the Log */
	return "control";
}
