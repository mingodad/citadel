/*
 * $Id$
 *
 * Read and write the citadel.config file
 *
 */

#include "sysdep.h"
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "config.h"

struct config config;

/*
 * get_config() is called during the initialization of any program which
 * directly accesses Citadel data files.  It verifies the system's integrity
 * and reads citadel.config into memory.
 */
void get_config(void) {
	FILE *cfp;
	struct stat st;

	if (chdir(ctdl_bbsbase_dir) != 0) {
		fprintf(stderr,
			"This program could not be started.\n"
		 	"Unable to change directory to %s\n"
			"Error: %s\n",
			ctdl_bbsbase_dir,
			strerror(errno));
		exit(CTDLEXIT_HOME);
	}
	cfp = fopen(file_citadel_config, "rb");
	if (cfp == NULL) {
		fprintf(stderr, "This program could not be started.\n"
				"Unable to open %s\n"
				"Error: %s\n",
				file_citadel_config,
				strerror(errno));
		exit(CTDLEXIT_CONFIG);
	}
	fread((char *) &config, sizeof(struct config), 1, cfp);
	if (fstat(fileno(cfp), &st)) {
		perror(file_citadel_config);
		exit(CTDLEXIT_CONFIG);
	}

#ifndef __CYGWIN__
	if (st.st_uid != CTDLUID) {
		fprintf(stderr, "%s must be owned by uid="F_UID_T" but "F_UID_T" owns it!\n", 
			file_citadel_config, CTDLUID, st.st_uid);
		exit(CTDLEXIT_CONFIG);
	}
	int desired_mode = (S_IFREG | S_IRUSR | S_IWUSR) ;
	if (st.st_mode != desired_mode) {
		fprintf(stderr, "%s must be set to permissions mode %03o but they are %03o\n",
			file_citadel_config, (desired_mode & 0xFFF), (st.st_mode & 0xFFF));
		exit(CTDLEXIT_CONFIG);
	}
#endif

	fclose(cfp);

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
	if (config.c_auth_mode == AUTHMODE_LDAP) {
		fprintf(stderr, "Your system is configured for LDAP authentication,\n"
				"but you are running a server built without OpenLDAP support.\n");
		exit(CTDL_EXIT_UNSUP_AUTH);
	}
#endif

	/* Check to see whether 'setup' must first be run to update data file formats */
	if (config.c_setup_level < REV_MIN) {
		fprintf(stderr, "Your data files are out of date.  Run setup to update them.\n");
		fprintf(stderr, "        This program requires level %d.%02d\n",
			(REV_LEVEL / 100), (REV_LEVEL % 100));
		fprintf(stderr, "        Data files are currently at %d.%02d\n",
			(config.c_setup_level / 100),
			(config.c_setup_level % 100));
		exit(CTDLEXIT_OOD);
	}

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

	if ((cfp = fopen(file_citadel_config, "rb+")) == NULL)
		perror(file_citadel_config);
	else {
		fwrite((char *) &config, sizeof(struct config), 1, cfp);
		fclose(cfp);
	}
}
