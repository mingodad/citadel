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
 * implemented in this way is because GDBM (and perhaps some other backends we
 * may hook into in the future) explicitly do _not_ support the deletion of
 * records from a file while the file is being traversed.  The delete operation
 * will succeed, but the traversal is not guaranteed to visit every object if
 * this is done.  Therefore we utilize the two-stage purge.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
#include <syslog.h>
#include <time.h>
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"
#include "room_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "user_ops.h"
#include "control.h"
#include "tools.h"


struct oldvisit {
	char v_roomname[ROOMNAMELEN];
	long v_generation;
	long v_lastseen;
	unsigned int v_flags;
};

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


struct roomref {
	struct roomref *next;
	long msgnum;
};


struct PurgeList *UserPurgeList = NULL;
struct PurgeList *RoomPurgeList = NULL;
struct ValidRoom *ValidRoomList = NULL;
struct ValidUser *ValidUserList = NULL;
int messages_purged;

struct roomref *rr = NULL;

extern struct CitContext *ContextList;

void DoPurgeMessages(struct quickroom *qrbuf, void *data) {
	struct ExpirePolicy epbuf;
	long delnum;
	time_t xtime, now;
	struct CtdlMessage *msg;
	int a;
        struct cdbdata *cdbfr;
	long *msglist = NULL;
	int num_msgs = 0;

	time(&now);
	GetExpirePolicy(&epbuf, qrbuf);
	
	/* If the room is set to never expire messages ... do nothing */
	if (epbuf.expire_mode == EXPIRE_NEXTLEVEL) return;
	if (epbuf.expire_mode == EXPIRE_MANUAL) return;

	begin_critical_section(S_QUICKROOM);
        cdbfr = cdb_fetch(CDB_MSGLISTS, &qrbuf->QRnumber, sizeof(long));

        if (cdbfr != NULL) {
        	msglist = mallok(cdbfr->len);
        	memcpy(msglist, cdbfr->ptr, cdbfr->len);
        	num_msgs = cdbfr->len / sizeof(long);
        	cdb_free(cdbfr);
	}
	
	/* Nothing to do if there aren't any messages */
	if (num_msgs == 0) {
		end_critical_section(S_QUICKROOM);
		return;
	}

	/* If the room is set to expire by count, do that */
	if (epbuf.expire_mode == EXPIRE_NUMMSGS) {
		while (num_msgs > epbuf.expire_value) {
			delnum = msglist[0];
			lprintf(5, "Expiring message %ld\n", delnum);
			AdjRefCount(delnum, -1); 
			memcpy(&msglist[0], &msglist[1],
				(sizeof(long)*(num_msgs - 1)));
			--num_msgs;
			++messages_purged;
		}
	}

	/* If the room is set to expire by age... */
	if (epbuf.expire_mode == EXPIRE_AGE) {
		for (a=0; a<num_msgs; ++a) {
			delnum = msglist[a];

			msg = CtdlFetchMessage(delnum);
			if (msg != NULL) {
				xtime = atol(msg->cm_fields['T']);
				CtdlFreeMessage(msg);
			} else {
				xtime = 0L;
			}

			if ((xtime > 0L)
			   && (now - xtime > (time_t)(epbuf.expire_value * 86400L))) {
				lprintf(5, "Expiring message %ld\n", delnum);
				AdjRefCount(delnum, -1); 
				msglist[a] = 0L;
				++messages_purged;
			}
		}
	}

	if (num_msgs > 0) {
		num_msgs = sort_msglist(msglist, num_msgs);
	}
	
	cdb_store(CDB_MSGLISTS, &qrbuf->QRnumber, sizeof(long),
		msglist, (num_msgs * sizeof(long)) );

	if (msglist != NULL) phree(msglist);

	end_critical_section(S_QUICKROOM);
}

void PurgeMessages(void) {
	lprintf(5, "PurgeMessages() called\n");
	messages_purged = 0;
	ForEachRoom(DoPurgeMessages, NULL);
}


