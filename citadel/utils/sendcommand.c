/*
 * Command-line utility to transmit a server command.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <string.h>
#include <fcntl.h>
#include <stdio.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>
#include <limits.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "citadel.h"
#include "include/citadel_dirs.h"
#include <libcitadel.h>


int serv_sock = (-1);


int uds_connectsock(char *sockpath)
{
	int s;
	struct sockaddr_un addr;

	memset(&addr, 0, sizeof(addr));
	addr.sun_family = AF_UNIX;
	strncpy(addr.sun_path, sockpath, sizeof addr.sun_path);

	s = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s < 0) {
		fprintf(stderr, "sendcommand: Can't create socket: %s\n", strerror(errno));
		exit(3);
	}

	if (connect(s, (struct sockaddr *) &addr, sizeof(addr)) < 0) {
		fprintf(stderr, "sendcommand: can't connect: %s\n", strerror(errno));
		close(s);
		exit(3);
	}

	return s;
}


/*
 * input binary data from socket
 */
void serv_read(char *buf, int bytes)
{
	int len, rlen;

	len = 0;
	while (len < bytes) {
		rlen = read(serv_sock, &buf[len], bytes - len);
		if (rlen < 1) {
			return;
		}
		len = len + rlen;
	}
}


/*
 * send binary to server
 */
void serv_write(char *buf, int nbytes)
{
	int bytes_written = 0;
	int retval;
	while (bytes_written < nbytes) {
		retval = write(serv_sock, &buf[bytes_written], nbytes - bytes_written);
		if (retval < 1) {
			return;
		}
		bytes_written = bytes_written + retval;
	}
}



/*
 * input string from socket - implemented in terms of serv_read()
 */
void serv_gets(char *buf)
{
	int i;

	/* Read one character at a time.
	 */
	for (i = 0;; i++) {
		serv_read(&buf[i], 1);
		if (buf[i] == '\n' || i == (SIZ-1))
			break;
	}

	/* If we got a long line, discard characters until the newline.
	 */
	if (i == (SIZ-1)) {
		while (buf[i] != '\n') {
			serv_read(&buf[i], 1);
		}
	}

	/* Strip all trailing nonprintables (crlf)
	 */
	buf[i] = 0;
}


/*
 * send line to server - implemented in terms of serv_write()
 */
void serv_puts(char *buf)
{
	serv_write(buf, strlen(buf));
	serv_write("\n", 1);
}




/*
 * Main loop.  Do things and have fun.
 */
int main(int argc, char **argv)
{
	int a;
	int watchdog = 60;
	char buf[SIZ];
	int xfermode = 0;
	int relh=0;
	int home=0;
	char relhome[PATH_MAX]="";
	char ctdldir[PATH_MAX]=CTDLDIR;

	StartLibCitadel(SIZ);

	/* Parse command line */
	while ((a = getopt(argc, argv, "h:w:")) != EOF) {
		switch (a) {
		case 'h':
			relh=optarg[0]!='/';
			if (!relh) {
				strncpy(ctdl_home_directory, optarg, sizeof ctdl_home_directory);
			} else {
				strncpy(relhome, optarg, sizeof relhome);
			}
			home = 1;
			break;
		case 'w':
			watchdog = atoi(optarg);
			break;
		default:
			fprintf(stderr, "sendcommand: usage: sendcommand [-h server_dir] [-w watchdog_timeout]\n");
			return(1);
		}
	}

	calc_dirs_n_files(relh, home, relhome, ctdldir, 0);

	fprintf(stderr, "sendcommand: started (pid=%d) connecting to Citadel server at %s\n",
		(int) getpid(),
		file_citadel_admin_socket
	);
	fflush(stderr);

	alarm(watchdog);
	serv_sock = uds_connectsock(file_citadel_admin_socket);

	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);

	strcpy(buf, "");
	for (a=optind; a<argc; ++a) {
		if (a != optind) {
			strcat(buf, " ");
		}
		strcat(buf, argv[a]);
	}

	fprintf(stderr, "%s\n", buf);
	serv_puts(buf);
	serv_gets(buf);
	fprintf(stderr, "%s\n", buf);

	xfermode = buf[0];

	if ((xfermode == '4') || (xfermode == '8')) {		/* send text */
		IOBuffer IOB;
		FDIOBuffer FDIO;
		const char *ErrStr;

		memset(&IOB, 0, sizeof(0));
		IOB.Buf = NewStrBufPlain(NULL, SIZ);
		IOB.fd = serv_sock;
		FDIOBufferInit(&FDIO, &IOB, fileno(stdin), -1);

		while (FileSendChunked(&FDIO, &ErrStr) >= 0)
			alarm(watchdog);			/* reset the watchdog timer */
		if (ErrStr != NULL)
			fprintf(stderr, "Error while piping stuff: %s\n", ErrStr);
		FDIOBufferDelete(&FDIO);
		FreeStrBuf(&IOB.Buf);
		serv_puts("000");
	}

	if ((xfermode == '1') || (xfermode == '8')) {		/* receive text */
		IOBuffer IOB;
		StrBuf *Line, *OutBuf;
		int Finished = 0;

		memset(&IOB, 0, sizeof(IOB));
		IOB.Buf = NewStrBufPlain(NULL, SIZ);
		IOB.fd = serv_sock;
		Line = NewStrBufPlain(NULL, SIZ);
		OutBuf = NewStrBufPlain(NULL, SIZ * 10);

		while (!Finished && (StrBuf_read_one_chunk_callback (serv_sock, 0, &IOB) >= 0))
		{
			eReadState State;

			State = eReadSuccess;
			while (!Finished && (State == eReadSuccess))
			{
				if (IOBufferStrLength(&IOB) == 0)
				{
					State = eMustReadMore;
					break;
				}
				State = StrBufChunkSipLine(Line, &IOB);
				switch (State)
				{
				case eReadSuccess:
					if (!strcmp(ChrPtr(Line), "000"))
					{
						Finished = 1;
						break;
					}
					StrBufAppendBuf(OutBuf, Line, 0);
					StrBufAppendBufPlain(OutBuf, HKEY("\n"), 0);
					alarm(watchdog);			/* reset the watchdog timer */
					break;
				case eBufferNotEmpty:
					break;
				case eMustReadMore:
					continue;
				case eReadFail:
					fprintf(stderr, "WTF? Exit!\n");
					exit(-1);
					break;
				}
				if (StrLength(OutBuf) > 5*SIZ)
				{
					fwrite(ChrPtr(OutBuf), 1, StrLength(OutBuf), stdout);
					FlushStrBuf(OutBuf);
				}
			}
		}
		if (StrLength(OutBuf) > 0)
		{
			fwrite(ChrPtr(OutBuf), 1, StrLength(OutBuf), stdout);
		}
		FreeStrBuf(&Line);
		FreeStrBuf(&OutBuf);
		FreeStrBuf(&IOB.Buf);
	}
	
	if (xfermode == '6') {					/* receive binary */
		size_t len = atoi(&buf[4]);
		size_t bytes_remaining = len;

		while (bytes_remaining > 0) {
			size_t this_block = bytes_remaining;
			if (this_block > SIZ) this_block = SIZ;
			serv_read(buf, this_block);
			fwrite(buf, this_block, 1, stdout);
			bytes_remaining -= this_block;
		}
	}

	close(serv_sock);
	alarm(0);						/* cancel the watchdog timer */
	fprintf(stderr, "sendcommand: processing ended.\n");
	if (xfermode == '5') {
		return(1);
	}
	return(0);
}














