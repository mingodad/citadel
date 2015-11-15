/*
 * This module handles states which are global to the entire server.
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

#include <stdio.h>
#include <sys/file.h>
#include <libcitadel.h>

#include "ctdl_module.h"
#include "config.h"
#include "citserver.h"
#include "user_ops.h"

long control_highest_user = 0;

/*
 * This is the control record for the message base... 
 */
struct legacy_ctrl_format {
	long MMhighest;			/* highest message number in file   */
	unsigned MMflags;		/* Global system flags              */
	long MMnextuser;		/* highest user number on system    */
	long MMnextroom;		/* highest room number on system    */
	int MM_hosted_upgrade_level;	/* Server-hosted upgrade level      */
	int MM_fulltext_wordbreaker;	/* ID of wordbreaker in use         */
	long MMfulltext;		/* highest message number indexed   */
	int MMdbversion;		/* Version of Berkeley DB used on previous server run */
};



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
	
	if (qrbuf->QRnumber > CtdlGetConfigLong("MMnextroom"))
	{
		CtdlSetConfigLong("MMnextroom", qrbuf->QRnumber);
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
			if (msglist[c] > CtdlGetConfigLong("MMhighest"))
			{
				CtdlSetConfigLong("MMhighest", msglist[c]);
				message_fixed = 1;
			}
		}
	}
	cdb_free(cdbfr);
	if (room_fixed) {
		syslog(LOG_INFO, "Control record checking....Fixed room counter\n");
	}
	if (message_fixed) {
		syslog(LOG_INFO, "Control record checking....Fixed message count\n");
	}
	return;
}


/*
 * Callback to get highest user number.
 */
 
void control_find_user (struct ctdluser *EachUser, void *out_data)
{
	int user_fixed = 0;
	
	if (EachUser->usernum > CtdlGetConfigLong("MMnextuser"))
	{
		CtdlSetConfigLong("MMnextuser", EachUser->usernum);
		user_fixed = 1;
	}
	if(user_fixed)
		syslog(LOG_INFO, "Control record checking....Fixed user count\n");
}


/*
 * If we have a legacy format control record on disk, import it.
 */
void migrate_legacy_control_record(void)
{
	FILE *fp = NULL;
	struct legacy_ctrl_format c;
	memset(&c, 0, sizeof(c));

	fp = fopen(file_citadel_control, "rb+");
	if (fp != NULL) {
		syslog(LOG_INFO, "Legacy format control record found -- importing to db");
		fread(&c, sizeof(struct legacy_ctrl_format), 1, fp);
		
		CtdlSetConfigLong(	"MMhighest",			c.MMhighest);
		CtdlSetConfigInt(	"MMflags",			c.MMflags);
		CtdlSetConfigLong(	"MMnextuser",			c.MMnextuser);
		CtdlSetConfigLong(	"MMnextroom",			c.MMnextroom);
		CtdlSetConfigInt(	"MM_hosted_upgrade_level",	c.MM_hosted_upgrade_level);
		CtdlSetConfigInt(	"MM_fulltext_wordbreaker",	c.MM_fulltext_wordbreaker);
		CtdlSetConfigLong(	"MMfulltext",			c.MMfulltext);

		fclose(fp);
		if (unlink(file_citadel_control) != 0) {
			fprintf(stderr, "Unable to remove legacy control record %s after migrating it.\n", file_citadel_control);
			fprintf(stderr, "Exiting to prevent data corruption.\n");
			exit(CTDLEXIT_CONFIG);
		}
	}
}


/*
 * check_control   -  check the control record has sensible values for message, user and room numbers
 */
void check_control(void)
{
	syslog(LOG_INFO, "Sanity checking the recorded highest message, user, and room numbers\n");
	CtdlForEachRoom(control_find_highest, NULL);
	ForEachUser(control_find_user, NULL);
}



/*
 * get_new_message_number()  -  Obtain a new, unique ID to be used for a message.
 */
long get_new_message_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	retval = CtdlGetConfigLong("MMhighest");
	++retval;
	CtdlSetConfigLong("MMhighest", retval);
	end_critical_section(S_CONTROL);
	return(retval);
}


/*
 * CtdlGetCurrentMessageNumber()  -  Obtain the current highest message number in the system
 * This provides a quick way to initialise a variable that might be used to indicate
 * messages that should not be processed. EG. a new Sieve script will use this
 * to record determine that messages older than this should not be processed.
 *
 * (Why is this function here?  Can't we just go straight to the config variable it fetches?)
 */
