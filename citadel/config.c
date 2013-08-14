/*
 * Read and write the citadel.config file
 *
 * Copyright (c) 1987-2012 by the citadel.org team
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
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "config.h"
#include "ctdl_module.h"

struct config config;

/*
 * Put some sane default values into our configuration.  Some will be overridden when we run setup.
 */
void brand_new_installation_set_defaults(void) {

	struct passwd *pw;
	struct utsname my_utsname;
	struct hostent *he;

	/* Determine our host name, in case we need to use it as a default */
	uname(&my_utsname);

	/* set some sample/default values in place of blanks... */
	extract_token(config.c_nodename, my_utsname.nodename, 0, '.', sizeof config.c_nodename);
	if (IsEmptyStr(config.c_fqdn) ) {
		if ((he = gethostbyname(my_utsname.nodename)) != NULL) {
			safestrncpy(config.c_fqdn, he->h_name, sizeof config.c_fqdn);
		}
		else {
			safestrncpy(config.c_fqdn, my_utsname.nodename, sizeof config.c_fqdn);
		}
	}

	safestrncpy(config.c_humannode, "Citadel Server", sizeof config.c_humannode);
	safestrncpy(config.c_phonenum, "US 800 555 1212", sizeof config.c_phonenum);
	config.c_initax = 4;
	safestrncpy(config.c_moreprompt, "<more>", sizeof config.c_moreprompt);
	safestrncpy(config.c_twitroom, "Trashcan", sizeof config.c_twitroom);
	safestrncpy(config.c_baseroom, BASEROOM, sizeof config.c_baseroom);
	safestrncpy(config.c_aideroom, "Aide", sizeof config.c_aideroom);
	config.c_port_number = 504;
	config.c_sleeping = 900;

	if (config.c_ctdluid == 0) {
		pw = getpwnam("citadel");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("bbs");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
	}
	if (config.c_ctdluid == 0) {
		pw = getpwnam("guest");
		if (pw != NULL) {
			config.c_ctdluid = pw->pw_uid;
		}
	}
	if (config.c_createax == 0) {
		config.c_createax = 3;
	}

	/*
	 * Default port numbers for various services
	 */
	config.c_smtp_port = 25;
	config.c_pop3_port = 110;
	config.c_imap_port = 143;
	config.c_msa_port = 587;
	config.c_smtps_port = 465;
	config.c_pop3s_port = 995;
	config.c_imaps_port = 993;
	config.c_pftcpdict_port = -1 ;
	config.c_managesieve_port = 2020;
	config.c_xmpp_c2s_port = 5222;
	config.c_xmpp_s2s_port = 5269;
}



/*
 * get_config() is called during the initialization of Citadel server.
 * It verifies the system's integrity and reads citadel.config into memory.
 */
void get_config(void) {
	FILE *cfp;
	int rv;

	if (chdir(ctdl_bbsbase_dir) != 0) {
		fprintf(stderr,
			"This program could not be started.\nUnable to change directory to %s\nError: %s\n",
			ctdl_bbsbase_dir,
			strerror(errno)
		);
		exit(CTDLEXIT_HOME);
	}

	memset(&config, 0, sizeof(struct config));
	cfp = fopen(file_citadel_config, "rb");
	if (cfp != NULL) {
		rv = fread((char *) &config, sizeof(struct config), 1, cfp);
		if (rv != 1)
		{
			fprintf(stderr, 
				"Warning: The config file %s has unexpected size. \n",
				file_citadel_config
			);
		}
		fclose(cfp);
	}
	else {
		brand_new_installation_set_defaults();
	}

	/* Ensure that we are linked to the correct version of libcitadel */
	if (libcitadel_version_number() < LIBCITADEL_VERSION_NUMBER) {
		fprintf(stderr, "    You are running libcitadel version %d.%02d\n",
			(libcitadel_version_number() / 100), (libcitadel_version_number() % 100));
		fprintf(stderr, "citserver was compiled against version %d.%02d\n",
			(LIBCITADEL_VERSION_NUMBER / 100), (LIBCITADEL_VERSION_NUMBER % 100));
		exit(CTDLEXIT_LIBCITADEL);
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
        if (config.c_maxmsglen <= 0)
                config.c_maxmsglen = 10485760;
        if (config.c_maxmsglen < 8192)
                config.c_maxmsglen = 8192;

        /* Default lower and upper limits on number of worker threads */

	if (config.c_min_workers < 3)		/* no less than 3 */
		config.c_min_workers = 5;

	if (config.c_max_workers == 0)			/* default maximum */
		config.c_max_workers = 256;

	if (config.c_max_workers < config.c_min_workers)   /* max >= min */
		config.c_max_workers = config.c_min_workers;

	/* Networking more than once every five minutes just isn't sane */
	if (config.c_net_freq == 0L)
		config.c_net_freq = 3600L;	/* once per hour default */
	if (config.c_net_freq < 300L) 
		config.c_net_freq = 300L;

	/* Same goes for POP3 */
	if (config.c_pop3_fetch == 0L)
		config.c_pop3_fetch = 3600L;	/* once per hour default */
	if (config.c_pop3_fetch < 300L) 
		config.c_pop3_fetch = 300L;
	if (config.c_pop3_fastest == 0L)
		config.c_pop3_fastest = 3600L;	/* once per hour default */
	if (config.c_pop3_fastest < 300L) 
		config.c_pop3_fastest = 300L;

	/* "create new user" only works with native authentication mode */
	if (config.c_auth_mode != AUTHMODE_NATIVE) {
		config.c_disable_newu = 1;
	}
}


/*
 * Occasionally, we will need to write the config file, because some operations
 * change site-wide parameters.
 */
void put_config(void)
{
	FILE *cfp;
	int blocks_written = 0;

	if ((cfp = fopen(file_citadel_config, "w")) != NULL) {
		blocks_written = fwrite((char *) &config, sizeof(struct config), 1, cfp);
		if (blocks_written == 1) {
			fclose(cfp);
			chown(file_citadel_config, CTDLUID, (-1));
			chmod(file_citadel_config, 0600);
			return;
		}
	}
	syslog(LOG_EMERG, "%s: %s", file_citadel_config, strerror(errno));
}
