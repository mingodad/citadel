/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: file.o
 ** Date: 2000-11-16
 ** Last Revision: 2000-11-16
 ** Description: File directory/transfer functions.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<CxClient.h>
#include	"autoconf.h"
#include	"uname.h"

static void	(*_CxFiFunc)(const char *, void *);

/**
 ** CxFiIndex(): Retrieve an index of files IN THE
 ** CURRENT ROOM. 
 **
 ** [Returns]
 **  Success: 1 blank entry + List of files in current room.
 **  Success, No Files: 1 blank entry
 **  Failure: NULL list.
 **/
CXLIST		CxFiIndex( int id ) {
int		rc;
char		buf[512];
CXLIST		flist = 0;

	DPF((DFA,"Retrieving file index."));

	/**
	 ** Request directory listing from server.
	 **/
	DPF((DFA,"Sending request..."));
	CxClSend(id, "RDIR");
	rc = CxClRecv(id, buf);

	/**
	 ** If this room allows directory listings...
	 **/
	if(CHECKRC(rc,RC_LISTING)) {
		DPF((DFA,"LISTING_FOLLOWS..."));

		do {
			rc = CxClRecv(id, buf);
			DPF((DFA,"%s", buf));
			if(rc<0) {
				flist = CxLlInsert(flist, buf);
			}
		} while(rc<0);
		DPF((DFA,"LISTING_COMPLETE"));

		return(flist);
	
	/**
	 ** ...otherwise, there's nothing to see here...
	 **/
	} else {
		DPF((DFA, "No files found"));
		return(NULL);
	}
}

/**
 ** CxFiPut(): Send a file to the server.
 **
 ** [Expects]
 **  (FILEINFO) f_info: File information
 **  (int) f_ptr: open() file pointer.
 **
 ** [Returns]
 **  Success: 0
 **  Failure; Not Here: 1
 **  Failure; Malformed file information: 2
 **  Failure; File Exists: 3
 **  Failure; Nonexistent FILE pointer.
 **/
int		CxFiPut(int id, FILEINFO f_info, int f_ptr) {
	return(0);
}

/**
 ** CxFiGet(): Download a file from the server.
 **
 ** [Expects]
 **  (char *) name: Name of the file we are downloading.
 **
 ** [Returns]
 **  Success: Ptr to malloc()ed tmp filename containing file data.
 **  Failure: NULL
 **/
char		*CxFiGet(int id, const char *name) {


	/**
	 ** Failed, return NULL.
	 **/
	return(NULL);
}

/**
 ** _CxFiHook(): We will hook ourselves into the Transport layer to
 ** handle incoming file transfers.
 **/
void		_CxFiHook(void *data) {
	DPF((DFA, "Message received"));
}

/**
 ** CxFiHook(): The user wishes to provide a hook application to
 ** handle incoming file transfers.
 **
 ** [Expects]
 **  func: The function that the user has written to handle file
 **        downloads.
 **        void func( const char *FILE_NAME, void *FILE_DATA);
 **/
void            CxFiHook(void *func) {

        DPF((DFA, "Hooking user func@0x%08x",func));

        /**
         ** If libCxClient has not already hooked this type of
         ** message, we need to go ahead and hook it to our
         ** internal routing function.
         **/
        if(!CxClCbExists(902)) {
                DPF((DFA, "Hooking into RC_902"));
                CxClCbRegister(902, _CxFiHook);
        }

        /**
         ** Now, register the user's hooked function with
         ** ourselves.  This instructs _CxFiHook() on
         ** where to route data.
         **/
        DPF((DFA,"Registering user hook"));
        _CxFiFunc = func;

        DPF((DFA,"Ok, at this point, RC_902 messages should be routed to the user."));
        DPF((DFA,"Don't blame me if it doesn't work.  You told me what to do, Brian."));
}
