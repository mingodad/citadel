/*
 * Read and write the citadel.config file
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

#include "sysdep.h"
#include <stdio.h>
#include <sys/utsname.h>
#include <libcitadel.h>
#include "config.h"
#include "ctdl_module.h"

long config_msgnum = 0;
HashList *ctdlconfig = NULL;	// new configuration


void config_warn_if_port_unset(char *key, int default_port)			\
{
	int p = CtdlGetConfigInt(key);
	if ((p < -1) ||	(p == 0) || (p > UINT16_MAX))
	{
		syslog(LOG_EMERG,
			"configuration setting %s is not -1 (disabled) or a valid TCP-Port - check your config! Default setting is: %d",
			key, default_port
		);
	}
}


void config_warn_if_empty(char *key)
{
	if (IsEmptyStr(CtdlGetConfigStr(key)))
	{
		syslog(LOG_EMERG, "configuration setting %s is empty, but must not - check your config!", key);
	}
}



void validate_config(void) {

	/*
	 * these shouldn't be empty
	 */
	config_warn_if_empty("c_fqdn");
	config_warn_if_empty("c_baseroom");
	config_warn_if_empty("c_aideroom");
	config_warn_if_empty("c_twitroom");
	config_warn_if_empty("c_nodename");
	config_warn_if_empty("c_default_cal_zone");

	/*
	 * Sanity check for port bindings
	 */
	config_warn_if_port_unset("c_smtp_port", 25);
	config_warn_if_port_unset("c_pop3_port", 110);
	config_warn_if_port_unset("c_imap_port", 143);
	config_warn_if_port_unset("c_msa_port", 587);
	config_warn_if_port_unset("c_port_number", 504);
	config_warn_if_port_unset("c_smtps_port", 465);
	config_warn_if_port_unset("c_pop3s_port", 995);
	config_warn_if_port_unset("c_imaps_port", 993);
	config_warn_if_port_unset("c_pftcpdict_port", -1);
	config_warn_if_port_unset("c_managesieve_port", 2020);
	config_warn_if_port_unset("c_xmpp_c2s_port", 5222);
	config_warn_if_port_unset("c_xmpp_s2s_port", 5269);
	config_warn_if_port_unset("c_nntp_port", 119);
	config_warn_if_port_unset("c_nntps_port", 563);

	if (getpwuid(ctdluid) == NULL) {
		syslog(LOG_EMERG, "The UID (%d) citadel is configured to use is not defined in your system (/etc/passwd?)!", ctdluid);
	}
	
}

/*
 * Put some sane default values into our configuration.  Some will be overridden when we run setup.
 */
void brand_new_installation_set_defaults(void) {

	struct utsname my_utsname;
	struct hostent *he;
	char detected_hostname[256];

	/* Determine our host name, in case we need to use it as a default */
	uname(&my_utsname);

	/* set some sample/default values in place of blanks... */
	extract_token(detected_hostname, my_utsname.nodename, 0, '.', sizeof detected_hostname);
	CtdlSetConfigStr("c_nodename", detected_hostname);

	if ((he = gethostbyname(my_utsname.nodename)) != NULL) {
		CtdlSetConfigStr("c_fqdn", he->h_name);
	}
	else {
		CtdlSetConfigStr("c_fqdn", my_utsname.nodename);
	}

	CtdlSetConfigStr("c_humannode",		"Citadel Server");
	CtdlSetConfigInt("c_initax",		4);
	CtdlSetConfigStr("c_moreprompt",	"<more>");
	CtdlSetConfigStr("c_twitroom",		"Trashcan");
	CtdlSetConfigStr("c_baseroom",		BASEROOM);
	CtdlSetConfigStr("c_aideroom",		"Aide");
	CtdlSetConfigInt("c_sleeping",		900);

	if (CtdlGetConfigInt("c_createax") == 0) {
		CtdlSetConfigInt("c_createax", 3);
	}

	/*
	 * Default port numbers for various services
	 */
	CtdlSetConfigInt("c_port_number",	504);
	CtdlSetConfigInt("c_smtp_port",		25);
	CtdlSetConfigInt("c_pop3_port",		110);
	CtdlSetConfigInt("c_imap_port",		143);
	CtdlSetConfigInt("c_msa_port",		587);
	CtdlSetConfigInt("c_smtps_port",	465);
	CtdlSetConfigInt("c_pop3s_port",	995);
	CtdlSetConfigInt("c_imaps_port",	993);
	CtdlSetConfigInt("c_pftcpdict_port",	-1);
	CtdlSetConfigInt("c_managesieve_port",	2020);
	CtdlSetConfigInt("c_xmpp_c2s_port",	5222);
	CtdlSetConfigInt("c_xmpp_s2s_port",	5269);
	CtdlSetConfigInt("c_nntp_port",		119);
	CtdlSetConfigInt("c_nntps_port",	563);

	/*
	 * Prevent the "new installation, set defaults" behavior from occurring again
	 */
	CtdlSetConfigLong("c_config_created_or_migrated", (long)time(NULL));
}



