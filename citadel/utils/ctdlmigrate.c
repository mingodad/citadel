/*
 * Across-the-wire migration utility for Citadel
 *
 * Yes, we used goto, and gets(), and committed all sorts of other heinous sins here.
 * The scope of this program isn't wide enough to make a difference.  If you don't like
 * it you can rewrite it.
 *
 * Copyright (c) 2009-2016 citadel.org
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * (Note: a useful future enhancement might be to support "-h" on both sides)
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <signal.h>
#include <netdb.h>
#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <time.h>
#include <libcitadel.h>
#include "citadel.h"
#include "axdefs.h"
#include "sysdep.h"
#include "config.h"
#include "citadel_dirs.h"
#if HAVE_BACKTRACE
#include <execinfo.h>
#endif



/*
 * Replacement for gets() that doesn't throw a compiler warning.
 * We're only using it for some simple prompts, so we don't need
 * to worry about attackers exploiting it.
 */
void getz(char *buf) {
	char *ptr;

	ptr = fgets(buf, SIZ, stdin);
	if (!ptr) {
		buf[0] = 0;
		return;
	}

	ptr = strchr(buf, '\n');
	if (ptr) *ptr = 0;
}





int main(int argc, char *argv[])
{
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	char yesno[5];
	char sendcommand[PATH_MAX];
	int cmdexit;
	char cmd[PATH_MAX];
	char buf[PATH_MAX];
	char socket_path[PATH_MAX];
	char remote_user[SIZ];
	char remote_host[SIZ];
	char remote_sendcommand[PATH_MAX];
	FILE *sourcefp = NULL;
	FILE *targetfp = NULL;
	int linecount = 0;
	char spinning[4] = "-\\|/" ;
	int exitcode = 0;
	
	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);
	CtdlMakeTempFileName(socket_path, sizeof socket_path);

	cmdexit = system("clear");
	printf(	"-------------------------------------------\n"
		"Over-the-wire migration utility for Citadel\n"
		"-------------------------------------------\n"
		"\n"
		"This utility is designed to migrate your Citadel installation\n"
		"to a new host system via a network connection, without disturbing\n"
		"the source system.  The target may be a different CPU architecture\n"
		"and/or operating system.  The source system should be running\n"
		"Citadel %d.%02d or newer, and the target system should be running\n"
		"either the same version or a newer version.  You will also need\n"
		"the 'rsync' utility, and OpenSSH v4 or newer.\n"
		"\n"
		"You must run this utility on the TARGET SYSTEM.  Any existing data\n"
		"on this system will be ERASED.\n"
		"\n"
		"Do you wish to continue? "
		,
		EXPORT_REV_MIN / 100,
		EXPORT_REV_MIN % 100
	);

	if ((fgets(yesno, sizeof yesno, stdin) == NULL) || (tolower(yesno[0]) != 'y')) {
		exit(0);
	}

	printf("\n\nGreat!  First we will check some things out here on our target\n"
		"system to make sure it is ready to receive data.\n\n");

	printf("Locating 'sendcommand' and checking connectivity to Citadel...\n");
	snprintf(sendcommand, sizeof sendcommand, "%s/sendcommand", ctdl_utilbin_dir);
	snprintf(cmd, sizeof cmd, "%s 'NOOP'", sendcommand);
	cmdexit = system(cmd);
	if (cmdexit != 0) {
		printf("\nctdlmigrate was unable to attach to the Citadel server\n"
			"here on the target system.  Is Citadel running?\n\n");
		exit(1);
	}
	printf("\nOK, this side is ready to go.  Now we must connect to the source system.\n\n");

	printf("Enter the host name or IP address of the source system\n"
		"(example: ctdl.foo.org)\n"
		"--> ");
	getz(remote_host);

