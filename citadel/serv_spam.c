/*
 * $Id$
 *
 * Reject incoming SMTP messages containing strings that tell us that the
 * message is probably spam.
 *
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
#include <sys/socket.h>
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "control.h"
#include "dynloader.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "clientsocket.h"



#ifdef ___NOT_CURRENTLY_IN_USE___
/* Scan a message for spam */
int spam_filter(struct CtdlMessage *msg) {
	int spam_strings_found = 0;
	struct spamstrings_t *sptr;
	char *ptr;

	/* Bail out if there's no message text */
	if (msg->cm_fields['M'] == NULL) return(0);


	/* Scan! */
	ptr = msg->cm_fields['M'];
	while (ptr++[0] != 0) {
		for (sptr = spamstrings; sptr != NULL; sptr = sptr->next) {
			if (!strncasecmp(ptr, sptr->string,
			   strlen(sptr->string))) {
				++spam_strings_found;
			}
		}
	}

	if (spam_strings_found) {
		if (msg->cm_fields['0'] != NULL) {
			phree(msg->cm_fields['0']);
		}
		msg->cm_fields['0'] = strdoop("Unsolicited spam rejected");
		return(spam_strings_found);
	}

	return(0);
}
#endif



/*
 * Connect to the SpamAssassin server and scan a message.
 */
int spam_assassin(struct CtdlMessage *msg) {
	int sock = (-1);
	char buf[SIZ];
	FILE *msg_fp;
	long content_length;
	long block_length;
	int is_spam = 0;

#define SPAMASSASSIN_HOST	"127.0.0.1"
#define SPAMASSASSIN_PORT	"783"

	msg_fp = tmpfile();
	if (msg_fp == NULL) return(0);

	/* Connect to the SpamAssassin server */
	lprintf(9, "Connecting to SpamAssassin\n");
	sock = sock_connect(SPAMASSASSIN_HOST, SPAMASSASSIN_PORT, "tcp");
	if (sock < 0) {
		/* If the service isn't running, just pass the mail
		 * through.  Potentially throwing away mails isn't good.
		 */
		return(0);
	}

	/* Measure the message (I don't like doing this with a tempfile
	   but right now it's the only way)
	 */
	lprintf(9, "Measuring message\n");
	CtdlRedirectOutput(msg_fp, -1);
	CtdlOutputPreLoadedMsg(msg, 0L, MT_RFC822, 0, 0, 1);
	CtdlRedirectOutput(NULL, -1);
	fseek(msg_fp, 0L, SEEK_END);
	content_length = ftell(msg_fp);
	rewind(msg_fp);
	lprintf(9, "Content-length is %ld\n", content_length);

	/* Command */
	lprintf(9, "Transmitting command\n");
	sprintf(buf, "CHECK SPAMC/1.2\r\nContent-length: %ld\r\n\r\n",
		content_length);
	lprintf(9, buf);
	lprintf(9, "sock_write() returned %d\n",
		sock_write(sock, buf, strlen(buf))
	);
	while (content_length > 0) {
		block_length = sizeof(buf);
		if (block_length > content_length) {
			block_length = content_length;
		}
		fread(buf, block_length, 1, msg_fp);
		sock_write(sock, buf, block_length);
		content_length -= block_length;
		lprintf(9, "Wrote %ld bytes (%ld remaining)\n",
			block_length, content_length);
	}
	fclose(msg_fp);	/* this also deletes the file */

	/* Close one end of the socket connection; this tells SpamAssassin
	 * that we're done.
	 */
	lprintf(9, "sock_shutdown() returned %d\n", 
		sock_shutdown(sock, SHUT_WR)
	);
	
	/* Response */
	lprintf(9, "Awaiting response\n");
        if (sock_gets(sock, buf) < 0) {
                goto bail;
        }
        lprintf(9, "<%s\n", buf);
	if (strncasecmp(buf, "SPAMD", 5)) {
		goto bail;
	}
        if (sock_gets(sock, buf) < 0) {
                goto bail;
        }
        lprintf(9, "<%s\n", buf);
	if (!strncasecmp(buf, "Spam: True", 10)) {
		is_spam = 1;
	}

	if (is_spam) {
		if (msg->cm_fields['0'] != NULL) {
			phree(msg->cm_fields['0']);
		}
		msg->cm_fields['0'] = strdoop(
			"Message rejected by SpamAssassin");
	}

bail:	close(sock);
	return(is_spam);
}



char *Dynamic_Module_Init(void)
{

/* ** This one isn't in use.  It's a spam filter I wrote, but we're going to
      try the SpamAssassin stuff instead.
	CtdlRegisterMessageHook(spam_filter, EVT_SMTPSCAN);
 */


	CtdlRegisterMessageHook(spam_assassin, EVT_SMTPSCAN);


        return "$Id$";
}