/*
 * Migrate a supplied legacy configuration to the new in-db format.
 * No individual site should ever have to do this more than once.
 */
void migrate_legacy_config(struct legacy_config *lconfig)
{
	CtdlSetConfigStr(	"c_nodename"		,	lconfig->c_nodename		);
	CtdlSetConfigStr(	"c_fqdn"		,	lconfig->c_fqdn			);
	CtdlSetConfigStr(	"c_humannode"		,	lconfig->c_humannode		);
	CtdlSetConfigInt(	"c_creataide"		,	lconfig->c_creataide		);
	CtdlSetConfigInt(	"c_sleeping"		,	lconfig->c_sleeping		);
	CtdlSetConfigInt(	"c_initax"		,	lconfig->c_initax		);
	CtdlSetConfigInt(	"c_regiscall"		,	lconfig->c_regiscall		);
	CtdlSetConfigInt(	"c_twitdetect"		,	lconfig->c_twitdetect		);
	CtdlSetConfigStr(	"c_twitroom"		,	lconfig->c_twitroom		);
	CtdlSetConfigStr(	"c_moreprompt"		,	lconfig->c_moreprompt		);
	CtdlSetConfigInt(	"c_restrict"		,	lconfig->c_restrict		);
	CtdlSetConfigStr(	"c_site_location"	,	lconfig->c_site_location	);
	CtdlSetConfigStr(	"c_sysadm"		,	lconfig->c_sysadm		);
	CtdlSetConfigInt(	"c_maxsessions"		,	lconfig->c_maxsessions		);
	CtdlSetConfigStr(	"c_ip_addr"		,	lconfig->c_ip_addr		);
	CtdlSetConfigInt(	"c_port_number"		,	lconfig->c_port_number		);
	CtdlSetConfigInt(	"c_ep_mode"		,	lconfig->c_ep.expire_mode	);
	CtdlSetConfigInt(	"c_ep_value"		,	lconfig->c_ep.expire_value	);
	CtdlSetConfigInt(	"c_userpurge"		,	lconfig->c_userpurge		);
	CtdlSetConfigInt(	"c_roompurge"		,	lconfig->c_roompurge		);
	CtdlSetConfigStr(	"c_logpages"		,	lconfig->c_logpages		);
	CtdlSetConfigInt(	"c_createax"		,	lconfig->c_createax		);
	CtdlSetConfigLong(	"c_maxmsglen"		,	lconfig->c_maxmsglen		);
	CtdlSetConfigInt(	"c_min_workers"		,	lconfig->c_min_workers		);
	CtdlSetConfigInt(	"c_max_workers"		,	lconfig->c_max_workers		);
	CtdlSetConfigInt(	"c_pop3_port"		,	lconfig->c_pop3_port		);
	CtdlSetConfigInt(	"c_smtp_port"		,	lconfig->c_smtp_port		);
	CtdlSetConfigInt(	"c_rfc822_strict_from"	,	lconfig->c_rfc822_strict_from	);
	CtdlSetConfigInt(	"c_aide_zap"		,	lconfig->c_aide_zap		);
	CtdlSetConfigInt(	"c_imap_port"		,	lconfig->c_imap_port		);
	CtdlSetConfigLong(	"c_net_freq"		,	lconfig->c_net_freq		);
	CtdlSetConfigInt(	"c_disable_newu"	,	lconfig->c_disable_newu		);
	CtdlSetConfigInt(	"c_enable_fulltext"	,	lconfig->c_enable_fulltext	);
	CtdlSetConfigStr(	"c_baseroom"		,	lconfig->c_baseroom		);
	CtdlSetConfigStr(	"c_aideroom"		,	lconfig->c_aideroom		);
	CtdlSetConfigInt(	"c_purge_hour"		,	lconfig->c_purge_hour		);
	CtdlSetConfigInt(	"c_mbxep_mode"		,	lconfig->c_mbxep.expire_mode	);
	CtdlSetConfigInt(	"c_mbxep_value"		,	lconfig->c_mbxep.expire_value	);
	CtdlSetConfigStr(	"c_ldap_host"		,	lconfig->c_ldap_host		);
	CtdlSetConfigInt(	"c_ldap_port"		,	lconfig->c_ldap_port		);
	CtdlSetConfigStr(	"c_ldap_base_dn"	,	lconfig->c_ldap_base_dn		);
	CtdlSetConfigStr(	"c_ldap_bind_dn"	,	lconfig->c_ldap_bind_dn		);
	CtdlSetConfigStr(	"c_ldap_bind_pw"	,	lconfig->c_ldap_bind_pw		);
	CtdlSetConfigInt(	"c_msa_port"		,	lconfig->c_msa_port		);
	CtdlSetConfigInt(	"c_imaps_port"		,	lconfig->c_imaps_port		);
	CtdlSetConfigInt(	"c_pop3s_port"		,	lconfig->c_pop3s_port		);
	CtdlSetConfigInt(	"c_smtps_port"		,	lconfig->c_smtps_port		);
	CtdlSetConfigInt(	"c_auto_cull"		,	lconfig->c_auto_cull		);
	CtdlSetConfigInt(	"c_allow_spoofing"	,	lconfig->c_allow_spoofing	);
	CtdlSetConfigInt(	"c_journal_email"	,	lconfig->c_journal_email	);
	CtdlSetConfigInt(	"c_journal_pubmsgs"	,	lconfig->c_journal_pubmsgs	);
	CtdlSetConfigStr(	"c_journal_dest"	,	lconfig->c_journal_dest		);
	CtdlSetConfigStr(	"c_default_cal_zone"	,	lconfig->c_default_cal_zone	);
	CtdlSetConfigInt(	"c_pftcpdict_port"	,	lconfig->c_pftcpdict_port	);
	CtdlSetConfigInt(	"c_managesieve_port"	,	lconfig->c_managesieve_port	);
	CtdlSetConfigInt(	"c_auth_mode"		,	lconfig->c_auth_mode		);
	CtdlSetConfigStr(	"c_funambol_host"	,	lconfig->c_funambol_host	);
	CtdlSetConfigInt(	"c_funambol_port"	,	lconfig->c_funambol_port	);
	CtdlSetConfigStr(	"c_funambol_source"	,	lconfig->c_funambol_source	);
	CtdlSetConfigStr(	"c_funambol_auth"	,	lconfig->c_funambol_auth	);
	CtdlSetConfigInt(	"c_rbl_at_greeting"	,	lconfig->c_rbl_at_greeting	);
	CtdlSetConfigStr(	"c_master_user"		,	lconfig->c_master_user		);
	CtdlSetConfigStr(	"c_master_pass"		,	lconfig->c_master_pass		);
	CtdlSetConfigStr(	"c_pager_program"	,	lconfig->c_pager_program	);
	CtdlSetConfigInt(	"c_imap_keep_from"	,	lconfig->c_imap_keep_from	);
	CtdlSetConfigInt(	"c_xmpp_c2s_port"	,	lconfig->c_xmpp_c2s_port	);
	CtdlSetConfigInt(	"c_xmpp_s2s_port"	,	lconfig->c_xmpp_s2s_port	);
	CtdlSetConfigLong(	"c_pop3_fetch"		,	lconfig->c_pop3_fetch		);
	CtdlSetConfigLong(	"c_pop3_fastest"	,	lconfig->c_pop3_fastest		);
	CtdlSetConfigInt(	"c_spam_flag_only"	,	lconfig->c_spam_flag_only	);
	CtdlSetConfigInt(	"c_guest_logins"	,	lconfig->c_guest_logins		);
	CtdlSetConfigInt(	"c_nntp_port"		,	lconfig->c_nntp_port		);
	CtdlSetConfigInt(	"c_nntps_port"		,	lconfig->c_nntps_port		);
}



