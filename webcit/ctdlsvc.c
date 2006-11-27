/*
 * $Id: $
 *
 * This is just a quick little hack to start a program and automatically
 * restart it if it exits with a nonzero exit status.
 *
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>


int main(int argc, char **argv)
{
	pid_t child = 0;
	int status = 0;

	--argc;
	++argv;

	do {
		child = fork();
	
		if (child < 0) {
			perror("fork");
			exit(errno);
		}
	
		else if (child == 0) {
			exit(execvp(argv[0], &argv[0]));
		}
	
		else {
			printf("%s: started.  pid = %d\n", argv[0], child);
			waitpid(child, &status, 0);
			printf("Exit code %d\n", status);
		}

	} while (status != 0);

	exit(0);
}

