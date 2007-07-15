/*
 * $Id$ 
 *
 * This module handles the loading/saving and maintenance of the
 * system's Internet configuration.  It's not an optional component; I
 * wrote it as a module merely to keep things as clean and loosely coupled
 * as possible.
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
#include "citadel.h"
#include "server.h"
#include "sysdep_decls.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "serv_extensions.h"
#include "room_ops.h"
#include "user_ops.h"
#include "policy.h"
#include "database.h"
#include "msgbase.h"
#include "tools.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"




void inetcfg_setTo(struct CtdlMessage *msg) {
	char *conf;
	char buf[SIZ];
	
	if (msg->cm_fields['M']==NULL) return;
	conf = strdup(msg->cm_fields['M']);

	if (conf != NULL) {
		do {
			extract_token(buf, conf, 0, '\n', sizeof buf);
			strcpy(conf, &conf[strlen(buf)+1]);
		} while ( (strlen(conf)>0) && (strlen(buf)>0) );

		if (inetcfg != NULL) free(inetcfg);
		inetcfg = conf;
	}
}


#ifdef ___NOT_CURRENTLY_IN_USE___
void spamstrings_setTo(struct CtdlMessage *msg) {
	char buf[SIZ];
	char *conf;
	struct spamstrings_t *sptr;
	int i, n;

	/* Clear out the existing list */
	while (spamstrings != NULL) {
		sptr = spamstrings;
		spamstrings = spamstrings->next;
		free(sptr->string);
		free(sptr);
	}

	/* Read in the new list */
	if (msg->cm_fields['M']==NULL) return;
	conf = strdup(msg->cm_fields['M']);
	if (conf == NULL) return;

	n = num_tokens(conf, '\n');
	for (i=0; i<n; ++i) {
		extract_token(buf, conf, i, '\n', sizeof buf);
		sptr = malloc(sizeof(struct spamstrings_t));
		sptr->string = strdup(buf);
		sptr->next = spamstrings;
		spamstrings = sptr;
	}

}
#endif


/*
 * This handler detects changes being made to the system's Internet
 * configuration.
 */
int inetcfg_aftersave(struct CtdlMessage *msg) {
	char *ptr;
	int linelen;

	/* If this isn't the configuration room, or if this isn't a MIME
	 * message, don't bother.
	 */
	if (strcasecmp(msg->cm_fields['O'], SYSCONFIGROOM)) return(0);
	if (msg->cm_format_type != 4) return(0);

	ptr = msg->cm_fields['M'];
	while (ptr != NULL) {
	
		linelen = strcspn(ptr, "\n");
		if (linelen == 0) return(0);	/* end of headers */	
		
		if (!strncasecmp(ptr, "Content-type: ", 14)) {
			if (!strncasecmp(&ptr[14], INTERNETCFG,
		   	   strlen(INTERNETCFG))) {
				inetcfg_setTo(msg);	/* changing configs */
			}
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}


void inetcfg_init_backend(long msgnum, void *userdata) {
	struct CtdlMessage *msg;

       	msg = CtdlFetchMessage(msgnum, 1);
       	if (msg != NULL) {
		inetcfg_setTo(msg);
               	CtdlFreeMessage(msg);
	}
}


#ifdef ___NOT_CURRENTLY_IN_USE___
void spamstrings_init_backend(long msgnum, void *userdata) {
	struct CtdlMessage *msg;

       	msg = CtdlFetchMessage(msgnum, 1);
       	if (msg != NULL) {
		spamstrings_setTo(msg);
               	CtdlFreeMessage(msg);
	}
}
#endif


void inetcfg_init(void) {
	if (getroom(&CC->room, SYSCONFIGROOM) != 0) return;
	CtdlForEachMessage(MSGS_LAST, 1, NULL, INTERNETCFG, NULL,
		inetcfg_init_backend, NULL);
}




/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *serv_inetcfg_init(void)
{
	CtdlRegisterMessageHook(inetcfg_aftersave, EVT_AFTERSAVE);
	inetcfg_init();

	/* return our Subversion id for the Log */
	return "$Id$";
}

