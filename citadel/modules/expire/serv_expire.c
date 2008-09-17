/*
 * $Id$
 *
 * This module handles the expiry of old messages and the purging of old users.
 *
 */


/*
 * A brief technical discussion:
 *
 * Several of the purge operations found in this module operate in two
 * stages: the first stage generates a linked list of objects to be deleted,
 * then the second stage deletes all listed objects from the database.
 *
 * At first glance this may seem cumbersome and unnecessary.  The reason it is
 * implemented in this way is because Berkeley DB, and possibly other backends
 * we may hook into in the future, explicitly do _not_ support the deletion of
 * records from a file while the file is being traversed.  The delete operation
 * will succeed, but the traversal is not guaranteed to visit every object if
 * this is done.  Therefore we utilize the two-stage purge.
 *
 * When using Berkeley DB, there's another reason for the two-phase purge: we
 * don't want the entire thing being done as one huge transaction.
 *
 * You'll also notice that we build the in-memory list of records to be deleted
 * sometimes with a linked list and sometimes with a hash table.  There is no
 * reason for this aside from the fact that the linked list ones were written
 * before we had the hash table library available.
 */


#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <pwd.h>
#include <errno.h>
#include <sys/types.h>

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

#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "user_ops.h"
#include "control.h"
#include "serv_network.h"	/* Needed for defenition of UseTable */
#include "threads.h"

#include "ctdl_module.h"


struct PurgeList {
	struct PurgeList *next;
	char name[ROOMNAMELEN];	/* use the larger of username or roomname */
};

struct VPurgeList {
	struct VPurgeList *next;
	long vp_roomnum;
	long vp_roomgen;
	long vp_usernum;
};

struct ValidRoom {
	struct ValidRoom *next;
	long vr_roomnum;
	long vr_roomgen;
};

struct ValidUser {
	struct ValidUser *next;
	long vu_usernum;
};


struct ctdlroomref {
	struct ctdlroomref *next;
	long msgnum;
};

struct UPurgeList {
	struct UPurgeList *next;
	char up_key[256];
};

struct EPurgeList {
	struct EPurgeList *next;
	int ep_keylen;
	char *ep_key;
};


struct PurgeList *UserPurgeList = NULL;
struct PurgeList *RoomPurgeList = NULL;
struct ValidRoom *ValidRoomList = NULL;
struct ValidUser *ValidUserList = NULL;
int messages_purged;
int users_not_purged;
char *users_corrupt_msg = NULL;
char *users_zero_msg = NULL;
struct ctdlroomref *rr = NULL;
extern struct CitContext *ContextList;
int force_purge_now = 0;			/* set to nonzero to force a run right now */


/*
 * First phase of message purge -- gather the locations of messages which
 * qualify for purging and write them to a temp file.
 */
void GatherPurgeMessages(struct ctdlroom *qrbuf, void *data) {
	struct ExpirePolicy epbuf;
	long delnum;
	time_t xtime, now;
	struct CtdlMessage *msg = NULL;
	int a;
        struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;
	FILE *purgelist;

	purgelist = (FILE *)data;
	fprintf(purgelist, "r=%s\n", qrbuf->QRname);

	time(&now);
	GetExpirePolicy(&epbuf, qrbuf);

	/* If the room is set to never expire messages ... do nothing */
	if (epbuf.expire_mode == EXPIRE_NEXTLEVEL) return;
	if (epbuf.expire_mode == EXPIRE_MANUAL) return;

	/* Don't purge messages containing system configuration, dumbass. */
	if (!strcasecmp(qrbuf->QRname, SYSCONFIGROOM)) return;

	/* Ok, we got this far ... now let's see what's in the room */
        cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf->QRnumber, sizeof(long));

        if (cdbfr != NULL) {
        	msglist = malloc(cdbfr->len);
        	memcpy(msglist, cdbfr->ptr, cdbfr->len);
        	num_msgs = cdbfr->len / sizeof(long);
        	cdb_free(cdbfr);
	}

	/* Nothing to do if there aren't any messages */
	if (num_msgs == 0) {
		if (msglist != NULL) free(msglist);
		return;
	}


	/* If the room is set to expire by count, do that */
	if (epbuf.expire_mode == EXPIRE_NUMMSGS) {
		if (num_msgs > epbuf.expire_value) {
			for (a=0; a<(num_msgs - epbuf.expire_value); ++a) {
				fprintf(purgelist, "m=%ld\n", msglist[a]);
				++messages_purged;
			}
		}
	}

	/* If the room is set to expire by age... */
	if (epbuf.expire_mode == EXPIRE_AGE) {
		for (a=0; a<num_msgs; ++a) {
			delnum = msglist[a];

			msg = CtdlFetchMessage(delnum, 0); /* dont need body */
			if (msg != NULL) {
				xtime = atol(msg->cm_fields['T']);
				CtdlFreeMessage(msg);
			} else {
				xtime = 0L;
			}

			if ((xtime > 0L)
			   && (now - xtime > (time_t)(epbuf.expire_value * 86400L))) {
				fprintf(purgelist, "m=%ld\n", delnum);
				++messages_purged;
			}
		}
	}

	if (msglist != NULL) free(msglist);
}