long CtdlGetCurrentMessageNumber(void)
{
	return CtdlGetConfigLong("MMhighest");
}


/*
 * get_new_user_number()  -  Obtain a new, unique ID to be used for a user.
 */
long get_new_user_number(void)
{
	long retval = 0L;
	begin_critical_section(S_CONTROL);
	retval = CtdlGetConfigLong("MMnextuser");
	++retval;
	CtdlSetConfigLong("MMnextuser", retval);
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
	retval = CtdlGetConfigLong("MMnextroom");
	++retval;
	CtdlSetConfigLong("MMnextroom", retval);
	end_critical_section(S_CONTROL);
	return(retval);
}



/*
 * Helper function for cmd_conf() to handle boolean values
 */
int confbool(char *v)
{
	if (IsEmptyStr(v)) return(0);
	if (atoi(v) != 0) return(1);
	return(0);
}


/* 
 * Get or set global configuration options
 *
 * IF YOU ADD OR CHANGE FIELDS HERE, YOU *MUST* DOCUMENT YOUR CHANGES AT:
 * http://www.citadel.org/doku.php/documentation:appproto:system_config
 *
 */
void cmd_conf(char *argbuf)
{
	char cmd[16];
	char buf[1024];
	int a, i;
	long ii;
	char *confptr;
	char confname[128];

	if (CtdlAccessCheck(ac_aide)) return;

	extract_token(cmd, argbuf, 0, '|', sizeof cmd);

	// CONF GET - retrieve system configuration in legacy format (deprecated)
	if (!strcasecmp(cmd, "GET")) {
		cprintf("%d Configuration...\n", LISTING_FOLLOWS);
		cprintf("%s\n",		CtdlGetConfigStr("c_nodename"));
		cprintf("%s\n",		CtdlGetConfigStr("c_fqdn"));
		cprintf("%s\n",		CtdlGetConfigStr("c_humannode"));
		cprintf("xxx\n"); /* placeholder -- field no longer in use */
		cprintf("%d\n",		CtdlGetConfigInt("c_creataide"));
		cprintf("%d\n",		CtdlGetConfigInt("c_sleeping"));
		cprintf("%d\n",		CtdlGetConfigInt("c_initax"));
		cprintf("%d\n",		CtdlGetConfigInt("c_regiscall"));
		cprintf("%d\n",		CtdlGetConfigInt("c_twitdetect"));
		cprintf("%s\n",		CtdlGetConfigStr("c_twitroom"));
		cprintf("%s\n",		CtdlGetConfigStr("c_moreprompt"));
		cprintf("%d\n",		CtdlGetConfigInt("c_restrict"));
		cprintf("%s\n",		CtdlGetConfigStr("c_site_location"));
		cprintf("%s\n",		CtdlGetConfigStr("c_sysadm"));
		cprintf("%d\n",		CtdlGetConfigInt("c_maxsessions"));
		cprintf("xxx\n"); /* placeholder -- field no longer in use */
		cprintf("%d\n",		CtdlGetConfigInt("c_userpurge"));
		cprintf("%d\n",		CtdlGetConfigInt("c_roompurge"));
		cprintf("%s\n",		CtdlGetConfigStr("c_logpages"));
		cprintf("%d\n",		CtdlGetConfigInt("c_createax"));
		cprintf("%ld\n",	CtdlGetConfigLong("c_maxmsglen"));
		cprintf("%d\n",		CtdlGetConfigInt("c_min_workers"));
		cprintf("%d\n",		CtdlGetConfigInt("c_max_workers"));
		cprintf("%d\n",		CtdlGetConfigInt("c_pop3_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_smtp_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_rfc822_strict_from"));
		cprintf("%d\n",		CtdlGetConfigInt("c_aide_zap"));
		cprintf("%d\n",		CtdlGetConfigInt("c_imap_port"));
		cprintf("%ld\n",	CtdlGetConfigLong("c_net_freq"));
		cprintf("%d\n",		CtdlGetConfigInt("c_disable_newu"));
		cprintf("1\n");	/* niu */
		cprintf("%d\n",		CtdlGetConfigInt("c_purge_hour"));
#ifdef HAVE_LDAP
		cprintf("%s\n",		CtdlGetConfigStr("c_ldap_host"));
		cprintf("%d\n",		CtdlGetConfigInt("c_ldap_port"));
		cprintf("%s\n",		CtdlGetConfigStr("c_ldap_base_dn"));
		cprintf("%s\n",		CtdlGetConfigStr("c_ldap_bind_dn"));
		cprintf("%s\n",		CtdlGetConfigStr("c_ldap_bind_pw"));
#else
		cprintf("\n");
		cprintf("0\n");
		cprintf("\n");
		cprintf("\n");
		cprintf("\n");
#endif
		cprintf("%s\n",		CtdlGetConfigStr("c_ip_addr"));
		cprintf("%d\n",		CtdlGetConfigInt("c_msa_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_imaps_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_pop3s_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_smtps_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_enable_fulltext"));
		cprintf("%d\n",		CtdlGetConfigInt("c_auto_cull"));
		cprintf("1\n");
		cprintf("%d\n",		CtdlGetConfigInt("c_allow_spoofing"));
		cprintf("%d\n",		CtdlGetConfigInt("c_journal_email"));
		cprintf("%d\n",		CtdlGetConfigInt("c_journal_pubmsgs"));
		cprintf("%s\n",		CtdlGetConfigStr("c_journal_dest"));
		cprintf("%s\n",		CtdlGetConfigStr("c_default_cal_zone"));
		cprintf("%d\n",		CtdlGetConfigInt("c_pftcpdict_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_managesieve_port"));
	        cprintf("%d\n",		CtdlGetConfigInt("c_auth_mode"));
	        cprintf("%s\n",		CtdlGetConfigStr("c_funambol_host"));
	        cprintf("%d\n",		CtdlGetConfigInt("c_funambol_port"));
	        cprintf("%s\n",		CtdlGetConfigStr("c_funambol_source"));
	        cprintf("%s\n",		CtdlGetConfigStr("c_funambol_auth"));
		cprintf("%d\n",		CtdlGetConfigInt("c_rbl_at_greeting"));
		cprintf("%s\n",		CtdlGetConfigStr("c_master_user"));
		cprintf("%s\n",		CtdlGetConfigStr("c_master_pass"));
		cprintf("%s\n",		CtdlGetConfigStr("c_pager_program"));
		cprintf("%d\n",		CtdlGetConfigInt("c_imap_keep_from"));
		cprintf("%d\n",		CtdlGetConfigInt("c_xmpp_c2s_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_xmpp_s2s_port"));
		cprintf("%ld\n",	CtdlGetConfigLong("c_pop3_fetch"));
		cprintf("%ld\n",	CtdlGetConfigLong("c_pop3_fastest"));
		cprintf("%d\n",		CtdlGetConfigInt("c_spam_flag_only"));
		cprintf("%d\n",		CtdlGetConfigInt("c_guest_logins"));
		cprintf("%d\n",		CtdlGetConfigInt("c_port_number"));
		cprintf("%d\n",		ctdluid);
		cprintf("%d\n",		CtdlGetConfigInt("c_nntp_port"));
		cprintf("%d\n",		CtdlGetConfigInt("c_nntps_port"));
		cprintf("000\n");
	}

	// CONF SET - set system configuration in legacy format (really deprecated)
	else if (!strcasecmp(cmd, "SET")) {
		unbuffer_output();
		cprintf("%d Send configuration...\n", SEND_LISTING);
		a = 0;
		while (client_getln(buf, sizeof buf) >= 0 && strcmp(buf, "000")) {
			switch (a) {
			case 0:
				CtdlSetConfigStr("c_nodename", buf);
				break;
			case 1:
				CtdlSetConfigStr("c_fqdn", buf);
				break;
			case 2:
				CtdlSetConfigStr("c_humannode", buf);
				break;
			case 3:
				/* placeholder -- field no longer in use */
				break;
			case 4:
				CtdlSetConfigInt("c_creataide", atoi(buf));
				break;
			case 5:
				CtdlSetConfigInt("c_sleeping", atoi(buf));
				break;
			case 6:
				i = atoi(buf);
				if (i < 1) i = 1;
				if (i > 6) i = 6;
				CtdlSetConfigInt("c_initax", i);
				break;
			case 7:
				CtdlSetConfigInt("c_regiscall", confbool(buf));
				break;
			case 8:
				CtdlSetConfigInt("c_twitdetect", confbool(buf));
				break;
			case 9:
				CtdlSetConfigStr("c_twitroom", buf);
				break;
			case 10:
				CtdlSetConfigStr("c_moreprompt", buf);
				break;
			case 11:
				CtdlSetConfigInt("c_restrict", confbool(buf));
				break;
			case 12:
				CtdlSetConfigInt("c_site_location", confbool(buf));
				break;
			case 13:
				CtdlSetConfigInt("c_sysadm", confbool(buf));
				break;
			case 14:
				i = atoi(buf);
				if (i < 0) i = 0;
				CtdlSetConfigInt("c_maxsessions", i);
				break;
			case 15:
				/* placeholder -- field no longer in use */
				break;
			case 16:
				CtdlSetConfigInt("c_userpurge", atoi(buf));
				break;
			case 17:
				CtdlSetConfigInt("c_roompurge", atoi(buf));
				break;
			case 18:
				CtdlSetConfigStr("c_logpages", buf);
				break;
			case 19:
				i = atoi(buf);
				if (i < 1) i = 1;
				if (i > 6) i = 6;
				CtdlSetConfigInt("c_createax", i);
				break;
			case 20:
				ii = atol(buf);
				if (ii >= 8192) {
					CtdlSetConfigLong("c_maxmsglen", ii);
				}
				break;
			case 21:
				i = atoi(buf);
				if (i >= 3) {					// minimum value
					CtdlSetConfigInt("c_min_workers", i);
				}
				break;
			case 22:
				i = atoi(buf);
				if (i >= CtdlGetConfigInt("c_min_workers")) {	// max must be >= min
					CtdlSetConfigInt("c_max_workers", i);
				}
				break;
			case 23:
				CtdlSetConfigInt("c_pop3_port", atoi(buf));
				break;
			case 24:
				CtdlSetConfigInt("c_smtp_port", atoi(buf));
				break;
			case 25:
				CtdlSetConfigInt("c_rfc822_strict_from", atoi(buf));
				break;
			case 26:
				CtdlSetConfigInt("c_aide_zap", confbool(buf));
				break;
			case 27:
				CtdlSetConfigInt("c_imap_port", atoi(buf));
				break;
			case 28:
				CtdlSetConfigLong("c_net_freq", atol(buf));
				break;
			case 29:
				CtdlSetConfigInt("c_disable_newu", confbool(buf));
				break;
			case 30:
				/* niu */
				break;
			case 31:
				i = atoi(buf);
				if ((i >= 0) && (i <= 23)) {
					CtdlSetConfigInt("c_purge_hour", i);
				}
				break;
			case 32:
				CtdlSetConfigStr("c_ldap_host", buf);
				break;
			case 33:
				CtdlSetConfigInt("c_ldap_port", atoi(buf));
				break;
			case 34:
				CtdlSetConfigStr("c_ldap_base_dn", buf);
				break;
			case 35:
				CtdlSetConfigStr("c_ldap_bind_dn", buf);
				break;
			case 36:
				CtdlSetConfigStr("c_ldap_bind_pw", buf);
				break;
			case 37:
				CtdlSetConfigStr("c_ip_addr", buf);
				break;
			case 38:
				CtdlSetConfigInt("c_msa_port", atoi(buf));
				break;
			case 39:
				CtdlSetConfigInt("c_imaps_port", atoi(buf));
				break;
			case 40:
				CtdlSetConfigInt("c_pop3s_port", atoi(buf));
				break;
			case 41:
				CtdlSetConfigInt("c_smtps_port", atoi(buf));
				break;
			case 42:
				CtdlSetConfigInt("c_enable_fulltext", confbool(buf));
				break;
			case 43:
				CtdlSetConfigInt("c_auto_cull", confbool(buf));
				break;
			case 44:
				/* niu */
				break;
			case 45:
				CtdlSetConfigInt("c_allow_spoofing", confbool(buf));
				break;
			case 46:
				CtdlSetConfigInt("c_journal_email", confbool(buf));
				break;
			case 47:
				CtdlSetConfigInt("c_journal_pubmsgs", confbool(buf));
				break;
			case 48:
				CtdlSetConfigStr("c_journal_dest", buf);
				break;
			case 49:
				CtdlSetConfigStr("c_default_cal_zone", buf);
				break;
			case 50:
				CtdlSetConfigInt("c_pftcpdict_port", atoi(buf));
				break;
			case 51:
				CtdlSetConfigInt("c_managesieve_port", atoi(buf));
				break;
			case 52:
				CtdlSetConfigInt("c_auth_mode", atoi(buf));
				break;
			case 53:
				CtdlSetConfigStr("c_funambol_host", buf);
				break;
			case 54:
				CtdlSetConfigInt("c_funambol_port", atoi(buf));
				break;
			case 55:
				CtdlSetConfigStr("c_funambol_source", buf);
				break;
			case 56:
				CtdlSetConfigStr("c_funambol_auth", buf);
				break;
			case 57:
				CtdlSetConfigInt("c_rbl_at_greeting", confbool(buf));
				break;
			case 58:
				CtdlSetConfigStr("c_master_user", buf);
				break;
			case 59:
				CtdlSetConfigStr("c_master_pass", buf);
				break;
			case 60:
				CtdlSetConfigStr("c_pager_program", buf);
				break;
			case 61:
				CtdlSetConfigInt("c_imap_keep_from", confbool(buf));
				break;
			case 62:
				CtdlSetConfigInt("c_xmpp_c2s_port", atoi(buf));
				break;
			case 63:
				CtdlSetConfigInt("c_xmpp_s2s_port", atoi(buf));
				break;
			case 64:
				CtdlSetConfigLong("c_pop3_fetch", atol(buf));
				break;
			case 65:
				CtdlSetConfigLong("c_pop3_fastest", atol(buf));
				break;
			case 66:
				CtdlSetConfigInt("c_spam_flag_only", confbool(buf));
				break;
			case 67:
				CtdlSetConfigInt("c_guest_logins", confbool(buf));
				break;
			case 68:
				CtdlSetConfigInt("c_port_number", atoi(buf));
				break;
			case 69:
				/* niu */
				break;
			case 70:
				CtdlSetConfigInt("c_nntp_port", atoi(buf));
				break;
			case 71:
				CtdlSetConfigInt("c_nntps_port", atoi(buf));
				break;
			}
			++a;
		}
		snprintf(buf, sizeof buf,
			"The global system configuration has been edited by %s.\n",
			 (CC->logged_in ? CC->curr_user : "an administrator")
		);
		CtdlAideMessage(buf,"Citadel Configuration Manager Message");

		if (!IsEmptyStr(CtdlGetConfigStr("c_logpages")))
			CtdlCreateRoom(CtdlGetConfigStr("c_logpages"), 3, "", 0, 1, 1, VIEW_BBS);

		/* If full text indexing has been disabled, invalidate the
		 * index so it doesn't try to use it later.
		 */
		if (CtdlGetConfigInt("c_enable_fulltext") == 0) {
			CtdlSetConfigInt("MM_fulltext_wordbreaker", 0);
		}
	}

	// CONF GETSYS - retrieve arbitrary system configuration stanzas stored in the message base
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

	// CONF PUTSYS - store arbitrary system configuration stanzas in the message base
	else if (!strcasecmp(cmd, "PUTSYS")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		unbuffer_output();
		cprintf("%d %s\n", SEND_LISTING, confname);
		confptr = CtdlReadMessageBody(HKEY("000"), CtdlGetConfigLong("c_maxmsglen"), NULL, 0, 0);
		CtdlPutSysConfig(confname, confptr);
		free(confptr);
	}

	else if (!strcasecmp(cmd, "GETVAL")) {
		extract_token(confname, argbuf, 1, '|', sizeof confname);
		char *v = CtdlGetConfigStr(confname);
		if (v) {
			cprintf("%d %s|\n", CIT_OK, v);
		}
		else {
			cprintf("%d |\n", ERROR);
		}
	}

	else if (!strcasecmp(cmd, "PUTVAL")) {
		if (num_tokens(argbuf, '|') < 3) {
			cprintf("%d name and value required\n", ERROR);
		}
		else {
			extract_token(confname, argbuf, 1, '|', sizeof confname);
			extract_token(buf, argbuf, 2, '|', sizeof buf);
			CtdlSetConfigStr(confname, buf);
			cprintf("%d setting '%s' to '%s'\n", CIT_OK, confname, buf);
		}
	}

	else {
		cprintf("%d Illegal option(s) specified.\n", ERROR + ILLEGAL_VALUE);
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
	Cfg = NewStrBufPlain(CtdlGetConfigStr("c_fqdn"), -1);
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
