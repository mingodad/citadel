/*
 * $Id$
 * 
 * 5.80 to 5.90 migration utility for network files
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include "citadel.h"
#include "ipc.h"
#include "tools.h"
#include "config.h"


void logoff(int code)
{
	exit(code);
}

int main(int argc, char **argv)
{
	char buf[SIZ];
	char hostbuf[SIZ];
	char portbuf[SIZ];
	DIR *dp;
	struct dirent *d;
	long highest = 0L;
	char room[SIZ];
	long thighest;
	FILE *fp;
	FILE *tempfp;

	printf("\n\n\n\n\n"
"This utility migrates your network settings (room sharing with other\n"
"Citadel systems) from 5.80 to 5.90.  You should only do this ONCE.  It\n"
"will ERASE your 5.80 configuration files when it is finished, and it will\n"
"ERASE any 5.90 configuration files that you have already set up.\n\n"
"Are you sure you want to do this? ");

	gets(buf);
	if (tolower(buf[0]) != 'y') exit(0);

	get_config();

	attach_to_server(argc, argv, hostbuf, portbuf);
	serv_gets(buf);
	printf("%s\n", &buf[4]);
	if ( (buf[0]!='2') && (strncmp(buf,"551",3)) ) {
		fprintf(stderr, "%s: %s\n", argv[0], &buf[4]);
		logoff(atoi(buf));
	}

	sprintf(buf, "IPGM %d", config.c_ipgm_secret);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", &buf[4]);
	if (buf[0] != '2') {
		exit(2);
	}

	if (chdir("network/systems") != 0) {
		perror("cannot chdir network/systems");
		exit(errno);
	}

	tempfp = tmpfile();
	if (tempfp == NULL) {
		perror("cannot open temp file");
		exit(errno);
	}

	dp = opendir(".");
	if (dp == NULL) {
		perror("cannot open directory");
		exit(errno);
	}

	while (d = readdir(dp), d != NULL) {
		fp = NULL;
		if ( (d->d_name[0] != '.') && strcasecmp(d->d_name, "CVS")) {
			fp = fopen(d->d_name, "r");
		}
		if (fp != NULL) {
			printf("*** Processing '%s'\n", d->d_name);
			while (fgets(room, sizeof room, fp),
				fscanf(fp, "%ld\n", &thighest),
				!feof(fp) ) {
					room[strlen(room) - 1] = 0;
					fprintf(tempfp, "%s|%s\n",
						room, d->d_name);
					if (thighest > highest) {
						highest = thighest;
					}
			}
			fclose(fp);
		}
	}

	closedir(dp);
	fclose(tempfp);
	return(0);
}



