/* $Id$ 
 *
 * This module handles the loading/saving and maintenance of the
 * system's Internet configuration.
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
			lprintf(9, "Internet configuration changed FIX\n");
		}

		ptr = strchr((char *)ptr, '\n');
		if (ptr != NULL) ++ptr;
	}

	return(0);
}



/*****************************************************************************/
/*                      MODULE INITIALIZATION STUFF                          */
/*****************************************************************************/


char *Dynamic_Module_Init(void)
{
	CtdlRegisterMessageHook(inetcfg_aftersave, EVT_AFTERSAVE);
	return "$Id$";
}

