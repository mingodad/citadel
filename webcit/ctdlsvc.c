/*
 * $Id: $
 *
 * This is just a quick little hack to start a program in the background,
 * and automatically restart it if it exits with a nonzero exit status.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>
#include <string.h>

char *pidfilename = NULL;
pid_t current_child = 0;

void graceful_shutdown(int signum) {
	kill(current_child, signum);
	if (pidfilename != NULL) {
		unlink(pidfilename);
	}
	exit(0);
}
	

int main(int argc, char **argv)
{
	pid_t child = 0;
	int status = 0;
	FILE *fp;
	int nullfd;

	--argc;
	++argv;

	pidfilename = argv[0];
	--argc;
	++argv;

	if (access(argv[0], X_OK)) {
		fprintf(stderr, "%s: cannot execute\n", argv[0]);
		exit(1);
	}

	/* Close stdin/stdout/stderr and replace them with /dev/null.
	 * We don't just call close() because we don't want these fd's
	 * to be reused for other files.
	 */
	nullfd = open("/dev/null", O_RDWR);
	if (nullfd < 0) {
		fprintf(stderr, "/dev/null: %s\n", strerror(errno));
		exit(2);
	}
	dup2(nullfd, 0);
	dup2(nullfd, 1);
	dup2(nullfd, 2);
	close(nullfd);

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);

	child = fork();
	if (child != 0) {
		fp = fopen(pidfilename, "w");
		if (fp != NULL) {
			fprintf(fp, "%d\n", child);
			fclose(fp);
		}
		exit(0);
	}

	do {
		current_child = fork();

		signal(SIGTERM, graceful_shutdown);
	
		if (current_child < 0) {
			perror("fork");
			exit(errno);
		}
	
		else if (current_child == 0) {
			exit(execvp(argv[0], &argv[0]));
		}
	
		else {
			waitpid(current_child, &status, 0);
		}

	} while (status != 0);

	unlink(pidfilename);
	exit(0);
}

