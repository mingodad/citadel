/*
 * Index messages by EUID per room.
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>

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


#include <ctype.h>
#include <string.h>
#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "room_ops.h"
#include "user_ops.h"
#include "config.h"
#include "control.h"
#include "euidindex.h"

#include "ctdl_module.h"

/*
 * The structure of an euidindex record *key* is:
 *
 * |----room_number----|----------EUID-------------|
 *    (sizeof long)       (actual length of euid)
 *
 *
 * The structure of an euidindex record *value* is:
 *
 * |-----msg_number----|----room_number----|----------EUID-------------|
 *    (sizeof long)       (sizeof long)       (actual length of euid)
 *
 */



/*
 * Return nonzero if the supplied room is one which should have
 * an EUID index.
 */
int DoesThisRoomNeedEuidIndexing(struct ctdlroom *qrbuf) {

	switch(qrbuf->QRdefaultview) {
		case VIEW_BBS:		return(0);
		case VIEW_MAILBOX:	return(0);
		case VIEW_ADDRESSBOOK:	return(1);
		case VIEW_DRAFTS:       return(0);
		case VIEW_CALENDAR:	return(1);
		case VIEW_TASKS:	return(1);
		case VIEW_NOTES:	return(1);
		case VIEW_WIKI:		return(1);
		case VIEW_BLOG:		return(1);
	}
	
	return(0);
}






/*
 * Locate a message in a given room with a given euid, and return
 * its message number.
 */
long locate_message_by_euid(char *euid, struct ctdlroom *qrbuf) {
	return CtdlLocateMessageByEuid (euid, qrbuf);
}

long CtdlLocateMessageByEuid(char *euid, struct ctdlroom *qrbuf) {
	char *key;
	int key_len;
	struct cdbdata *cdb_euid;
	long msgnum = (-1L);

	syslog(LOG_DEBUG, "Searching for EUID <%s> in <%s>\n", euid, qrbuf->QRname);

	key_len = strlen(euid) + sizeof(long) + 1;
	key = malloc(key_len);
	memcpy(key, &qrbuf->QRnumber, sizeof(long));
	strcpy(&key[sizeof(long)], euid);

	cdb_euid = cdb_fetch(CDB_EUIDINDEX, key, key_len);
	free(key);

	if (cdb_euid == NULL) {
		msgnum = (-1L);
	}
	else {
		/* The first (sizeof long) of the record is what we're
		 * looking for.  Throw away the rest.
		 */
		memcpy(&msgnum, cdb_euid->ptr, sizeof(long));
		cdb_free(cdb_euid);
	}
	syslog(LOG_DEBUG, "returning msgnum = %ld\n", msgnum);
	return(msgnum);
}


/*
 * Store the euid index for a message, which has presumably just been
 * stored in this room by the caller.
 */
void index_message_by_euid(char *euid, struct ctdlroom *qrbuf, long msgnum) {
	char *key;
	int key_len;
	char *data;
	int data_len;

	syslog(LOG_DEBUG, "Indexing message #%ld <%s> in <%s>\n", msgnum, euid, qrbuf->QRname);

	key_len = strlen(euid) + sizeof(long) + 1;
	key = malloc(key_len);
	memcpy(key, &qrbuf->QRnumber, sizeof(long));
	strcpy(&key[sizeof(long)], euid);

	data_len = sizeof(long) + key_len;
	data = malloc(data_len);

	memcpy(data, &msgnum, sizeof(long));
	memcpy(&data[sizeof(long)], key, key_len);

	cdb_store(CDB_EUIDINDEX, key, key_len, data, data_len);
	free(key);
	free(data);
}



/*
 * Called by rebuild_euid_index_for_room() to index one message.
 */
void rebuild_euid_index_for_msg(long msgnum, void *userdata) {
	struct CtdlMessage *msg = NULL;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) return;
	if (!CM_IsEmpty(msg, eExclusiveID)) {
		index_message_by_euid(msg->cm_fields[eExclusiveID], &CC->room, msgnum);
	}
	CM_Free(msg);
}


void rebuild_euid_index_for_room(struct ctdlroom *qrbuf, void *data) {
	static struct RoomProcList *rplist = NULL;
	struct RoomProcList *ptr;
	struct ctdlroom qr;

	/* Lazy programming here.  Call this function as a CtdlForEachRoom backend
	 * in order to queue up the room names, or call it with a null room
	 * to make it do the processing.
	 */
	if (qrbuf != NULL) {
		ptr = (struct RoomProcList *)
			malloc(sizeof (struct RoomProcList));
		if (ptr == NULL) return;

		safestrncpy(ptr->name, qrbuf->QRname, sizeof ptr->name);
		ptr->next = rplist;
		rplist = ptr;
		return;
	}

	while (rplist != NULL) {
		if (CtdlGetRoom(&qr, rplist->name) == 0) {
			if (DoesThisRoomNeedEuidIndexing(&qr)) {
				syslog(LOG_DEBUG,
					"Rebuilding EUID index for <%s>\n",
					rplist->name);
				CtdlUserGoto(rplist->name, 0, 0, NULL, NULL);
				CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, NULL,
					rebuild_euid_index_for_msg, NULL);
			}
		}
		ptr = rplist;
		rplist = rplist->next;
		free(ptr);
	}
}


/*
 * Globally rebuild the EUID indices in every room.
 */
void rebuild_euid_index(void) {
	cdb_trunc(CDB_EUIDINDEX);		/* delete the old indices */
	CtdlForEachRoom(rebuild_euid_index_for_room, NULL);	/* enumerate rm names */
	rebuild_euid_index_for_room(NULL, NULL);	/* and index them */
}



/*
 * Server command to fetch a message number given an euid.
 */
void cmd_euid(char *cmdbuf) {
	char euid[256];
	long msgnum;
        struct cdbdata *cdbfr;
        long *msglist = NULL;
        int num_msgs = 0;
	int i;

	if (CtdlAccessCheck(ac_logged_in_or_guest)) return;

	extract_token(euid, cmdbuf, 0, '|', sizeof euid);
	msgnum = CtdlLocateMessageByEuid(euid, &CC->room);
	if (msgnum <= 0L) {
		cprintf("%d not found\n", ERROR + MESSAGE_NOT_FOUND);
		return;
	}

        cdbfr = cdb_fetch(CDB_MSGLISTS, &CC->room.QRnumber, sizeof(long));
	if (cdbfr != NULL) {
                num_msgs = cdbfr->len / sizeof(long);
                msglist = (long *) cdbfr->ptr;
                for (i = 0; i < num_msgs; ++i) {
                        if (msglist[i] == msgnum) {
				cdb_free(cdbfr);
				cprintf("%d %ld\n", CIT_OK, msgnum);
				return;
			}
		}
                cdb_free(cdbfr);
	}

	cprintf("%d not found\n", ERROR + MESSAGE_NOT_FOUND);
}

CTDL_MODULE_INIT(euidindex)
{
	if (!threading) {
		CtdlRegisterProtoHook(cmd_euid, "EUID", "Perform operations on Extended IDs for messages");
	}
	/* return our Subversion id for the Log */
	return "euidindex";
}