/*
 * Called during the initialization of Citadel server.
 * It verifies the system's integrity and reads citadel.config into memory.
 */
void initialize_config_system(void) {
	FILE *cfp;
	int rv;
	struct legacy_config lconfig;	// legacy configuration
	ctdlconfig = NewHash(1, NULL);	// set up the real config system

	/* Ensure that we are linked to the correct version of libcitadel */
	if (libcitadel_version_number() < LIBCITADEL_VERSION_NUMBER) {
		fprintf(stderr, "You are running libcitadel version %d.%02d\n",
			(libcitadel_version_number() / 100), (libcitadel_version_number() % 100)
		);
		fprintf(stderr, "citserver was compiled against version %d.%02d\n",
			(LIBCITADEL_VERSION_NUMBER / 100), (LIBCITADEL_VERSION_NUMBER % 100)
		);
		exit(CTDLEXIT_LIBCITADEL);
	}

	if (chdir(ctdl_bbsbase_dir) != 0) {
		fprintf(stderr,
			"This program could not be started.\nUnable to change directory to %s\nError: %s\n",
			ctdl_bbsbase_dir,
			strerror(errno)
		);
		exit(CTDLEXIT_HOME);
	}

	memset(&lconfig, 0, sizeof(struct legacy_config));
	cfp = fopen(file_citadel_config, "rb");
	if (cfp != NULL) {
		if (CtdlGetConfigLong("c_config_created_or_migrated") <= 0) {
			fprintf(stderr, "Citadel Server found BOTH legacy and new configurations present.\n");
			fprintf(stderr, "Exiting to prevent data corruption.\n");
			exit(CTDLEXIT_CONFIG);
		}
		rv = fread((char *) &lconfig, sizeof(struct legacy_config), 1, cfp);
		if (rv != 1)
		{
			fprintf(stderr, 
				"Warning: Found a legacy config file %s has unexpected size. \n",
				file_citadel_config
			);
		}

		migrate_legacy_config(&lconfig);

		fclose(cfp);
		if (unlink(file_citadel_config) != 0) {
			fprintf(stderr, "Unable to remove legacy config file %s after migrating it.\n", file_citadel_config);
			fprintf(stderr, "Exiting to prevent data corruption.\n");
			exit(CTDLEXIT_CONFIG);
		}

		/*
		 * Prevent migration/initialization from happening again.
		 */
		CtdlSetConfigLong("c_config_created_or_migrated", (long)time(NULL));

	}

	/* New installation?  Set up configuration */
	if (CtdlGetConfigLong("c_config_created_or_migrated") <= 0) {
		brand_new_installation_set_defaults();
	}

	/* Only allow LDAP auth mode if we actually have LDAP support */
#ifndef HAVE_LDAP
	if ((config.c_auth_mode == AUTHMODE_LDAP) || (config.c_auth_mode == AUTHMODE_LDAP_AD)) {
		fprintf(stderr, "Your system is configured for LDAP authentication,\n"
				"but you are running a server built without OpenLDAP support.\n");
		exit(CTDL_EXIT_UNSUP_AUTH);
	}
#endif

        /* Default maximum message length is 10 megabytes.  This is site
	 * configurable.  Also check to make sure the limit has not been
	 * set below 8192 bytes.
         */
        if (CtdlGetConfigLong("c_maxmsglen") <= 0)	CtdlSetConfigLong("c_maxmsglen", 10485760);
        if (CtdlGetConfigLong("c_maxmsglen") < 8192)	CtdlSetConfigLong("c_maxmsglen", 8192);

        /*
	 * Default lower and upper limits on number of worker threads
	 */
	if (CtdlGetConfigInt("c_min_workers") < 5)	CtdlSetConfigInt("c_min_workers", 5);	// min
	if (CtdlGetConfigInt("c_max_workers") == 0)	CtdlSetConfigInt("c_max_workers", 256);	// default max
	if (CtdlGetConfigInt("c_max_workers") < CtdlGetConfigInt("c_min_workers")) {
		CtdlSetConfigInt("c_max_workers", CtdlGetConfigInt("c_min_workers"));		// max >= min
	}

	/* Networking more than once every five minutes just isn't sane */
	if (CtdlGetConfigLong("c_net_freq") == 0)	CtdlSetConfigLong("c_net_freq", 3600);	// once per hour default
	if (CtdlGetConfigLong("c_net_freq") < 300)	CtdlSetConfigLong("c_net_freq", 300);	// minimum 5 minutes

	/* Same goes for POP3 */
	if (CtdlGetConfigLong("c_pop3_fetch") == 0)	CtdlSetConfigLong("c_pop3_fetch", 3600);	// once per hour default
	if (CtdlGetConfigLong("c_pop3_fetch") < 300)	CtdlSetConfigLong("c_pop3_fetch", 300);		// 5 minutes min
	if (CtdlGetConfigLong("c_pop3_fastest") == 0)	CtdlSetConfigLong("c_pop3_fastest", 3600);	// once per hour default
	if (CtdlGetConfigLong("c_pop3_fastest") < 300)	CtdlSetConfigLong("c_pop3_fastest", 300);	// 5 minutes min

	/* "create new user" only works with native authentication mode */
	if (CtdlGetConfigInt("c_auth_mode") != AUTHMODE_NATIVE) {
		CtdlSetConfigInt("c_disable_newu", 1);
	}
}



