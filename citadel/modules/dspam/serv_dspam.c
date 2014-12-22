/*
 * This module glues libDSpam to the Citadel server in order to implement
 * DSPAM Spamchecking 
 *
 * Copyright (c) 2012 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 3.
 *  
 *  
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  
 *  
 *  
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
#include "database.h"
#include "msgbase.h"
#include "internet_addressing.h"


#include "ctdl_module.h"


#ifdef HAVE_LIBDSPAM
#define CONFIG_DEFAULT file_dpsam_conf
#define LOGDIR file_dspam_log


//#define HAVE_CONFIG_H
#include <dspam/libdspam.h>
//#define HAVE_CONFIG_H

typedef struct stringlist stringlist;

struct stringlist {
	char *Str;
	long len;
	stringlist *Next;
};
	

/*
 * Citadel protocol to manage sieve scripts.
 * This is basically a simplified (read: doesn't resemble IMAP) version
 * of the 'managesieve' protocol.
 */
void cmd_tspam(char *argbuf) {
	char buf[SIZ];
	long len;
	long count;
	stringlist *Messages; 
	stringlist *NextMsg; 

	Messages = NULL;
	NextMsg = NULL;
	count = 0;
	if (CtdlAccessCheck(ac_room_aide)) return;
	if (atoi(argbuf) == 0) {
		cprintf("%d Ok.\n", CIT_OK);
		return;
	}
	cprintf("%d Send info...\n", SEND_LISTING);

	do {
		len = client_getln(buf, sizeof buf);
		if (strcmp(buf, "000")) {
			if (Messages == NULL) {
				Messages = malloc (sizeof (stringlist));
				NextMsg = Messages;
			}
			else {
				Messages->Next = malloc (sizeof (stringlist));
				NextMsg = NextMsg->Next;
			}
			NextMsg->Next = NULL;
			NextMsg->Str = malloc (len+1);
			NextMsg->len = len;
			memcpy (NextMsg->Str, buf, len + 1);/// maybe split spam /ham per line?
			count++;
		}
	} while (strcmp(buf, "000"));
/// is there a way to filter foreachmessage by a list?
	/* tag mails as spam or Ham */
	/* probably do: dspam_init(ctdl_dspam_dir); dspam_process dspam_addattribute; dspam_destroy*/
	// extract DSS_ERROR or DSS_CORPUS from the commandline. error->ham; corpus -> spam?
	/// todo: send answer listing...
}



void ctdl_dspam_init(void) {

///	libdspam_init("bdb");/* <which database backend do we prefer? */

}

void dspam_do_msg(long msgnum, void *userdata) 
{
	char *msgtext;
	DSPAM_CTX *CTX;               	/* DSPAM Context */
	struct CtdlMessage *msg;
	struct _ds_spam_signature SIG;        /* signature */

	CTX = *(DSPAM_CTX**) userdata;
	msg = CtdlFetchMessage(msgnum, 0);
	if (msg == NULL) return;


	/* Message */
	CC->redirect_buffer = malloc(SIZ);
	CC->redirect_len = 0;
	CC->redirect_alloc = SIZ;
	CtdlOutputPreLoadedMsg(msg, MT_RFC822, HEADERS_ALL, 0, 1, 0);
	msgtext = CC->redirect_buffer;
// don't need?	msglen = CC->redirect_len;
	CC->redirect_buffer = NULL;
	CC->redirect_len = 0;
	CC->redirect_alloc = 0;

	/* Call DSPAM's processor with the message text */
	if (dspam_process (CTX, msgtext) != 0)
	{
		free(msgtext);
		syslog(LOG_CRIT, "ERROR: dspam_process failed");
		return;
	}
	if (CTX->signature == NULL)
	{
		syslog(LOG_CRIT,"No signature provided\n");
	}
	else
	{
/* Copy to a safe place */
		// TODO: len -> cm_fields?
		msg->cm_fields[eErrorMsg] = malloc (CTX->signature->length * 2);
		size_t len = CtdlEncodeBase64(msg->cm_fields[eErrorMsg], CTX->signature->data, CTX->signature->length, 0);

		if (msg->cm_fields[eErrorMsg][len - 1] == '\n') {
			msg->cm_fields[eErrorMsg][len - 1] = '\0';
		}
	}
	free(msgtext);

	SIG.length = CTX->signature->length;
	/* Print processing results */
	syslog(LOG_DEBUG, "Probability: %2.4f Confidence: %2.4f, Result: %s\n",
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
	if ((room->QRflags & QR_PRIVATE) &&/* Are we sending to a private mailbox? */
	    (strstr(room->QRname, ".Mail")!=NULL))

	{
		char User[64];
		// maybe we should better get our realname here?
		snprintf(User, 64, "%ld", room->QRroomaide);
		extract_token(User, room->QRname, 0, '.', sizeof(User));
		CTX = dspam_init(User, 
				 NULL,
				 ctdl_dspam_dir, 
				 DSM_PROCESS, 
				 DSF_SIGNATURE | DSF_NOISE);
	}
	else return 0;//// 
	/// else -> todo: global user for public rooms etc.
	if (CTX == NULL)
	{
		syslog(LOG_CRIT, "ERROR: dspam_init failed!\n");
		return ERROR + INTERNAL_ERROR;
	}
	/* Use graham and robinson algorithms, graham's p-values */
	CTX->algorithms = DSA_GRAHAM | DSA_BURTON | DSP_GRAHAM;

	/* Use CHAIN tokenizer */
	CTX->tokenizer = DSZ_CHAIN;

	CtdlForEachMessage(MSGS_GT, 1, NULL, NULL, NULL,
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
	return "disabled.";
	if (!threading)
	{
#ifdef HAVE_LIBDSPAM

		ctdl_dspam_init();
		CtdlRegisterCleanupHook(serv_dspam_shutdown);
		CtdlRegisterProtoHook(cmd_tspam, "SPAM", "Tag Message as Spam/Ham to learn DSPAM");

	        CtdlRegisterRoomHook(serv_dspam_room);

        	///CtdlRegisterSessionHook(perform_dspam_processing, EVT_HOUSE);

#else	/* HAVE_LIBDSPAM */

		syslog(LOG_INFO, "This server is missing libdspam Spam filtering will be disabled.\n");

#endif	/* HAVE_LIBDSPAM */
	}
	
        /* return our module name for the log */
	return "dspam";
}

