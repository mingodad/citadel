#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

main() {
	int fdin, fdout;
	char buf[256];

	fdin = open("/dev/tty10", O_RDONLY);
	fdout = open("/dev/tty10", O_WRONLY);
	dup2(fdin, 0);
	dup2(fdout, 1);
	printf("Hello world: ");
	gets(buf);
	printf("Ok then... %s\n", buf);
	exit(0);
	}
