/* 
 * Server functions which perform operations on room objects.
 *
 * Copyright (c) 1987-2012 by the citadel.org team
 *
 * This program is open source software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <dirent.h>	/* for cmd_rdir to read contents of the directory */

#if TIME_WITH_SYS_TIME
# include <sys/time.h>
# include <time.h>
#else
# if HAVE_SYS_TIME_H
#  include <sys/time.h>
# else
#  include <time.h>
# endif
#endif

#include <limits.h>
#include <errno.h>
#include "citadel.h"
#include <libcitadel.h>
#include "server.h"
#include "database.h"
#include "config.h"
#include "room_ops.h"
#include "sysdep_decls.h"
#include "support.h"
#include "msgbase.h"
#include "citserver.h"
#include "control.h"
#include "citadel_dirs.h"
#include "threads.h"

#include "ctdl_module.h"
#include "user_ops.h"

struct floor *floorcache[MAXFLOORS];

/*
 * Retrieve access control information for any user/room pair
 */
void CtdlRoomAccess(struct ctdlroom *roombuf, struct ctdluser *userbuf,
		int *result, int *view)
{
	int retval = 0;
	visit vbuf;
	int is_me = 0;
	int is_guest = 0;

	if (userbuf == &CC->user) {
		is_me = 1;
	}

	if ((is_me) && (config.c_guest_logins) && (!CC->logged_in)) {
		is_guest = 1;
	}

	/* for internal programs, always do everything */
	if (((CC->internal_pgm)) && (roombuf->QRflags & QR_INUSE)) {
		retval = (UA_KNOWN | UA_GOTOALLOWED | UA_POSTALLOWED | UA_DELETEALLOWED | UA_REPLYALLOWED);
		vbuf.v_view = 0;
		goto SKIP_EVERYTHING;
	}

	/* If guest mode is enabled, always grant access to the Lobby */
	if ((is_guest) && (!strcasecmp(roombuf->QRname, BASEROOM))) {
		retval = (UA_KNOWN | UA_GOTOALLOWED);
		vbuf.v_view = 0;
		goto SKIP_EVERYTHING;
	}

	/* Locate any applicable user/room relationships */
	if (is_guest) {
		memset(&vbuf, 0, sizeof vbuf);
	}
	else {
		CtdlGetRelationship(&vbuf, userbuf, roombuf);
	}

	/* Force the properties of the Aide room */
	if (!strcasecmp(roombuf->QRname, config.c_aideroom)) {
		if (userbuf->axlevel >= AxAideU) {
			retval = UA_KNOWN | UA_GOTOALLOWED | UA_POSTALLOWED | UA_DELETEALLOWED | UA_REPLYALLOWED;
		} else {
			retval = 0;
		}
		goto NEWMSG;
	}

	/* If this is a public room, it's accessible... */
	if (	((roombuf->QRflags & QR_PRIVATE) == 0) 
		&& ((roombuf->QRflags & QR_MAILBOX) == 0)
	) {
		retval = retval | UA_KNOWN | UA_GOTOALLOWED;
	}

	/* If this is a preferred users only room, check access level */
	if (roombuf->QRflags & QR_PREFONLY) {
		if (userbuf->axlevel < AxPrefU) {
			retval = retval & ~UA_KNOWN & ~UA_GOTOALLOWED;
		}
	}

	/* For private rooms, check the generation number matchups */
	if (	(roombuf->QRflags & QR_PRIVATE) 
		&& ((roombuf->QRflags & QR_MAILBOX) == 0)
	) {

		/* An explicit match means the user belongs in this room */
		if (vbuf.v_flags & V_ACCESS) {
			retval = retval | UA_KNOWN | UA_GOTOALLOWED;
		}
		/* Otherwise, check if this is a guess-name or passworded
		 * room.  If it is, a goto may at least be attempted
		 */
		else if (	(roombuf->QRflags & QR_PRIVATE)
				|| (roombuf->QRflags & QR_PASSWORDED)
		) {
			retval = retval & ~UA_KNOWN;
			retval = retval | UA_GOTOALLOWED;
		}
	}

	/* For mailbox rooms, also check the namespace */
	/* Also, mailbox owners can delete their messages */
	if ( (roombuf->QRflags & QR_MAILBOX) && (atol(roombuf->QRname) != 0)) {
		if (userbuf->usernum == atol(roombuf->QRname)) {
			retval = retval | UA_KNOWN | UA_GOTOALLOWED | UA_POSTALLOWED | UA_DELETEALLOWED | UA_REPLYALLOWED;
		}
		/* An explicit match means the user belongs in this room */
		if (vbuf.v_flags & V_ACCESS) {
			retval = retval | UA_KNOWN | UA_GOTOALLOWED | UA_POSTALLOWED | UA_DELETEALLOWED | UA_REPLYALLOWED;
		}
	}

	/* For non-mailbox rooms... */
	else {

		/* User is allowed to post in the room unless:
		 * - User is not validated
		 * - User has no net privileges and it is a shared network room
		 * - It is a read-only room
		 * - It is a blog room (in which case we only allow replies to existing messages)
		 */
		int post_allowed = 1;
		int reply_allowed = 1;
		if (userbuf->axlevel < AxProbU) {
			post_allowed = 0;
			reply_allowed = 0;
		}
		if ((userbuf->axlevel < AxNetU) && (roombuf->QRflags & QR_NETWORK)) {
			post_allowed = 0;
			reply_allowed = 0;
		}
		if (roombuf->QRflags & QR_READONLY) {
			post_allowed = 0;
			reply_allowed = 0;
		}
		if (roombuf->QRdefaultview == VIEW_BLOG) {
			post_allowed = 0;
		}
		if (post_allowed) {
			retval = retval | UA_POSTALLOWED | UA_REPLYALLOWED;
		}
		if (reply_allowed) {
			retval = retval | UA_REPLYALLOWED;
		}

		/* If "collaborative deletion" is active for this room, any user who can post
		 * is also allowed to delete
		 */
		if (roombuf->QRflags2 & QR2_COLLABDEL) {
			if (retval & UA_POSTALLOWED) {
				retval = retval | UA_DELETEALLOWED;
			}
		}

	}

	/* Check to see if the user has forgotten this room */
	if (vbuf.v_flags & V_FORGET) {
		retval = retval & ~UA_KNOWN;
		if (	( ((roombuf->QRflags & QR_PRIVATE) == 0) 
			&& ((roombuf->QRflags & QR_MAILBOX) == 0)
		) || (	(roombuf->QRflags & QR_MAILBOX) 
			&& (atol(roombuf->QRname) == CC->user.usernum))
		) {
			retval = retval | UA_ZAPPED;
		}
	}

	/* If user is explicitly locked out of this room, deny everything */
	if (vbuf.v_flags & V_LOCKOUT) {
		retval = retval & ~UA_KNOWN & ~UA_GOTOALLOWED & ~UA_POSTALLOWED & ~UA_REPLYALLOWED;
	}

	/* Aides get access to all private rooms */
	if (	(userbuf->axlevel >= AxAideU)
		&& ((roombuf->QRflags & QR_MAILBOX) == 0)
	) {
		if (vbuf.v_flags & V_FORGET) {
			retval = retval | UA_GOTOALLOWED | UA_POSTALLOWED | UA_REPLYALLOWED;
		}
		else {
			retval = retval | UA_KNOWN | UA_GOTOALLOWED | UA_POSTALLOWED | UA_REPLYALLOWED;
		}
	}

	/* Aides can gain access to mailboxes as well, but they don't show
	 * by default.
	 */
	if (	(userbuf->axlevel >= AxAideU)
		&& (roombuf->QRflags & QR_MAILBOX)
	) {
		retval = retval | UA_GOTOALLOWED | UA_POSTALLOWED | UA_REPLYALLOWED;
	}

	/* Aides and Room Aides have admin privileges */
	if (	(userbuf->axlevel >= AxAideU)
		|| (userbuf->usernum == roombuf->QRroomaide)
	) {
		retval = retval | UA_ADMINALLOWED | UA_DELETEALLOWED | UA_POSTALLOWED | UA_REPLYALLOWED;
	}

NEWMSG:	/* By the way, we also check for the presence of new messages */
	if (is_msg_in_sequence_set(vbuf.v_seen, roombuf->QRhighest) == 0) {
		retval = retval | UA_HASNEWMSGS;
	}

	/* System rooms never show up in the list. */
	if (roombuf->QRflags2 & QR2_SYSTEM) {
		retval = retval & ~UA_KNOWN;
	}

SKIP_EVERYTHING:
	/* Now give the caller the information it wants. */
	if (result != NULL) *result = retval;
	if (view != NULL) *view = vbuf.v_view;
}


/*
 * Self-checking stuff for a room record read into memory
 */
void room_sanity_check(struct ctdlroom *qrbuf)
{
	/* Mailbox rooms are always on the lowest floor */
	if (qrbuf->QRflags & QR_MAILBOX) {
		qrbuf->QRfloor = 0;
	}
	/* Listing order of 0 is illegal except for base rooms */
	if (qrbuf->QRorder == 0)
		if (!(qrbuf->QRflags & QR_MAILBOX) &&
		    strncasecmp(qrbuf->QRname, config.c_baseroom, ROOMNAMELEN)
		    &&
		    strncasecmp(qrbuf->QRname, config.c_aideroom, ROOMNAMELEN))
			qrbuf->QRorder = 64;
}


