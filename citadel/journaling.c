/*
 * Message journaling functions.
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
#include "file_ops.h"
#include "config.h"
#include "control.h"
#include "genstamp.h"
#include "internet_addressing.h"
#include "serv_vcard.h"			/* Needed for vcard_getuser and extract_inet_email_addrs */
#include "journaling.h"

#include "ctdl_module.h"
#include "threads.h"

struct jnlq *jnlq = NULL;	/* journal queue */

/*
 * Hand off a copy of a message to be journalized.
 */
void JournalBackgroundSubmit(struct CtdlMessage *msg,
			StrBuf *saved_rfc822_version,
			struct recptypes *recps) {

	struct jnlq *jptr = NULL;

	/* Avoid double journaling! */
	if (msg->cm_fields['J'] != NULL) {
		FreeStrBuf(&saved_rfc822_version);
		return;
	}

	jptr = (struct jnlq *)malloc(sizeof(struct jnlq));
	if (jptr == NULL) {
		FreeStrBuf(&saved_rfc822_version);
		return;
	}
	memset(jptr, 0, sizeof(struct jnlq));
	if (recps != NULL) memcpy(&jptr->recps, recps, sizeof(struct recptypes));
	if (msg->cm_fields['A'] != NULL) jptr->from = strdup(msg->cm_fields['A']);
	if (msg->cm_fields['N'] != NULL) jptr->node = strdup(msg->cm_fields['N']);
	if (msg->cm_fields['F'] != NULL) jptr->rfca = strdup(msg->cm_fields['F']);
	if (msg->cm_fields['U'] != NULL) jptr->subj = strdup(msg->cm_fields['U']);
	if (msg->cm_fields['I'] != NULL) jptr->msgn = strdup(msg->cm_fields['I']);
	jptr->rfc822 = SmashStrBuf(&saved_rfc822_version);

	/* Add to the queue */
	begin_critical_section(S_JOURNAL_QUEUE);
	jptr->next = jnlq;
	jnlq = jptr;
	end_critical_section(S_JOURNAL_QUEUE);
}


/*
 * Convert a local user name to an internet email address for the journal
 */
 
/*
 * TODODRW: change this into a CtdlModuleDo type function that returns alternative address info
 * for this local user. Something like CtdlModuleGoGetAddr(char *localuser, int type, char *alt_addr, size_t alt_addr_len)
 * where type can be ADDR_EMAIL, ADDR_FIDO, ADDR_UUCP, ADDR_WEB, ADDR_POSTAL etc etc.
 * This then begs the question of what should be returned. Is it wise to return a single char* using a comma as a
 * delimiter? Or would it be better to return a linked list of some kind?
 */
void local_to_inetemail(char *inetemail, char *localuser, size_t inetemail_len) {
	struct ctdluser us;
	struct vCard *v;

	strcpy(inetemail, "");
	if (CtdlGetUser(&us, localuser) != 0) {
		return;
	}

	v = vcard_get_user(&us);
	if (v == NULL) {
		return;
	}

	extract_inet_email_addrs(inetemail, inetemail_len, NULL, 0, v, 1);
	vcard_free(v);
}


/*
 * Called by JournalRunQueue() to send an individual message.
 */