/*
 * Called when Citadel server is shutting down.
 * Clears out the config hash table.
 */
void shutdown_config_system(void) 
{
	DeleteHash(&ctdlconfig);
}



/*
 * Set a system config value.  Simple key/value here.
 */
void CtdlSetConfigStr(char *key, char *value)
{
	int key_len = strlen(key);
	int value_len = strlen(value);

	/* Save it in memory */
	Put(ctdlconfig, key, key_len, strdup(value), NULL);

	/* Also write it to the config database */

	int dbv_size = key_len + value_len + 2;
	char *dbv = malloc(dbv_size);
	strcpy(dbv, key);
	strcpy(&dbv[key_len + 1], value);
	cdb_store(CDB_CONFIG, key, key_len, dbv, dbv_size);
	free(dbv);
}


/*
 * Set a numeric system config value (long integer)
 */
void CtdlSetConfigLong(char *key, long value)
{
	char longstr[256];
	sprintf(longstr, "%ld", value);
	CtdlSetConfigStr(key, longstr);
}


/*
 * Set a numeric system config value (integer)
 */
void CtdlSetConfigInt(char *key, int value)
{
	char intstr[256];
	sprintf(intstr, "%d", value);
	CtdlSetConfigStr(key, intstr);
}


/*
 * Fetch a system config value.  Caller does *not* own the returned value and may not alter it.
 */
