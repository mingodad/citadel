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

#include <stdio.h>
#include <libcitadel.h>

#include "citserver.h"

#include "ctdl_module.h"
#include "config.h"
#include "control.h"
#include "user_ops.h"
#include "room_ops.h"

struct floor *floorcache[MAXFLOORS];

/* 
 * Determine whether the currently logged in session has permission to read
 * messages in the current room.
 */
int CtdlDoIHavePermissionToReadMessagesInThisRoom(void) {
	if (	(!(CC->logged_in))
		&& (!(CC->internal_pgm))
		&& (!config.c_guest_logins)
	) {
		return(om_not_logged_in);
	}
	return(om_ok);
}

/*
 * Check to see whether we have permission to post a message in the current
 * room.  Returns a *CITADEL ERROR CODE* and puts a message in errmsgbuf, or
 * returns 0 on success.
 */
int CtdlDoIHavePermissionToPostInThisRoom(
	char *errmsgbuf, 
	size_t n, 
	const char* RemoteIdentifier,
	PostType PostPublic,
	int is_reply
	) {
	int ra;

	if (!(CC->logged_in) && 
	    (PostPublic == POST_LOGGED_IN)) {
		snprintf(errmsgbuf, n, "Not logged in.");
		return (ERROR + NOT_LOGGED_IN);
	}
	else if (PostPublic == CHECK_EXISTANCE) {
		return (0); // We're Evaling whether a recipient exists
	}
	else if (!(CC->logged_in)) {
		
		if ((CC->room.QRflags & QR_READONLY)) {
			snprintf(errmsgbuf, n, "Not logged in.");
			return (ERROR + NOT_LOGGED_IN);
		}
		if (CC->room.QRflags2 & QR2_MODERATED) {
			snprintf(errmsgbuf, n, "Not logged in Moderation feature not yet implemented!");
			return (ERROR + NOT_LOGGED_IN);
		}
		if ((PostPublic!=POST_LMTP) &&(CC->room.QRflags2 & QR2_SMTP_PUBLIC) == 0) {

			return CtdlNetconfigCheckRoomaccess(errmsgbuf, n, RemoteIdentifier);
		}
		return (0);

	}

	if ((CC->user.axlevel < AxProbU)
	    && ((CC->room.QRflags & QR_MAILBOX) == 0)) {
		snprintf(errmsgbuf, n, "Need to be validated to enter (except in %s> to sysop)", MAILROOM);
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);

	if (ra & UA_POSTALLOWED) {
		strcpy(errmsgbuf, "OK to post or reply here");
		return(0);
	}

	if ( (ra & UA_REPLYALLOWED) && (is_reply) ) {
		/*
		 * To be thorough, we ought to check to see if the message they are
		 * replying to is actually a valid one in this room, but unless this
		 * actually becomes a problem we'll go with high performance instead.
		 */
		strcpy(errmsgbuf, "OK to reply here");
		return(0);
	}

	if ( (ra & UA_REPLYALLOWED) && (!is_reply) ) {
		/* Clarify what happened with a better error message */
		snprintf(errmsgbuf, n, "You may only reply to existing messages here.");
		return (ERROR + HIGHER_ACCESS_REQUIRED);
	}

	snprintf(errmsgbuf, n, "Higher access is required to post in this room.");
	return (ERROR + HIGHER_ACCESS_REQUIRED);

}

/*
 * Check whether the current user has permission to delete messages from
 * the current room (returns 1 for yes, 0 for no)
 */
int CtdlDoIHavePermissionToDeleteMessagesFromThisRoom(void) {
	int ra;
	CtdlRoomAccess(&CC->room, &CC->user, &ra, NULL);
	if (ra & UA_DELETEALLOWED) return(1);
	return(0);
}

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
	int i = 0;

	numitems = oldcount;
	if (numitems < 2) {
		return (oldcount);
	}

	/* do the sort */
	qsort(listptrs, numitems, sizeof(long), sort_msglist_cmp);

	/* and yank any nulls */
	while ((i < numitems) && (listptrs[i] == 0L)) i++;

	if (i > 0)
	{
		memmove(&listptrs[0], &listptrs[i], (sizeof(long) * (numitems - i)));
		numitems-=i;
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
