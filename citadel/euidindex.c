/*
 * $Id$
 *
 * Index messages by EUID per room.
 *
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
#include "citadel.h"
#include "server.h"
#include "serv_extensions.h"
#include "database.h"
#include "msgbase.h"
#include "support.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "room_ops.h"
#include "user_ops.h"
#include "file_ops.h"
#include "config.h"
#include "control.h"
#include "tools.h"
#include "euidindex.h"


long locate_message_by_euid(char *euid, struct ctdlroom *qrbuf) {
	return(0);
}

void index_message_by_euid(char *euid, struct ctdlroom *qrbuf, long msgnum) {
	char *key;
	int key_len;

	lprintf(CTDL_DEBUG, "Indexing message #%ld <%s> in <%s>\n", msgnum, euid, qrbuf->QRname);

	key_len = strlen(euid) + sizeof(long) + 1;
	key = malloc(key_len);
	memcpy(key, &qrbuf->QRnumber, sizeof(long));
	strcpy(&key[sizeof(long)], euid);

	cdb_store(CDB_EUIDINDEX, key, key_len, &msgnum, sizeof(long));
}



/*
 * Called by rebuild_euid_index_for_room() to index one message.
 */
void rebuild_euid_index_for_msg(long msgnum, void *userdata) {
	struct CtdlMessage *msg;

	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) return;
	if (msg->cm_fields['E'] != NULL) {
		index_message_by_euid(msg->cm_fields['E'], &CC->room, msgnum);
	}
	CtdlFreeMessage(msg);
}


void rebuild_euid_index_for_room(struct ctdlroom *qrbuf, void *data) {
	static struct RoomProcList *rplist = NULL;
	struct RoomProcList *ptr;
	struct ctdlroom qr;

	/* Lazy programming here.  Call this function as a ForEachRoom backend
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
		if (getroom(&qr, rplist->name) == 0) {
			lprintf(CTDL_DEBUG, "Rebuilding EUID index for <%s>\n", rplist->name);
			usergoto(rplist->name, 0, 0, NULL, NULL);
			CtdlForEachMessage(MSGS_ALL, 0L, NULL, NULL, rebuild_euid_index_for_msg, NULL);
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
	cdb_trunc(CDB_EUIDINDEX);

	ForEachRoom(rebuild_euid_index_for_room, NULL);
	rebuild_euid_index_for_room(NULL, NULL);
}