char *CtdlGetConfigStr(char *key)
{
	char *value = NULL;
	struct cdbdata *cdb;
	int key_len = strlen(key);

	if (IsEmptyStr(key)) return(NULL);

	/* Temporary hack to make sure we didn't mess up any porting - FIXME remove this after testing thoroughly */
	if (!strncmp(key, "config", 6)) {
		syslog(LOG_EMERG, "You requested a key starting with 'config' which probably means a porting error: %s", key);
		abort();
	}

	/* First look in memory */
	if (GetHash(ctdlconfig, key, key_len, (void *)&value))
	{
		return value;
	}

	/* Then look in the database. */

	cdb = cdb_fetch(CDB_CONFIG, key, key_len);

	if (cdb == NULL) {	/* nope, not there either. */
		return(NULL);
	}

	/* Got it.  Save it in memory for the next fetch. */
	value = strdup(cdb->ptr + key_len + 1);		/* The key was stored there too; skip past it */
	cdb_free(cdb);
	Put(ctdlconfig, key, key_len, value, NULL);
	return value;
}





/**********************************************************************/










void CtdlGetSysConfigBackend(long msgnum, void *userdata) {
	config_msgnum = msgnum;
}


char *CtdlGetSysConfig(char *sysconfname) {
	char hold_rm[ROOMNAMELEN];
	long msgnum;
	char *conf;
	struct CtdlMessage *msg;
	char buf[SIZ];
	
	strcpy(hold_rm, CC->room.QRname);
	if (CtdlGetRoom(&CC->room, SYSCONFIGROOM) != 0) {
		CtdlGetRoom(&CC->room, hold_rm);
		return NULL;
	}


	/* We want the last (and probably only) config in this room */
	begin_critical_section(S_CONFIG);
	config_msgnum = (-1L);
	CtdlForEachMessage(MSGS_LAST, 1, NULL, sysconfname, NULL,
			   CtdlGetSysConfigBackend, NULL);
	msgnum = config_msgnum;
	end_critical_section(S_CONFIG);

	if (msgnum < 0L) {
		conf = NULL;
	}
	else {
		msg = CtdlFetchMessage(msgnum, 1);
		if (msg != NULL) {
			conf = strdup(msg->cm_fields[eMesageText]);
			CM_Free(msg);
		}
		else {
			conf = NULL;
		}
	}

	CtdlGetRoom(&CC->room, hold_rm);

	if (conf != NULL) do {
			extract_token(buf, conf, 0, '\n', sizeof buf);
			strcpy(conf, &conf[strlen(buf)+1]);
		} while ( (!IsEmptyStr(conf)) && (!IsEmptyStr(buf)) );

	return(conf);
}


void CtdlPutSysConfig(char *sysconfname, char *sysconfdata) {
	CtdlWriteObject(SYSCONFIGROOM, sysconfname, sysconfdata, (strlen(sysconfdata)+1), NULL, 0, 1, 0);
}

