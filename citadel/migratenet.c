/*
 * $Id$
 * 
 * 5.80 to 5.90 migration utility for network files
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include "citadel.h"
#include "citadel_ipc.h"
#include "tools.h"
#include "config.h"

struct mn_roomlist {
	struct mn_roomlist *next;
	char roomname[SIZ];
};

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
	char node[SIZ];
	char room[SIZ];
	long thighest;
	FILE *fp;
	FILE *roomfp;
	char roomfilename[SIZ];
	FILE *nodefp;
	char nodefilename[SIZ];
	char *listing = NULL;
	struct mn_roomlist *mn = NULL;
	struct mn_roomlist *mnptr = NULL;
	CtdlIPC *ipc = NULL;
	int r;

	printf("\n\n\n\n\n"
"This utility migrates your network settings (room sharing with other\n"
"Citadel systems) from 5.80 to 5.90.  You should only do this ONCE.  It\n"
"will ERASE your 5.80 configuration files when it is finished, and it will\n"
"ERASE any 5.90 configuration files that you have already set up.\n\n"
"Are you sure you want to do this? ");

	fgets(buf, sizeof buf, stdin);
	if (tolower(buf[0]) != 'y') exit(0);

	get_config();

	ipc = CtdlIPC_new(argc, argv, hostbuf, portbuf);
	CtdlIPC_getline(ipc, buf);
	printf("%s\n", &buf[4]);
	if ( (buf[0]!='2') && (strncmp(buf,"551",3)) ) {
		fprintf(stderr, "%s: %s\n", argv[0], &buf[4]);
		logoff(atoi(buf));
	}

	r = CtdlIPCInternalProgram(ipc, config.c_ipgm_secret, buf);
	fprintf(stderr, "%s\n", buf);
	if (r / 100 != 2) {
		exit(2);
	}

	if (chdir("network/systems") != 0) {
		perror("cannot chdir network/systems");
		exit(errno);
	}

	strcpy(roomfilename, tmpnam(NULL));
	roomfp = fopen(roomfilename, "w");
	if (roomfp == NULL) {
		perror("cannot open temp file");
		exit(errno);
	}

	strcpy(nodefilename, tmpnam(NULL));
	nodefp = fopen(nodefilename, "w");
	if (roomfp == NULL) {
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
			printf("\n*** Processing '%s'\n", d->d_name);

			fprintf(nodefp, "%s|", d->d_name);
			printf("Enter shared secret: ");
			myfgets(buf, sizeof buf, stdin);
			if (buf[0] == 0) strcpy(buf, config.c_net_password);
			fprintf(nodefp, "%s|", buf);
			printf("Enter host name/IP : ");
			myfgets(buf, sizeof buf, stdin);
			if (buf[0] == 0) snprintf(buf, sizeof buf, "%s.citadel.org",
				d->d_name);
			fprintf(nodefp, "%s|", buf);
			printf("Enter port number  : ");
			fgets(buf, sizeof buf, stdin);
			if (buf[0] == 0) strcpy(buf, "504");
			fprintf(nodefp, "%s\n", buf);

			fgets(buf, sizeof buf, fp);
			printf("skipping: %s", buf);
			while (fgets(room, sizeof room, fp),
			      (fgets(buf, sizeof buf, fp) != NULL) ) {
				thighest = atol(buf),
				room[strlen(room) - 1] = 0;
				fprintf(roomfp, "%s|%s\n",
					d->d_name, room);
				if (thighest > highest) {
					highest = thighest;
				}
			}
			fclose(fp);
		}
	}

	closedir(dp);
	fclose(roomfp);
	fclose(nodefp);

	/* Point of no return */
	nodefp = fopen(nodefilename, "r");
	if (nodefp != NULL) {
		fseek(nodefp, 0L, SEEK_END);
		listing = malloc(ftell(nodefp) + 1);
		*listing = 0;
		while (fgets(buf, sizeof buf, nodefp) != NULL) {
			strcat(listing, buf);
		}
		fclose(nodefp);
	}

	/* Set up the node table */
	printf("Creating neighbor node table\n");
	r = CtdlIPCSetSystemConfigByType(ipc, IGNETCFG, listing, buf);
	free(listing);
	listing = NULL;
	if (r / 100 != 4) {
		printf("%s\n", buf);
	}

	/* Now go through the table looking for node names to enter */
	snprintf(buf, sizeof buf, "cat %s |awk -F \"|\" '{ print $2 }' |sort -f |uniq -i",
		roomfilename);
	roomfp = popen(buf, "r");
	if (roomfp == NULL) {
		perror("cannot open pipe");
		unlink(roomfilename);
	}

	while (fgets(room, sizeof room, roomfp) != NULL) {
		room[strlen(room)-1] = 0;
		if (strlen(room) > 0) {
			mnptr = (struct mn_roomlist *)
				malloc(sizeof (struct mn_roomlist));
			strcpy(mnptr->roomname, room);
			mnptr->next = mn;
			mn = mnptr;
		}
	}

	pclose(roomfp);

	/* Enter configurations for each room... */
	while (mn != NULL) {
		struct ctdlipcroom *current_room = NULL;
	
		printf("Room <%s>\n", mn->roomname);

		r = CtdlIPCGotoRoom(ipc, mn->roomname, NULL,
				&current_room, buf);
		printf("%s\n", buf);
		if (r / 100 != 2) goto roomerror;

		/* Hey IG, what the hell is SNET? */
		CtdlIPC_putline(ipc, "SNET");
		CtdlIPC_getline(ipc, buf);
		if (buf[0] != '4') goto roomerror;

		snprintf(buf, sizeof buf, "lastsent|%ld", highest);
		CtdlIPC_putline(ipc, buf);

		roomfp = fopen(roomfilename, "r");
		if (roomfp != NULL) {
			while (fgets(buf, sizeof buf, roomfp) != NULL) {
				buf[strlen(buf)-1] = 0;
				extract(node, buf, 0);
				extract(room, buf, 1);
				if (!strcasecmp(room, mn->roomname)) {
					snprintf(buf, sizeof buf,
						"ignet_push_share|%s", node);
					CtdlIPC_putline(ipc, buf);
				}
			}
			fclose(roomfp);
		}

		CtdlIPC_putline(ipc, "000");

roomerror:	/* free this record */
		mnptr = mn->next;
		free(mn);
		mn = mnptr;
	}

	unlink(roomfilename);
	unlink(nodefilename);

	printf("\n\n"
		"If this conversion was successful, you do not need your\n"
		"old network configuration files.  Delete them now? "
	);

	fgets(buf, sizeof buf, stdin);
	if (tolower(buf[0]) != 'y') exit(0);

	get_config();
	system("rm -fr ./network/systems");
	system("rm -f ./network/mail.sysinfo");
	system("rm -f ./network/internetmail.config");

	exit(0);
}
