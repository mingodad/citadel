/**
 ** libCxClient - Citadel/UX Extensible Client API
 ** Copyright (c) 2000, Flaming Sword Productions
 ** Copyright (c) 2001, The Citadel/UX Consortium
 ** All Rights Reserved
 **
 ** Module: rooms.o
 ** Date: 2000-10-15
 ** Last Revision: 2000-10-15
 ** Description: Functions which manipulate (build) room & floor lists.
 ** CVS: $Id$
 **/
#include	<stdio.h>
#include	<stdlib.h>
#include	<stdarg.h>
#include	<string.h>
#include	<CxClient.h>
#include	"autoconf.h"

/**
 ** CxRmGoto(): Go to a room.  
 **
 ** [Expects]
 **  (char *) room: The name of the room the user wishes to go to.
 **  (int) operation: Which room to go to?
 **		0: Goto specified room
 **		1: Goto next room w/ unread messages
 **		2: Ungoto
 **
 ** [Returns]
 **  On Success: The room's full information structure [*]
 **  On Failure: NULL
 **/
ROOMINFO	*CxRmGoto(int id, const char *room, int operation) {
ROOMINFO	*room_info;
char		*xmit, buf[255], *g_Ser[20];
int		rc, i;

	if((room && *room) && !operation) {
		xmit = (char *)CxMalloc(strlen(room)+6);
		sprintf(xmit, "GOTO %s", room);
		CxClSend(id, xmit);
		CxFree(xmit);

		rc = CxClRecv(id, buf);

		/**
		 ** If we successfully went to this room, return the
		 ** room's information structure.
		 **/
		if(CHECKRC(rc, RC_OK)) {
			CxSerialize(buf, (char **) &g_Ser);

			room_info = (ROOMINFO *)CxMalloc(sizeof(ROOMINFO));
			strcpy(room_info->name, g_Ser[0]);
			room_info->msgs_unread = atol(g_Ser[1]);
			room_info->msgs_total = atol(g_Ser[2]);
			room_info->info_flag = (short int) atoi(g_Ser[3]);
			room_info->flags = atol(g_Ser[4]);
			room_info->msgs_highest = atol(g_Ser[5]);
			room_info->msgs_highest_u = atol(g_Ser[6]);
			room_info->mailroom = (short int) atoi(g_Ser[7]);
			room_info->aide = (short int) atoi(g_Ser[8]);
			room_info->msgs_newmail = atol(g_Ser[9]);
			room_info->floor_id = atol(g_Ser[10]);

			DPF((DFA,"MEM/MDA:\t-1\t@0x%08x (Needs manual deallocation)", room_info));

			return(room_info);
		}

		/**
		 ** Room not found, Returning NULL.
		 **/
		return(NULL);

	/**
	 ** GOTO Next Unread Room
	 **/
	} else if(operation==1) {

		/**
		 ** Set last-read pointer for this room.
		 **/
		CxClSend(id, "SLRP highest");
		CxClRecv(id, buf);

		/**
		 ** Retrieve a list of all rooms w/ new messages.
		 **/
		CxClSend(id, "LKRN");
		rc = CxClRecv(id, buf);
		i = (int) xmit = 0;
		if(CHECKRC(rc, RC_LISTING)) {
			do {
				rc = CxClRecv(id, buf);
				if(rc) {
					if(!i) {
						xmit = (char *)CxMalloc(strlen(buf)+6);
						strcpy(xmit, "GOTO ");
						strcat(xmit, buf);
					}
				}
			} while(rc<0);

			if(xmit) {
				CxClSend(id, xmit);
				CxFree(xmit);

				rc = CxClRecv(id, buf);
				if(CHECKRC(rc, RC_OK)) {
					CxSerialize(buf, (char **) &g_Ser);
		 
					room_info = (ROOMINFO *)CxMalloc(sizeof(ROOMINFO));
					strcpy(room_info->name, g_Ser[0]);
					room_info->msgs_unread = atol(g_Ser[1]);
					room_info->msgs_total = atol(g_Ser[2]);
					room_info->info_flag = (short int) atoi(g_Ser[3]);
					room_info->flags = atol(g_Ser[4]);
					room_info->msgs_highest = atol(g_Ser[5]);
					room_info->msgs_highest_u = atol(g_Ser[6]);
					room_info->mailroom = (short int) atoi(g_Ser[7]);
					room_info->aide = (short int) atoi(g_Ser[8]);
					room_info->msgs_newmail = atol(g_Ser[9]);
					room_info->floor_id = atol(g_Ser[9]);

					DPF((DFA,"MEM/MDA:\t-1\t@0x%08x (Needs manual deallocation)", room_info));
		 
					return(room_info);
				}
				
			} else {
				return(NULL);
			}
		}
		return(NULL);

	/**
	 ** Unknown Operation
	 **/
	} else {
		return(NULL);
	}
}

