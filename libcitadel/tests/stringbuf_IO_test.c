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
#include <pthread.h>
#include <signal.h>
#include <sys/utsname.h>
#include <string.h>


typedef void testfunc(int Sock);

#define LISTEN_QUEUE_LENGTH 100
#include <string.h>

#include "stringbuf_test.h"
#include "../lib/libcitadel.h"

int msock;			/* master listening socket */
int BindPort;
int time_to_die=0;
int listen_port=6666;
int n_Lines_to_read = 0;
int blobsize = 0;
int timeout = 5;
int selres = 1;
char ip_addr[256]="0.0.0.0";


static void TestRevalidateStrBuf(StrBuf *Buf)
{
	CU_ASSERT(strlen(ChrPtr(Buf)) == StrLength(Buf));
}

/* 
 * This is a generic function to set up a master socket for listening on
 * a TCP port.  The server shuts down if the bind fails.
 *
 * ip_addr 	IP address to bind
 * port_number	port number to bind
 * queue_len	number of incoming connections to allow in the queue
 */
static int ig_tcp_server(char *ip_addr, int port_number, int queue_len)
{
	struct protoent *p;
	struct sockaddr_in sin;
	int s, i;

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	if (ip_addr == NULL) {
		sin.sin_addr.s_addr = INADDR_ANY;
	} else {
		sin.sin_addr.s_addr = inet_addr(ip_addr);
	}

	if (sin.sin_addr.s_addr == INADDR_NONE) {
		sin.sin_addr.s_addr = INADDR_ANY;
	}

	if (port_number == 0) {
		printf("Cannot start: no port number specified.\n");
		return (-1);
	}
	sin.sin_port = htons((u_short) port_number);

	p = getprotobyname("tcp");

	s = socket(PF_INET, SOCK_STREAM, (p->p_proto));
	if (s < 0) {
		printf("Can't create a socket: %s\n", strerror(errno));
		return (-2);
	}
	/* Set some socket options that make sense. */
	i = 1;
	setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &i, sizeof(i));

	#ifndef __APPLE__
	fcntl(s, F_SETFL, O_NONBLOCK); /* maide: this statement is incorrect
					  there should be a preceding F_GETFL
					  and a bitwise OR with the previous
					  fd flags */
	#endif
	
	if (bind(s, (struct sockaddr *) &sin, sizeof(sin)) < 0) {
		printf("Can't bind: %s\n", strerror(errno));
		return (-3);
	}
	if (listen(s, queue_len) < 0) {
		printf("Can't listen: %s\n", strerror(errno));
		return (-4);
	}
	return (s);
}



/*
 * Entry point for worker threads
 */
static void worker_entry(testfunc F)
{
	int ssock;
	int i = 0;
	int ret;
	struct timeval tv;
	fd_set readset, tempset;

	
	tv.tv_sec = 0;
	tv.tv_usec = 10000;
	FD_ZERO(&readset);
	FD_SET(msock, &readset);

	do {
		/* Only one thread can accept at a time */
		ssock = -1; 
		errno = EAGAIN;
		do {
			ret = -1; /* just one at once should select... */
			
			FD_ZERO(&tempset);
			if (msock > 0) FD_SET(msock, &tempset);
			tv.tv_sec = 0;
			tv.tv_usec = 10000;
			if (msock > 0)	
				ret = select(msock+1, &tempset, NULL, NULL,  &tv);
			if ((ret < 0) && (errno != EINTR) && (errno != EAGAIN))
			{/* EINTR and EAGAIN are thrown but not of interest. */
				printf("accept() failed:%d %s\n",
					errno, strerror(errno));
			}
			else if ((ret > 0) && (msock > 0) && FD_ISSET(msock, &tempset))
			{/* Successfully selected, and still not shutting down? Accept! */
				ssock = accept(msock, NULL, 0);
			}
			
		} while ((msock > 0) && (ssock < 0)  && (time_to_die == 0));

		if ((msock == -1)||(time_to_die))
		{/* ok, we're going down. */
			return;
		}
		if (ssock < 0 ) continue;

		if (msock < 0) {
			if (ssock > 0) close (ssock);
			printf( "inbetween.");
			return;
		} else { /* Got it? do some real work! */
			/* Set the SO_REUSEADDR socket option */
			int fdflags; 
			i = 1;
			setsockopt(ssock, SOL_SOCKET, SO_REUSEADDR,
				   &i, sizeof(i));

			fdflags = fcntl(ssock, F_GETFL);
			if (fdflags < 0)
				printf("unable to get server socket flags! %s \n",
					strerror(errno));
			fdflags = fdflags | O_NONBLOCK;
			if (fcntl(ssock, F_SETFL, fdflags) < 0)
				printf("unable to set server socket nonblocking flags! %s \n",
					strerror(errno));


			F(ssock);
		}

	} while (!time_to_die);
	printf ("bye\n");
}


