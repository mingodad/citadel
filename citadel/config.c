/*
 * This function reads the citadel.config file.  It should be called at
 * the beginning of EVERY Citadel program.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "config_decls.h"

struct config config;
char bbs_home_directory[PATH_MAX];
int home_specified = 0;

void get_config(void) {
	FILE *cfp;

	if (chdir( home_specified ? bbs_home_directory : BBSDIR ) != 0) {
		fprintf(stderr, "Cannot start.\nThere is no Citadel installation in %s\n%s\n",
			(home_specified ? bbs_home_directory : BBSDIR),
			strerror(errno));
		exit(errno);
		}
	cfp=fopen("citadel.config","r");
	if (cfp==NULL) {
		fprintf(stderr, "Cannot start.\n");
		fprintf(stderr, "There is no citadel.config in %s\n%s\n",
			(home_specified ? bbs_home_directory : BBSDIR),
			strerror(errno));
		exit(errno);
		}
	fread((char *)&config,sizeof(struct config),1,cfp);
	fclose(cfp);
	if ( (config.c_setup_level / 10) != (REV_LEVEL/10) ) {
		fprintf(stderr, "config: Your data files are out of date.  ");
		fprintf(stderr, "Run setup to update them.\n");
		fprintf(stderr,
			"        This program requires level %d.%02d\n",
				(REV_LEVEL / 100), (REV_LEVEL % 100) );
		fprintf(stderr,
			"        Data files are currently at %d.%02d\n",
				(config.c_setup_level / 100),
				(config.c_setup_level % 100) );
		exit(1);
		}
	}