/**
 ** CxRmCreate(): Create a new room, using CERTAIN information provided in
 ** a ROOMINFO struct.  Any unnecessary information is ignored.
 **
 ** [Expects]
 **  ROOMINFO: Information about the room to be created.
 **
 ** [Returns]
 **  On Success: 0
 **  On Failure: 1: rm.mode is invalid.
 **		 2: rm.floor_id is invalid.
 **		 3: room exists.
 **		 4: not here/not allowed.
 **/
int		CxRmCreate(int id, ROOMINFO rm) {
char		buf[512];
int		rc;

	DPF((DFA,"Creating room '%s'",rm.name));

	/**
	 ** User provided an illegal room mode.  Can't continue.
	 **/
	if((rm.mode<0) || (rm.mode>4)) {
		DPF((DFA,"FAILED rm.mode_is_invalid"));
		return(1);
	}

	/**
	 ** Floor id invalid (How do we check this?)
	 **/
	if( 0 ) {
		DPF((DFA,"FAILED rm.floor_id_is_invalid"));
		return(2);
	}

	/**
	 ** Does the room already exist?
	 **/
	if( 0 ) {
		DPF((DFA,"FAILED room_exists"));
		return(3);
	}

	sprintf(buf, "CRE8 1|%s|%d||%d", rm.name, rm.mode, rm.floor_id );
	CxClSend(id, buf);
	rc = CxClRecv(id, buf);
	if( CHECKRC(rc, RC_OK)) {
		DPF((DFA,"Success!"));
		return(0);
	} else {
		DPF((DFA,"FAILED %d:%s", rc, buf));
		return(4);
	}
}

/**
 ** CxRmList(): Retrieve a list of rooms on the current floor.  Return it
 ** as a Character array.  THE CALLER IS RESPONSIBLE FOR DEALLOCATING THIS
 ** MEMORY!!
 **/
CXLIST		CxRmList(int id) {
int		rc;
char		buf[255];
CXLIST		rooms = NULL;

	DPF((DFA,"Retrieving list of rooms from the server."));

	CxClSend(id, "LKRA");
	rc = CxClRecv( id, buf );
	DPF((DFA,"%s [%d]",buf,rc));

	if( CHECKRC(rc, RC_LISTING)) {

		do {
			rc = CxClRecv( id, buf );
			DPF((DFA,"%s [%d]",buf,rc));

			if(rc) {
				rooms = (CXLIST) CxLlInsert(rooms,buf);
			}
		} while(rc < 0);

		return(rooms);
	} else {
		return(NULL);
	}
}

/**
 ** CxFlList(): Retrieve a list of floors.
 **/
CXLIST		CxFlList(int id) {
int		rc;
char		buf[255];
CXLIST		floors = NULL;

	DPF((DFA,"Retrieving list of floors from the server."));

	CxClSend(id, "LFLR");
	rc = CxClRecv( id, buf );
	DPF((DFA,"%s [%d]",buf,rc));

	if( CHECKRC(rc, RC_LISTING)) {

		do {
			rc = CxClRecv( id, buf );
			DPF((DFA,"%s [%d]",buf,rc));

			if(rc) {
				floors = (CXLIST) CxLlInsert(floors,buf);
			}
		} while(rc < 0);

		return(floors);
	} else {
		return(NULL);
	}
}