get_remote_user:
	printf("\nEnter the name of a user on %s who has full access to Citadel files\n"
		"(usually root)\n--> ",
		remote_host);
	getz(remote_user);
	if (IsEmptyStr(remote_user))
		goto get_remote_user;

	printf("\nEstablishing an SSH connection to the source system...\n\n");
	unlink(socket_path);
	snprintf(cmd, sizeof cmd, "ssh -MNf -S %s %s@%s", socket_path, remote_user, remote_host);
	cmdexit = system(cmd);
	printf("\n");
	if (cmdexit != 0) {
		printf("This program was unable to establish an SSH session to the source system.\n\n");
		exitcode = cmdexit;
		goto THEEND;
	}

	printf("\nTesting a command over the connection...\n\n");
	snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s 'echo Remote commands are executing successfully.'",
		socket_path, remote_user, remote_host);
	cmdexit = system(cmd);
	printf("\n");
	if (cmdexit != 0) {
		printf("Remote commands are not succeeding.\n\n");
		exitcode = cmdexit;
		goto THEEND;
	}

	printf("\nLocating the remote 'sendcommand' and Citadel installation...\n");
	snprintf(remote_sendcommand, sizeof remote_sendcommand, "/usr/local/citadel/sendcommand");
	snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s %s NOOP",
		socket_path, remote_user, remote_host, remote_sendcommand);
	cmdexit = system(cmd);
	if (cmdexit != 0) {
		snprintf(remote_sendcommand, sizeof remote_sendcommand, "/usr/sbin/sendcommand");
		snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s %s NOOP",
			socket_path, remote_user, remote_host, remote_sendcommand);
		cmdexit = system(cmd);
	}
	if (cmdexit != 0) {
		printf("\nUnable to locate Citadel programs on the remote system.  Please enter\n"
			"the name of the directory on %s which contains the 'sendcommand' program.\n"
			"(example: /opt/foo/citadel)\n"
			"--> ", remote_host);
		getz(buf);
		snprintf(remote_sendcommand, sizeof remote_sendcommand, "%s/sendcommand", buf);
		snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s %s NOOP",
			socket_path, remote_user, remote_host, remote_sendcommand);
		cmdexit = system(cmd);
	}
	printf("\n");
	if (cmdexit != 0) {
		printf("ctdlmigrate was unable to attach to the remote Citadel system.\n\n");
		exitcode = cmdexit;
		goto THEEND;
	}

	printf("ctdlmigrate will now begin a database migration...\n");
	printf("  if the system doesn't start working, \n");
	printf("  have a look at the syslog for pending jobs needing to be terminated.\n");

	snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s %s -w3600 MIGR export",
		socket_path, remote_user, remote_host, remote_sendcommand);
	sourcefp = popen(cmd, "r");
	if (!sourcefp) {
		printf("\n%s\n\n", strerror(errno));
		exitcode = 2;
		goto THEEND;
	}

	snprintf(cmd, sizeof cmd, "%s -w3600 MIGR import", sendcommand);
	targetfp = popen(cmd, "w");
	if (!targetfp) {
		printf("\n%s\n\n", strerror(errno));
		exitcode = 3;
		goto THEEND;
	}

	while (fgets(buf, sizeof buf, sourcefp) != NULL) {
		if (fwrite(buf, strlen(buf), 1, targetfp) < 1) {
			exitcode = 4;
			printf("%s\n", strerror(errno));
			goto FAIL;
		}
		++linecount;
		if ((linecount % 100) == 0) {
			printf("%c\r", spinning[((linecount / 100) % 4)]);
			fflush(stdout);
		}
	}

FAIL:	if (sourcefp) pclose(sourcefp);
	if (targetfp) pclose(targetfp);
	if (exitcode != 0) goto THEEND;

	/* We need to copy a bunch of other stuff, and will do so using rsync */

	snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s %s MIGR listdirs",
		socket_path, remote_user, remote_host, remote_sendcommand);
	sourcefp = popen(cmd, "r");
	if (!sourcefp) {
		printf("\n%s\n\n", strerror(errno));
		exitcode = 2;
		goto THEEND;
	}
	while ((fgets(buf, sizeof buf, sourcefp)) && (strcmp(buf, "000"))) {
		buf[strlen(buf)-1] = 0;

		if (!strncasecmp(buf, "files|", 6)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[6], ctdl_file_dir);
		}
		else if (!strncasecmp(buf, "userpics|", 9)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[9], ctdl_usrpic_dir);
		}
		else if (!strncasecmp(buf, "messages|", 9)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[9], ctdl_message_dir);
		}
		else if (!strncasecmp(buf, "keys|", 5)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[5], ctdl_key_dir);
		}
		else if (!strncasecmp(buf, "images|", 7)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[7], ctdl_image_dir);
		}
		else if (!strncasecmp(buf, "info|", 5)) {
			snprintf(cmd, sizeof cmd, "rsync -va --rsh='ssh -S %s' %s@%s:%s/ %s/",
				socket_path, remote_user, remote_host, &buf[5], ctdl_info_dir);
		}
		else {
			strcpy(cmd, "false");	/* cheap and sleazy way to throw an error */
		}
		printf("%s\n", cmd);
		cmdexit = system(cmd);
		if (cmdexit != 0) {
			exitcode += cmdexit;
		}
	}
	pclose(sourcefp);

THEEND:	if (exitcode == 0) {
		printf("\n\n *** Citadel migration was successful! *** \n\n");
	}
	else {
		printf("\n\n *** Citadel migration was unsuccessful. *** \n\n");
	}
	printf("\nShutting down the socket connection...\n\n");
	snprintf(cmd, sizeof cmd, "ssh -S %s -N -O exit %s@%s",
		socket_path, remote_user, remote_host);
	cmdexit = system(cmd);
	printf("\n");
	if (cmdexit != 0) {
		printf("There was an error shutting down the socket.\n\n");
		exitcode = cmdexit;
	}

	unlink(socket_path);
	exit(exitcode);
}
