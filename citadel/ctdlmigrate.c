/*
 * $Id: $
 *
 * Across-the-wire migration utility for Citadel
 *
 * Copyright (c) 2009 citadel.org
 *
 * This program is licensed to you under the terms of the GNU General Public License v3
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




int main(int argc, char *argv[])
{
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;
	char yesno[5];
	char sendcommand[PATH_MAX];
	int exitcode;
	char cmd[PATH_MAX];
	char socket_path[PATH_MAX];
	char remote_user[256];
	char remote_host[256];
	
	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);
	CtdlMakeTempFileName(socket_path, sizeof socket_path);

	system("clear");
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
	exitcode = system(cmd);
	if (exitcode != 0) {
		printf("\nctdlmigrate was unable to attach to the Citadel server\n"
			"here on the target system.  Is Citadel running?\n\n");
		exit(1);
	}
	printf("\nOK, this side is ready to go.  Now we must connect to the source system.\n\n");

	printf("Enter the host name or IP address of the source system\n"
		"(example: ctdl.foo.org)\n"
		"--> ");
	gets(remote_host);
	printf("\nEnter the name of a user on %s who has full access to Citadel files\n"
		"(usually root)\n--> ",
		remote_host);
	gets(remote_user);

	printf("\nEstablishing an SSH connection to the source system...\n\n");
	unlink(socket_path);
	snprintf(cmd, sizeof cmd, "ssh -MNf -S %s %s@%s", socket_path, remote_user, remote_host);
	exitcode = system(cmd);
	if (exitcode != 0) {
		printf("\nctdlmigrate was unable to establish an SSH connection to the\n"
			"source system, and cannot continue.\n\n");
		exit(exitcode);
	}

	printf("\nTesting a command over the connection...\n\n");
	snprintf(cmd, sizeof cmd, "ssh -S %s %s@%s 'echo Remote commands are executing successfully.'",
		socket_path, remote_user, remote_host);
	exitcode = system(cmd);
	printf("\n");
	if (exitcode != 0) {
		printf("Remote commands are not succeeding.\n\n");
		exit(exitcode);
	}



	// FIXME kill the master ssh session
	printf("If this program was finished we would do more.  FIXME\n");
	exit(0);
}
