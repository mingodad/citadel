/* */
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
#ifdef HAVE_PTHREAD_H
#include <pthread.h>
#endif
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
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "vcard.h"

struct vcard_internal_info {
	long msgnum;
};

/* Message number symbol used internally by these functions */
unsigned long SYM_VCARD;
#define VC ((struct vcard_internal_info *)CtdlGetUserData(SYM_VCARD))


/*
 * This handler detects whether the user is attempting to save a new
 * vCard as part of his/her personal configuration, and handles the replace
 * function accordingly.
 */
int vcard_personal_upload(struct CtdlMessage *msg) {
	char *ptr;
	int linelen;

	/* If this isn't the configuration room, or if this isn't a MIME
	 * message, don't bother.
	 */
	if (strcasecmp(msg->cm_fields['O'], CONFIGROOM)) return(0);
	if (msg->cm_format_type != 4) return(0);

	ptr = msg->cm_fields['M'];
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		lprintf(9, "linelen == %d\n", linelen);
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: text/x-vcard", 26)) {
			/* Bingo!  The user is uploading a new vCard, so
			 * delete the old one.
			 */
			CtdlDeleteMessages(msg->cm_fields['O'],
					0L, "text/x-vcard");
			return(0);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/*
 * back end function used by vcard_get_my()
 */
void vcard_gm_backend(long msgnum) {
	VC->msgnum = msgnum;
}


/*
 * If this user has a vcard on disk, read it into memory, otherwise allocate
 * and return an empty vCard.
 */
struct vCard *vcard_get_my(void) {
        char hold_rm[ROOMNAMELEN];
        char config_rm[ROOMNAMELEN];
	struct CtdlMessage *msg;
	struct vCard *v;

        strcpy(hold_rm, CC->quickroom.QRname);	/* save current room */
        MailboxName(config_rm, &CC->usersupp, CONFIGROOM);

        if (getroom(&CC->quickroom, config_rm) != 0) {
                getroom(&CC->quickroom, hold_rm);
                return new_vcard();
        }

        /* We want the last (and probably only) vcard in this room */
	VC->msgnum = (-1);
        CtdlForEachMessage(MSGS_LAST, 1, "text/x-vcard", vcard_gm_backend);
        getroom(&CC->quickroom, hold_rm);	/* return to saved room */

	if (VC->msgnum < 0L) return new_vcard();

	msg = CtdlFetchMessage(VC->msgnum);
	if (msg == NULL) return new_vcard();

	v = load_vcard(msg->cm_fields['M']);
	CtdlFreeMessage(msg);
	return v;
}


/*
 * Store this user's vCard in the appropriate place
 */
/*
 * Write our config to disk
 */
void vcard_write_my(struct vCard *v) {
        char temp[PATH_MAX];
        FILE *fp;
	char *ser;

        strcpy(temp, tmpnam(NULL));

        fp = fopen(temp, "w");
        if (fp == NULL) return;
	fwrite("FIXFIXFIXFIX FIX FIX", 100, 1, fp);
        fclose(fp);

        /* this handy API function does all the work for us */
        CtdlWriteObject(CONFIGROOM, "text/x-vcard", temp, 1, 0, 1);

        unlink(temp);
}




char *Dynamic_Module_Init(void)
{
	CtdlAllocUserData(SYM_VCARD, sizeof(struct vcard_internal_info));
	CtdlRegisterMessageHook(vcard_personal_upload, EVT_BEFORESAVE);
	return "$Id$";
}