void JournalRunQueueMsg(struct jnlq *jmsg) {

	struct CtdlMessage *journal_msg = NULL;
	struct recptypes *journal_recps = NULL;
	char *message_text = NULL;
	char mime_boundary[256];
	char recipient[256];
	char inetemail[256];
	static int seq = 0;
	int i;

	if (jmsg == NULL)
		return;
	journal_recps = validate_recipients(config.c_journal_dest, NULL, 0);
	if (journal_recps != NULL) {

		if (  (journal_recps->num_local > 0)
		   || (journal_recps->num_internet > 0)
		   || (journal_recps->num_ignet > 0)
		   || (journal_recps->num_room > 0)
		) {

			/*
			 * Construct journal message.
			 * Note that we are transferring ownership of some of the memory here.
			 */
			journal_msg = malloc(sizeof(struct CtdlMessage));
			memset(journal_msg, 0, sizeof(struct CtdlMessage));
			journal_msg->cm_magic = CTDLMESSAGE_MAGIC;
			journal_msg->cm_anon_type = MES_NORMAL;
			journal_msg->cm_format_type = FMT_RFC822;
			journal_msg->cm_fields['J'] = strdup("is journal");
			journal_msg->cm_fields['A'] = jmsg->from;
			journal_msg->cm_fields['N'] = jmsg->node;
			journal_msg->cm_fields['F'] = jmsg->rfca;
			journal_msg->cm_fields['U'] = jmsg->subj;

			sprintf(mime_boundary, "--Citadel-Journal-%08lx-%04x--", time(NULL), ++seq);
			message_text = malloc(strlen(jmsg->rfc822) + sizeof(struct recptypes) + 1024);

			/*
			 * Here is where we begin to compose the journalized message.
			 * NOTE: the superfluous "Content-Identifer: ExJournalReport" header was
			 *       requested by a paying customer, and yes, it is intentionally
			 *       spelled wrong.  Do NOT remove or change it.
			 */
			sprintf(message_text,
				"Content-type: multipart/mixed; boundary=\"%s\"\r\n"
				"Content-Identifer: ExJournalReport\r\n"
				"MIME-Version: 1.0\r\n"
				"\n"
				"--%s\r\n"
				"Content-type: text/plain\r\n"
				"\r\n"
				"Sender: %s "
			,
				mime_boundary,
				mime_boundary,
				( journal_msg->cm_fields['A'] ? journal_msg->cm_fields['A'] : "(null)" )
			);

			if (journal_msg->cm_fields['F']) {
				sprintf(&message_text[strlen(message_text)], "<%s>",
					journal_msg->cm_fields['F']);
			}
			else if (journal_msg->cm_fields['N']) {
				sprintf(&message_text[strlen(message_text)], "@ %s",
					journal_msg->cm_fields['N']);
			}

			sprintf(&message_text[strlen(message_text)],
				"\r\n"
				"Message-ID: <%s>\r\n"
				"Recipients:\r\n"
			,
				jmsg->msgn
			);

			if (jmsg->recps.num_local > 0) {
				for (i=0; i<jmsg->recps.num_local; ++i) {
					extract_token(recipient, jmsg->recps.recp_local,
							i, '|', sizeof recipient);
					local_to_inetemail(inetemail, recipient, sizeof inetemail);
					sprintf(&message_text[strlen(message_text)],
						"	%s <%s>\r\n", recipient, inetemail);
				}
			}

			if (jmsg->recps.num_ignet > 0) {
				for (i=0; i<jmsg->recps.num_ignet; ++i) {
					extract_token(recipient, jmsg->recps.recp_ignet,
							i, '|', sizeof recipient);
					sprintf(&message_text[strlen(message_text)],
						"	%s\r\n", recipient);
				}
			}

			if (jmsg->recps.num_internet > 0) {
				for (i=0; i<jmsg->recps.num_internet; ++i) {
					extract_token(recipient, jmsg->recps.recp_internet,
							i, '|', sizeof recipient);
					sprintf(&message_text[strlen(message_text)],
						"	%s\r\n", recipient);
				}
			}

			sprintf(&message_text[strlen(message_text)],
				"\r\n"
				"--%s\r\n"
				"Content-type: message/rfc822\r\n"
				"\r\n"
				"%s"
				"--%s--\r\n"
			,
				mime_boundary,
				jmsg->rfc822,
				mime_boundary
			);

			journal_msg->cm_fields['M'] = message_text;
			free(jmsg->rfc822);
			free(jmsg->msgn);
			
			/* Submit journal message */
			CtdlSubmitMsg(journal_msg, journal_recps, "", 0);
			CtdlFreeMessage(journal_msg);
		}

		free_recipients(journal_recps);
	}

	/* We are responsible for freeing this memory. */
	free(jmsg);
}


/*
 * Run the queue.
 */
void JournalRunQueue(void) {
	struct jnlq *jptr = NULL;

	while (jnlq != NULL) {
		begin_critical_section(S_JOURNAL_QUEUE);
		if (jnlq != NULL) {
			jptr = jnlq;
			jnlq = jnlq->next;
		}
		end_critical_section(S_JOURNAL_QUEUE);
		JournalRunQueueMsg(jptr);
	}
}


