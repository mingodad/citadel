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
	
	/* lprintf(9, "ExpirePolicy for <%s> is <%d> <%d>\n",
		qrbuf->QRname, epbuf.expire_mode, epbuf.expire_value);
	 */

	/* If the room is set to never expire messages ... do nothing */
	if (epbuf.expire_mode == EXPIRE_NEXTLEVEL) return;
	if (epbuf.expire_mode == EXPIRE_MANUAL) return;

	get_msglist(qrbuf);
	
	/* Nothing to do if there aren't any messages */
	if (CC->num_msgs == 0) return;

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
	}

void PurgeMessages(void) {
	ForEachRoom(DoPurgeMessages);
	}


void do_user_purge(struct usersupp *us) {
	int purge;
	time_t now;
	time_t purge_time;
	struct PurgedUser *pptr;

	purge_time = ((time_t)config.c_userpurge) * 86400L;

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



void PurgeUsers(void) {
	struct PurgedUser *pptr;

	if (config.c_userpurge > 0) {
		ForEachUser(do_user_purge);
		}

	while (plist != NULL) {
		purge_user(plist->name);
		pptr = plist->next;
		free(plist);
		plist = pptr;
		}

	}


struct DLModule_Info *Dynamic_Module_Init(void)
{
   CtdlRegisterCleanupHook(PurgeMessages);
   CtdlRegisterCleanupHook(PurgeUsers);
   return &info;
}
