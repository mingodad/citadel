/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: misc.o
 ** Date: 2000-11-16
 ** Last Revision: 2000-11-16
 ** Description: Miscellaneous support functions.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<signal.h>
#include	<string.h>
#include	<CxClient.h>
#include	"autoconf.h"
#include	"uname.h"

/**
 ** Function which we have hooked to.
 **/
static void	(*_MiExpFunc)(const char*, const char*) = 0;

/**
 ** CxRevision(): Report revision information about libCxClient.
 **/
float		CxRevision() {
	return(atof(VERSION));
}

/**
 ** CxSerialize(): Take a pipe-separated string & convert it into a 2-dimensional
 ** array.  THIS FUNCTION WILL HANDLE AN ABSOLUTE MAXIMUM OF 50 ARGUMENTS.
 **/
void            CxSerialize(const char *s, char **Ser) {
char            **ap, *argv[50], *end;
int             i;
 
	DPF((DFA,"Serializing '%s'",s));

        for (ap = argv; (*ap = (char *)strsep((char **)&s, "|")) != NULL;) {
                if (++ap >= &argv[50]) {
                        break;
                }
        }
        end = (char *)(ap - 1);
 
        i = 0;
        for(ap=&argv[0]; (char *)ap <= end; ap++) {
                Ser[i] = *ap;
                i++;
        }

        Ser[i] = 0;

	DPF((DFA,"Done"));
} 

/**
 ** CxMiExpSend(): Send an express message.  Think of this like AIM
 ** without AOL...
 **
 ** [Expects]
 **  (char *) user: Username
 **  (char *) msg: Express message.
 **
 ** [Returns]
 **  Success: 0
 **  Failure; Unknown Error: 1
 **/
int		CxMiExpSend(int id, const char *user, const char *msg) {
char		*xmit, buf[255];
int		rc;

	DPF((DFA,"Send Express Message"));
	xmit = (char *)CxMalloc(strlen(user)+strlen(msg) + 7);
	sprintf(xmit,"SEXP %s|%s",user, msg);
	CxClSend(id, xmit);
	CxFree(xmit);

	rc = CxClRecv(id, buf);
	if( CHECKRC(rc, RC_OK) ) {
		return(0);

	} else {
		return(1);
	}
}

/**
 ** CxMiExpRecv(): Receive an express message.  Usually, this is
 ** called after a NOOP loop returns RC_xxxx...
 **
 ** [Returns]
 **  Success: Ptr to malloc()ed EXPRMESG struct.  [*]
 **  Failure: NULL
 **/
EXPRMESG	*CxMiExpRecv( int id ) {
char		buf[255], *Ser[20];
EXPRMESG	*toret;
int		rc;

	/**
	 ** Ask the server for the latest Express Message [GEXP].
	 **/
	DPF((DFA,"Receive Express Message"));
	CxClSend(id, "GEXP");
	rc = CxClRecv(id, buf);
	DPF((DFA,"buf=%s\n",buf));
	toret = 0L;

	/**
	 ** If rc==RC_LISTING, then we have a valid Express Message.
	 **/
	DPF((DFA,"Checking result = ", rc));
	if( CHECKRC(rc, RC_LISTING)) {

		DPF((DFA,"Preparing to return"));
		toret = (EXPRMESG *) CxMalloc( sizeof(EXPRMESG) );
		bzero( &toret, sizeof(EXPRMESG) );

		CxSerialize( buf, (char **)&Ser );

		toret->more_follows = atoi( Ser[0] );
		toret->timestamp = (time_t) strtoul( Ser[1], 0, 10 );
		toret->flags = atoi( Ser[2] );
		strcpy( toret->sender, Ser[3] );
		strcpy( toret->node, Ser[4] );
		toret->message = 0L;
		do {
			if((rc = CxClRecv(id, buf))) {
				DPF((DFA, "%s", buf));
				toret->message = (char *) realloc(toret, strlen(toret->message)+strlen(buf)+1);
				strcat(toret->message, buf);
			}
		} while( rc < 0 );

/****		toret = (char *) CxMalloc(strlen(buf)+2);
		strcpy(toret,buf);
		strcat(toret,"|");
		do {
			if((rc = CxClRecv(id, buf))) {
				DPF((DFA,"%s",buf));
				toret = (char *) realloc(toret, strlen(toret)+strlen(buf)+1);
				strcat(toret,buf);
			}
		} while(rc<0);
 ****/
	}

	return(toret);
}

/**
 ** CxMiExpCheck(): Check to see if there are any EXPress MEssages
 ** waiting for the currently logged-in user.
 **
 ** [Returns]
 **  Message Waiting: 1
 **  No Messages: 0
 **/
int		CxMiExpCheck( int id ) {
int		rc;
char		buf[255];

	DPF((DFA,"Sending NOOP"));
	CxClSend(id, "NOOP");
	DPF((DFA,"Checking response"));
	rc = CxClRecv(id, buf);

	/**
	 ** CxClRecv() tacks on a RC_MESGWAIT flag to the result
	 ** code upon seeing a Message Waiting note from the
	 ** server.  This behaviour is deprecated in updated
	 ** versions of Citadel/UX, but is still included for
	 ** compatibility's sake.
	 **/
	if(CHECKRC(rc,RC_MESGWAIT)) {
		DPF((DFA,"Express Message waiting!"));
		return(1);

	} else {
		DPF((DFA,"No express message, loser."));
		return(0);
	}
}

