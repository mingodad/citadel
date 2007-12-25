/*
 * $Id: serv_dspam.c 5876 2007-12-10 23:22:03Z dothebart $
 *
 * This module glues libDSpam to the Citadel server in order to implement
 * DSPAM Spamchecking 
 *
 * This code is released under the terms of the GNU General Public License. 
 */

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <ctype.h>
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
#include "internet_addressing.h"


#include "ctdl_module.h"


#ifdef HAVE_LIBDSPAM

#undef HAVE_CONFIG_H
#include <dspam/libdspam.h>
#define HAVE_CONFIG_H
/*
 * Citadel protocol to manage sieve scripts.
 * This is basically a simplified (read: doesn't resemble IMAP) version
 * of the 'managesieve' protocol.
 */
void cmd_tspam(char *argbuf) {
	

	/* tag mails as spam or Ham */
	/* probably do: dspam_init(ctdl_dspam_dir); dspam_process dspam_addattribute; dspam_destroy*/
	// extract DSS_ERROR or DSS_CORPUS from the commandline. error->ham; corpus -> spam?
}



void ctdl_dspam_init(void) {

	libdspam_init("bdb");/* <which database backend do we prefer? */

}

void dspam_do_msg(long msgnum, void *userdata) 
{
	DSPAM_CTX *CTX;               	/* DSPAM Context */
	struct CtdlMessage *msg;
	struct _ds_spam_signature SIG;        /* signature */

	CTX = *(DSPAM_CTX**) userdata;
	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) return;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1);

	/* Call DSPAM's processor with the message text */
	if (dspam_process (CTX, msg->cm_fields['A']) != 0)
	{
		lprintf(CTDL_CRIT, "ERROR: dspam_process failed");
		return;
	}
	if (CTX->signature == NULL)
	{
		lprintf(CTDL_CRIT,"No signature provided\n");
	}
	else
	{
/* Copy to a safe place */

		SIG.data = malloc (CTX->signature->length);
		if (SIG.data != NULL)
			memcpy (SIG.data, CTX->signature->data, CTX->signature->length);
	}
	SIG.length = CTX->signature->length;
	/* Print processing results */
	printf ("Probability: %2.4f Confidence: %2.4f, Result: %s\n",
		CTX->probability,
		CTX->confidence,
		(CTX->result == DSR_ISSPAM) ? "Spam" : "Innocent");

	//// todo: put signature into the citadel message
	//// todo: save message; destroy message.
}

int serv_dspam_room(struct ctdlroom *room)
{
	DSPAM_CTX *CTX;               	/* DSPAM Context */

	/* scan for spam; do */
	/* probably do: dspam_init; dspam_process dspam_addattribute; dspam_destroy*/
//DSS_NONE
//#define	DSR_ISSPAM		0x01
//#define DSR_ISINNOCENT		0x02
// dspam_init (cc->username, NULL, ctdl_dspam_home, DSM_PROCESS,
	//                  DSF_SIGNATURE | DSF_NOISE);
	/// todo: if roomname = spam / ham -> learn!
	if (room->QRflags & QR_PRIVATE) /* Are we sending to a private mailbox? */
	{
		char User[64];
		// maybe we should better get our realname here?
		snprintf(User, 64, "%ld", room->QRroomaide);

		CTX = dspam_init(User, 
				 NULL,
				 ctdl_dspam_dir, 
				 DSM_PROCESS, 
				 DSF_SIGNATURE | DSF_NOISE);
	}
	/// else -> todo: global user for public rooms etc.
	if (CTX == NULL)
	{
		lprintf(CTDL_CRIT, "ERROR: dspam_init failed!\n");
		return ERROR + INTERNAL_ERROR;
	}
	/* Use graham and robinson algorithms, graham's p-values */
	CTX->algorithms = DSA_GRAHAM | DSA_BURTON | DSP_GRAHAM;

	/* Use CHAIN tokenizer */
	CTX->tokenizer = DSZ_CHAIN;

	CtdlForEachMessage(MSGS_GT, NULL, NULL, NULL, NULL,
			   dspam_do_msg,
			   (void *) &CTX);

	return 0;
}

void serv_dspam_shutdown (void)
{
	libdspam_shutdown ();
}
#endif	/* HAVE_LIBDSPAM */

CTDL_MODULE_INIT(dspam)
{
	return;
	if (!threading)
	{
#ifdef HAVE_LIBDSPAM

		ctdl_dspam_init();
		CtdlRegisterCleanupHook(serv_dspam_shutdown);
		CtdlRegisterProtoHook(cmd_tspam, "SPAM", "Tag Message as Spam/Ham to learn DSPAM");

	        CtdlRegisterRoomHook(serv_dspam_room);

        	///CtdlRegisterSessionHook(perform_dspam_processing, EVT_HOUSE);

#else	/* HAVE_LIBDSPAM */

		lprintf(CTDL_INFO, "This server is missing libdspam Spam filtering will be disabled.\n");

#endif	/* HAVE_LIBDSPAM */
	}
	
        /* return our Subversion id for the Log */
	return "$Id: serv_dspam.c 5876 2007-12-10 23:22:03Z dothebart $";
}

