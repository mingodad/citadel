/* $Id$ 
 *
 * This module handles the loading/saving and maintenance of the
 * system's Internet configuration.  It's not an optional component; I
 * wrote it as a module merely to keep things as clean and loosely coupled
 * as possible.
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
#include <sys/time.h>
#include <sys/wait.h>
#include <string.h>
#include <limits.h>
#include "citadel.h"
#include "server.h"
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
#include "tools.h"
#include "internet_addressing.h"
#include "genstamp.h"
#include "domain.h"

void inetcfg_setTo(struct CtdlMessage *msg) {
	char *conf;
	char buf[256];

	if (msg->cm_fields['M']==NULL) return;
	conf = strdoop(msg->cm_fields['M']);

	if (conf != NULL) {
		do {
			extract_token(buf, conf, 0, '\n');
			strcpy(conf, &conf[strlen(buf)+1]);
		} while ( (strlen(conf)>0) && (strlen(buf)>0) );

		if (inetcfg != NULL) phree(inetcfg);
		inetcfg = conf;
	}
}


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
		
		if ( (!strncasecmp(ptr, "Content-type: ", 14))
		   && (!strncasecmp(&ptr[14], INTERNETCFG,
		   strlen(INTERNETCFG) )) ) {
			/* Bingo!  The user is changing configs.
			 */
			inetcfg_setTo(msg);
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}


void inetcfg_init_backend(long msgnum) {
	struct CtdlMessage *msg;

       	msg = CtdlFetchMessage(msgnum);
       	if (msg != NULL) {
		inetcfg_setTo(msg);
               	CtdlFreeMessage(msg);
	}
}


void inetcfg_init(void) {
	if (getroom(&CC->quickroom, SYSCONFIGROOM) != 0) return;
	CtdlForEachMessage(MSGS_LAST, 1, (-127), INTERNETCFG, NULL,
		inetcfg_init_backend);
}




/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *Dynamic_Module_Init(void)
{
	CtdlRegisterMessageHook(inetcfg_aftersave, EVT_AFTERSAVE);
	inetcfg_init();
	return "$Id$";
}