/*
 * CtdlGetRoom()  -  retrieve room data from disk
 */
int CtdlGetRoom(struct ctdlroom *qrbuf, const char *room_name)
{
	struct cdbdata *cdbqr;
	char lowercase_name[ROOMNAMELEN];
	char personal_lowercase_name[ROOMNAMELEN];
	const char *sptr;
	char *dptr, *eptr;

	dptr = lowercase_name;
	sptr = room_name;
	eptr = (dptr + (sizeof lowercase_name - 1));
	while (!IsEmptyStr(sptr) && (dptr < eptr)){
		*dptr = tolower(*sptr);
		sptr++; dptr++;
	}
	*dptr = '\0';

	memset(qrbuf, 0, sizeof(struct ctdlroom));

	/* First, try the public namespace */
	cdbqr = cdb_fetch(CDB_ROOMS,
			  lowercase_name, strlen(lowercase_name));

	/* If that didn't work, try the user's personal namespace */
	if (cdbqr == NULL) {
		snprintf(personal_lowercase_name,
			 sizeof personal_lowercase_name, "%010ld.%s",
			 CC->user.usernum, lowercase_name);
		cdbqr = cdb_fetch(CDB_ROOMS,
				  personal_lowercase_name,
				  strlen(personal_lowercase_name));
	}
	if (cdbqr != NULL) {
		memcpy(qrbuf, cdbqr->ptr,
		       ((cdbqr->len > sizeof(struct ctdlroom)) ?
			sizeof(struct ctdlroom) : cdbqr->len));
		cdb_free(cdbqr);

		room_sanity_check(qrbuf);

		return (0);
	} else {
		return (1);
	}
}

/*
 * CtdlGetRoomLock()  -  same as getroom() but locks the record (if supported)
 */
int CtdlGetRoomLock(struct ctdlroom *qrbuf, char *room_name)
{
	register int retval;
	retval = CtdlGetRoom(qrbuf, room_name);
	if (retval == 0) begin_critical_section(S_ROOMS);
	return(retval);
}


/*
 * b_putroom()  -  back end to putroom() and b_deleteroom()
 *              (if the supplied buffer is NULL, delete the room record)
 */
void b_putroom(struct ctdlroom *qrbuf, char *room_name)
{
	char lowercase_name[ROOMNAMELEN];
	char *aptr, *bptr;
	long len;

	aptr = room_name;
	bptr = lowercase_name;
	while (!IsEmptyStr(aptr))
	{
		*bptr = tolower(*aptr);
		aptr++;
		bptr++;
	}
	*bptr='\0';

	len = bptr - lowercase_name;
	if (qrbuf == NULL) {
		cdb_delete(CDB_ROOMS, lowercase_name, len);
	} else {
		time(&qrbuf->QRmtime);
		cdb_store(CDB_ROOMS, lowercase_name, len, qrbuf, sizeof(struct ctdlroom));
	}
}


/* 
 * CtdlPutRoom()  -  store room data to disk
 */
void CtdlPutRoom(struct ctdlroom *qrbuf) {
	b_putroom(qrbuf, qrbuf->QRname);
}


/*
 * b_deleteroom()  -  delete a room record from disk
 */
void b_deleteroom(char *room_name) {
	b_putroom(NULL, room_name);
}


/*
 * CtdlPutRoomLock()  -  same as CtdlPutRoom() but unlocks the record (if supported)
 */
void CtdlPutRoomLock(struct ctdlroom *qrbuf)
{

	CtdlPutRoom(qrbuf);
	end_critical_section(S_ROOMS);

}


/*
 * CtdlGetFloorByName()  -  retrieve the number of the named floor
 * return < 0 if not found else return floor number
 */
int CtdlGetFloorByName(const char *floor_name)
{
	int a;
	struct floor *flbuf = NULL;

	for (a = 0; a < MAXFLOORS; ++a) {
		flbuf = CtdlGetCachedFloor(a);

		/* check to see if it already exists */
		if ((!strcasecmp(flbuf->f_name, floor_name))
		    && (flbuf->f_flags & F_INUSE)) {
			return a;
		}
	}
	return -1;
}


/*
 * CtdlGetFloorByNameLock()  -  retrieve floor number for given floor and lock the floor list.
 */
int CtdlGetFloorByNameLock(const char *floor_name)
{
	begin_critical_section(S_FLOORTAB);
	return CtdlGetFloorByName(floor_name);
}



/*
 * CtdlGetAvailableFloor()  -  Return number of first unused floor
 * return < 0 if none available
 */
int CtdlGetAvailableFloor(void)
{
	int a;
	struct floor *flbuf = NULL;

	for (a = 0; a < MAXFLOORS; a++) {
		flbuf = CtdlGetCachedFloor(a);

		/* check to see if it already exists */
		if ((flbuf->f_flags & F_INUSE) == 0) {
			return a;
		}
	}
	return -1;
}


/*
 * CtdlGetFloor()  -  retrieve floor data from disk
 */
void CtdlGetFloor(struct floor *flbuf, int floor_num)
{
	struct cdbdata *cdbfl;

	memset(flbuf, 0, sizeof(struct floor));
	cdbfl = cdb_fetch(CDB_FLOORTAB, &floor_num, sizeof(int));
	if (cdbfl != NULL) {
		memcpy(flbuf, cdbfl->ptr,
		       ((cdbfl->len > sizeof(struct floor)) ?
			sizeof(struct floor) : cdbfl->len));
		cdb_free(cdbfl);
	} else {
		if (floor_num == 0) {
			safestrncpy(flbuf->f_name, "Main Floor", 
				sizeof flbuf->f_name);
			flbuf->f_flags = F_INUSE;
			flbuf->f_ref_count = 3;
		}
	}

}


/*
 * lgetfloor()  -  same as CtdlGetFloor() but locks the record (if supported)
 */
void lgetfloor(struct floor *flbuf, int floor_num)
{

	begin_critical_section(S_FLOORTAB);
	CtdlGetFloor(flbuf, floor_num);
}


/*
 * CtdlGetCachedFloor()  -  Get floor record from *cache* (loads from disk if needed)
 *    
 * This is strictly a performance hack.
 */
struct floor *CtdlGetCachedFloor(int floor_num) {
	static int initialized = 0;
	int i;
	int fetch_new = 0;
	struct floor *fl = NULL;

	begin_critical_section(S_FLOORCACHE);
	if (initialized == 0) {
		for (i=0; i<MAXFLOORS; ++i) {
			floorcache[floor_num] = NULL;
		}
	initialized = 1;
	}
	if (floorcache[floor_num] == NULL) {
		fetch_new = 1;
	}
	end_critical_section(S_FLOORCACHE);

	if (fetch_new) {
		fl = malloc(sizeof(struct floor));
		CtdlGetFloor(fl, floor_num);
		begin_critical_section(S_FLOORCACHE);
		if (floorcache[floor_num] != NULL) {
			free(floorcache[floor_num]);
		}
		floorcache[floor_num] = fl;
		end_critical_section(S_FLOORCACHE);
	}

	return(floorcache[floor_num]);
}


/*
 * CtdlPutFloor()  -  store floor data on disk
 */
void CtdlPutFloor(struct floor *flbuf, int floor_num)
{
	/* If we've cached this, clear it out, 'cuz it's WRONG now! */
	begin_critical_section(S_FLOORCACHE);
	if (floorcache[floor_num] != NULL) {
		free(floorcache[floor_num]);
		floorcache[floor_num] = malloc(sizeof(struct floor));
		memcpy(floorcache[floor_num], flbuf, sizeof(struct floor));
	}
	end_critical_section(S_FLOORCACHE);

	cdb_store(CDB_FLOORTAB, &floor_num, sizeof(int),
		  flbuf, sizeof(struct floor));
}


/*
 * CtdlPutFloorLock()  -  same as CtdlPutFloor() but unlocks the record (if supported)
 */
void CtdlPutFloorLock(struct floor *flbuf, int floor_num)
{

	CtdlPutFloor(flbuf, floor_num);
	end_critical_section(S_FLOORTAB);

}


/*
 * lputfloor()  -  same as CtdlPutFloor() but unlocks the record (if supported)
 */
void lputfloor(struct floor *flbuf, int floor_num)
{
	CtdlPutFloorLock(flbuf, floor_num);
}


/* 
 *  Traverse the room file...
 */
void CtdlForEachRoom(ForEachRoomCallBack CB, void *in_data)
{
	struct ctdlroom qrbuf;
	struct cdbdata *cdbqr;

	cdb_rewind(CDB_ROOMS);

	while (cdbqr = cdb_next_item(CDB_ROOMS), cdbqr != NULL) {
		memset(&qrbuf, 0, sizeof(struct ctdlroom));
		memcpy(&qrbuf, cdbqr->ptr,
		       ((cdbqr->len > sizeof(struct ctdlroom)) ?
			sizeof(struct ctdlroom) : cdbqr->len)
		);
		cdb_free(cdbqr);
		room_sanity_check(&qrbuf);
		if (qrbuf.QRflags & QR_INUSE) {
			CB(&qrbuf, in_data);
		}
	}
}