static void SimpleLineBufTestFunc(int sock)
{
	StrBuf *ReadBuffer;
	StrBuf *Line;
	const char *Pos = NULL;
	const char *err = NULL;
	int i;

	ReadBuffer = NewStrBuf();
	Line = NewStrBuf();

	for (i = 0; i < n_Lines_to_read; i++) {
		StrBufTCP_read_buffered_line_fast(Line, 
						  ReadBuffer, 
						  &Pos,
						  &sock,
						  timeout,
						  selres,
						  &err);
		TestRevalidateStrBuf(Line);
		if (err != NULL)
			printf("%s", err);
		CU_ASSERT_PTR_NULL(err);
		CU_ASSERT_NOT_EQUAL(sock, -1);
		if (sock == -1)
			break;
		printf("LINE: >%s<\n", ChrPtr(Line));
	}
	FreeStrBuf(&ReadBuffer);
	FreeStrBuf(&Line);
	time_to_die = 1;
}

static void SimpleLinebufferTest(void)
{
	msock = ig_tcp_server(ip_addr, listen_port, LISTEN_QUEUE_LENGTH);

	worker_entry(SimpleLineBufTestFunc);
	close (msock);
}


static void SimpleBlobTestFunc(int sock)
{
	StrBuf *ReadBuffer;
	StrBuf *Blob;
	const char *Pos = NULL;
	const char *err = NULL;
	
	ReadBuffer = NewStrBuf();
	Blob = NewStrBuf();

	StrBufReadBLOBBuffered(Blob, 
			       ReadBuffer, 
			       &Pos,
			       &sock,
			       0,
			       blobsize,
			       0,
			       &err);
	TestRevalidateStrBuf(Blob);
	if (err != NULL)
		printf("%s", err);
	CU_ASSERT(blobsize == StrLength(Blob));
	CU_ASSERT_PTR_NULL(err);
	CU_ASSERT_NOT_EQUAL(sock, -1);
	if (sock == -1)
	printf("BLOB: >%s<\n", ChrPtr(Blob));
	
	FreeStrBuf(&ReadBuffer);
	FreeStrBuf(&Blob);
	time_to_die = 1;
}


static void SimpleHttpPostTestFunc(int sock)
{
	StrBuf *ReadBuffer;
	StrBuf *Blob;
	StrBuf *Line;
	const char *Pos = NULL;
	const char *err = NULL;
	int blobsize = 0;
	int i;
	const char *pch;
	
	ReadBuffer = NewStrBuf();
	Blob = NewStrBuf();
	Line = NewStrBuf();

	for (i = 0; 1; i++) {
		StrBufTCP_read_buffered_line_fast(Line, 
						  ReadBuffer, 
						  &Pos,
						  &sock,
						  timeout,
						  selres,
						  &err);
		TestRevalidateStrBuf(Line);
		if (err != NULL)
			printf("%s", err);
		CU_ASSERT_PTR_NULL(err);
		CU_ASSERT_NOT_EQUAL(sock, -1);
		if (sock == -1)
			break;
		printf("LINE: >%s<\n", ChrPtr(Line));
		pch = strstr(ChrPtr(Line), "Content-Length");
		if (pch != NULL) {
			blobsize = atol(ChrPtr(Line) + 
					sizeof("Content-Length:"));

		}
		if (StrLength(Line) == 0)
			break;
		FlushStrBuf(Line);
	}

	StrBufReadBLOBBuffered(Blob, 
			       ReadBuffer, 
			       &Pos,
			       &sock,
			       0,
			       blobsize,
			       0,
			       &err);
	TestRevalidateStrBuf(Blob);
	if (err != NULL)
		printf("%s", err);
	printf("Blob said/read: %d / %d\n", blobsize, StrLength(Blob));
	CU_ASSERT(blobsize != 0);
	CU_ASSERT(blobsize == StrLength(Blob));
	CU_ASSERT_PTR_NULL(err);
	CU_ASSERT_NOT_EQUAL(sock, -1);
	if (sock == -1)
	printf("BLOB: >%s<\n", ChrPtr(Blob));
	
	FreeStrBuf(&ReadBuffer);
	FreeStrBuf(&Blob);
	FreeStrBuf(&Line);
	time_to_die = 1;
}


