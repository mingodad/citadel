/**
 ** This is a new test program for libCxClient, which should demonstrate
 ** the new thread-safe code.  It should now be possible to develop
 ** multithreaded, multiconnection Citadel/UX clients, using libCxClient 
 ** (which takes most of the effort out of creating Cit/UX clients!).
 **
 ** If you wish to test this program, try something like:
 **
 **	$ gcc *.o newtest.c -pthread -o newtest
 **     (adjust based on your platform, of course)
 **
 ** http://www.shadowcom.net/Software/libCxClient/
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<string.h>
#include	<pthread.h>
#include	"CxClient.h"

/*
     int
     pthread_create(pthread_t *thread, const pthread_attr_t *attr,
             void *(*start_routine)(void *), void *arg)
 */

/**
 ** 901 express message callback...
 **/
void		chathook(const char *user, const char *msg) {
	printf("Chat Message Handler\n");

	printf("[[[[[[ %s ]]]]]]\n", user);
	printf("%s\n", msg);
}

/**
 ** THREAD 1: Connect to Uncensored! BBS
 **/
void		*session1( void *args ) {
int		cxhndl;
USERINFO	*user_info = 0;
CXLIST		fl=0, foo;


	/**
	 ** The primary method of creating a new Connection Handle is to specify all
	 ** options as arguments to CxClConnection.
	 **/
	printf("1: Requesting connection handle...\n");
	cxhndl = CxClConnection( "uncensored.citadel.org", 504, "detsaoT", "Loudness" );

	if(!cxhndl) {
		printf("1: MEMORY ERROR!\n");
		pthread_exit(0);
	}

	/**
	 ** The handles you receive are only descriptive numeric values for CXTBL (Connection Table)
	 ** entries.  You can't do anything with them.  Invalid cxhndl's passed to any of the
	 ** support functions are ignored.
	 **/
	printf("1: handle: %d\n", cxhndl);

	/**
	 ** At any point in time, you can issue the CONNECT command to your CXHNDL.  This will
	 ** instruct it to connect using the stored parameters.  (see above)
	 **/
	printf("1: Connecting to ucg...\n");
	if(CxClConnect( cxhndl )) {
		printf("1: Connection to ucg failed!\n");
		CxClDelete(cxhndl);
		pthread_exit(NULL);
	}
	printf("1: Connected to ucg!\n");

	printf("1: Authenticating..\n");
	if(!(user_info = CxUsAuth( cxhndl, NULL, NULL ))) {
		printf("1: Failed authenticating %s!\n", CxClGetUser( cxhndl ));
		CxClDisconnect( cxhndl );
		CxClDelete( cxhndl );
		pthread_exit(0);
	}
	printf("1: Authenticated!\n");
	free(user_info);

	printf("1: Retrieving online user list...\n");
	fl = CxUsOnline( cxhndl, 0 );
	printf("1: Done!\n");
	foo = fl;
	printf("1: Users on uncensored:\n");
	while( foo ) {
		printf("1: %s\n", foo->data);
		foo = foo->next;
	}
	fl = CxLlFlush( fl );

	/**
	 ** Similarly, you can Disconnect() from these handles at any time.  Disconnecting the
	 ** handle does not destroy it, though!  This means that...
	 **/
	printf("1: Disconnecting...\n");
	CxClDisconnect( cxhndl );

	/**
	 ** ... you can re-connect a handle without having to create it again.
	 **/
	printf("1: Connecting to ucg...\n");
	CxClConnect( cxhndl );
	printf("1: Disconnecting...\n");
	CxClDisconnect( cxhndl );

	/**
	 ** When you are done with a CXHNDL, just delete it.
	 **/
	printf("1: Destroying handle...\n");
	CxClDelete( cxhndl );

	printf("1: DONE\n");
	return(NULL);
}

/**
 ** THREAD 2: Connect to Pixel! BBS
 **/
void		*session2( void *args ) {
int		cxhndl2;
USERINFO	*user_info = 0;
CXLIST		fl = 0, foo;


	/**
	 ** However, if you don't know all of the information right away, that's ok.
	 ** You can create the connection, and adjust the values later.
	 **/
	printf("2: Requesting connection handle...\n");
	cxhndl2 = CxClConnection( NULL, 0, NULL, NULL );

	/**
	 ** If CxClConnection() returns NULL, you should not try to use the
	 ** handle provided (you're outta memory, dude!)
	 **/
	if(!cxhndl2) {
		pthread_exit(0);
	}

	printf("2: handle: %d\n", cxhndl2);

	/**
	 ** Adjust the values of a CXHNDL.
	 **/
	printf("2: Setting up handle...\n");
	CxClSetHost( cxhndl2, "pixel.citadel.org" );
	CxClSetUser( cxhndl2, "detsaoT" );
	CxClSetPass( cxhndl2, "Loudness" );

	printf("2: Connecting to pixel...\n");
	if(CxClConnect( cxhndl2 )) {
		printf("2: Connection to pixel failed!\n");
		CxClDelete(cxhndl2);
		pthread_exit(NULL);
	}
	printf("2: Connected to pixel!\n");

	printf("2: Requesting online user list\n");
	fl = CxUsOnline( cxhndl2, 0 );
	printf("2: Done!\n");
	printf("2: Users on pixel:\n");
	while( foo ) {
		printf("2: %s\n", foo->data);
		foo = foo->next;
	}
	fl = CxLlFlush( fl );

	printf("2: Disconnecting...\n");
	CxClDisconnect( cxhndl2 );
	printf("2: Destroying handle...\n");
	CxClDelete( cxhndl2 );

	printf("2: DONE\n");
	return(0);
}

/**
 ** main() launches our test threads.
 **/
int		main(int argc, char *argv[]) {
int		cxhndl;
pthread_t	t1 = 0, thread2 = 0;

	printf("libCxClient Multithreaded Test Program\n");
	printf("Library Revision %0.2f\n\n", CxRevision());

	/**
	 ** As a developer, you should start by registering your client name with
	 ** libCxClient.  This adjusts the IDEN string that is sent to the server
	 ** upon connection (CxClConnect()).
	 **/
	CxClRegClient("mt test program");

	/**
	 ** You can register callbacks for the ASYNchronous server mode.  These callbacks are
	 ** local functions which should handle whichever event was generated by the server.
	 **/
	printf("Registering callbacks\n");
	CxMiExpHook(chathook);

	printf("Going Multithreaded...\n\n");

	/**
	 ** Create threads to test the new multithread-safe library.
	 **/
	printf("0: Creating thread 1...\n");
	if(cxhndl = pthread_create( &t1, NULL, session1, NULL )) {
		printf("Failed creating thread 1\n");
		printf("Error %d: %s\n", cxhndl, strerror(cxhndl));
	}

	printf("0: Creating thread 2...\n");
	if(cxhndl = pthread_create( &thread2, NULL, session2, NULL )) {
		printf("0: Failed creating thread 2\n");
		printf("0: Error %d: %s\n", cxhndl, strerror(cxhndl));
	}

	if(cxhndl = pthread_join( t1, NULL )) {
		printf("0: Error #%d: %s\n", cxhndl, strerror(cxhndl));
		printf("0: Thread1: %d\n", t1);
	}
	printf("0: Thread 1 completed.\n");

	if(cxhndl = pthread_join( thread2, NULL )) {
		printf("0: Error #%d: %s\n", cxhndl, strerror(cxhndl));
		printf("0: Thread1: %d\n", thread2);
	}

	/**
	 ** libCxClient ignores invalid handle id's, of course!!
	 **/
	CxClDelete( cxhndl );
	printf("0: DONE\n");
}
