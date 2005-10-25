/*
 * $Id$
 *
 * This function reads the citadel.config file.  It should be called at
 * the beginning of EVERY Citadel program.
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
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "config.h"

struct config config;
char ctdl_home_directory[PATH_MAX] = CTDLDIR;
int home_specified = 0;

/*
 * get_config() is called during the initialization of any program which
 * directly accesses Citadel data files.  It verifies the system's integrity
 * and reads citadel.config into memory.
 */
void get_config(void) {
	FILE *cfp;
	struct stat st;

	if (chdir(home_specified ? ctdl_home_directory : CTDLDIR) != 0) {
		fprintf(stderr,
			"This program could not be started.\n"
		 	"Unable to change directory to %s\n"
			"Error: %s\n",
			(home_specified ? ctdl_home_directory : CTDLDIR),
			strerror(errno));
		exit(1);
	}
	cfp = fopen(
#ifndef HAVE_ETC_DIR
				"."
#else
				ETC_DIR
#endif
				"/citadel.config", "rb");
	if (cfp == NULL) {
		fprintf(stderr, "This program could not be started.\n"
			"Unable to open %s/citadel.config\n"
			"Error: %s\n",
			(home_specified ? ctdl_home_directory : CTDLDIR),
			strerror(errno));
		exit(1);
	}
	fread((char *) &config, sizeof(struct config), 1, cfp);
	if (fstat(fileno(cfp), &st)) {
		perror("citadel.config");
		exit(1);
	}
#ifndef __CYGWIN__
	if (st.st_uid != CTDLUID || st.st_mode != (S_IFREG | S_IRUSR | S_IWUSR)) {
		fprintf(stderr, "check the permissions on citadel.config\n");
		exit(1);
	}
#endif
	fclose(cfp);

	if (config.c_setup_level < REV_MIN) {
		fprintf(stderr, "config: Your data files are out of date.  ");
		fprintf(stderr, "Run setup to update them.\n");
		fprintf(stderr,
			"        This program requires level %d.%02d\n",
			(REV_LEVEL / 100), (REV_LEVEL % 100));
		fprintf(stderr,
			"        Data files are currently at %d.%02d\n",
			(config.c_setup_level / 100),
			(config.c_setup_level % 100));
		exit(1);
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
}


/*
 * Occasionally, we will need to write the config file, because some operations
 * change site-wide parameters.
 */
void put_config(void)
{
	FILE *cfp;

	if ((cfp = fopen(
#ifndef HAVE_ETC_DIR
					 "."
#else
					 ETC_DIR
#endif
					 "/citadel.config", "rb+")) == NULL)
		perror("citadel.config");
	else {
		fwrite((char *) &config, sizeof(struct config), 1, cfp);
		fclose(cfp);
	}
}