/*
 * Second phase of message purge -- read list of msgs from temp file and
 * delete them.
 */
void DoPurgeMessages(FILE *purgelist) {
	char roomname[ROOMNAMELEN];
	long msgnum;
	char buf[SIZ];

	rewind(purgelist);
	strcpy(roomname, "nonexistent room ___ ___");
	while (fgets(buf, sizeof buf, purgelist) != NULL) {
		buf[strlen(buf)-1]=0;
		if (!strncasecmp(buf, "r=", 2)) {
			strcpy(roomname, &buf[2]);
		}
		if (!strncasecmp(buf, "m=", 2)) {
			msgnum = atol(&buf[2]);
			if (msgnum > 0L) {
				CtdlDeleteMessages(roomname, &msgnum, 1, "");
			}
		}
	}
}


void PurgeMessages(void) {
	FILE *purgelist;

	CtdlLogPrintf(CTDL_DEBUG, "PurgeMessages() called\n");
	messages_purged = 0;

	purgelist = tmpfile();
	if (purgelist == NULL) {
		CtdlLogPrintf(CTDL_CRIT, "Can't create purgelist temp file: %s\n",
			strerror(errno));
		return;
	}

	ForEachRoom(GatherPurgeMessages, (void *)purgelist );
	DoPurgeMessages(purgelist);
	fclose(purgelist);
}


void AddValidUser(struct ctdluser *usbuf, void *data) {
	struct ValidUser *vuptr;

	vuptr = (struct ValidUser *)malloc(sizeof(struct ValidUser));
	vuptr->next = ValidUserList;
	vuptr->vu_usernum = usbuf->usernum;
	ValidUserList = vuptr;
}

void AddValidRoom(struct ctdlroom *qrbuf, void *data) {
	struct ValidRoom *vrptr;

	vrptr = (struct ValidRoom *)malloc(sizeof(struct ValidRoom));
	vrptr->next = ValidRoomList;
	vrptr->vr_roomnum = qrbuf->QRnumber;
	vrptr->vr_roomgen = qrbuf->QRgen;
	ValidRoomList = vrptr;
}

