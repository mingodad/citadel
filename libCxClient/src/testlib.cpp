/**
 ** This is a test program for libCxClient.  It's not important.
 **
 ** $ gcc *.o testlib.c -o testlib
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	"CxClient.h"

/**
 ** 901 express message callback...
 **/
void		chathook(const char *user, const char *msg) {

	printf("------------------------------- Chat Message Handler\n");

	printf("[[[[[[ %s ]]]]]]\n", user);
	printf("%s\n", msg);
}

int		main(int argc, char *argv[]) {
CXLIST		fl = 0;
USERINFO	*user_info = 0;
ROOMINFO	*room_info = 0;
char		buf[255],*s = 0;
int		hndl;

	printf("libCxClient Test Program\n");
	printf("Library Revision %0.2f\n\n", CxRevision());

	if(argc<3) {
		printf("\nUsage:\n	%s system username password\n\n", argv[0]);
		exit(0);
	}

	CxClRegClient("test program");
	printf("Registering callbacks\n");
	CxMiExpHook(chathook);

	if(!(hndl = CxClConnection( NULL, 504, NULL, NULL ))) {
		printf("Failed creating connection handle.  Dying.\n");
		exit(-1);
	}
	CxClSetHost( hndl, argv[1] );
	CxClSetUser( hndl, argv[2] );
	CxClSetPass( hndl, argv[3] );

	// I suggest 'tesseract.citadel.org'
	printf("Connecting to '%s'...\n",argv[1]);
	if(!CxClConnect( hndl )) {

		printf("Logging in\n");
		if(user_info = CxUsAuth(hndl, NULL, NULL)) {
			printf("FRE\n");
			free(user_info);
			user_info = 0;

			room_info = CxRmGoto(hndl, "_BASEROOM_",0);
			printf("FRE\n");
			free(room_info);
			room_info = 0;

			fl = CxLlFlush(fl);
			fl = CxMsList(hndl, 0, 0);

			fl = CxLlFlush(fl);

			CxMiExpSend(hndl, "detsaoT","Hello, World.  This is a long diatribe\n on the evils of something\nof which the world may never know.");
			CxMiExpSend(hndl, "detsaoT","How are you?  I am fine.  If you see the optional potential of the explicit implicity of the file, you'll know what I mean.");
			CxMiExpSend(hndl, "detsaoT","Blah blah blah.");


			CxClSend(hndl, "ECHO Hello");
			CxClRecv(hndl, buf);

		}

		CxClCbShutdown();
		CxClDelete( hndl );

	} else {
		printf("Unable to connect to '%s'!\n", argv[1]);
	}
}