void AddValidUser(struct usersupp *usbuf, void *data) {
	struct ValidUser *vuptr;

	vuptr = (struct ValidUser *)mallok(sizeof(struct ValidUser));
	vuptr->next = ValidUserList;
	vuptr->vu_usernum = usbuf->usernum;
	ValidUserList = vuptr;
}

void AddValidRoom(struct quickroom *qrbuf, void *data) {
	struct ValidRoom *vrptr;

	vrptr = (struct ValidRoom *)mallok(sizeof(struct ValidRoom));
	vrptr->next = ValidRoomList;
	vrptr->vr_roomnum = qrbuf->QRnumber;
	vrptr->vr_roomgen = qrbuf->QRgen;
	ValidRoomList = vrptr;
}

void DoPurgeRooms(struct quickroom *qrbuf, void *data) {
	time_t age, purge_secs;
	struct PurgeList *pptr;
	struct ValidUser *vuptr;
	int do_purge = 0;

	/* For mailbox rooms, there's only one purging rule: if the user who
	 * owns the room still exists, we keep the room; otherwise, we purge
	 * it.  Bypass any other rules.
	 */
	if (qrbuf->QRflags & QR_MAILBOX) {
		for (vuptr=ValidUserList; vuptr!=NULL; vuptr=vuptr->next) {
			if (vuptr->vu_usernum == atol(qrbuf->QRname)) {
				do_purge = 0;
				goto BYPASS;
			}
		}
		/* user not found */
		do_purge = 1;
		goto BYPASS;
	}

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
	lprintf(9, "<%s> is <%ld> seconds old\n", qrbuf->QRname, age);
	if (age > purge_secs) do_purge = 1;

BYPASS:	if (do_purge) {
		pptr = (struct PurgeList *) mallok(sizeof(struct PurgeList));
		pptr->next = RoomPurgeList;
		strcpy(pptr->name, qrbuf->QRname);
		RoomPurgeList = pptr;
	}

}
	


int PurgeRooms(void) {
	struct PurgeList *pptr;
	int num_rooms_purged = 0;
	struct quickroom qrbuf;
	struct ValidUser *vuptr;
	char *transcript = NULL;

	lprintf(5, "PurgeRooms() called\n");


	/* Load up a table full of valid user numbers so we can delete
	 * user-owned rooms for users who no longer exist */
	ForEachUser(AddValidUser, NULL);

	/* Then cycle through the room file */
	ForEachRoom(DoPurgeRooms, NULL);

	/* Free the valid user list */
	while (ValidUserList != NULL) {
		vuptr = ValidUserList->next;
		phree(ValidUserList);
		ValidUserList = vuptr;
	}


	transcript = mallok(256);
	strcpy(transcript, "The following rooms have been auto-purged:\n");

	while (RoomPurgeList != NULL) {
		if (getroom(&qrbuf, RoomPurgeList->name) == 0) {
			transcript=reallok(transcript, strlen(transcript)+256);
			sprintf(&transcript[strlen(transcript)], " %s\n",
				qrbuf.QRname);
			delete_room(&qrbuf);
		}
		pptr = RoomPurgeList->next;
		phree(RoomPurgeList);
		RoomPurgeList = pptr;
		++num_rooms_purged;
	}

	if (num_rooms_purged > 0) aide_message(transcript);
	phree(transcript);

	lprintf(5, "Purged %d rooms.\n", num_rooms_purged);
	return(num_rooms_purged);
}


void do_user_purge(struct usersupp *us, void *data) {
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

	if (purge == 1) {
		pptr = (struct PurgeList *) mallok(sizeof(struct PurgeList));
		pptr->next = UserPurgeList;
		strcpy(pptr->name, us->fullname);
		UserPurgeList = pptr;
	}

}



