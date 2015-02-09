/*
 *  CUnit - A Unit testing framework library for C.
 *  Copyright (C) 2001  Anil Kumar
 *  
 *  This library is open source software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 */

#include <stdio.h>
#include <stdlib.h>

#include <sys/select.h>

#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <limits.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <sys/poll.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <sys/utsname.h>
#include <string.h>


typedef void testfunc(int Sock);

#include <string.h>

#include "../lib/libcitadel.h"


int DeZip = -1;
int DeBase64 = -1;

int msock;			/* master listening socket */
int BindPort;
int time_to_die=0;
int n_Lines_to_read = 0;
int blobsize = 0;
int timeout = 5;
int selres = 1;
char ip_addr[256]="0.0.0.0";



static void StreamEncode(void)
{
	IOBuffer ReadBuffer;
	IOBuffer WriteBuffer;
	int err;
	const char *Err = NULL;
	int ret = 0;
	int done = 0;
	vStreamT *vStream;
	
	memset(&ReadBuffer, 0, sizeof(IOBuffer));
	memset(&WriteBuffer, 0, sizeof(IOBuffer));
	ReadBuffer.fd = fileno(stdin);
	WriteBuffer.fd = fileno(stdout);
	ReadBuffer.Buf = NewStrBufPlain(NULL, SIZ*2);
	WriteBuffer.Buf = NewStrBufPlain(NULL, SIZ*2);

	int fdin = fileno(stdin);
	int fdout = fileno(stdout);
	eStreamType ST;

				
	if (DeZip == 0)
		ST = eZLibEncode;
	else if (DeZip == 1)
		ST = eZLibDecode;
	else if (DeBase64 == 0)
		ST = eBase64Encode;
	else if (DeBase64 == 1)
		ST = eBase64Decode;
	else
		ST = eEmtyCodec;
	vStream = StrBufNewStreamContext(ST, &Err);

	while (!done && (fdin >= 0) && (fdout >= 0) && (!feof(stdin)))
	{

		done = StrBuf_read_one_chunk_callback(fdin,
						      0,
						      &ReadBuffer) < (SIZ * 4) -1 ;
		if (IOBufferStrLength(&ReadBuffer) == 0)
		{
			done = 1;
		}
		do
		{
			do {
				ret = StrBufStreamTranscode(ST, &WriteBuffer, &ReadBuffer, NULL, -1, vStream, done, &Err);
				
				while (IOBufferStrLength(&WriteBuffer) > 0)
				{
					err = StrBuf_write_one_chunk_callback(fdout,
									      0,
									      &WriteBuffer);
				}
			} while (ret > 0);
		} while (IOBufferStrLength(&ReadBuffer) > 0);
	}

	
	StrBufDestroyStreamContext(ST, &vStream, &Err);
	
	FreeStrBuf(&ReadBuffer.Buf);
	FreeStrBuf(&WriteBuffer.Buf);
	time_to_die = 1;
}




int main(int argc, char* argv[])
{
	char a;
	setvbuf(stdout, NULL, _IONBF, 0);


	while ((a = getopt(argc, argv, "z:b")) != EOF)
	{
		switch (a) {

		case 'z':
			// zip...
			DeZip = *optarg == 'x';
			break;
		case 'b':
			// base64
			DeBase64 = *optarg == 'x';

		}
	}


	StartLibCitadel(8);

	StreamEncode();
	return 0;
}


/// run -z 1 < ~/testfile1.txt > /tmp/blarg.z
