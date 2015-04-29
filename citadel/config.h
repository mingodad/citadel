/*
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

#include "serv_extensions.h"
#include "citadel_dirs.h"

#define CtdlGetConfigInt(x)	atoi(CtdlGetConfigStr(x))
#define CtdlGetConfigLong(x)	atol(CtdlGetConfigStr(x))




/*
 * This is the format of the legacy config file.  Do not attempt to do anything with it other
 * than migrate it into the new format. 
 */
struct legacy_config {
	char c_nodename[16];		/* short name of this node on a Citadel network */
	char c_fqdn[64];		/* this site's fully qualified domain name */
	char c_humannode[21];		/* human-readable site name */
	char c_niu_7[16];
	uid_t c_niu_6;
	char c_creataide;		/* 1 = creating a room auto-grants room aide privileges */
	int c_sleeping;			/* watchdog timer (seconds) */
	char c_initax;			/* initial access level for new users */
	char c_regiscall;		/* after c_regiscall logins user will be asked to register */
	char c_twitdetect;		/* automatically move messages from problem users to trashcan */
	char c_twitroom[ROOMNAMELEN];	/* name of trashcan */
	char c_moreprompt[80];		/* paginator prompt */
	char c_restrict;		/* require per-user permission to send Internet mail */
	long c_niu_1;
	char c_site_location[32];	/* geographic location of this Citadel site */
	char c_sysadm[26];		/* name of system administrator */
	char c_niu_2[15];
	int c_niu_3;
	int c_maxsessions;		/* maximum number of concurrent sessions allowed */
	char c_ip_addr[20];		/* bind address for listening sockets */
	int c_port_number;		/* port number for Citadel protocol (usually 504) */
	int c_niu_4;
	struct ExpirePolicy c_ep;	/* default expire policy for the entire site */
	int c_userpurge;		/* user purge time (in days) */
	int c_roompurge;		/* room purge time (in days) */
	char c_logpages[ROOMNAMELEN];
	char c_createax;
	long c_maxmsglen;
	int c_min_workers;
	int c_max_workers;
	int c_pop3_port;
	int c_smtp_port;
	int c_rfc822_strict_from;
	int c_aide_zap;
	int c_imap_port;
	time_t c_net_freq;
	char c_disable_newu;
	char c_enable_fulltext;
	char c_baseroom[ROOMNAMELEN];
	char c_aideroom[ROOMNAMELEN];
	int c_purge_hour;
	struct ExpirePolicy c_mbxep;
	char c_ldap_host[128];
	int c_ldap_port;
	char c_ldap_base_dn[256];
	char c_ldap_bind_dn[256];
	char c_ldap_bind_pw[256];
	int c_msa_port;
	int c_imaps_port;
	int c_pop3s_port;
	int c_smtps_port;
	char c_auto_cull;
	char c_niu_5;
	char c_allow_spoofing;
	char c_journal_email;
	char c_journal_pubmsgs;
	char c_journal_dest[128];
	char c_default_cal_zone[128];
	int c_pftcpdict_port;
	int c_managesieve_port;
	int c_auth_mode;
	char c_funambol_host[256];
	int c_funambol_port;
	char c_funambol_source[256];
	char c_funambol_auth[256];
	char c_rbl_at_greeting;
	char c_master_user[32];
	char c_master_pass[32];
	char c_pager_program[256];
	char c_imap_keep_from;
	int c_xmpp_c2s_port;
	int c_xmpp_s2s_port;
	time_t c_pop3_fetch;
	time_t c_pop3_fastest;
	int c_spam_flag_only;
	int c_guest_logins;
	int c_nntp_port;
	int c_nntps_port;
};




void initialize_config_system(void);
void shutdown_config_system(void);
void put_config(void);
void CtdlSetConfigStr(char *, char *);
char *CtdlGetConfigStr(char *);
void CtdlSetConfigInt(char *key, int value);
void CtdlSetConfigLong(char *key, long value);

char *CtdlGetSysConfig(char *sysconfname);
void CtdlPutSysConfig(char *sysconfname, char *sysconfdata);
void validate_config(void);