static void SimpleBLOBbufferTest(void)
{
	msock = ig_tcp_server(ip_addr, listen_port, LISTEN_QUEUE_LENGTH);

	worker_entry(SimpleBlobTestFunc);
	close (msock);
}

static void SimpleMixedLineBlob(void)
{
	msock = ig_tcp_server(ip_addr, listen_port, LISTEN_QUEUE_LENGTH);

	worker_entry(SimpleHttpPostTestFunc);
	close (msock);
}





/*
Some samples from the original...
	CU_ASSERT_EQUAL(10, 10);
	CU_ASSERT_EQUAL(0, -0);
	CU_ASSERT_EQUAL(-12, -12);
	CU_ASSERT_NOT_EQUAL(10, 11);
	CU_ASSERT_NOT_EQUAL(0, -1);
	CU_ASSERT_NOT_EQUAL(-12, -11);
	CU_ASSERT_PTR_EQUAL((void*)0x100, (void*)0x100);
	CU_ASSERT_PTR_NOT_EQUAL((void*)0x100, (void*)0x101);
	CU_ASSERT_PTR_NULL(NULL);
	CU_ASSERT_PTR_NULL(0x0);
	CU_ASSERT_PTR_NOT_NULL((void*)0x23);
	CU_ASSERT_STRING_EQUAL(str1, str2);
	CU_ASSERT_STRING_NOT_EQUAL(str1, str2);
	CU_ASSERT_NSTRING_EQUAL(str1, str2, strlen(str1));
	CU_ASSERT_NSTRING_EQUAL(str1, str1, strlen(str1));
	CU_ASSERT_NSTRING_EQUAL(str1, str1, strlen(str1) + 1);
	CU_ASSERT_NSTRING_NOT_EQUAL(str1, str2, 3);
	CU_ASSERT_NSTRING_NOT_EQUAL(str1, str3, strlen(str1) + 1);
	CU_ASSERT_DOUBLE_EQUAL(10, 10.0001, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(10, 10.0001, -0.0001);
	CU_ASSERT_DOUBLE_EQUAL(-10, -10.0001, 0.0001);
	CU_ASSERT_DOUBLE_EQUAL(-10, -10.0001, -0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(10, 10.001, 0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(10, 10.001, -0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(-10, -10.001, 0.0001);
	CU_ASSERT_DOUBLE_NOT_EQUAL(-10, -10.001, -0.0001);
*/





static void AddStrBufSimlpeTests(void)
{
	CU_pSuite pGroup = NULL;
	CU_pTest pTest = NULL;

	pGroup = CU_add_suite("TestStringBufSimpleAppenders", NULL, NULL);
	if (n_Lines_to_read > 0)
		pTest = CU_add_test(pGroup, "testSimpleLinebufferTest", SimpleLinebufferTest);
	else if (blobsize > 0)
		pTest = CU_add_test(pGroup, "testSimpleBLOBbufferTest", SimpleBLOBbufferTest);
	else 
		pTest = CU_add_test(pGroup,"testSimpleMixedLineBlob", SimpleMixedLineBlob);

}


int main(int argc, char* argv[])
{
	char a;
	setvbuf(stdout, NULL, _IONBF, 0);


	while ((a = getopt(argc, argv, "p:i:n:b:t:s")) != EOF)
	{
		switch (a) {

		case 'p':
			listen_port = atoi(optarg);
			break;
		case 'i':
			safestrncpy(ip_addr, optarg, sizeof ip_addr);
			break;
		case 'n':
			// do linetest?
			n_Lines_to_read = atoi(optarg);
			break;
		case 'b':
			// or blobtest?
			blobsize = atoi(optarg);
			// else run the simple http test
			break;
		case 't':
			if (optarg != NULL)
				timeout = atoi(optarg);
			break;
		case 's':
			if (optarg != NULL)
				selres = atoi(optarg);
			break;
		}
	}


	StartLibCitadel(8);
	CU_BOOL Run = CU_FALSE ;
	
	CU_set_output_filename("TestAutomated");
	if (CU_initialize_registry()) {
		printf("\nInitialize of test Registry failed.");
	}
	
	Run = CU_TRUE ;
	AddStrBufSimlpeTests();
	
	if (CU_TRUE == Run) {
		//CU_console_run_tests();
    printf("\nTests completed with return value %d.\n", CU_basic_run_tests());
    
    ///CU_automated_run_tests();
	}
	
	CU_cleanup_registry();

	return 0;
}
