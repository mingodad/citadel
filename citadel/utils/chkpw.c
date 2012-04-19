/* 
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <limits.h>
#include <dirent.h>


#include "citadel.h"
#include "sysdep.h"
#include "citadel_dirs.h"
/* These pipes are used to talk to the chkpwd daemon, which is forked during startup */
int chkpwd_write_pipe[2];
int chkpwd_read_pipe[2];

/*
 * Validate a password on the host unix system by talking to the chkpwd daemon
 */
static int validpw(uid_t uid, const char *pass)
{
	char buf[256];
	int rv;

	rv = write(chkpwd_write_pipe[1], &uid, sizeof(uid_t));
	if (rv == -1) {
		printf( "Communicatino with chkpwd broken: %s\n", strerror(errno));
		return 0;
	}

	rv = write(chkpwd_write_pipe[1], pass, 256);
	if (rv == -1) {
		printf( "Communicatino with chkpwd broken: %s\n", strerror(errno));
		return 0;
	}
	rv = read(chkpwd_read_pipe[0], buf, 4);
	if (rv == -1) {
		printf( "Communicatino with chkpwd broken: %s\n", strerror(errno));
		return 0;
	}
	if (!strncmp(buf, "PASS", 4)) {
		printf("pass\n");
		return(1);
	}

	printf("fail\n");
	return 0;
}

/* 
 * Start up the chkpwd daemon so validpw() has something to talk to
 */
void start_chkpwd_daemon(void) {
	pid_t chkpwd_pid;
	struct stat filestats;
	int i;

	printf("Starting chkpwd daemon for host authentication mode\n");

	if ((stat(file_chkpwd, &filestats)==-1) ||
	    (filestats.st_size==0)){
		printf("didn't find chkpwd daemon in %s: %s\n", file_chkpwd, strerror(errno));
		abort();
	}
	if (pipe(chkpwd_write_pipe) != 0) {
		printf("Unable to create pipe for chkpwd daemon: %s\n", strerror(errno));
		abort();
	}
	if (pipe(chkpwd_read_pipe) != 0) {
		printf("Unable to create pipe for chkpwd daemon: %s\n", strerror(errno));
		abort();
	}

	chkpwd_pid = fork();
	if (chkpwd_pid < 0) {
		printf("Unable to fork chkpwd daemon: %s\n", strerror(errno));
		abort();
	}
	if (chkpwd_pid == 0) {
		dup2(chkpwd_write_pipe[0], 0);
		dup2(chkpwd_read_pipe[1], 1);
		for (i=2; i<256; ++i) close(i);
		execl(file_chkpwd, file_chkpwd, NULL);
		printf("Unable to exec chkpwd daemon: %s\n", strerror(errno));
		abort();
		exit(errno);
	}
}



int main(int argc, char **argv) {
	char buf[256];
	struct passwd *p;
	int uid;
	char ctdldir[PATH_MAX]=CTDLDIR;
	
	calc_dirs_n_files(0,0,"", ctdldir, 0);
	
	printf("\n\n ** host auth mode test utility **\n\n");
	start_chkpwd_daemon();

	if (getuid() != 0){
		printf("\n\nERROR: you need to be root to run this!\n\n");
		return(1);
	}
	while(1) {
		printf("\n\nUsername: ");
		fgets(buf, sizeof buf, stdin);
		buf[strlen(buf)-1] = 0;
		p = getpwnam(buf);
		if (p == NULL) {
			printf("Not found\n");
		}
		else {
			uid = p->pw_uid;
			printf("     uid: %d\n", uid);
			printf("Password: ");
			fgets(buf, sizeof buf, stdin);
			buf[strlen(buf)-1] = 0;
			validpw(uid, buf);
		}
	}

	return(0);
}
