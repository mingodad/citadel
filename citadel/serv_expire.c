/*
 * serv_expire.c
 *
 * This module handles the expiry of old messages and the purging of old users.
 *
 */
/* $Id$ */

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
#include <pthread.h>
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


struct oldvisit {
	char v_roomname[ROOMNAMELEN];
	long v_generation;
	long v_lastseen;
	unsigned int v_flags;
	};




struct PurgedUser {
	struct PurgedUser *next;
	char name[26];
	};

struct PurgedUser *plist = NULL;

extern struct CitContext *ContextList;

#define MODULE_NAME 	"Expire old messages, users, rooms"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	1

static struct DLModule_Info info = {
	MODULE_NAME,
	MODULE_AUTHOR,
	MODULE_EMAIL,
	MAJOR_VERSION,
	MINOR_VERSION
	};

void DoPurgeMessages(struct quickroom *qrbuf) {
	struct ExpirePolicy epbuf;
	long delnum;
	time_t xtime, now;
	char msgid[64];
	int a;

	time(&now);
	GetExpirePolicy(&epbuf, qrbuf);
	
	/*
	lprintf(9, "ExpirePolicy for <%s> is <%d> <%d>\n",
		qrbuf->QRname, epbuf.expire_mode, epbuf.expire_value);
	*/

	/* If the room is set to never expire messages ... do nothing */
	if (epbuf.expire_mode == EXPIRE_NEXTLEVEL) return;
	if (epbuf.expire_mode == EXPIRE_MANUAL) return;

	begin_critical_section(S_QUICKROOM);
	get_msglist(qrbuf);
	
	/* Nothing to do if there aren't any messages */
	if (CC->num_msgs == 0) {
		end_critical_section(S_QUICKROOM);
		return;
		}

	/* If the room is set to expire by count, do that */
	if (epbuf.expire_mode == EXPIRE_NUMMSGS) {
		while (CC->num_msgs > epbuf.expire_value) {
			delnum = MessageFromList(0);
			lprintf(5, "Expiring message %ld\n", delnum);
			cdb_delete(CDB_MSGMAIN, &delnum, sizeof(long));
			memcpy(&CC->msglist[0], &CC->msglist[1],
				(sizeof(long)*(CC->num_msgs - 1)));
			CC->num_msgs = CC->num_msgs - 1;
			}
		}

	/* If the room is set to expire by age... */
	if (epbuf.expire_mode == EXPIRE_AGE) {
		for (a=0; a<(CC->num_msgs); ++a) {
			delnum = MessageFromList(a);
			sprintf(msgid, "%ld", delnum);
			xtime = output_message(msgid, MT_DATE, 0, 0);

			if ((xtime > 0L)
			   && (now - xtime > (time_t)(epbuf.expire_value * 86400L))) {
				cdb_delete(CDB_MSGMAIN, &delnum, sizeof(long));
				SetMessageInList(a, 0L);
				lprintf(5, "Expiring message %ld\n", delnum);
				}
			}
		}
	CC->num_msgs = sort_msglist(CC->msglist, CC->num_msgs);
	put_msglist(qrbuf);
	end_critical_section(S_QUICKROOM);
	}

void PurgeMessages(void) {
	lprintf(5, "PurgeMessages() called\n");
	ForEachRoom(DoPurgeMessages);
	}


void do_user_purge(struct usersupp *us) {
	int purge;
	time_t now;
	time_t purge_time;
	struct PurgedUser *pptr;

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
		pptr = (struct PurgedUser *) malloc(sizeof(struct PurgedUser));
		pptr->next = plist;
		strcpy(pptr->name, us->fullname);
		plist = pptr;
		}

	}



int PurgeUsers(void) {
	struct PurgedUser *pptr;
	int num_users_purged = 0;

	lprintf(5, "PurgeUsers() called\n");
	if (config.c_userpurge > 0) {
		ForEachUser(do_user_purge);
		}

	while (plist != NULL) {
		purge_user(plist->name);
		pptr = plist->next;
		free(plist);
		plist = pptr;
		++num_users_purged;
		}

	lprintf(5, "Purged %d users.\n", num_users_purged);
	return(num_users_purged);
	}


