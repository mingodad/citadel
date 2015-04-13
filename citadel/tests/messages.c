/*
 * Implements tests for the message store.
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
#include <regex.h>
#include <libcitadel.h>

#include "md5.h"

#include "ctdl_module.h"
#include "citserver.h"
#include "control.h"
#include "clientsocket.h"
#include "genstamp.h"
#include "room_ops.h"
#include "user_ops.h"

#include "internet_addressing.h"
#include "euidindex.h"
#include "msgbase.h"
#include "journaling.h"


int main(int argc, char**argv) {
	char a;
	int fd;
	char *filename = NULL;
	struct stat statbuf;
	const char *Err;

	StrBuf *MsgBuf;
	long MsgLen;
	char *MsgStr;
	int do_proto = 0;
	int dont_decode = 1;
	struct CtdlMessage *msg;

	setvbuf(stdout, NULL, _IONBF, 0);

	while ((a = getopt(argc, argv, "dpf:P:")) != EOF)
	{
		switch (a) {
		case 'f':
			filename = optarg;
			break;

		case 'p':
			do_proto = 1;
			break;

		case 'd':
			dont_decode = 0;
			break;

		}
	}
	StartLibCitadel(8);

	if (filename == NULL) {
		printf("Filename requried! -f\n");
		return 1;
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
	MsgBuf = NewStrBufPlain(NULL, statbuf.st_size + 1);
	if (StrBufReadBLOB(MsgBuf, &fd, 1, statbuf.st_size, &Err) < 0) {
		printf("Error reading file [%s] %d [%s] [%s]\n", filename, errno, strerror(errno), Err);
		FreeStrBuf(&MsgBuf);
		return 1;
	}
	StrBufDecodeBase64(MsgBuf);
	MsgLen = StrLength(MsgBuf);
	MsgStr = SmashStrBuf(&MsgBuf);
	
	msg = CtdlDeserializeMessage(MsgStr, MsgStr + MsgLen, 1234, 1);
	free(MsgStr);

	return 0;
}
