/*
 * $Id$
 *
 * Command-line utility to transmit a server command.
 *
 */


#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <signal.h>
#include <errno.h>
#include <limits.h>
#include "citadel.h"
#include "tools.h"
#include "ipc.h"
#include "config.h"

#define LOCKFILE "/tmp/LCK.sendcommand"

struct config config;
extern int home_specified;


/*
 * make sure only one copy of sendcommand runs at a time, using lock files
 */
int set_lockfile(void)
{
	FILE *lfp;
	int onppid;

	if ((lfp = fopen(LOCKFILE, "r")) != NULL) {
		fscanf(lfp, "%d", &onppid);
		fclose(lfp);
		if (!kill(onppid, 0) || errno == EPERM)
			return 1;
	}
	lfp = fopen(LOCKFILE, "w");
	fprintf(lfp, "%ld\n", (long) getpid());
	fclose(lfp);
	return (0);
}

void remove_lockfile(void)
{
	unlink(LOCKFILE);
}

/*
 * Why both cleanup() and nq_cleanup() ?  Notice the alarm() call in
 * cleanup() .  If for some reason sendcommand hangs waiting for the server
 * to clean up, the alarm clock goes off and the program exits anyway.
 * The cleanup() routine makes a check to ensure it's not reentering, in
 * case the ipc module looped it somehow.
 */
void nq_cleanup(int e)
{
	remove_lockfile();
	exit(e);
}

void cleanup(int e)
{
	static int nested = 0;

	alarm(30);
	signal(SIGALRM, nq_cleanup);
	if (nested++ < 1)
		serv_puts("QUIT");
	nq_cleanup(e);
}

/*
 * This is implemented as a function rather than as a macro because the
 * client-side IPC modules expect logoff() to be defined.  They call logoff()
 * when a problem connecting or staying connected to the server occurs.
 */
void logoff(int e)
{
	cleanup(e);
}

/*
 * Connect sendcommand to the Citadel server running on this computer.
 */
void np_attach_to_server(void)
{
	char hostbuf[SIZ], portbuf[SIZ];
	char buf[SIZ];
	char *args[] =
	{"sendcommand", NULL};

	fprintf(stderr, "Attaching to server...\n");
	attach_to_server(1, args, hostbuf, portbuf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", &buf[4]);
	sprintf(buf, "IPGM %d", config.c_ipgm_secret);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", &buf[4]);
	if (buf[0] != '2') {
		cleanup(2);
	}
}



/*
 * main
 */
int main(int argc, char **argv)
{
	int a;
	char cmd[SIZ];
	char buf[SIZ];

	strcpy(bbs_home_directory, BBSDIR);

	strcpy(cmd, "");
	/*
	 * Change directories if specified
	 */
	for (a = 1; a < argc; ++a) {
		if (!strncmp(argv[a], "-h", 2)) {
			strcpy(bbs_home_directory, argv[a]);
			strcpy(bbs_home_directory, &bbs_home_directory[2]);
			home_specified = 1;
		} else {
			if (strlen(cmd) > 0)
				strcat(cmd, " ");
			strcat(cmd, argv[a]);
		}
	}

	get_config();

	signal(SIGINT, cleanup);
	signal(SIGQUIT, cleanup);
	signal(SIGHUP, cleanup);
	signal(SIGTERM, cleanup);

	fprintf(stderr, "sendcommand: started.  pid=%ld\n", (long) getpid());
	fprintf(stderr, "Running from %s\n", bbs_home_directory);
	fflush(stderr);
	np_attach_to_server();
	fflush(stderr);

	fprintf(stderr, "%s\n", cmd);
	serv_puts(cmd);
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);

	if (buf[0] == '1') {
		while (serv_gets(buf), strcmp(buf, "000")) {
			printf("%s\n", buf);
		}
	} else if (buf[0] == '4') {
		do {
			if (fgets(buf, sizeof buf, stdin) == NULL)
				strcpy(buf, "000");
			if (strlen(buf) > 0)
				if (buf[strlen(buf) - 1] == '\n')
					buf[strlen(buf) - 1] = 0;
			if (strlen(buf) > 0)
				if (buf[strlen(buf) - 1] == '\r')
					buf[strlen(buf) - 1] = 0;
			if (strcmp(buf, "000"))
				serv_puts(buf);
		} while (strcmp(buf, "000"));
		serv_puts("000");
	}
	fprintf(stderr, "sendcommand: processing ended.\n");
	cleanup(0);
	return 0;
}
