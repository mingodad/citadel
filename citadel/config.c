/*
 * This function reads the citadel.config file.  It should be called at
 * the beginning of EVERY Citadel program.
 *
 * $Id$
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
#include "config.h"

struct config config;
char bbs_home_directory[PATH_MAX] = BBSDIR;
int home_specified = 0;

/*
 * get_config() is called during the initialization of any program which
 * directly accesses Citadel data files.  It verifies the system's integrity
 * and reads citadel.config into memory.
 */
void get_config(void) {
	FILE *cfp;
	struct stat st;

	if (chdir(home_specified ? bbs_home_directory : BBSDIR) != 0) {
		fprintf(stderr,
			"This program could not be started.\n"
		 	"Unable to change directory to %s\n"
			"Error: %s\n",
			(home_specified ? bbs_home_directory : BBSDIR),
			strerror(errno));
		exit(1);
	}
	cfp = fopen("citadel.config", "rb");
	if (cfp == NULL) {
		fprintf(stderr, "This program could not be started.\n"
			"Unable to open %s/citadel.config\n"
			"Error: %s\n",
			(home_specified ? bbs_home_directory : BBSDIR),
			strerror(errno));
		exit(1);
	}
	fread((char *) &config, sizeof(struct config), 1, cfp);
	if (fstat(fileno(cfp), &st)) {
		perror("citadel.config");
		exit(1);
	}
	if (st.st_uid != BBSUID || st.st_mode != (S_IFREG | S_IRUSR | S_IWUSR)) {
		fprintf(stderr, "check the permissions on citadel.config\n");
		exit(1);
	}
	fclose(cfp);

	if (config.c_setup_level != REV_LEVEL) {
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

        /* Default maximum message length is 'unlimited' (max int)
         * and the minimum is 8192
         */
        if (config.c_maxmsglen <= 0)
                config.c_maxmsglen = INT_MAX;
        if (config.c_maxmsglen < 8192)
                config.c_maxmsglen = 8192;

        /* Default lower and upper limits on number of worker threads */

	if (config.c_min_workers < 3)		/* no less than 3 */
		config.c_min_workers = 5;

	if (config.c_max_workers == 0)			/* default maximum */
		config.c_max_workers = 256;

	if (config.c_max_workers < config.c_min_workers)   /* max >= min */
		config.c_max_workers = config.c_min_workers;
}


/*
 * Occasionally, we will need to write the config file, because some operations
 * change site-wide parameters.
 */
void put_config(void)
{
	FILE *cfp;

	if ((cfp = fopen("citadel.config", "rb+")) == NULL)
		perror("citadel.config");
	else {
		fwrite((char *) &config, sizeof(struct config), 1, cfp);
		fclose(cfp);
	}
}