void DoPurgeRooms(struct ctdlroom *qrbuf, void *data) {
	time_t age, purge_secs;
	struct PurgeList *pptr;
	struct ValidUser *vuptr;
	int do_purge = 0;

	/* For mailbox rooms, there's only one purging rule: if the user who
	 * owns the room still exists, we keep the room; otherwise, we purge
	 * it.  Bypass any other rules.
	 */
	if (qrbuf->QRflags & QR_MAILBOX) {
		/* if user not found, do_purge will be 1 */
		do_purge = 1;
		for (vuptr=ValidUserList; vuptr!=NULL; vuptr=vuptr->next) {
			if (vuptr->vu_usernum == atol(qrbuf->QRname)) {
				do_purge = 0;
			}
		}
	}
	else {
		/* Any of these attributes render a room non-purgable */
		if (qrbuf->QRflags & QR_PERMANENT) return;
		if (qrbuf->QRflags & QR_DIRECTORY) return;
		if (qrbuf->QRflags & QR_NETWORK) return;
		if (qrbuf->QRflags2 & QR2_SYSTEM) return;
		if (!strcasecmp(qrbuf->QRname, SYSCONFIGROOM)) return;
		if (is_noneditable(qrbuf)) return;

		/* If we don't know the modification date, be safe and don't purge */
		if (qrbuf->QRmtime <= (time_t)0) return;

		/* If no room purge time is set, be safe and don't purge */
		if (config.c_roompurge < 0) return;

		/* Otherwise, check the date of last modification */
		age = time(NULL) - (qrbuf->QRmtime);
		purge_secs = (time_t)config.c_roompurge * (time_t)86400;
		if (purge_secs <= (time_t)0) return;
		CtdlLogPrintf(CTDL_DEBUG, "<%s> is <%ld> seconds old\n", qrbuf->QRname, (long)age);
		if (age > purge_secs) do_purge = 1;
	} /* !QR_MAILBOX */

	if (do_purge) {
		pptr = (struct PurgeList *) malloc(sizeof(struct PurgeList));
		pptr->next = RoomPurgeList;
		strcpy(pptr->name, qrbuf->QRname);
		RoomPurgeList = pptr;
	}

}



int PurgeRooms(void) {
	struct PurgeList *pptr;
	int num_rooms_purged = 0;
	struct ctdlroom qrbuf;
	struct ValidUser *vuptr;
	char *transcript = NULL;

	CtdlLogPrintf(CTDL_DEBUG, "PurgeRooms() called\n");


	/* Load up a table full of valid user numbers so we can delete
	 * user-owned rooms for users who no longer exist */
	ForEachUser(AddValidUser, NULL);

	/* Then cycle through the room file */
	ForEachRoom(DoPurgeRooms, NULL);

	/* Free the valid user list */
	while (ValidUserList != NULL) {
		vuptr = ValidUserList->next;
		free(ValidUserList);
		ValidUserList = vuptr;
	}


	transcript = malloc(SIZ);
	strcpy(transcript, "The following rooms have been auto-purged:\n");

	while (RoomPurgeList != NULL) {
		if (getroom(&qrbuf, RoomPurgeList->name) == 0) {
			transcript=realloc(transcript, strlen(transcript)+SIZ);
			snprintf(&transcript[strlen(transcript)], SIZ, " %s\n",
				qrbuf.QRname);
			delete_room(&qrbuf);
		}
		pptr = RoomPurgeList->next;
		free(RoomPurgeList);
		RoomPurgeList = pptr;
		++num_rooms_purged;
	}

	if (num_rooms_purged > 0) aide_message(transcript, "Room Autopurger Message");
	free(transcript);

	CtdlLogPrintf(CTDL_DEBUG, "Purged %d rooms.\n", num_rooms_purged);
	return(num_rooms_purged);
}


/*
 * Back end function to check user accounts for associated Unix accounts
 * which no longer exist.  (Only relevant for host auth mode.)
 */
void do_uid_user_purge(struct ctdluser *us, void *data) {
	struct PurgeList *pptr;

	if ((us->uid != (-1)) && (us->uid != CTDLUID)) {
		if (getpwuid(us->uid) == NULL) {
			pptr = (struct PurgeList *)
				malloc(sizeof(struct PurgeList));
			pptr->next = UserPurgeList;
			strcpy(pptr->name, us->fullname);
			UserPurgeList = pptr;
		}
	}
	else {
		++users_not_purged;
	}
}




/*
 * Back end function to check user accounts for expiration.
 */