/**
 ** _CxMiExpHook(): Hook to RC_901 messages [Contain express messages]
 ** [Not Intended For External Use]
 **/
static
void		_CxMiExpHook(int id, const void* data) {
char		buf[512], *user_buf, *data_buf;
int		rc;

	DPF((DFA, "*ASYN* Received RC_901 message on CXID %d", id));
	DPF((DFA, "Raw data: %s\n", (char *)data));

	rc = CxClRecv(id, buf);
	user_buf = (char *)CxMalloc(strlen(buf)+1);
	strcpy(user_buf, buf);

	data_buf = (char *)CxMalloc(1);
	*(data_buf) = 0;

	/**
	 ** If this is a multi-line message:
	 **/
	if(CHECKRC(rc, RC_LISTING)) {
		do {
			rc = CxClRecv( id, buf );
			if(rc<0) {
				realloc(data_buf, strlen(data_buf)+strlen(buf));
				strcat(data_buf, buf);
			}
		} while(rc < 0);
	}

	/**
	 ** Pass this information off to the user's function.
	 **/
	_MiExpFunc(user_buf, data_buf);
	CxFree(user_buf);
	CxFree(data_buf);
}

/**
 ** CxMiExpHook(): We will allow the user to hook themselves into
 ** our express message handler.  Only one function is permitted to
 ** hook here.
 **
 ** [Expects]
 **  func: The function that the user has written to handle Express
 **        Messages.  
 **        void func( const char *USER_FROM, const char *TEXT);
 **/
void		CxMiExpHook(void (*func)(const char*, const char *)) {

	DPF((DFA, "Hooking user func@0x%08x",func));

	/**
	 ** If libCxClient has not already hooked this type of
	 ** message, we need to go ahead and hook it to our
	 ** internal routing function.
	 **/
	if(!CxClCbExists(901)) {
		DPF((DFA, "Hooking into RC_901"));
		CxClCbRegister(901, _CxMiExpHook);
	}

	/**
	 ** Now, register the user's hooked function with
	 ** ourselves.  This instructs _CxMiExpHook() on
	 ** where to route data.
	 **/
	DPF((DFA,"Registering user hook"));
	_MiExpFunc = func;

	DPF((DFA,"Ok, at this point, RC_901 messages should be routed to the user."));	 
	DPF((DFA,"Don't blame me if it doesn't work.  You told me what to do, Brian."));
}

/**
 ** CxMiMessage(): Read a system message file, ONE LINE AT A TIME.
 ** This function will return the current line sent by the server,
 ** and will return NULL on completion.  The caller is responsible
 ** for freeing EACH LINE OF MEMORY passed to it.
 **
 ** [Expects]
 **  (char *)file: The name of the file we want, or NULL to continue
 **  the current stream...
 **
 ** [Returns]
 **  Success: Ptr to malloc()ed file data.  [*]
 **  Failure; File not found: NULL
 **/
char		*CxMiMessage(int id, const char *file) {
char		buf[255], *toret;
int		rc;

	if((file!=NULL) && file[0]) {
		DPF((DFA,"Requesting %s from server.",file));
		sprintf(buf,"MESG %s", file);
		CxClSend(id, buf);
		rc = CxClRecv(id, buf);
		if(CHECKRC(rc, RC_LISTING)) {
			DPF((DFA,"Retrieving line from file..."));
			rc = CxClRecv(id, buf);
			if(rc < 0) {
				toret = (char *)CxMalloc(strlen(buf)+1);
				strcpy(toret, buf);
				DPF((DFA,"MEM/MDA:\t-1\t@0x%08x (Needs manual deallocation)", toret)); 
				return(toret);
			} else {
				return(NULL);
			}
		} else {
			return(NULL);
		}
	} else {

		DPF((DFA,"Retrieving line from file..."));
		rc = CxClRecv( id, buf);
		if(rc < 0) {
			toret = (char *)CxMalloc(strlen(buf)+1);
			strcpy(toret,buf);
			DPF((DFA,"MEM/MDA:\t-1\t@0x%08x (Needs manual deallocation)", toret)); 
			return(toret);
		} else {
			return(NULL);
		}
	}

	/**
	 ** Insurance measure...
	 **/
	return(NULL);
}

/**
 ** CxMiImage(): Read a system image.  Images are defined by the 
 ** Citadel/UX session protocol manual as always being in GIF
 ** format.  
 **
 ** [Expects]
 **  (char *)img: Name of the image we are requesting.
 **
 ** [Returns]
 **  Success: Ptr to malloc()ed image data.  [*]
 **  Failure; File not found: NULL
 **/
char		*CxMiImage(int id, const char *img) {

	/**
	 ** Hmm.. Not sure how similar this is to MESG...
	 ** Will defer this code until I can reference the 
	 ** specs..
	 **/

	return(NULL);
}
