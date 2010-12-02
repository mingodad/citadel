#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#include <fcntl.h>

#include <unistd.h>
#include <stddef.h>


#include "../lib/libcitadel.h"

int main(int argc, char* argv[])
{
	char a;
	int fd;
	char *filename = NULL;
	struct stat statbuf;
	const char *Err;

	StrBuf *MimeBuf;
	long MimeLen;
	char *MimeStr;
	int by_extension = 0;

	setvbuf(stdout, NULL, _IONBF, 0);


	while ((a = getopt(argc, argv, "xf:")) != EOF)
	{
		switch (a) {
		case 'x':
			by_extension = 1;
			break;
		case 'f':
			filename = optarg;
			break;
		}
	}
	StartLibCitadel(8);

	if (filename == NULL) {
		printf("Filename requried! -f\n");
		return 1;
	}

	if (by_extension) {
		printf("Mimetype: %s\n", GuessMimeByFilename(filename, strlen(filename)));
		return 0;
	}

	fd = open(filename, 0);
	if (fd < 0) {
		printf("Error opening file [%s] %d [%s]\n", filename, errno, strerror(errno));
		return 1;
	}
	if (fstat(fd, &statbuf) == -1) {
		printf("Error stating file [%s] %d [%s]\n", filename, errno, strerror(errno));
		return 1;
	}
	MimeBuf = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(MimeBuf, &fd, 1, statbuf.st_size, &Err) < 0) {
		printf("Error reading file [%s] %d [%s] [%s]\n", filename, errno, strerror(errno), Err);
		FreeStrBuf(&MimeBuf);
		return 1;
	}
	MimeLen = StrLength(MimeBuf);
	MimeStr = SmashStrBuf(&MimeBuf);

	printf("Mimetype: %s\n", GuessMimeType(MimeStr, MimeLen));

	free(MimeStr);
	return 0;
}