void do_user_purge(struct ctdluser *us, void *data) {
	int purge;
	time_t now;
	time_t purge_time;
	struct PurgeList *pptr;

	/* Set purge time; if the user overrides the system default, use it */
	if (us->USuserpurge > 0) {
		purge_time = ((time_t)us->USuserpurge) * 86400L;
	}
	else {
		purge_time = ((time_t)config.c_userpurge) * 86400L;
	}

	/* The default rule is to not purge. */
	purge = 0;
	
	/* don't attempt to purge system users. */
	if (!strncmp(us->fullname, "SYS_", 4))
		goto skip_all_this;

	/* If the user hasn't called in two months and expiring of accounts is turned on, his/her account
	 * has expired, so purge the record.
	 */
	if (config.c_userpurge > 0)
	{
		now = time(NULL);
		if ((now - us->lastcall) > purge_time) purge = 1;
	}

	/* If the record is marked as permanent, don't purge it.
	 */
	if (us->flags & US_PERM) purge = 0;

	/* If the user is an Aide, don't purge him/her/it.
	 */
	if (us->axlevel == 6) purge = 0;

	/* If the access level is 0, the record should already have been
	 * deleted, but maybe the user was logged in at the time or something.
	 * Delete the record now.
	 */
	if (us->axlevel == 0) purge = 1;

	/* If the user set his/her password to 'deleteme', he/she
	 * wishes to be deleted, so purge the record.
	 * Moved this lower down so that aides and permanent users get purged if they ask to be.
	 */
	if (!strcasecmp(us->password, "deleteme")) purge = 1;
	
	/* 0 calls is impossible.  If there are 0 calls, it must
	 * be a corrupted record, so purge it.
	 * Actually it is possible if an Aide created the user so now we check for less than 0 (DRW)
	 */
	if (us->timescalled < 0) purge = 1;

	/* any negative user number, is
	 * also impossible.
	 */
	if (us->usernum < 0L) purge = 1;
	
	/** Don't purge user 0. That user is there for the system */
	if (us->usernum == 0L)
	{
		/* FIXME: Temporary log message. Until we do unauth access with user 0 we should
		 * try to get rid of all user 0 occurences. Many will be remnants from old code so
		 * we will need to try and purge them from users data bases.Some will not have names but
		 * those with names should be purged.
		 */
		CtdlLogPrintf(CTDL_DEBUG, "Auto purger found a user 0 with name \"%s\"\n", us->fullname);
		// purge = 0;
	}
	
	/* If the user has no full name entry then we can't purge them
	 * since the actual purge can't find them.
	 * This shouldn't happen but does somehow.
	 */
	if (IsEmptyStr(us->fullname))
	{
		purge = 0;
		
		if (us->usernum > 0L)
		{
			purge=0;
			if (users_corrupt_msg == NULL)
			{
				users_corrupt_msg = malloc(SIZ);
				strcpy(users_corrupt_msg, "The auto-purger found the following user numbers with no name.\n"
				"The system has no way to purge user with no name and should not be able to\n"
				"create them either.\n"
				"This indicates corruption of the user DB or possibly a bug.\n"
				"It may be a good idea to restore your DB from a backup.\n");
			}
		
			users_corrupt_msg=realloc(users_corrupt_msg, strlen(users_corrupt_msg)+30);
			snprintf(&users_corrupt_msg[strlen(users_corrupt_msg)], 29, " %ld\n", us->usernum);
		}
	}

skip_all_this:
		
	if (purge == 1) {
		pptr = (struct PurgeList *) malloc(sizeof(struct PurgeList));
		pptr->next = UserPurgeList;
		strcpy(pptr->name, us->fullname);
		UserPurgeList = pptr;
	}
	else {
		++users_not_purged;
	}

}



