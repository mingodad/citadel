/* 
 * $Id$ 
 *
 * This module implements iCalendar object processing and the My Calendar>
 * room on a Citadel/UX server.  It handles iCalendar objects using the
 * iTIP protocol.  See RFCs 2445 and 2446.
 *
 */

#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include "sysdep.h"
#include "serv_ical.h"
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "sysdep_decls.h"
#include "support.h"
#include "config.h"
#include "dynloader.h"


/* Tell clients what level of support to expect */
void cmd_ical(char *argbuf)
{
	/* argbuf is not used */
	if (!(CC->logged_in)) {
		cprintf("%d Not logged in.\n", ERROR+NOT_LOGGED_IN);
		return;
	}

	cprintf("%d I (will) support|ICAL,ITIP\n", OK);
	return;
}


int ical_obj_beforesave(struct CtdlMessage *msg)
{
	return 0;
}


int ical_obj_aftersave(struct CtdlMessage *msg)
{
	return 0;
}


/* Register this module with the Citadel server. */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
	CtdlRegisterMessageHook(ical_obj_aftersave, EVT_AFTERSAVE);
	CtdlRegisterProtoHook(cmd_ical, "ICAL", "Register iCalendar support");
	return "$Id$";
}
