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

extern struct CitContext *ContextList;

#define MODULE_NAME 	"Dummy test module"
#define MODULE_AUTHOR	"Art Cancro"
#define MODULE_EMAIL	"ajc@uncnsrd.mt-kisco.ny.us"
#define MAJOR_VERSION	0
#define MINOR_VERSION	3

static struct DLModule_Info info = {
	MODULE_NAME,
	MODULE_AUTHOR,
	MODULE_EMAIL,
	MAJOR_VERSION,
	MINOR_VERSION
	};

void CleanupTest(void) {
	lprintf(9, "--- test of adding an unload hook --- \n");
	}

void NewRoomTest(void) {
	lprintf(9, "--- test module was told we're now in %s ---\n",
		CC->cs_room);
	}

void SessionStartTest(void) {
	lprintf(9, "--- starting up session %d ---\n",
		CC->cs_pid);
	}

void SessionStopTest(void) {
	lprintf(9, "--- ending session %d ---\n", 
		CC->cs_pid);
	}

void LoginTest(void) {
	lprintf(9, "--- Hello, %s ---\n", CC->curr_user);
	}


void Ygorl(char *username, long usernum) {
	if (!strcasecmp(username, "Hexslinger")) {
		strcpy(username, "Flaming Asshole");
		}
	}



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

struct DLModule_Info *Dynamic_Module_Init(void)
{
   CtdlRegisterCleanupHook(CleanupTest);
   CtdlRegisterCleanupHook(PurgeMessages);
   CtdlRegisterSessionHook(NewRoomTest, EVT_NEWROOM);
   CtdlRegisterSessionHook(SessionStartTest, EVT_START);
   CtdlRegisterSessionHook(SessionStopTest, EVT_STOP);
   CtdlRegisterSessionHook(LoginTest, EVT_LOGIN);
   CtdlRegisterUserHook(Ygorl, EVT_OUTPUTMSG);
   return &info;
}
