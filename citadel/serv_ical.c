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
#include "user_ops.h"
#include "room_ops.h"


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


/* We can't know if the calendar room exists so we just create it at login */
void ical_create_room(void)
{
	char roomname[ROOMNAMELEN];
	struct quickroom qr;

	/* Create the room if it doesn't already exist */
	MailboxName(roomname, &CC->usersupp, USERCALENDARROOM);
	create_room(roomname, 4, "", 0);
	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, roomname)) {
		lprintf(3, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	lputroom(&qr);
	return;
}


/* User is reading a message */
int ical_obj_beforeread(struct CtdlMessage *msg)
{
	return 0;
}


/* See if we need to prevent the object from being saved */
int ical_obj_beforesave(struct CtdlMessage *msg)
{
	return 0;
}


/* aftersave processing */
int ical_obj_aftersave(struct CtdlMessage *msg)
{
	return 0;
}


/* Register this module with the Citadel server. */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterSessionHook(ical_create_room, EVT_LOGIN);
	CtdlRegisterMessageHook(ical_obj_beforeread, EVT_BEFOREREAD);
	CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
	CtdlRegisterMessageHook(ical_obj_aftersave, EVT_AFTERSAVE);
	CtdlRegisterProtoHook(cmd_ical, "ICAL", "Register iCalendar support");
	return "$Id$";
}
