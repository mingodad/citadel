/* $Id$ */
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include "citadel.h"
#include "tools.h"
#include "ipc.h"

/*
 * This variable defines the amount of network spool data that may be carried
 * in one server transfer command.  For some reason, some networks get hung
 * up on larger packet sizes.  We don't know why.  In any case, never set the
 * packet size higher than 4096 or your server sessions will crash.
 */
#define IGNET_PACKET_SIZE 4000

long atol(const char *);

void serv_read(char *buf, int bytes);
void serv_write(char *buf, int nbytes);
void get_config(void);
struct config config;


void logoff(int code)
{
	exit(code);
}


/*
 * receive network spool from the remote system
 */
void receive_spool(void)
{
	long download_len;
	long bytes_received;
	char buf[256];
	static char pbuf[IGNET_PACKET_SIZE];
	char tempfilename[PATH_MAX];
	long plen;
	FILE *fp;

	sprintf(tempfilename, tmpnam(NULL));
	serv_puts("NDOP");
	serv_gets(buf);
	printf("%s\n", buf);
	if (buf[0] != '2')
		return;
	download_len = extract_long(&buf[4], 0);

	bytes_received = 0L;
	fp = fopen(tempfilename, "w");
	if (fp == NULL) {
		perror("cannot open download file locally");
		return;
	}
	while (bytes_received < download_len) {
		sprintf(buf, "READ %ld|%ld",
			bytes_received,
		     ((download_len - bytes_received > IGNET_PACKET_SIZE)
		 ? IGNET_PACKET_SIZE : (download_len - bytes_received)));
		serv_puts(buf);
		serv_gets(buf);
		if (buf[0] == '6') {
			plen = extract_long(&buf[4], 0);
			serv_read(pbuf, plen);
			fwrite((char *) pbuf, plen, 1, fp);
			bytes_received = bytes_received + plen;
		}
	}

	fclose(fp);
	serv_puts("CLOS");
	serv_gets(buf);
	printf("%s\n", buf);
	sprintf(buf, "mv %s %s/network/spoolin/netpoll.%ld",
		tempfilename, BBSDIR, (long) getpid());
	system(buf);
	system("exec nohup ./netproc >/dev/null 2>&1 &");
}

/*
 * transmit network spool to the remote system
 */
void transmit_spool(char *remote_nodename)
{
	char buf[256];
	char pbuf[4096];
	long plen;
	long bytes_to_write, thisblock;
	int fd;
	char sfname[128];

	serv_puts("NUOP");
	serv_gets(buf);
	printf("%s\n", buf);
	if (buf[0] != '2')
		return;

	sprintf(sfname, "%s/network/spoolout/%s", BBSDIR, remote_nodename);
	fd = open(sfname, O_RDONLY);
	if (fd < 0) {
		if (errno == ENOENT) {
			printf("Nothing to send.\n");
		} else {
			perror("cannot open upload file locally");
		}
		return;
	}
	while (plen = (long) read(fd, pbuf, IGNET_PACKET_SIZE), plen > 0L) {
		bytes_to_write = plen;
		while (bytes_to_write > 0L) {
			sprintf(buf, "WRIT %ld", bytes_to_write);
			serv_puts(buf);
			serv_gets(buf);
			thisblock = atol(&buf[4]);
			if (buf[0] == '7') {
				serv_write(pbuf, (int) thisblock);
				bytes_to_write = bytes_to_write - thisblock;
			} else {
				goto ABORTUPL;
			}
		}
	}

ABORTUPL:
	close(fd);
	serv_puts("UCLS 1");
	serv_gets(buf);
	printf("%s\n", buf);
	if (buf[0] == '2')
		unlink(sfname);
}




int main(int argc, char **argv)
{
	char buf[256];
	char remote_nodename[32];
	int a;

	if (argc != 4) {
		fprintf(stderr,
			"%s: usage: %s <address> <port number> <remote netpassword>\n",
			argv[0], argv[0]);
		exit(1);
	}
	get_config();

	attach_to_server(argc, argv, NULL, NULL);
	serv_gets(buf);
	printf("%s\n", buf);
	if ((buf[0] != '2') && (strncmp(buf, "551", 3))) {
		fprintf(stderr, "%s: %s\n", argv[0], &buf[4]);
		logoff(atoi(buf));
	}
	serv_puts("INFO");
	serv_gets(buf);
	if (buf[0] == '1') {
		a = 0;
		while (serv_gets(buf), strcmp(buf, "000")) {
			if (a == 1)
				strcpy(remote_nodename, buf);
			if (a == 1)
				printf("Connected to: %s ", buf);
			if (a == 2)
				printf("(%s) ", buf);
			if (a == 6)
				printf("%s\n", buf);
			++a;
		}
	}
	if (!strcmp(remote_nodename, config.c_nodename)) {
		fprintf(stderr, "Connected to local system\n");
	} else {
		sprintf(buf, "NETP %s|%s", config.c_nodename, argv[3]);
		serv_puts(buf);
		serv_gets(buf);
		printf("%s\n", buf);

		/* only do the transfers if we authenticated correctly! */
		if (buf[0] == '2') {
			receive_spool();
			transmit_spool(remote_nodename);
		}
	}

	serv_puts("QUIT");
	serv_gets(buf);
	exit(0);
}
