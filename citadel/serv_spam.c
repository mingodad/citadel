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



char *Dynamic_Module_Init(void)
{
	CtdlRegisterMessageHook(spam_filter, EVT_SMTPSCAN);
        return "$Id$";
}