void cmd_expi(char *argbuf) {
	char cmd[256];
	int retval;


	if ((!(CC->logged_in))&&(!(CC->internal_pgm))) {
		cprintf("%d Not logged in.\n",ERROR+NOT_LOGGED_IN);
		return;
		}

	if ((!is_room_aide()) && (!(CC->internal_pgm)) ) {
		cprintf("%d Higher access required.\n",
			ERROR+HIGHER_ACCESS_REQUIRED);
		return;
		}

	extract(cmd, argbuf, 0);
	if (!strcasecmp(cmd, "users")) {
		retval = PurgeUsers();
		cprintf("%d Purged %d users.\n", OK, retval);
		return;
		}
	else if (!strcasecmp(cmd, "messages")) {
		PurgeMessages();
		cprintf("%d Finished purging messages.\n", OK);
		return;
		}
	else {
		cprintf("%d Invalid command.\n", ERROR+ILLEGAL_VALUE);
		return;
		}
	}

/****************************************************************************
                  BEGIN TEMPORARY STUFF 
 */

void reindex_this_msglist(struct quickroom *qrbuf) {
	struct cdbdata *c;
	char dbkey[256];
	int a;

	getroom(qrbuf, qrbuf->QRname);

	sprintf(dbkey, "%s%ld", qrbuf->QRname, qrbuf->QRgen);
	for (a=0; a<strlen(dbkey); ++a) dbkey[a]=tolower(dbkey[a]);

	c = cdb_fetch(CDB_MSGLISTS, dbkey, strlen(dbkey));
	if (c != NULL) {
		cprintf("Converting msglist for <%s><%ld>\n",
			qrbuf->QRname, qrbuf->QRnumber);
		cdb_delete(CDB_MSGLISTS, dbkey, strlen(dbkey));
		cdb_store(CDB_MSGLISTS,
			&qrbuf->QRnumber, sizeof(long),
			c->ptr, c->len);
		cdb_free(c);
		}
	else {
		cprintf("NOT converting msglist for <%s><%ld>\n",
			qrbuf->QRname, qrbuf->QRnumber);
		}

	}

void reindex_msglists(void) {
	ForEachRoom(reindex_this_msglist);
	}

void number_thisroom(struct quickroom *qrbuf) {
	getroom(qrbuf, qrbuf->QRname);
	cprintf("%s\n", qrbuf->QRname);
	if (qrbuf->QRnumber == 0L) {
		qrbuf->QRnumber = get_new_room_number();
		cprintf("    * assigned room number %ld\n", qrbuf->QRnumber);
		}
	putroom(qrbuf, qrbuf->QRname);
	}

void rewrite_a_visit(struct usersupp *rel_user) {
	struct cdbdata *cdbvisit;
	struct oldvisit *visits;
	struct quickroom qrbuf;
	int num_visits;
	int a;
	struct visit vbuf;

	cdbvisit = cdb_fetch(CDB_VISIT, &rel_user->usernum, sizeof(long));
	if (cdbvisit != NULL) {
		num_visits = cdbvisit->len / sizeof(struct oldvisit);
		visits = (struct oldvisit *)
			malloc(num_visits * sizeof(struct oldvisit));
		memcpy(visits, cdbvisit->ptr,
			(num_visits * sizeof(struct oldvisit)));
		cdb_free(cdbvisit);
		cdb_delete(CDB_VISIT, &rel_user->usernum, sizeof(long));
		}
	else {
		num_visits = 0;
		visits = NULL;
		}

	if (num_visits > 0) {
		for (a=0; a<num_visits; ++a) {
			getroom(&qrbuf, visits[a].v_roomname);
			vbuf.v_flags = visits[a].v_flags;
			vbuf.v_lastseen = visits[a].v_lastseen;
			CtdlSetRelationship(&vbuf,
				rel_user,
				&qrbuf);
			cprintf("SetR: <%ld><%ld><%ld> <%d> <%ld>\n",
				vbuf.v_roomnum,
				vbuf.v_roomgen,
				vbuf.v_usernum,
				vbuf.v_flags,
				vbuf.v_lastseen);
			}
		free(visits);
		}
	}

void rewrite_visits(void) {
	ForEachUser(rewrite_a_visit);
	}

void number_rooms(void) {
	ForEachRoom(number_thisroom);
	}

void cmd_aaaa(char *argbuf) {
	cprintf("%d Here goes...\n", LISTING_FOLLOWS);
	number_rooms();
	reindex_msglists();
	rewrite_visits();
	cprintf("000\n");
	}



/*                  END TEMPORARY STUFF
 ****************************************************************************/

struct DLModule_Info *Dynamic_Module_Init(void)
{
   CtdlRegisterProtoHook(cmd_aaaa, "AAAA", "convert");
   CtdlRegisterProtoHook(cmd_expi, "EXPI", "Expire old system objects");
   return &info;
}
