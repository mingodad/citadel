/*
 * Everything which needs some logging...
 * $Id$
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <pthread.h>
#include <syslog.h>
#include "citadel.h"
#include "server.h"
#include "logging.h"


void rec_log(unsigned int lrtype, char *name) {
	FILE *fp;
	time_t now;

	time(&now);
	fp = fopen("citadel.log", "a");
	fprintf(fp, "%ld|%u|%s\n", now, lrtype, name);
	fclose(fp);
	}