int PurgeUsers(void) {
	struct PurgeList *pptr;
	int num_users_purged = 0;
	char *transcript = NULL;

	lprintf(5, "PurgeUsers() called\n");
	if (config.c_userpurge > 0) {
		ForEachUser(do_user_purge, NULL);
	}

	transcript = mallok(256);
	strcpy(transcript, "The following users have been auto-purged:\n");

	while (UserPurgeList != NULL) {
		transcript=reallok(transcript, strlen(transcript)+256);
		sprintf(&transcript[strlen(transcript)], " %s\n",
			UserPurgeList->name);
		purge_user(UserPurgeList->name);
		pptr = UserPurgeList->next;
		phree(UserPurgeList);
		UserPurgeList = pptr;
		++num_users_purged;
	}

	if (num_users_purged > 0) aide_message(transcript);
	phree(transcript);

	lprintf(5, "Purged %d users.\n", num_users_purged);
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
				mallok(sizeof(struct VPurgeList));
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
		phree(ValidRoomList);
		ValidRoomList = vrptr;
	}

	/* Free the valid user list */
	while (ValidUserList != NULL) {
		vuptr = ValidUserList->next;
		phree(ValidUserList);
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
		phree(VisitPurgeList);
		VisitPurgeList = vptr;
		++purged;
	}
	
	return(purged);
}


void cmd_expi(char *argbuf) {
	char cmd[256];
	int retval;

	if (CtdlAccessCheck(ac_aide)) return;

	extract(cmd, argbuf, 0);
	if (!strcasecmp(cmd, "users")) {
		retval = PurgeUsers();
		cprintf("%d Purged %d users.\n", OK, retval);
		return;
	}
	else if (!strcasecmp(cmd, "messages")) {
		PurgeMessages();
		cprintf("%d Expired %d messages.\n", OK, messages_purged);
		return;
	}
	else if (!strcasecmp(cmd, "rooms")) {
		retval = PurgeRooms();
		cprintf("%d Expired %d rooms.\n", OK, retval);
		return;
	}
	else if (!strcasecmp(cmd, "visits")) {
		retval = PurgeVisits();
		cprintf("%d Purged %d visits.\n", OK, retval);
	}
	else if (!strcasecmp(cmd, "defrag")) {
		defrag_databases();
		cprintf("%d Defragmented the databases.\n", OK);
	}
	else {
		cprintf("%d Invalid command.\n", ERROR+ILLEGAL_VALUE);
		return;
	}
}

/*****************************************************************************/


void do_fsck_msg(long msgnum, void *userdata) {
	struct roomref *ptr;

	ptr = (struct roomref *)mallok(sizeof(struct roomref));
	ptr->next = rr;
	ptr->msgnum = msgnum;
	rr = ptr;
}

void do_fsck_room(struct quickroom *qrbuf, void *data)
{
	getroom(&CC->quickroom, qrbuf->QRname);
	CtdlForEachMessage(MSGS_ALL, 0L, (-127), NULL, NULL,
		do_fsck_msg, NULL);
}

/*
 * Check message reference counts
 */
void cmd_fsck(char *argbuf) {
	long msgnum;
	struct cdbdata *cdbmsg;
	struct SuppMsgInfo smi;
	struct roomref *ptr;
	int realcount;

	if (CtdlAccessCheck(ac_aide)) return;

	/* Lame way of checking whether anyone else is doing this now */
	if (rr != NULL) {
		cprintf("%d Another FSCK is already running.\n", ERROR);
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

			GetSuppMsgInfo(&smi, msgnum);
			cprintf("refcount=%-2d   ", smi.smi_refcount);

			realcount = 0;
			for (ptr = rr; ptr != NULL; ptr = ptr->next) {
				if (ptr->msgnum == msgnum) ++realcount;
			}
			cprintf("realcount=%-2d\n", realcount);

			if ( (smi.smi_refcount != realcount)
			   || (realcount == 0) ) {
				smi.smi_refcount = realcount;
				PutSuppMsgInfo(&smi);
				AdjRefCount(msgnum, 0); /* deletes if needed */
			}

		}

	}

	cprintf("Freeing memory...\n");
	while (rr != NULL) {
		ptr = rr->next;
		phree(rr);
		rr = ptr;
	}

	cprintf("Done!\n");
	cprintf("000\n");

}




/*****************************************************************************/

char *Dynamic_Module_Init(void)
{
	CtdlRegisterProtoHook(cmd_expi, "EXPI", "Expire old system objects");
	CtdlRegisterProtoHook(cmd_fsck, "FSCK", "Check message ref counts");
	return "$Id$";
}
