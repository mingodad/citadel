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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "user_ops.h"
#include "control.h"
#include "serv_network.h"
#include "tools.h"


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

struct ctdlroomref *rr = NULL;

extern struct CitContext *ContextList;


/*
 * First phase of message purge -- gather the locations of messages which
 * qualify for purging and write them to a temp file.
 */
void GatherPurgeMessages(struct ctdlroom *qrbuf, void *data) {
	struct ExpirePolicy epbuf;
	long delnum;
	time_t xtime, now;
	struct CtdlMessage *msg;
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
				CtdlDeleteMessages(roomname, &msgnum, 1, "", 0);
			}
		}
	}
}


void PurgeMessages(void) {
	FILE *purgelist;

	lprintf(CTDL_DEBUG, "PurgeMessages() called\n");
	messages_purged = 0;

	purgelist = tmpfile();
	if (purgelist == NULL) {
		lprintf(CTDL_CRIT, "Can't create purgelist temp file: %s\n",
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
		lprintf(CTDL_DEBUG, "<%s> is <%ld> seconds old\n", qrbuf->QRname, (long)age);
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

	lprintf(CTDL_DEBUG, "PurgeRooms() called\n");


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

	lprintf(CTDL_DEBUG, "Purged %d rooms.\n", num_rooms_purged);
	return(num_rooms_purged);
}


/*
 * Back end function to check user accounts for associated Unix accounts
 * which no longer exist.
 */
void do_uid_user_purge(struct ctdluser *us, void *data) {
#ifdef ENABLE_AUTOLOGIN
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

#endif /* ENABLE_AUTOLOGIN */
}



/*
 * Back end function to check user accounts for expiration.
 */
void do_user_purge(struct ctdluser *us, void *data) {
	int purge;
	time_t now;
	time_t purge_time;
	struct PurgeList *pptr;

	/* stupid recovery routine to re-create missing mailboxen.
	 * don't enable this.
	struct ctdlroom qrbuf;
	char mailboxname[ROOMNAMELEN];
	MailboxName(mailboxname, us, MAILROOM);
	create_room(mailboxname, 4, "", 0, 1, 1, VIEW_BBS);
	if (getroom(&qrbuf, mailboxname) != 0) return;
	lprintf(CTDL_DEBUG, "Got %s\n", qrbuf.QRname);
	 */


	/* Set purge time; if the user overrides the system default, use it */
	if (us->USuserpurge > 0) {
		purge_time = ((time_t)us->USuserpurge) * 86400L;
	}
	else {
		purge_time = ((time_t)config.c_userpurge) * 86400L;
	}

	/* The default rule is to not purge. */
	purge = 0;

	/* If the user hasn't called in two months, his/her account
	 * has expired, so purge the record.
	 */
	now = time(NULL);
	if ((now - us->lastcall) > purge_time) purge = 1;

	/* If the user set his/her password to 'deleteme', he/she
	 * wishes to be deleted, so purge the record.
	 */
	if (!strcasecmp(us->password, "deleteme")) purge = 1;

	/* If the record is marked as permanent, don't purge it.
	 */
	if (us->flags & US_PERM) purge = 0;

	/* If the access level is 0, the record should already have been
	 * deleted, but maybe the user was logged in at the time or something.
	 * Delete the record now.
	 */
	if (us->axlevel == 0) purge = 1;

	/* 0 calls is impossible.  If there are 0 calls, it must
	 * be a corrupted record, so purge it.
	 */
	if (us->timescalled == 0) purge = 1;

	/* User number 0, as well as any negative user number, is
	 * also impossible.
	 */
	if (us->usernum < 1L) purge = 1;

	if (purge == 1) {
		pptr = (struct PurgeList *) malloc(sizeof(struct PurgeList));
		pptr->next = UserPurgeList;
		strcpy(pptr->name, us->fullname);
		UserPurgeList = pptr;
	}

}



int PurgeUsers(void) {
	struct PurgeList *pptr;
	int num_users_purged = 0;
	char *transcript = NULL;

	lprintf(CTDL_DEBUG, "PurgeUsers() called\n");
#ifdef ENABLE_AUTOLOGIN
	ForEachUser(do_uid_user_purge, NULL);
#else
	if (config.c_userpurge > 0) {
		ForEachUser(do_user_purge, NULL);
	}
#endif

	transcript = malloc(SIZ);
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

	if (num_users_purged > 0) aide_message(transcript,"User Purge Message");
	free(transcript);

	lprintf(CTDL_DEBUG, "Purged %d users.\n", num_users_purged);
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
	lprintf(CTDL_DEBUG, "Purge use table: phase 1\n");
	cdb_rewind(CDB_USETABLE);
	while(cdbut = cdb_next_item(CDB_USETABLE), cdbut != NULL) {

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
	lprintf(CTDL_DEBUG, "Purge use table: phase 2\n");
	while (ul != NULL) {
		cdb_delete(CDB_USETABLE, ul->up_key, strlen(ul->up_key));
		uptr = ul->next;
		free(ul);
		ul = uptr;
	}

	lprintf(CTDL_DEBUG, "Purge use table: finished (purged %d records)\n", purged);
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
	struct CtdlMessage *msg;

	/* Phase 1: traverse through the table, discovering old records... */
	lprintf(CTDL_DEBUG, "Purge EUID index: phase 1\n");
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
	lprintf(CTDL_DEBUG, "Purge euid index: phase 2\n");
	while (el != NULL) {
		cdb_delete(CDB_EUIDINDEX, el->ep_key, el->ep_keylen);
		free(el->ep_key);
		eptr = el->next;
		free(el);
		el = eptr;
	}

	lprintf(CTDL_DEBUG, "Purge euid index: finished (purged %d records)\n", purged);
	return(purged);
}


void purge_databases(void) {
	int retval;
	static time_t last_purge = 0;
	time_t now;
	struct tm tm;

	/* Do the auto-purge if the current hour equals the purge hour,
	 * but not if the operation has already been performed in the
	 * last twelve hours.  This is usually enough granularity.
	 */
	now = time(NULL);
	localtime_r(&now, &tm);
	if (tm.tm_hour != config.c_purge_hour) return;
	if ((now - last_purge) < 43200) return;

	lprintf(CTDL_INFO, "Auto-purger: starting.\n");

	retval = PurgeUsers();
	lprintf(CTDL_NOTICE, "Purged %d users.\n", retval);

	PurgeMessages();
	lprintf(CTDL_NOTICE, "Expired %d messages.\n", messages_purged);

	retval = PurgeRooms();
	lprintf(CTDL_NOTICE, "Expired %d rooms.\n", retval);

	retval = PurgeVisits();
	lprintf(CTDL_NOTICE, "Purged %d visits.\n", retval);

	retval = PurgeUseTable();
	lprintf(CTDL_NOTICE, "Purged %d entries from the use table.\n", retval);

	retval = PurgeEuidIndexTable();
	lprintf(CTDL_NOTICE, "Purged %d entries from the EUID index.\n", retval);

	lprintf(CTDL_INFO, "Auto-purger: finished.\n");

	last_purge = now;	/* So we don't do it again soon */
}

/*****************************************************************************/


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
				smi.meta_refcount = realcount;
				PutMetaData(&smi);
				AdjRefCount(msgnum, 0); /* deletes if needed */
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




/*****************************************************************************/

char *serv_expire_init(void)
{
	CtdlRegisterSessionHook(purge_databases, EVT_TIMER);
	CtdlRegisterProtoHook(cmd_fsck, "FSCK", "Check message ref counts");
	return "$Id$";
}
