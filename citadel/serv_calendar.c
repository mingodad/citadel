/* 
 * $Id$ 
 *
 * This module implements iCalendar object processing and the Calendar>
 * room on a Citadel/UX server.  It handles iCalendar objects using the
 * iTIP protocol.  See RFCs 2445 and 2446.
 *
 */

#include "sysdep.h"
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include "serv_calendar.h"
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

	cprintf("%d I support|ICAL\n", CIT_OK);
	return;
}


/*
 * We don't know if the calendar room exists so we just create it at login
 */
void ical_create_room(void)
{
	struct quickroom qr;
	struct visit vbuf;

	/* Create the calendar room if it doesn't already exist */
	create_room(USERCALENDARROOM, 4, "", 0, 1, 0);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERCALENDARROOM)) {
		lprintf(3, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	lputroom(&qr);

	/* Set the view to a calendar view */
	CtdlGetRelationship(&vbuf, &CC->usersupp, &qr);
	vbuf.v_view = 3;	/* 3 = calendar */
	CtdlSetRelationship(&vbuf, &CC->usersupp, &qr);

	/* Create the tasks list room if it doesn't already exist */
	create_room(USERTASKSROOM, 4, "", 0, 1, 0);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERTASKSROOM)) {
		lprintf(3, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	lputroom(&qr);

	/* Set the view to a task list view */
	CtdlGetRelationship(&vbuf, &CC->usersupp, &qr);
	vbuf.v_view = 4;	/* 4 = tasks */
	CtdlSetRelationship(&vbuf, &CC->usersupp, &qr);

	return;
}


/* See if we need to prevent the object from being saved */
int ical_obj_beforesave(struct CtdlMessage *msg)
{
	char roomname[ROOMNAMELEN];
	char *p;
	int a;
	
	/*
	 * Only messages with content-type text/calendar or text/x-calendar
	 * may be saved to Calendar>.  If the message is bound for
	 * Calendar> but doesn't have this content-type, throw an error
	 * so that the message may not be posted.
	 */

	/* First determine if this is our room */
	MailboxName(roomname, sizeof roomname, &CC->usersupp, USERCALENDARROOM);
	if (strncmp(roomname, msg->cm_fields['O'], ROOMNAMELEN))
		return 0;	/* It's not us... */

	/* Then determine content-type of the message */
	
	/* It must be an RFC822 message! */
	/* FIXME: Not handling MIME multipart messages; implement with IMIP */
	if (msg->cm_format_type != 4)
		return 1;	/* You tried to save a non-RFC822 message! */
	
	/* Find the Content-Type: header */
	p = msg->cm_fields['M'];
	a = strlen(p);
	while (--a > 0) {
		if (!strncasecmp(p, "Content-Type: ", 14)) {	/* Found it */
			if (!strncasecmp(p + 14, "text/x-calendar", 15) ||
			    !strncasecmp(p + 14, "text/calendar", 13))
				return 0;
			else
				return 1;
		}
		p++;
	}
	
	/* Oops!  No Content-Type in this message!  How'd that happen? */
	lprintf(7, "RFC822 message with no Content-Type header!\n");
	return 1;
}



/* Register this module with the Citadel server. */
char *Dynamic_Module_Init(void)
{
	CtdlRegisterSessionHook(ical_create_room, EVT_LOGIN);
	CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
	CtdlRegisterProtoHook(cmd_ical, "ICAL", "Register iCalendar support");
	return "$Id$";
}
