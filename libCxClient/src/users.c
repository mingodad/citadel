/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: users.o
 ** Date: 2000-10-15
 ** Last Revision: 2000-10-15
 ** Description: Functions which manipulate user lists. (who's online, Directory, etc.)
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<CxClient.h>
#include	"autoconf.h"

/**
 ** CxUsOnline(fmt): List online users.
 ** [fmt]: Format of list you are expecting to receive:
 ** 0: (Default) Session ID|User|Room
 **/
CXLIST		CxUsOnline(int fmt) {
CXLIST		toret = 0;
int		rc;
char		buf[255];

	DPF((DFA,"Retrieving online user list"));
	CxClSend("RWHO");
	rc = CxClRecv(buf);

	/**
	 ** The session protocol specs say that
	 ** this process will never return anything
	 ** but LISTING_FOLLOWS.  I don't belive it. ;)
	 **/
	if( CHECKRC(rc, RC_LISTING) ) {

		do {
			rc = CxClRecv(buf);
			DPF((DFA,"[%d] %s",rc,buf));

			if( rc ) {
				toret = (CXLIST)CxLlInsert(toret,buf);
			}

		} while(rc<0);

		return(toret);

	/**
	 ** In the event that the moon IS made of
	 ** green cheese.......
	 **/
	} else {
		DPF((DFA,"Session spec violation!! IG!!! AUGH!!!"));
		return(toret);
	}

	return(NULL);
}

/**
 ** CxUsList(): Fetch the Global Address Book.
 **/
CXLIST		CxUsList() {
CXLIST		toret = 0;
int		rc;
char		buf[512];

	DPF((DFA,"Requesting user list..."));
	CxClSend("LIST");
	rc = CxClRecv(buf);
	if( CHECKRC(rc, RC_LISTING) ) {
		do {
			rc = CxClRecv(buf);
			if(rc) {
				toret = CxLlInsert(toret,buf);
			}
		} while(rc < 0);
	}

	return(toret);
}

/**
 ** CxUsAuth(): Authenticate a username/password combination.  If we're
 ** already logged in, then we need to abort this.  [How can we tell if
 ** we are??]
 **
 ** [Expects]
 **  (char *) uname: Username we wish to verify
 **  (char *) passwd: Password of this user.
 **
 ** [Returns]
 **  On Success: USERINFO: User Information structure. [*]
 **  On Failure: NULL
 **/
USERINFO	*CxUsAuth(const char *uname, const char *passwd) {
USERINFO	*user_info;
char		*xmit, buf[512], *g_Ser[20];
int		rc;

	DPF((DFA,"Authenticating '%s'",uname));
	xmit = (char *)CxMalloc(strlen(uname)+6);
	sprintf(xmit,"USER %s",uname);
	CxClSend(xmit);
	CxFree(xmit);

	DPF((DFA,"Validating username"));
	rc = CxClRecv(buf);
	DPF((DFA,"%d", rc));
	if( CHECKRC(rc, RC_MOREDATA) ) {
		xmit = (char *)CxMalloc(strlen(passwd)+6);
		sprintf(xmit,"PASS %s",passwd);
		CxClSend(xmit);
		CxFree(xmit);

		DPF((DFA,"Validating password"));
		rc = CxClRecv(buf);

		/**
		 ** RETURN: Authentication information O.K.
		 **/
		if( CHECKRC(rc, RC_OK) ) {
			user_info = (USERINFO *)CxMalloc(sizeof(USERINFO));

			CxSerialize(buf, &g_Ser);
			strcpy(user_info->username, g_Ser[0]);
			user_info->system.access_level = atoi(g_Ser[1]);
			user_info->system.times_called = atol(g_Ser[2]);
			user_info->system.messages_posted = atol(g_Ser[3]);
			user_info->system.user_flags = atol(g_Ser[4]);
			user_info->system.user_number = atol(g_Ser[5]);
			DPF((DFA,"MEM/MDA:\t-1\t@0x%08x (Needs manual deallocation)", user_info));
			
			return(user_info);

		/**
		 ** RETURN: Invalid password.
		 **/
		} else {
			return(NULL);
		}

	/**
	 ** RETURN: Invalid username
	 **/
	} else {
		return(NULL);
	}

	/**
	 ** SHOULD be unreachable...
	 **/
	return(NULL);
}

/**
 ** CxUsCreate(): Create a user account.
 **
 ** [Expects]
 **  USERINFO user: Populated USERINFO structure.
 **
 ** [Returns]
 **  On Success: 0
 **  On Failure: 1 - Not enough information
 **              2 - USER ALREADY EXISTS
 **/
int		CxUsCreate(USERINFO user) {
char		buf[512];
int		rc;

	/**
	 ** RETURN: Not enough information.
	 **/
	if(!user.username[0] || !user.password[0]) return(1);

	/**
	 ** Phase 1: Create new account.
	 **/
	DPF((DFA,"Creating user account '%s'", user.username));

	sprintf(buf, "NEWU %s", user.username);
	CxClSend(buf);
	rc = CxClRecv(buf);

	/**
	 ** RETURN: User already exists.
	 **/
	if( CHECKRC(rc, RC_OK)) {
		return(2);
	}

	sprintf(buf, "SETP %s", user.password);
	CxClSend(buf);
	rc = CxClRecv(buf);

	if( CHECKRC(rc, RC_OK)) {
		return(0); /** Non-fatal error.  User just has a blank "" password. **/
	}

	/**
	 ** Phase 2: Populate registration structures on server.
	 **/
	CxClSend("REGI");
	rc = CxClRecv(buf);
	if( CHECKRC(rc, RC_SENDLIST)) {
		sprintf(buf, "%s", user.fullname);
		CxClSend(buf);
		sprintf(buf, "%s", user.addr.street);
		CxClSend(buf);
		sprintf(buf, "%s", user.addr.city);
		CxClSend(buf);
		sprintf(buf, "%s", user.addr.st);
		CxClSend(buf);
		sprintf(buf, "%s", user.addr.zip);
		CxClSend(buf);
		sprintf(buf, "%s", user.contact.telephone);
		CxClSend(buf);
		sprintf(buf, "%s", user.contact.emailaddr);
		CxClSend(buf);
		CxClSend("000");
	}

	/**
	 ** Phase 3: Create personal rooms expected by CxClient.  [Please note that this will only work if
	 ** the server's default permissions allow new users to create rooms.  bbs.shadowcom.net will.]
	 **/
	CxClSend("CRE8 0");
	rc = CxClRecv(buf);
	if( CHECKRC(rc, RC_OK)) {
		CxClSend("CRE8 1|My Schedule|4||");
		rc = CxClRecv(buf);
		printf("My Schedule: rc = %d", rc);
		CxClSend("CRE8 1|My Notes|4||");
		rc = CxClRecv(buf);
		printf("My Notes: rc = %d", rc);
		CxClSend("CRE8 1|My Tasks|4||");
		rc = CxClRecv(buf);
		printf("My Tasks: rc = %d", rc);
		CxClSend("CRE8 1|My Journal|4||");
		rc = CxClRecv(buf);
		printf("My Journal: rc = %d", rc);
		CxClSend("CRE8 1|My Contacts|4||");
		rc = CxClRecv(buf);
		printf("My Contacts: rc = %d", rc);
	}

	/**
	 ** RETURN: Success!
	 **/
	return(0);
}