/* 
 *  Traverse the room file...
 */
void CtdlForEachNetCfgRoom(ForEachRoomNetCfgCallBack CB,
			   void *in_data,
			   RoomNetCfg filter)
{
	struct ctdlroom qrbuf;
	struct cdbdata *cdbqr;

	cdb_rewind(CDB_ROOMS);

	while (cdbqr = cdb_next_item(CDB_ROOMS), cdbqr != NULL) {
		memset(&qrbuf, 0, sizeof(struct ctdlroom));
		memcpy(&qrbuf, cdbqr->ptr,
		       ((cdbqr->len > sizeof(struct ctdlroom)) ?
			sizeof(struct ctdlroom) : cdbqr->len)
		);
		cdb_free(cdbqr);
		room_sanity_check(&qrbuf);
		if (qrbuf.QRflags & QR_INUSE)
		{
			OneRoomNetCfg* RNCfg;
			RNCfg = CtdlGetNetCfgForRoom(qrbuf.QRnumber);
			if ((RNCfg != NULL) &&
			    ((filter == maxRoomNetCfg) ||
			     (RNCfg->NetConfigs[filter] != NULL)))
			{
				CB(&qrbuf, in_data, RNCfg);
			}
		}
	}
}


/*
 * delete_msglist()  -  delete room message pointers
 */
void delete_msglist(struct ctdlroom *whichroom)
{
        struct cdbdata *cdbml;

	/* Make sure the msglist we're deleting actually exists, otherwise
	 * libdb will complain when we try to delete an invalid record
	 */
        cdbml = cdb_fetch(CDB_MSGLISTS, &whichroom->QRnumber, sizeof(long));
        if (cdbml != NULL) {
        	cdb_free(cdbml);

		/* Go ahead and delete it */
		cdb_delete(CDB_MSGLISTS, &whichroom->QRnumber, sizeof(long));
	}
}


/*
 * Message pointer compare function for sort_msglist()
 */
int sort_msglist_cmp(const void *m1, const void *m2) {
	if ((*(const long *)m1) > (*(const long *)m2)) return(1);
	if ((*(const long *)m1) < (*(const long *)m2)) return(-1);
	return(0);
}


/*
 * sort message pointers
 * (returns new msg count)
 */
int sort_msglist(long listptrs[], int oldcount)
{
	int numitems;

	numitems = oldcount;
	if (numitems < 2) {
		return (oldcount);
	}

	/* do the sort */
	qsort(listptrs, numitems, sizeof(long), sort_msglist_cmp);

	/* and yank any nulls */
	while ((numitems > 0) && (listptrs[0] == 0L)) {
		memmove(&listptrs[0], &listptrs[1], (sizeof(long) * (numitems - 1)));
		--numitems;
	}

	return (numitems);
}


/*
 * Determine whether a given room is non-editable.
 */
int CtdlIsNonEditable(struct ctdlroom *qrbuf)
{

	/* Mail> rooms are non-editable */
	if ( (qrbuf->QRflags & QR_MAILBOX)
	     && (!strcasecmp(&qrbuf->QRname[11], MAILROOM)) )
		return (1);

	/* Everything else is editable */
	return (0);
}


/*
 * Back-back-end for all room listing commands
 */
void list_roomname(struct ctdlroom *qrbuf, int ra, int current_view, int default_view)
{
	char truncated_roomname[ROOMNAMELEN];

	/* For my own mailbox rooms, chop off the owner prefix */
	if ( (qrbuf->QRflags & QR_MAILBOX)
	     && (atol(qrbuf->QRname) == CC->user.usernum) ) {
		safestrncpy(truncated_roomname, qrbuf->QRname, sizeof truncated_roomname);
		safestrncpy(truncated_roomname, &truncated_roomname[11], sizeof truncated_roomname);
		cprintf("%s", truncated_roomname);
	}
	/* For all other rooms, just display the name in its entirety */
	else {
		cprintf("%s", qrbuf->QRname);
	}

	/* ...and now the other parameters */
	cprintf("|%u|%d|%d|%d|%d|%d|%d|%ld|\n",
		qrbuf->QRflags,
		(int) qrbuf->QRfloor,
		(int) qrbuf->QRorder,
		(int) qrbuf->QRflags2,
		ra,
		current_view,
		default_view,
		qrbuf->QRmtime
	);
}


/* 
 * cmd_lrms()   -  List all accessible rooms, known or forgotten
 */