int PurgeUsers(void) {
	struct PurgeList *pptr;
	int num_users_purged = 0;
	char *transcript = NULL;

	CtdlLogPrintf(CTDL_DEBUG, "PurgeUsers() called\n");
	users_not_purged = 0;

	switch(config.c_auth_mode) {
		case AUTHMODE_NATIVE:
			ForEachUser(do_user_purge, NULL);
			break;
		case AUTHMODE_HOST:
			ForEachUser(do_uid_user_purge, NULL);
			break;
		default:
			CtdlLogPrintf(CTDL_DEBUG, "Unknown authentication mode!\n");
			break;
	}

	transcript = malloc(SIZ);

	if (users_not_purged == 0) {
		strcpy(transcript, "The auto-purger was told to purge every user.  It is\n"
				"refusing to do this because it usually indicates a problem\n"
				"such as an inability to communicate with a name service.\n"
		);
		while (UserPurgeList != NULL) {
			pptr = UserPurgeList->next;
			free(UserPurgeList);
			UserPurgeList = pptr;
			++num_users_purged;
		}
	}

	else {
		strcpy(transcript, "The following users have been auto-purged:\n");
		while (UserPurgeList != NULL) {
			transcript=realloc(transcript, strlen(transcript)+SIZ);
			snprintf(&transcript[strlen(transcript)], SIZ, " %s\n",
				UserPurgeList->name);
			purge_user(UserPurgeList->name);
			pptr = UserPurgeList->next;
			free(UserPurgeList);
			UserPurgeList = pptr;
			++num_users_purged;
		}
	}

	if (num_users_purged > 0) aide_message(transcript, "User Purge Message");
	free(transcript);

	if(users_corrupt_msg)
	{
		aide_message(users_corrupt_msg, "User Corruption Message");
		free (users_corrupt_msg);
		users_corrupt_msg = NULL;
	}
	
	if(users_zero_msg)
	{
		aide_message(users_zero_msg, "User Zero Message");
		free (users_zero_msg);
		users_zero_msg = NULL;
	}
		
	CtdlLogPrintf(CTDL_DEBUG, "Purged %d users.\n", num_users_purged);
	return(num_users_purged);
}


/*
 * Purge visits
 *
 * This is a really cumbersome "garbage collection" function.  We have to
 * delete visits which refer to rooms and/or users which no longer exist.  In
 * order to prevent endless traversals of the room and user files, we first
 * build linked lists of rooms and users which _do_ exist on the system, then
 * traverse the visit file, checking each record against those two lists and
 * purging the ones that do not have a match on _both_ lists.  (Remember, if
 * either the room or user being referred to is no longer on the system, the
 * record is completely useless.)
 */
int PurgeVisits(void) {
	struct cdbdata *cdbvisit;
	struct visit vbuf;
	struct VPurgeList *VisitPurgeList = NULL;
	struct VPurgeList *vptr;
	int purged = 0;
	char IndexBuf[32];
	int IndexLen;
	struct ValidRoom *vrptr;
	struct ValidUser *vuptr;
	int RoomIsValid, UserIsValid;

	/* First, load up a table full of valid room/gen combinations */
	ForEachRoom(AddValidRoom, NULL);

	/* Then load up a table full of valid user numbers */
	ForEachUser(AddValidUser, NULL);

	/* Now traverse through the visits, purging irrelevant records... */
	cdb_rewind(CDB_VISIT);
	while(cdbvisit = cdb_next_item(CDB_VISIT), cdbvisit != NULL) {
		memset(&vbuf, 0, sizeof(struct visit));
		memcpy(&vbuf, cdbvisit->ptr,
			( (cdbvisit->len > sizeof(struct visit)) ?
			sizeof(struct visit) : cdbvisit->len) );
		cdb_free(cdbvisit);

		RoomIsValid = 0;
		UserIsValid = 0;

		/* Check to see if the room exists */
		for (vrptr=ValidRoomList; vrptr!=NULL; vrptr=vrptr->next) {
			if ( (vrptr->vr_roomnum==vbuf.v_roomnum)
			     && (vrptr->vr_roomgen==vbuf.v_roomgen))
				RoomIsValid = 1;
		}

		/* Check to see if the user exists */
		for (vuptr=ValidUserList; vuptr!=NULL; vuptr=vuptr->next) {
			if (vuptr->vu_usernum == vbuf.v_usernum)
				UserIsValid = 1;
		}

		/* Put the record on the purge list if it's dead */
		if ((RoomIsValid==0) || (UserIsValid==0)) {
			vptr = (struct VPurgeList *)
				malloc(sizeof(struct VPurgeList));
			vptr->next = VisitPurgeList;
			vptr->vp_roomnum = vbuf.v_roomnum;
			vptr->vp_roomgen = vbuf.v_roomgen;
			vptr->vp_usernum = vbuf.v_usernum;
			VisitPurgeList = vptr;
		}

	}

	/* Free the valid room/gen combination list */
	while (ValidRoomList != NULL) {
		vrptr = ValidRoomList->next;
		free(ValidRoomList);
		ValidRoomList = vrptr;
	}

	/* Free the valid user list */
	while (ValidUserList != NULL) {
		vuptr = ValidUserList->next;
		free(ValidUserList);
		ValidUserList = vuptr;
	}

	/* Now delete every visit on the purged list */
	while (VisitPurgeList != NULL) {
		IndexLen = GenerateRelationshipIndex(IndexBuf,
				VisitPurgeList->vp_roomnum,
				VisitPurgeList->vp_roomgen,
				VisitPurgeList->vp_usernum);
		cdb_delete(CDB_VISIT, IndexBuf, IndexLen);
		vptr = VisitPurgeList->next;
		free(VisitPurgeList);
		VisitPurgeList = vptr;
		++purged;
	}

	return(purged);
}

