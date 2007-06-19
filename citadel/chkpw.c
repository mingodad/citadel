/* 
 *
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
#include <string.h>
#include <limits.h>

/* These pipes are used to talk to the chkpwd daemon, which is forked during startup */
int chkpwd_write_pipe[2];
int chkpwd_read_pipe[2];

/*
 * Validate a password on the host unix system by talking to the chkpwd daemon
 */
static int validpw(uid_t uid, const char *pass)
{
	char buf[256];

	write(chkpwd_write_pipe[1], &uid, sizeof(uid_t));
	write(chkpwd_write_pipe[1], pass, 256);
	read(chkpwd_read_pipe[0], buf, 4);

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
	int i;

	printf("Starting chkpwd daemon for host authentication mode\n");

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
		execl("./chkpwd", "chkpwd", NULL);
		printf("Unable to exec chkpwd daemon: %s\n", strerror(errno));
		abort();
		exit(errno);
	}
}



int main(int argc, char **argv) {
	char buf[256];
	struct passwd *p;
	int uid;
	
	printf("\n\n ** host auth mode test utility **\n\n");
	start_chkpwd_daemon();

	while(1) {
		printf("\n\nUsername: ");
		gets(buf);
		p = getpwnam(buf);
		if (p == NULL) {
			printf("Not found\n");
		}
		else {
			uid = p->pw_uid;
			printf("     uid: %d\n", uid);
			printf("Password: ");
			gets(buf);
			validpw(uid, buf);
		}
	}

	return(0);
}