void cmd_lrms_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((( ra & (UA_KNOWN | UA_ZAPPED)))
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lrms(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Accessible rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lrms_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkra()   -  List all known rooms
 */
void cmd_lkra_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((( ra & (UA_KNOWN)))
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkra(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Known rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkra_backend, &FloorBeingSearched);
	cprintf("000\n");
}



void cmd_lprm_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if (   ((qrbuf->QRflags & QR_PRIVATE) == 0)
		&& ((qrbuf->QRflags & QR_MAILBOX) == 0)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lprm(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	cprintf("%d Public rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lprm_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkrn()   -  List all known rooms with new messages
 */
void cmd_lkrn_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_KNOWN)
	    && (ra & UA_HASNEWMSGS)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkrn(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Rooms w/ new msgs:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkrn_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lkro()   -  List all known rooms
 */
void cmd_lkro_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_KNOWN)
	    && ((ra & UA_HASNEWMSGS) == 0)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lkro(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Rooms w/o new msgs:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lkro_backend, &FloorBeingSearched);
	cprintf("000\n");
}



/* 
 * cmd_lzrm()   -  List all forgotten rooms
 */
void cmd_lzrm_backend(struct ctdlroom *qrbuf, void *data)
{
	int FloorBeingSearched = (-1);
	int ra;
	int view;

	FloorBeingSearched = *(int *)data;
	CtdlRoomAccess(qrbuf, &CC->user, &ra, &view);

	if ((ra & UA_GOTOALLOWED)
	    && (ra & UA_ZAPPED)
	    && ((qrbuf->QRfloor == (FloorBeingSearched))
		|| ((FloorBeingSearched) < 0)))
		list_roomname(qrbuf, ra, view, qrbuf->QRdefaultview);
}

void cmd_lzrm(char *argbuf)
{
	int FloorBeingSearched = (-1);
	if (!IsEmptyStr(argbuf))
		FloorBeingSearched = extract_int(argbuf, 0);

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	
	CtdlGetUser(&CC->user, CC->curr_user);
	cprintf("%d Zapped rooms:\n", LISTING_FOLLOWS);

	CtdlForEachRoom(cmd_lzrm_backend, &FloorBeingSearched);
	cprintf("000\n");
}


/*
 * Make the specified room the current room for this session.  No validation
 * or access control is done here -- the caller should make sure that the
 * specified room exists and is ok to access.
 */
void CtdlUserGoto(char *where, int display_result, int transiently,
		int *retmsgs, int *retnew)
{
	struct CitContext *CCC = CC;
	int a;
	int new_messages = 0;
	int old_messages = 0;
	int total_messages = 0;
	int info = 0;
	int rmailflag;
	int raideflag;
	int newmailcount = 0;
	visit vbuf;
	char truncated_roomname[ROOMNAMELEN];
        struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	unsigned int original_v_flags;
	int num_sets;
	int s;
	char setstr[128], lostr[64], histr[64];
	long lo, hi;
	int is_trash = 0;

	/* If the supplied room name is NULL, the caller wants us to know that
	 * it has already copied the room record into CC->room, so
	 * we can skip the extra database fetch.
	 */
	if (where != NULL) {
		safestrncpy(CCC->room.QRname, where, sizeof CCC->room.QRname);
		CtdlGetRoom(&CCC->room, where);
	}

	/* Take care of all the formalities. */

	begin_critical_section(S_USERS);
	CtdlGetRelationship(&vbuf, &CCC->user, &CCC->room);
	original_v_flags = vbuf.v_flags;

	/* Know the room ... but not if it's the page log room, or if the
	 * caller specified that we're only entering this room transiently.
	 */
	if ((strcasecmp(CCC->room.QRname, config.c_logpages))
	   && (transiently == 0) ) {
		vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
		vbuf.v_flags = vbuf.v_flags | V_ACCESS;
	}
	
	/* Only rewrite the database record if we changed something */
	if (vbuf.v_flags != original_v_flags) {
		CtdlSetRelationship(&vbuf, &CCC->user, &CCC->room);
	}
	end_critical_section(S_USERS);

	/* Check for new mail */
	newmailcount = NewMailCount();

	/* set info to 1 if the user needs to read the room's info file */
	if (CCC->room.QRinfo > vbuf.v_lastseen) {
		info = 1;
	}

        cdbfr = cdb_fetch(CDB_MSGLISTS, &CCC->room.QRnumber, sizeof(long));
        if (cdbfr != NULL) {
        	msglist = (long *) cdbfr->ptr;
		cdbfr->ptr = NULL;	/* CtdlUserGoto() now owns this memory */
        	num_msgs = cdbfr->len / sizeof(long);
        	cdb_free(cdbfr);
	}

	total_messages = 0;
	for (a=0; a<num_msgs; ++a) {
		if (msglist[a] > 0L) ++total_messages;
	}

	num_sets = num_tokens(vbuf.v_seen, ',');
	for (s=0; s<num_sets; ++s) {
		extract_token(setstr, vbuf.v_seen, s, ',', sizeof setstr);

		extract_token(lostr, setstr, 0, ':', sizeof lostr);
		if (num_tokens(setstr, ':') >= 2) {
			extract_token(histr, setstr, 1, ':', sizeof histr);
			if (!strcmp(histr, "*")) {
				snprintf(histr, sizeof histr, "%ld", LONG_MAX);
			}
		} 
		else {
			strcpy(histr, lostr);
		}
		lo = atol(lostr);
		hi = atol(histr);

		for (a=0; a<num_msgs; ++a) if (msglist[a] > 0L) {
			if ((msglist[a] >= lo) && (msglist[a] <= hi)) {
				++old_messages;
				msglist[a] = 0L;
			}
		}
	}
	new_messages = total_messages - old_messages;

	if (msglist != NULL) free(msglist);

	if (CCC->room.QRflags & QR_MAILBOX)
		rmailflag = 1;
	else
		rmailflag = 0;

	if ((CCC->room.QRroomaide == CCC->user.usernum)
	    || (CCC->user.axlevel >= AxAideU))
		raideflag = 1;
	else
		raideflag = 0;

	safestrncpy(truncated_roomname, CCC->room.QRname, sizeof truncated_roomname);
	if ( (CCC->room.QRflags & QR_MAILBOX)
	   && (atol(CCC->room.QRname) == CCC->user.usernum) ) {
		safestrncpy(truncated_roomname, &truncated_roomname[11], sizeof truncated_roomname);
	}

	if (!strcasecmp(truncated_roomname, USERTRASHROOM)) {
		is_trash = 1;
	}

	if (retmsgs != NULL) *retmsgs = total_messages;
	if (retnew != NULL) *retnew = new_messages;
	MSG_syslog(LOG_INFO, "<%s> %d new of %d total messages\n",
		   CCC->room.QRname,
		   new_messages, total_messages
		);

	CCC->curr_view = (int)vbuf.v_view;

	if (display_result) {
		cprintf("%d%c%s|%d|%d|%d|%d|%ld|%ld|%d|%d|%d|%d|%d|%d|%d|%d|%ld|\n",
			CIT_OK, CtdlCheckExpress(),
			truncated_roomname,
			(int)new_messages,
			(int)total_messages,
			(int)info,
			(int)CCC->room.QRflags,
			(long)CCC->room.QRhighest,
			(long)vbuf.v_lastseen,
			(int)rmailflag,
			(int)raideflag,
			(int)newmailcount,
			(int)CCC->room.QRfloor,
			(int)vbuf.v_view,
			(int)CCC->room.QRdefaultview,
			(int)is_trash,
			(int)CCC->room.QRflags2,
			(long)CCC->room.QRmtime
		);
	}
}


/*
 * Handle some of the macro named rooms
 */
void convert_room_name_macros(char *towhere, size_t maxlen) {
	if (!strcasecmp(towhere, "_BASEROOM_")) {
		safestrncpy(towhere, config.c_baseroom, maxlen);
	}
	else if (!strcasecmp(towhere, "_MAIL_")) {
		safestrncpy(towhere, MAILROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_TRASH_")) {
		safestrncpy(towhere, USERTRASHROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_DRAFTS_")) {
		safestrncpy(towhere, USERDRAFTROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_BITBUCKET_")) {
		safestrncpy(towhere, config.c_twitroom, maxlen);
	}
	else if (!strcasecmp(towhere, "_CALENDAR_")) {
		safestrncpy(towhere, USERCALENDARROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_TASKS_")) {
		safestrncpy(towhere, USERTASKSROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_CONTACTS_")) {
		safestrncpy(towhere, USERCONTACTSROOM, maxlen);
	}
	else if (!strcasecmp(towhere, "_NOTES_")) {
		safestrncpy(towhere, USERNOTESROOM, maxlen);
	}
}


/* 
 * cmd_goto()  -  goto a new room
 */
void cmd_goto(char *gargs)
{
	struct ctdlroom QRscratch;
	int c;
	int ok = 0;
	int ra;
	char augmented_roomname[ROOMNAMELEN];
	char towhere[ROOMNAMELEN];
	char password[32];
	int transiently = 0;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	extract_token(towhere, gargs, 0, '|', sizeof towhere);
	extract_token(password, gargs, 1, '|', sizeof password);
	transiently = extract_int(gargs, 2);

	CtdlGetUser(&CC->user, CC->curr_user);

	/*
	 * Handle some of the macro named rooms
	 */
	convert_room_name_macros(towhere, sizeof towhere);

	/* First try a regular match */
	c = CtdlGetRoom(&QRscratch, towhere);

	/* Then try a mailbox name match */
	if (c != 0) {
		CtdlMailboxName(augmented_roomname, sizeof augmented_roomname,
			    &CC->user, towhere);
		c = CtdlGetRoom(&QRscratch, augmented_roomname);
		if (c == 0)
			safestrncpy(towhere, augmented_roomname, sizeof towhere);
	}

	/* And if the room was found... */
	if (c == 0) {

		/* Let internal programs go directly to any room. */
		if (CC->internal_pgm) {
			memcpy(&CC->room, &QRscratch,
				sizeof(struct ctdlroom));
			CtdlUserGoto(NULL, 1, transiently, NULL, NULL);
			return;
		}

		/* See if there is an existing user/room relationship */
		CtdlRoomAccess(&QRscratch, &CC->user, &ra, NULL);

		/* normal clients have to pass through security */
		if (ra & UA_GOTOALLOWED) {
			ok = 1;
		}

		if (ok == 1) {
			if ((QRscratch.QRflags & QR_MAILBOX) &&
			    ((ra & UA_GOTOALLOWED))) {
				memcpy(&CC->room, &QRscratch,
					sizeof(struct ctdlroom));
				CtdlUserGoto(NULL, 1, transiently, NULL, NULL);
				return;
			} else if ((QRscratch.QRflags & QR_PASSWORDED) &&
			    ((ra & UA_KNOWN) == 0) &&
			    (strcasecmp(QRscratch.QRpasswd, password)) &&
			    (CC->user.axlevel < AxAideU)
			    ) {
				cprintf("%d wrong or missing passwd\n",
					ERROR + PASSWORD_REQUIRED);
				return;
			} else if ((QRscratch.QRflags & QR_PRIVATE) &&
				   ((QRscratch.QRflags & QR_PASSWORDED) == 0) &&
				   ((QRscratch.QRflags & QR_GUESSNAME) == 0) &&
				   ((ra & UA_KNOWN) == 0) &&
			           (CC->user.axlevel < AxAideU)
                                  ) {
				syslog(LOG_DEBUG, "Failed to acquire private room\n");
			} else {
				memcpy(&CC->room, &QRscratch,
					sizeof(struct ctdlroom));
				CtdlUserGoto(NULL, 1, transiently, NULL, NULL);
				return;
			}
		}
	}

	cprintf("%d room '%s' not found\n", ERROR + ROOM_NOT_FOUND, towhere);
}


void cmd_whok(char *cmdbuf)
{
	struct ctdluser temp;
	struct cdbdata *cdbus;
	int ra;

	cprintf("%d Who knows room:\n", LISTING_FOLLOWS);
	cdb_rewind(CDB_USERS);
	while (cdbus = cdb_next_item(CDB_USERS), cdbus != NULL) {
		memset(&temp, 0, sizeof temp);
		memcpy(&temp, cdbus->ptr, sizeof temp);
		cdb_free(cdbus);

		CtdlRoomAccess(&CC->room, &temp, &ra, NULL);
		if ((!IsEmptyStr(temp.fullname)) && 
		    (CC->room.QRflags & QR_INUSE) &&
		    (ra & UA_KNOWN)
			)
			cprintf("%s\n", temp.fullname);
	}
	cprintf("000\n");
}


/*
 * RDIR command for room directory
 */
void cmd_rdir(char *cmdbuf)
{
	char buf[256];
	char comment[256];
	FILE *fd;
	struct stat statbuf;
	DIR *filedir = NULL;
	struct dirent *filedir_entry;
	int d_namelen;
	char buf2[SIZ];
	char mimebuf[64];
	long len;
	
	if (CtdlAccessCheck(ac_logged_in)) return;
	
	CtdlGetRoom(&CC->room, CC->room.QRname);
	CtdlGetUser(&CC->user, CC->curr_user);

	if ((CC->room.QRflags & QR_DIRECTORY) == 0) {
		cprintf("%d not here.\n", ERROR + NOT_HERE);
		return;
	}
	if (((CC->room.QRflags & QR_VISDIR) == 0)
	    && (CC->user.axlevel < AxAideU)
	    && (CC->user.usernum != CC->room.QRroomaide)) {
		cprintf("%d not here.\n", ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	snprintf(buf, sizeof buf, "%s/%s", ctdl_file_dir, CC->room.QRdirname);
	filedir = opendir (buf);
	
	if (filedir == NULL) {
		cprintf("%d not here.\n", ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}
	cprintf("%d %s|%s/%s\n", LISTING_FOLLOWS, config.c_fqdn, ctdl_file_dir, CC->room.QRdirname);
	
	snprintf(buf, sizeof buf, "%s/%s/filedir", ctdl_file_dir, CC->room.QRdirname);
	fd = fopen(buf, "r");
	if (fd == NULL)
		fd = fopen("/dev/null", "r");
	while ((filedir_entry = readdir(filedir)))
	{
		if (strcasecmp(filedir_entry->d_name, "filedir") && filedir_entry->d_name[0] != '.')
		{
#ifdef _DIRENT_HAVE_D_NAMELEN
			d_namelen = filedir_entry->d_namelen;
#else
			d_namelen = strlen(filedir_entry->d_name);
#endif
			snprintf(buf, sizeof buf, "%s/%s/%s", ctdl_file_dir, CC->room.QRdirname, filedir_entry->d_name);
			stat(buf, &statbuf);	/* stat the file */
			if (!(statbuf.st_mode & S_IFREG))
			{
				snprintf(buf2, sizeof buf2,
					"\"%s\" appears in the file directory for room \"%s\" but is not a regular file.  Directories, named pipes, sockets, etc. are not usable in Citadel room directories.\n",
					buf, CC->room.QRname
				);
				CtdlAideMessage(buf2, "Unusable data found in room directory");
				continue;	/* not a useable file type so don't show it */
			}
			safestrncpy(comment, "", sizeof comment);
			fseek(fd, 0L, 0);	/* rewind descriptions file */
			/* Get the description from the descriptions file */
			while ((fgets(buf, sizeof buf, fd) != NULL) && (IsEmptyStr(comment))) 
			{
				buf[strlen(buf) - 1] = 0;
				if ((!strncasecmp(buf, filedir_entry->d_name, d_namelen)) && (buf[d_namelen] == ' '))
					safestrncpy(comment, &buf[d_namelen + 1], sizeof comment);
			}
			len = extract_token (mimebuf, comment, 0,' ', 64);
			if ((len <0) || strchr(mimebuf, '/') == NULL)
			{
				snprintf (mimebuf, 64, "application/octetstream");
				len = 0;
			}
			cprintf("%s|%ld|%s|%s\n", 
				filedir_entry->d_name, 
				(long)statbuf.st_size, 
				mimebuf, 
				&comment[len]);
		}
	}
	fclose(fd);
	closedir(filedir);
	
	cprintf("000\n");
}

/*
 * get room parameters (admin or room admin command)
 */
void cmd_getr(char *cmdbuf)
{
	if (CtdlAccessCheck(ac_room_aide)) return;

	CtdlGetRoom(&CC->room, CC->room.QRname);
	cprintf("%d%c%s|%s|%s|%d|%d|%d|%d|%d|\n",
		CIT_OK,
		CtdlCheckExpress(),

		((CC->room.QRflags & QR_MAILBOX) ?
			&CC->room.QRname[11] : CC->room.QRname),

		((CC->room.QRflags & QR_PASSWORDED) ?
			CC->room.QRpasswd : ""),

		((CC->room.QRflags & QR_DIRECTORY) ?
			CC->room.QRdirname : ""),

		CC->room.QRflags,
		(int) CC->room.QRfloor,
		(int) CC->room.QRorder,

		CC->room.QRdefaultview,
		CC->room.QRflags2
		);
}


/*
 * Back end function to rename a room.
 * You can also specify which floor to move the room to, or specify -1 to
 * keep the room on the same floor it was on.
 *
 * If you are renaming a mailbox room, you must supply the namespace prefix
 * in *at least* the old name!
 */
int CtdlRenameRoom(char *old_name, char *new_name, int new_floor) {
	int old_floor = 0;
	struct ctdlroom qrbuf;
	struct ctdlroom qrtmp;
	int ret = 0;
	struct floor *fl;
	struct floor flbuf;
	long owner = 0L;
	char actual_old_name[ROOMNAMELEN];

	syslog(LOG_DEBUG, "CtdlRenameRoom(%s, %s, %d)\n",
		old_name, new_name, new_floor);

	if (new_floor >= 0) {
		fl = CtdlGetCachedFloor(new_floor);
		if ((fl->f_flags & F_INUSE) == 0) {
			return(crr_invalid_floor);
		}
	}

	begin_critical_section(S_ROOMS);

	if ( (CtdlGetRoom(&qrtmp, new_name) == 0) 
	   && (strcasecmp(new_name, old_name)) ) {
		ret = crr_already_exists;
	}

	else if (CtdlGetRoom(&qrbuf, old_name) != 0) {
		ret = crr_room_not_found;
	}

	else if ( (CC->user.axlevel < AxAideU) && (!CC->internal_pgm)
		  && (CC->user.usernum != qrbuf.QRroomaide)
		  && ( (((qrbuf.QRflags & QR_MAILBOX) == 0) || (atol(qrbuf.QRname) != CC->user.usernum))) )  {
		ret = crr_access_denied;
	}

	else if (CtdlIsNonEditable(&qrbuf)) {
		ret = crr_noneditable;
	}

	else {
		/* Rename it */
		safestrncpy(actual_old_name, qrbuf.QRname, sizeof actual_old_name);
		if (qrbuf.QRflags & QR_MAILBOX) {
			owner = atol(qrbuf.QRname);
		}
		if ( (owner > 0L) && (atol(new_name) == 0L) ) {
			snprintf(qrbuf.QRname, sizeof(qrbuf.QRname),
					"%010ld.%s", owner, new_name);
		}
		else {
			safestrncpy(qrbuf.QRname, new_name,
						sizeof(qrbuf.QRname));
		}

		/* Reject change of floor for baseroom/aideroom */
		if (!strncasecmp(old_name, config.c_baseroom, ROOMNAMELEN) ||
		    !strncasecmp(old_name, config.c_aideroom, ROOMNAMELEN)) {
			new_floor = 0;
		}

		/* Take care of floor stuff */
		old_floor = qrbuf.QRfloor;
		if (new_floor < 0) {
			new_floor = old_floor;
		}
		qrbuf.QRfloor = new_floor;
		CtdlPutRoom(&qrbuf);

		begin_critical_section(S_CONFIG);
	
		/* If baseroom/aideroom name changes, update config */
		if (!strncasecmp(old_name, config.c_baseroom, ROOMNAMELEN)) {
			safestrncpy(config.c_baseroom, new_name, ROOMNAMELEN);
			put_config();
		}
		if (!strncasecmp(old_name, config.c_aideroom, ROOMNAMELEN)) {
			safestrncpy(config.c_aideroom, new_name, ROOMNAMELEN);
			put_config();
		}
	
		end_critical_section(S_CONFIG);
	
		/* If the room name changed, then there are now two room
		 * records, so we have to delete the old one.
		 */
		if (strcasecmp(new_name, old_name)) {
			b_deleteroom(actual_old_name);
		}

		ret = crr_ok;
	}

	end_critical_section(S_ROOMS);

	/* Adjust the floor reference counts if necessary */
	if (new_floor != old_floor) {
		lgetfloor(&flbuf, old_floor);
		--flbuf.f_ref_count;
		lputfloor(&flbuf, old_floor);
		syslog(LOG_DEBUG, "Reference count for floor %d is now %d\n", old_floor, flbuf.f_ref_count);
		lgetfloor(&flbuf, new_floor);
		++flbuf.f_ref_count;
		lputfloor(&flbuf, new_floor);
		syslog(LOG_DEBUG, "Reference count for floor %d is now %d\n", new_floor, flbuf.f_ref_count);
	}

	/* ...and everybody say "YATTA!" */	
	return(ret);
}


/*
 * set room parameters (admin or room admin command)
 */
void cmd_setr(char *args)
{
	char buf[256];
	int new_order = 0;
	int r;
	int new_floor;
	char new_name[ROOMNAMELEN];

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (num_parms(args) >= 6) {
		new_floor = extract_int(args, 5);
	} else {
		new_floor = (-1);	/* don't change the floor */
	}

	/* When is a new name more than just a new name?  When the old name
	 * has a namespace prefix.
	 */
	if (CC->room.QRflags & QR_MAILBOX) {
		sprintf(new_name, "%010ld.", atol(CC->room.QRname) );
	} else {
		safestrncpy(new_name, "", sizeof new_name);
	}
	extract_token(&new_name[strlen(new_name)], args, 0, '|', (sizeof new_name - strlen(new_name)));

	r = CtdlRenameRoom(CC->room.QRname, new_name, new_floor);

	if (r == crr_room_not_found) {
		cprintf("%d Internal error - room not found?\n", ERROR + INTERNAL_ERROR);
	} else if (r == crr_already_exists) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_name);
	} else if (r == crr_noneditable) {
		cprintf("%d Cannot edit this room.\n", ERROR + NOT_HERE);
	} else if (r == crr_invalid_floor) {
		cprintf("%d Target floor does not exist.\n",
			ERROR + INVALID_FLOOR_OPERATION);
	} else if (r == crr_access_denied) {
		cprintf("%d You do not have permission to edit '%s'\n",
			ERROR + HIGHER_ACCESS_REQUIRED,
			CC->room.QRname);
	} else if (r != crr_ok) {
		cprintf("%d Error: CtdlRenameRoom() returned %d\n",
			ERROR + INTERNAL_ERROR, r);
	}

	if (r != crr_ok) {
		return;
	}

	CtdlGetRoom(&CC->room, new_name);

	/* Now we have to do a bunch of other stuff */

	if (num_parms(args) >= 7) {
		new_order = extract_int(args, 6);
		if (new_order < 1)
			new_order = 1;
		if (new_order > 127)
			new_order = 127;
	}

	CtdlGetRoomLock(&CC->room, CC->room.QRname);

	/* Directory room */
	extract_token(buf, args, 2, '|', sizeof buf);
	buf[15] = 0;
	safestrncpy(CC->room.QRdirname, buf,
		sizeof CC->room.QRdirname);

	/* Default view */
	if (num_parms(args) >= 8) {
		CC->room.QRdefaultview = extract_int(args, 7);
	}

	/* Second set of flags */
	if (num_parms(args) >= 9) {
		CC->room.QRflags2 = extract_int(args, 8);
	}

	/* Misc. flags */
	CC->room.QRflags = (extract_int(args, 3) | QR_INUSE);
	/* Clean up a client boo-boo: if the client set the room to
	 * guess-name or passworded, ensure that the private flag is
	 * also set.
	 */
	if ((CC->room.QRflags & QR_GUESSNAME)
	    || (CC->room.QRflags & QR_PASSWORDED))
		CC->room.QRflags |= QR_PRIVATE;

	/* Some changes can't apply to BASEROOM */
	if (!strncasecmp(CC->room.QRname, config.c_baseroom,
			 ROOMNAMELEN)) {
		CC->room.QRorder = 0;
		CC->room.QRpasswd[0] = '\0';
		CC->room.QRflags &= ~(QR_PRIVATE & QR_PASSWORDED &
			QR_GUESSNAME & QR_PREFONLY & QR_MAILBOX);
		CC->room.QRflags |= QR_PERMANENT;
	} else {	
		/* March order (doesn't apply to AIDEROOM) */
		if (num_parms(args) >= 7)
			CC->room.QRorder = (char) new_order;
		/* Room password */
		extract_token(buf, args, 1, '|', sizeof buf);
		buf[10] = 0;
		safestrncpy(CC->room.QRpasswd, buf,
			    sizeof CC->room.QRpasswd);
		/* Kick everyone out if the client requested it
		 * (by changing the room's generation number)
		 */
		if (extract_int(args, 4)) {
			time(&CC->room.QRgen);
		}
	}
	/* Some changes can't apply to AIDEROOM */
	if (!strncasecmp(CC->room.QRname, config.c_baseroom,
			 ROOMNAMELEN)) {
		CC->room.QRorder = 0;
		CC->room.QRflags &= ~QR_MAILBOX;
		CC->room.QRflags |= QR_PERMANENT;
	}

	/* Write the room record back to disk */
	CtdlPutRoomLock(&CC->room);

	/* Create a room directory if necessary */
	if (CC->room.QRflags & QR_DIRECTORY) {
		snprintf(buf, sizeof buf,"%s/%s",
				 ctdl_file_dir,
				 CC->room.QRdirname);
		mkdir(buf, 0755);
	}
	snprintf(buf, sizeof buf, "The room \"%s\" has been edited by %s.\n",
		CC->room.QRname,
		(CC->logged_in ? CC->curr_user : "an administrator")
	);
	CtdlAideMessage(buf, "Room modification Message");
	cprintf("%d Ok\n", CIT_OK);
}



/* 
 * get the name of the room admin for this room
 */
void cmd_geta(char *cmdbuf)
{
	struct ctdluser usbuf;

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (CtdlGetUserByNumber(&usbuf, CC->room.QRroomaide) == 0) {
		cprintf("%d %s\n", CIT_OK, usbuf.fullname);
	} else {
		cprintf("%d \n", CIT_OK);
	}
}


/* 
 * set the room admin for this room
 */
void cmd_seta(char *new_ra)
{
	struct ctdluser usbuf;
	long newu;
	char buf[SIZ];
	int post_notice;

	if (CtdlAccessCheck(ac_room_aide)) return;

	if (CtdlGetUser(&usbuf, new_ra) != 0) {
		newu = (-1L);
	} else {
		newu = usbuf.usernum;
	}

	CtdlGetRoomLock(&CC->room, CC->room.QRname);
	post_notice = 0;
	if (CC->room.QRroomaide != newu) {
		post_notice = 1;
	}
	CC->room.QRroomaide = newu;
	CtdlPutRoomLock(&CC->room);

	/*
	 * We have to post the change notice _after_ writing changes to 
	 * the room table, otherwise it would deadlock!
	 */
	if (post_notice == 1) {
		if (!IsEmptyStr(usbuf.fullname))
			snprintf(buf, sizeof buf,
				"%s is now the room admin for \"%s\".\n",
				usbuf.fullname, CC->room.QRname);
		else
			snprintf(buf, sizeof buf,
				"There is now no room admin for \"%s\".\n",
				CC->room.QRname);
		CtdlAideMessage(buf, "Admin Room Modification");
	}
	cprintf("%d Ok\n", CIT_OK);
}

/* 
 * retrieve info file for this room
 */
void cmd_rinf(char *gargs)
{
	char filename[PATH_MAX];
	char buf[SIZ];
	FILE *info_fp;

	assoc_file_name(filename, sizeof filename, &CC->room, ctdl_info_dir);
	info_fp = fopen(filename, "r");

	if (info_fp == NULL) {
		cprintf("%d No info file.\n", ERROR + FILE_NOT_FOUND);
		return;
	}
	cprintf("%d Info:\n", LISTING_FOLLOWS);
	while (fgets(buf, sizeof buf, info_fp) != NULL) {
		if (!IsEmptyStr(buf))
			buf[strlen(buf) - 1] = 0;
		cprintf("%s\n", buf);
	}
	cprintf("000\n");
	fclose(info_fp);
}

/*
 * Asynchronously schedule a room for deletion.  The room will appear
 * deleted to the user(s), but it won't actually get purged from the
 * database until THE DREADED AUTO-PURGER makes its next run.
 */
void CtdlScheduleRoomForDeletion(struct ctdlroom *qrbuf)
{
	char old_name[ROOMNAMELEN];
	static int seq = 0;

	syslog(LOG_NOTICE, "Scheduling room <%s> for deletion\n",
		qrbuf->QRname);

	safestrncpy(old_name, qrbuf->QRname, sizeof old_name);

	CtdlGetRoom(qrbuf, qrbuf->QRname);

	/* Turn the room into a private mailbox owned by a user who doesn't
	 * exist.  This will immediately make the room invisible to everyone,
	 * and qualify the room for purging.
	 */
	snprintf(qrbuf->QRname, sizeof qrbuf->QRname, "9999999999.%08lx.%04d.%s",
		time(NULL),
		++seq,
		old_name
	);
	qrbuf->QRflags |= QR_MAILBOX;
	time(&qrbuf->QRgen);	/* Use a timestamp as the new generation number  */

	CtdlPutRoom(qrbuf);

	b_deleteroom(old_name);
}



/*
 * Back end processing to delete a room and everything associated with it
 * (This one is synchronous and should only get called by THE DREADED
 * AUTO-PURGER in serv_expire.c.  All user-facing code should call
 * the asynchronous schedule_room_for_deletion() instead.)
 */
void CtdlDeleteRoom(struct ctdlroom *qrbuf)
{
	struct floor flbuf;
	char filename[100];
	/* TODO: filename magic? does this realy work? */

	syslog(LOG_NOTICE, "Deleting room <%s>\n", qrbuf->QRname);

	/* Delete the info file */
	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_info_dir);
	unlink(filename);

	/* Delete the image file */
	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_image_dir);
	unlink(filename);

	/* Delete the room's network config file */
	assoc_file_name(filename, sizeof filename, qrbuf, ctdl_netcfg_dir);
	unlink(filename);

	/* Delete the messages in the room
	 * (Careful: this opens an S_ROOMS critical section!)
	 */
	CtdlDeleteMessages(qrbuf->QRname, NULL, 0, "");

	/* Flag the room record as not in use */
	CtdlGetRoomLock(qrbuf, qrbuf->QRname);
	qrbuf->QRflags = 0;
	CtdlPutRoomLock(qrbuf);

	/* then decrement the reference count for the floor */
	lgetfloor(&flbuf, (int) (qrbuf->QRfloor));
	flbuf.f_ref_count = flbuf.f_ref_count - 1;
	lputfloor(&flbuf, (int) (qrbuf->QRfloor));

	/* Delete the room record from the database! */
	b_deleteroom(qrbuf->QRname);
}



/*
 * Check access control for deleting a room
 */
int CtdlDoIHavePermissionToDeleteThisRoom(struct ctdlroom *qr) {

	if ((!(CC->logged_in)) && (!(CC->internal_pgm))) {
		return(0);
	}

	if (CtdlIsNonEditable(qr)) {
		return(0);
	}

	/*
	 * For mailboxes, check stuff
	 */
	if (qr->QRflags & QR_MAILBOX) {

		if (strlen(qr->QRname) < 12) return(0); /* bad name */

		if (atol(qr->QRname) != CC->user.usernum) {
			return(0);	/* not my room */
		}

		/* Can't delete your Mail> room */
		if (!strcasecmp(&qr->QRname[11], MAILROOM)) return(0);

		/* Otherwise it's ok */
		return(1);
	}

	/*
	 * For normal rooms, just check for admin or room admin status.
	 */
	return(is_room_aide());
}

/*
 * admin command: kill the current room
 */
void cmd_kill(char *argbuf)
{
	char deleted_room_name[ROOMNAMELEN];
	char msg[SIZ];
	int kill_ok;

	kill_ok = extract_int(argbuf, 0);

	if (CtdlDoIHavePermissionToDeleteThisRoom(&CC->room) == 0) {
		cprintf("%d Can't delete this room.\n", ERROR + NOT_HERE);
		return;
	}
	if (kill_ok) {
		if (CC->room.QRflags & QR_MAILBOX) {
			safestrncpy(deleted_room_name, &CC->room.QRname[11], sizeof deleted_room_name);
		}
		else {
			safestrncpy(deleted_room_name, CC->room.QRname, sizeof deleted_room_name);
		}

		/* Do the dirty work */
		CtdlScheduleRoomForDeletion(&CC->room);

		/* Return to the Lobby */
		CtdlUserGoto(config.c_baseroom, 0, 0, NULL, NULL);

		/* tell the world what we did */
		snprintf(msg, sizeof msg, "The room \"%s\" has been deleted by %s.\n",
			 deleted_room_name,
			(CC->logged_in ? CC->curr_user : "an administrator")
		);
		CtdlAideMessage(msg, "Room Purger Message");
		cprintf("%d '%s' deleted.\n", CIT_OK, deleted_room_name);
	} else {
		cprintf("%d ok to delete.\n", CIT_OK);
	}
}


/*
 * Internal code to create a new room (returns room flags)
 *
 * Room types:  0=public, 1=hidden, 2=passworded, 3=invitation-only,
 *              4=mailbox, 5=mailbox, but caller supplies namespace
 */
unsigned CtdlCreateRoom(char *new_room_name,
		     int new_room_type,
		     char *new_room_pass,
		     int new_room_floor,
		     int really_create,
		     int avoid_access,
		     int new_room_view)
{

	struct ctdlroom qrbuf;
	struct floor flbuf;
	visit vbuf;

	syslog(LOG_DEBUG, "CtdlCreateRoom(name=%s, type=%d, view=%d)\n",
		new_room_name, new_room_type, new_room_view);

	if (CtdlGetRoom(&qrbuf, new_room_name) == 0) {
		syslog(LOG_DEBUG, "%s already exists.\n", new_room_name);
		return(0);
	}

	memset(&qrbuf, 0, sizeof(struct ctdlroom));
	safestrncpy(qrbuf.QRpasswd, new_room_pass, sizeof qrbuf.QRpasswd);
	qrbuf.QRflags = QR_INUSE;
	if (new_room_type > 0)
		qrbuf.QRflags = (qrbuf.QRflags | QR_PRIVATE);
	if (new_room_type == 1)
		qrbuf.QRflags = (qrbuf.QRflags | QR_GUESSNAME);
	if (new_room_type == 2)
		qrbuf.QRflags = (qrbuf.QRflags | QR_PASSWORDED);
	if ( (new_room_type == 4) || (new_room_type == 5) ) {
		qrbuf.QRflags = (qrbuf.QRflags | QR_MAILBOX);
		/* qrbuf.QRflags2 |= QR2_SUBJECTREQ; */
	}

	/* If the user is requesting a personal room, set up the room
	 * name accordingly (prepend the user number)
	 */
	if (new_room_type == 4) {
		CtdlMailboxName(qrbuf.QRname, sizeof qrbuf.QRname, &CC->user, new_room_name);
	}
	else {
		safestrncpy(qrbuf.QRname, new_room_name, sizeof qrbuf.QRname);
	}

	/* If the room is private, and the system administrator has elected
	 * to automatically grant room admin privileges, do so now.
	 */
	if ((qrbuf.QRflags & QR_PRIVATE) && (CREATAIDE == 1)) {
		qrbuf.QRroomaide = CC->user.usernum;
	}
	/* Blog owners automatically become room admins of their blogs.
	 * (In the future we will offer a site-wide configuration setting to suppress this behavior.)
	 */
	else if (new_room_view == VIEW_BLOG) {
		qrbuf.QRroomaide = CC->user.usernum;
	}
	/* Otherwise, set the room admin to undefined.
	 */
	else {
		qrbuf.QRroomaide = (-1L);
	}

	/* 
	 * If the caller is only interested in testing whether this will work,
	 * return now without creating the room.
	 */
	if (!really_create) return (qrbuf.QRflags);

	qrbuf.QRnumber = get_new_room_number();
	qrbuf.QRhighest = 0L;	/* No messages in this room yet */
	time(&qrbuf.QRgen);	/* Use a timestamp as the generation number */
	qrbuf.QRfloor = new_room_floor;
	qrbuf.QRdefaultview = new_room_view;

	/* save what we just did... */
	CtdlPutRoom(&qrbuf);

	/* bump the reference count on whatever floor the room is on */
	lgetfloor(&flbuf, (int) qrbuf.QRfloor);
	flbuf.f_ref_count = flbuf.f_ref_count + 1;
	lputfloor(&flbuf, (int) qrbuf.QRfloor);

	/* Grant the creator access to the room unless the avoid_access
	 * parameter was specified.
	 */
	if ( (CC->logged_in) && (avoid_access == 0) ) {
		CtdlGetRelationship(&vbuf, &CC->user, &qrbuf);
		vbuf.v_flags = vbuf.v_flags & ~V_FORGET & ~V_LOCKOUT;
		vbuf.v_flags = vbuf.v_flags | V_ACCESS;
		CtdlSetRelationship(&vbuf, &CC->user, &qrbuf);
	}

	/* resume our happy day */
	return (qrbuf.QRflags);
}


/*
 * create a new room
 */
void cmd_cre8(char *args)
{
	int cre8_ok;
	char new_room_name[ROOMNAMELEN];
	int new_room_type;
	char new_room_pass[32];
	int new_room_floor;
	int new_room_view;
	char *notification_message = NULL;
	unsigned newflags;
	struct floor *fl;
	int avoid_access = 0;

	cre8_ok = extract_int(args, 0);
	extract_token(new_room_name, args, 1, '|', sizeof new_room_name);
	new_room_name[ROOMNAMELEN - 1] = 0;
	new_room_type = extract_int(args, 2);
	extract_token(new_room_pass, args, 3, '|', sizeof new_room_pass);
	avoid_access = extract_int(args, 5);
	new_room_view = extract_int(args, 6);
	new_room_pass[9] = 0;
	new_room_floor = 0;

	if ((IsEmptyStr(new_room_name)) && (cre8_ok == 1)) {
		cprintf("%d Invalid room name.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (!strcasecmp(new_room_name, MAILROOM)) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_room_name);
		return;
	}

	if (num_parms(args) >= 5) {
		fl = CtdlGetCachedFloor(extract_int(args, 4));
		if (fl == NULL) {
			cprintf("%d Invalid floor number.\n",
				ERROR + INVALID_FLOOR_OPERATION);
			return;
		}
		else if ((fl->f_flags & F_INUSE) == 0) {
			cprintf("%d Invalid floor number.\n",
				ERROR + INVALID_FLOOR_OPERATION);
			return;
		} else {
			new_room_floor = extract_int(args, 4);
		}
	}

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (CC->user.axlevel < config.c_createax && !CC->internal_pgm) {
		cprintf("%d You need higher access to create rooms.\n",
			ERROR + HIGHER_ACCESS_REQUIRED);
		return;
	}

	if ((IsEmptyStr(new_room_name)) && (cre8_ok == 0)) {
		cprintf("%d Ok to create rooms.\n", CIT_OK);
		return;
	}

	if ((new_room_type < 0) || (new_room_type > 5)) {
		cprintf("%d Invalid room type.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (new_room_type == 5) {
		if (CC->user.axlevel < AxAideU) {
			cprintf("%d Higher access required\n", 
				ERROR + HIGHER_ACCESS_REQUIRED);
			return;
		}
	}

	/* Check to make sure the requested room name doesn't already exist */
	newflags = CtdlCreateRoom(new_room_name,
				new_room_type, new_room_pass, new_room_floor,
				0, avoid_access, new_room_view);
	if (newflags == 0) {
		cprintf("%d '%s' already exists.\n",
			ERROR + ALREADY_EXISTS, new_room_name);
		return;
	}

	if (cre8_ok == 0) {
		cprintf("%d OK to create '%s'\n", CIT_OK, new_room_name);
		return;
	}

	/* If we reach this point, the room needs to be created. */

	newflags = CtdlCreateRoom(new_room_name,
			   new_room_type, new_room_pass, new_room_floor, 1, 0,
			   new_room_view);

	/* post a message in Aide> describing the new room */
	notification_message = malloc(1024);
	snprintf(notification_message, 1024,
		"A new room called \"%s\" has been created by %s%s%s%s%s%s\n",
		new_room_name,
		(CC->logged_in ? CC->curr_user : "an administrator"),
		((newflags & QR_MAILBOX) ? " [personal]" : ""),
		((newflags & QR_PRIVATE) ? " [private]" : ""),
		((newflags & QR_GUESSNAME) ? " [hidden]" : ""),
		((newflags & QR_PASSWORDED) ? " Password: " : ""),
		((newflags & QR_PASSWORDED) ? new_room_pass : "")
	);
	CtdlAideMessage(notification_message, "Room Creation Message");
	free(notification_message);

	cprintf("%d '%s' has been created.\n", CIT_OK, new_room_name);
}



void cmd_einf(char *ok)
{				/* enter info file for current room */
	FILE *fp;
	char infofilename[SIZ];
	char buf[SIZ];

	unbuffer_output();

	if (CtdlAccessCheck(ac_room_aide)) return;

	if (atoi(ok) == 0) {
		cprintf("%d Ok.\n", CIT_OK);
		return;
	}
	assoc_file_name(infofilename, sizeof infofilename, &CC->room, ctdl_info_dir);
	syslog(LOG_DEBUG, "opening\n");
	fp = fopen(infofilename, "w");
	syslog(LOG_DEBUG, "checking\n");
	if (fp == NULL) {
		cprintf("%d Cannot open %s: %s\n",
		  ERROR + INTERNAL_ERROR, infofilename, strerror(errno));
		return;
	}
	cprintf("%d Send info...\n", SEND_LISTING);

	do {
		client_getln(buf, sizeof buf);
		if (strcmp(buf, "000"))
			fprintf(fp, "%s\n", buf);
	} while (strcmp(buf, "000"));
	fclose(fp);

	/* now update the room index so people will see our new info */
	CtdlGetRoomLock(&CC->room, CC->room.QRname);		/* lock so no one steps on us */
	CC->room.QRinfo = CC->room.QRhighest + 1L;
	CtdlPutRoomLock(&CC->room);
}


/* 
 * cmd_lflr()   -  List all known floors
 */
void cmd_lflr(char *gargs)
{
	int a;
	struct floor flbuf;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	cprintf("%d Known floors:\n", LISTING_FOLLOWS);

	for (a = 0; a < MAXFLOORS; ++a) {
		CtdlGetFloor(&flbuf, a);
		if (flbuf.f_flags & F_INUSE) {
			cprintf("%d|%s|%d\n",
				a,
				flbuf.f_name,
				flbuf.f_ref_count);
		}
	}
	cprintf("000\n");
}



/*
 * create a new floor
 */
void cmd_cflr(char *argbuf)
{
	char new_floor_name[256];
	struct floor flbuf;
	int cflr_ok;
	int free_slot = (-1);
	int a;

	extract_token(new_floor_name, argbuf, 0, '|', sizeof new_floor_name);
	cflr_ok = extract_int(argbuf, 1);

	if (CtdlAccessCheck(ac_aide)) return;

	if (IsEmptyStr(new_floor_name)) {
		cprintf("%d Blank floor name not allowed.\n",
			ERROR + ILLEGAL_VALUE);
		return;
	}

	for (a = 0; a < MAXFLOORS; ++a) {
		CtdlGetFloor(&flbuf, a);

		/* note any free slots while we're scanning... */
		if (((flbuf.f_flags & F_INUSE) == 0)
		    && (free_slot < 0))
			free_slot = a;

		/* check to see if it already exists */
		if ((!strcasecmp(flbuf.f_name, new_floor_name))
		    && (flbuf.f_flags & F_INUSE)) {
			cprintf("%d Floor '%s' already exists.\n",
				ERROR + ALREADY_EXISTS,
				flbuf.f_name);
			return;
		}
	}

	if (free_slot < 0) {
		cprintf("%d There is no space available for a new floor.\n",
			ERROR + INVALID_FLOOR_OPERATION);
		return;
	}
	if (cflr_ok == 0) {
		cprintf("%d ok to create...\n", CIT_OK);
		return;
	}
	lgetfloor(&flbuf, free_slot);
	flbuf.f_flags = F_INUSE;
	flbuf.f_ref_count = 0;
	safestrncpy(flbuf.f_name, new_floor_name, sizeof flbuf.f_name);
	lputfloor(&flbuf, free_slot);
	cprintf("%d %d\n", CIT_OK, free_slot);
}



/*
 * delete a floor
 */
void cmd_kflr(char *argbuf)
{
	struct floor flbuf;
	int floor_to_delete;
	int kflr_ok;
	int delete_ok;

	floor_to_delete = extract_int(argbuf, 0);
	kflr_ok = extract_int(argbuf, 1);

	if (CtdlAccessCheck(ac_aide)) return;

	lgetfloor(&flbuf, floor_to_delete);

	delete_ok = 1;
	if ((flbuf.f_flags & F_INUSE) == 0) {
		cprintf("%d Floor %d not in use.\n",
			ERROR + INVALID_FLOOR_OPERATION, floor_to_delete);
		delete_ok = 0;
	} else {
		if (flbuf.f_ref_count != 0) {
			cprintf("%d Cannot delete; floor contains %d rooms.\n",
				ERROR + INVALID_FLOOR_OPERATION,
				flbuf.f_ref_count);
			delete_ok = 0;
		} else {
			if (kflr_ok == 1) {
				cprintf("%d Ok\n", CIT_OK);
			} else {
				cprintf("%d Ok to delete...\n", CIT_OK);
			}

		}

	}

	if ((delete_ok == 1) && (kflr_ok == 1))
		flbuf.f_flags = 0;
	lputfloor(&flbuf, floor_to_delete);
}

/*
 * edit a floor
 */
void cmd_eflr(char *argbuf)
{
	struct floor flbuf;
	int floor_num;
	int np;

	np = num_parms(argbuf);
	if (np < 1) {
		cprintf("%d Usage error.\n", ERROR + ILLEGAL_VALUE);
		return;
	}

	if (CtdlAccessCheck(ac_aide)) return;

	floor_num = extract_int(argbuf, 0);
	lgetfloor(&flbuf, floor_num);
	if ((flbuf.f_flags & F_INUSE) == 0) {
		lputfloor(&flbuf, floor_num);
		cprintf("%d Floor %d is not in use.\n",
			ERROR + INVALID_FLOOR_OPERATION, floor_num);
		return;
	}
	if (np >= 2)
		extract_token(flbuf.f_name, argbuf, 1, '|', sizeof flbuf.f_name);
	lputfloor(&flbuf, floor_num);

	cprintf("%d Ok\n", CIT_OK);
}



/* 
 * cmd_stat()  -  return the modification time of the current room (maybe other things in the future)
 */
void cmd_stat(char *gargs)
{
	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;
	CtdlGetRoom(&CC->room, CC->room.QRname);
	cprintf("%d %s|%ld|\n", CIT_OK, CC->room.QRname, CC->room.QRmtime);
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/

CTDL_MODULE_INIT(room_ops)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_lrms, "LRMS", "List rooms");
		CtdlRegisterProtoHook(cmd_lkra, "LKRA", "List all known rooms");
		CtdlRegisterProtoHook(cmd_lkrn, "LKRN", "List known rooms with new messages");
		CtdlRegisterProtoHook(cmd_lkro, "LKRO", "List known rooms without new messages");
		CtdlRegisterProtoHook(cmd_lzrm, "LZRM", "List zapped rooms");
		CtdlRegisterProtoHook(cmd_lprm, "LPRM", "List public rooms");
		CtdlRegisterProtoHook(cmd_goto, "GOTO", "Goto a named room");
		CtdlRegisterProtoHook(cmd_stat, "STAT", "Get mtime of the current room");
		CtdlRegisterProtoHook(cmd_whok, "WHOK", "List users who know this room");
		CtdlRegisterProtoHook(cmd_rdir, "RDIR", "List files in room directory");
		CtdlRegisterProtoHook(cmd_getr, "GETR", "Get room parameters");
		CtdlRegisterProtoHook(cmd_setr, "SETR", "Set room parameters");
		CtdlRegisterProtoHook(cmd_geta, "GETA", "Get the room admin name");
		CtdlRegisterProtoHook(cmd_seta, "SETA", "Set the room admin for this room");
		CtdlRegisterProtoHook(cmd_rinf, "RINF", "Fetch room info file");
		CtdlRegisterProtoHook(cmd_kill, "KILL", "Kill (delete) the current room");
		CtdlRegisterProtoHook(cmd_cre8, "CRE8", "Create a new room");
		CtdlRegisterProtoHook(cmd_einf, "EINF", "Enter info file for the current room");
		CtdlRegisterProtoHook(cmd_lflr, "LFLR", "List all known floors");
		CtdlRegisterProtoHook(cmd_cflr, "CFLR", "Create a new floor");
		CtdlRegisterProtoHook(cmd_kflr, "KFLR", "Kill a floor");
		CtdlRegisterProtoHook(cmd_eflr, "EFLR", "Edit a floor");
	}
        /* return our Subversion id for the Log */
	return "room_ops";
}