/*
 * Purge the use table of old entries.
 *
 */
int PurgeUseTable(void) {
	int purged = 0;
	struct cdbdata *cdbut;
	struct UseTable ut;
	struct UPurgeList *ul = NULL;
	struct UPurgeList *uptr; 

	/* Phase 1: traverse through the table, discovering old records... */
	CtdlLogPrintf(CTDL_DEBUG, "Purge use table: phase 1\n");
	cdb_rewind(CDB_USETABLE);
	while(cdbut = cdb_next_item(CDB_USETABLE), cdbut != NULL) {

	/*
	 * TODODRW: change this to create a new function time_t cdb_get_timestamp( struct cdbdata *)
	 * this will release this file from the serv_network.h
	 * Maybe it could be a macro that extracts and casts the reult
	 */
                memcpy(&ut, cdbut->ptr,
                       ((cdbut->len > sizeof(struct UseTable)) ?
                        sizeof(struct UseTable) : cdbut->len));
                cdb_free(cdbut);

		if ( (time(NULL) - ut.ut_timestamp) > USETABLE_RETAIN ) {
			uptr = (struct UPurgeList *) malloc(sizeof(struct UPurgeList));
			if (uptr != NULL) {
				uptr->next = ul;
				safestrncpy(uptr->up_key, ut.ut_msgid, sizeof uptr->up_key);
				ul = uptr;
			}
			++purged;
		}

	}

	/* Phase 2: delete the records */
	CtdlLogPrintf(CTDL_DEBUG, "Purge use table: phase 2\n");
	while (ul != NULL) {
		cdb_delete(CDB_USETABLE, ul->up_key, strlen(ul->up_key));
		uptr = ul->next;
		free(ul);
		ul = uptr;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Purge use table: finished (purged %d records)\n", purged);
	return(purged);
}



/*
 * Purge the EUID Index of old records.
 *
 */
int PurgeEuidIndexTable(void) {
	int purged = 0;
	struct cdbdata *cdbei;
	struct EPurgeList *el = NULL;
	struct EPurgeList *eptr; 
	long msgnum;
	struct CtdlMessage *msg = NULL;

	/* Phase 1: traverse through the table, discovering old records... */
	CtdlLogPrintf(CTDL_DEBUG, "Purge EUID index: phase 1\n");
	cdb_rewind(CDB_EUIDINDEX);
	while(cdbei = cdb_next_item(CDB_EUIDINDEX), cdbei != NULL) {

		memcpy(&msgnum, cdbei->ptr, sizeof(long));

		msg = CtdlFetchMessage(msgnum, 0);
		if (msg != NULL) {
			CtdlFreeMessage(msg);	/* it still exists, so do nothing */
		}
		else {
			eptr = (struct EPurgeList *) malloc(sizeof(struct EPurgeList));
			if (eptr != NULL) {
				eptr->next = el;
				eptr->ep_keylen = cdbei->len - sizeof(long);
				eptr->ep_key = malloc(cdbei->len);
				memcpy(eptr->ep_key, &cdbei->ptr[sizeof(long)], eptr->ep_keylen);
				el = eptr;
			}
			++purged;
		}

                cdb_free(cdbei);

	}

	/* Phase 2: delete the records */
	CtdlLogPrintf(CTDL_DEBUG, "Purge euid index: phase 2\n");
	while (el != NULL) {
		cdb_delete(CDB_EUIDINDEX, el->ep_key, el->ep_keylen);
		free(el->ep_key);
		eptr = el->next;
		free(el);
		el = eptr;
	}

	CtdlLogPrintf(CTDL_DEBUG, "Purge euid index: finished (purged %d records)\n", purged);
	return(purged);
}



/*
 * Purge OpenID assocations for missing users (theoretically this will never delete anything)
 */
int PurgeStaleOpenIDassociations(void) {
	struct cdbdata *cdboi;
	struct ctdluser usbuf;
	HashList *keys = NULL;
	HashPos *HashPos;
	char *deleteme = NULL;
	long len;
	void *Value;
	char *Key;
	int num_deleted = 0;

	keys = NewHash(1, NULL);
	if (!keys) return(0);


	cdb_rewind(CDB_OPENID);
	while (cdboi = cdb_next_item(CDB_OPENID), cdboi != NULL) {
		if (cdboi->len > sizeof(long)) {
			long usernum;
			usernum = ((long)*(cdboi->ptr));
			if (getuserbynumber(&usbuf, usernum) != 0) {
				deleteme = strdup(cdboi->ptr + sizeof(long)),
				Put(keys, deleteme, strlen(deleteme), deleteme, generic_free_handler);
			}
		}
		cdb_free(cdboi);
	}

	/* Go through the hash list, deleting keys we stored in it */

	HashPos = GetNewHashPos();
	while (GetNextHashPos(keys, HashPos, &len, &Key, &Value)!=0)
	{
		CtdlLogPrintf(CTDL_DEBUG, "Deleting associated OpenID <%s>\n", Value);
		cdb_delete(CDB_OPENID, Value, strlen(Value));
		/* note: don't free(Value) -- deleting the hash list will handle this for us */
		++num_deleted;
	}
	DeleteHashPos(&HashPos);
	DeleteHash(&keys);
	return num_deleted;
}





void *purge_databases(void *args)
{
        int retval;
        static time_t last_purge = 0;
        time_t now;
        struct tm tm;
	struct CitContext purgerCC;

	CtdlLogPrintf(CTDL_DEBUG, "Auto-purger_thread() initializing\n");

	CtdlFillSystemContext(&purgerCC, "purger");
	citthread_setspecific(MyConKey, (void *)&purgerCC );

        while (!CtdlThreadCheckStop()) {
                /* Do the auto-purge if the current hour equals the purge hour,
                 * but not if the operation has already been performed in the
                 * last twelve hours.  This is usually enough granularity.
                 */
                now = time(NULL);
                localtime_r(&now, &tm);
                if (
			((tm.tm_hour != config.c_purge_hour) || ((now - last_purge) < 43200))
			&& (force_purge_now == 0)
		) {
                        CtdlThreadSleep(60);
                        continue;
                }


                CtdlLogPrintf(CTDL_INFO, "Auto-purger: starting.\n");

		if (!CtdlThreadCheckStop())
		{
			retval = PurgeUsers();
                	CtdlLogPrintf(CTDL_NOTICE, "Purged %d users.\n", retval);
		}
		
		if (!CtdlThreadCheckStop())
		{
                	PurgeMessages();
                	CtdlLogPrintf(CTDL_NOTICE, "Expired %d messages.\n", messages_purged);
		}

		if (!CtdlThreadCheckStop())
		{
                	retval = PurgeRooms();
                	CtdlLogPrintf(CTDL_NOTICE, "Expired %d rooms.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
                	retval = PurgeVisits();
                	CtdlLogPrintf(CTDL_NOTICE, "Purged %d visits.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
			retval = PurgeUseTable();
                	CtdlLogPrintf(CTDL_NOTICE, "Purged %d entries from the use table.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
                	retval = PurgeEuidIndexTable();
                	CtdlLogPrintf(CTDL_NOTICE, "Purged %d entries from the EUID index.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
			retval = PurgeStaleOpenIDassociations();
                	CtdlLogPrintf(CTDL_NOTICE, "Purged %d stale OpenID associations.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
                	retval = TDAP_ProcessAdjRefCountQueue();
                	CtdlLogPrintf(CTDL_NOTICE, "Processed %d message reference count adjustments.\n", retval);
		}

		if (!CtdlThreadCheckStop())
		{
                	CtdlLogPrintf(CTDL_INFO, "Auto-purger: finished.\n");
	                last_purge = now;	/* So we don't do it again soon */
			force_purge_now = 0;
		}
		else
                	CtdlLogPrintf(CTDL_INFO, "Auto-purger: STOPPED.\n");

        }
        return NULL;
}
/*****************************************************************************/


/* The FSCK command has been removed because people were misusing it */

#if 0

void do_fsck_msg(long msgnum, void *userdata) {
	struct ctdlroomref *ptr;

	ptr = (struct ctdlroomref *)malloc(sizeof(struct ctdlroomref));
	ptr->next = rr;
	ptr->msgnum = msgnum;
	rr = ptr;
}

void do_fsck_room(struct ctdlroom *qrbuf, void *data)
{
	getroom(&CC->room, qrbuf->QRname);
	CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL, do_fsck_msg, NULL);
}

/*
 * Check message reference counts
 */
void cmd_fsck(char *argbuf) {
	long msgnum;
	struct cdbdata *cdbmsg;
	struct MetaData smi;
	struct ctdlroomref *ptr;
	int realcount;

	if (CtdlAccessCheck(ac_aide)) return;

	/* Lame way of checking whether anyone else is doing this now */
	if (rr != NULL) {
		cprintf("%d Another FSCK is already running.\n", ERROR + RESOURCE_BUSY);
		return;
	}

	cprintf("%d Checking message reference counts\n", LISTING_FOLLOWS);

	cprintf("\nThis could take a while.  Please be patient!\n\n");
	cprintf("Gathering pointers...\n");
	ForEachRoom(do_fsck_room, NULL);

	get_control();
	cprintf("Checking message base...\n");
	for (msgnum = 0L; msgnum <= CitControl.MMhighest; ++msgnum) {

		cdbmsg = cdb_fetch(CDB_MSGMAIN, &msgnum, sizeof(long));
		if (cdbmsg != NULL) {
			cdb_free(cdbmsg);
			cprintf("Message %7ld    ", msgnum);

			GetMetaData(&smi, msgnum);
			cprintf("refcount=%-2d   ", smi.meta_refcount);

			realcount = 0;
			for (ptr = rr; ptr != NULL; ptr = ptr->next) {
				if (ptr->msgnum == msgnum) ++realcount;
			}
			cprintf("realcount=%-2d\n", realcount);

			if ( (smi.meta_refcount != realcount)
			   || (realcount == 0) ) {
				AdjRefCount(msgnum, (smi.meta_refcount - realcount));
			}

		}

	}

	cprintf("Freeing memory...\n");
	while (rr != NULL) {
		ptr = rr->next;
		free(rr);
		rr = ptr;
	}

	cprintf("Done!\n");
	cprintf("000\n");

}

#endif	/* end of commented-out fsck cmd */

/*
 * Manually initiate a run of The Dreaded Auto-Purger (tm)
 */
void cmd_tdap(char *argbuf) {
	if (CtdlAccessCheck(ac_aide)) return;
	force_purge_now = 1;
	cprintf("%d Manually initiating a purger run now.\n", CIT_OK);
}


/*****************************************************************************/

CTDL_MODULE_INIT(expire)
{
	if (!threading)
	{
		/* CtdlRegisterProtoHook(cmd_fsck, "FSCK", "Check message ref counts"); */
		CtdlRegisterProtoHook(cmd_tdap, "TDAP", "Manually initiate auto-purger");
	}
	else
		CtdlThreadCreate("Auto Purger", CTDLTHREAD_BIGSTACK, purge_databases, NULL);
	/* return our Subversion id for the Log */
	return "$Id$";
}
