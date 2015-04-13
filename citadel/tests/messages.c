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
	struct ser_ret smr;
	static int encoded_alloc = 0;
	static char *encoded_msg = NULL;

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
	StrBuf *enc = NewStrBufDup(MsgBuf);
	StrBufDecodeBase64(MsgBuf);
	MsgLen = StrLength(MsgBuf);
	MsgStr = SmashStrBuf(&MsgBuf);
	
	msg = CtdlDeserializeMessage(MsgStr, MsgStr + MsgLen, 1234, 1);

	CtdlSerializeMessage(&smr, msg);


	/* Predict the buffer size we need.  Expand the buffer if necessary. */
	int encoded_len = smr.len * 15 / 10 ;
	if (encoded_len > encoded_alloc) {
		encoded_alloc = encoded_len;
		encoded_msg = realloc(encoded_msg, encoded_alloc);
	}

	if (encoded_msg == NULL) {
		/* Questionable hack that hopes it'll work next time and we only lose one message */
		encoded_alloc = 0;
	}
	else {
		/* Once we do the encoding we know the exact size */
		encoded_len = CtdlEncodeBase64(encoded_msg, (char *)smr.ser, smr.len, 1);
		encoded_msg[encoded_len] = '\0';
	}

	if (strcmp(encoded_msg, ChrPtr(enc))) {
		fprintf(stderr, "doesn't match!\n");
		fwrite(encoded_msg, 1, encoded_len, stdout);
	}
	FreeStrBuf(&enc);
	free(encoded_msg);
	free(smr.ser);
	free(MsgStr);

	return 0;
}
