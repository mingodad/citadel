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
#include "tools.h"
#include "msgbase.h"
#include "mime_parser.h"


#ifdef HAVE_ICAL_H

#include <ical.h>

struct ical_respond_data {
	char desired_partnum[SIZ];
	icalcomponent *cal;
};


/*
 * Write our config to disk
 */
void ical_write_to_cal(struct usersupp *u, icalcomponent *cal) {
        char temp[PATH_MAX];
        FILE *fp;
	char *ser;

        strcpy(temp, tmpnam(NULL));
	ser = icalcomponent_as_ical_string(cal);
	if (ser == NULL) return;

	/* Make a temp file out of it */
        fp = fopen(temp, "w");
        if (fp == NULL) return;
	fwrite(ser, strlen(ser), 1, fp);
        fclose(fp);

        /* This handy API function does all the work for us.
	 */
        CtdlWriteObject(USERCALENDARROOM,	/* which room */
			"text/calendar",	/* MIME type */
			temp,			/* temp file */
			u,			/* which user */
			0,			/* not binary */
			0,		/* don't delete others of this type */
			0);			/* no flags */

        unlink(temp);
}


/*
 * Add a calendar object to the user's calendar
 */
void ical_add(icalcomponent *cal, int recursion_level) {
	icalcomponent *c;

	/*
 	 * The VEVENT subcomponent is the one we're interested in saving.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {
	
		ical_write_to_cal(&CC->usersupp, cal);

	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		ical_add(c, recursion_level+1);
	}

}



/*
 * Callback function for mime parser that hunts for calendar content types
 * and turns them into calendar objects
 */
void ical_locate_part(char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, size_t length, char *encoding,
		void *cbuserdata) {

	struct ical_respond_data *ird = NULL;

	ird = (struct ical_respond_data *) cbuserdata;
	if (ird->cal != NULL) {
		icalcomponent_free(ird->cal);
		ird->cal = NULL;
	}
	if (strcasecmp(partnum, ird->desired_partnum)) return;
	ird->cal = icalcomponent_new_from_string(content);
}


/*
 * Respond to a meeting request.
 */
void ical_respond(long msgnum, char *partnum, char *action) {
	struct CtdlMessage *msg;
	struct ical_respond_data ird;

	if (
	   (strcasecmp(action, "accept"))
	   && (strcasecmp(action, "decline"))
	) {
		cprintf("%d Action must be 'accept' or 'decline'\n",
			ERROR + ILLEGAL_VALUE
		);
		return;
	}

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n",
			ERROR+ILLEGAL_VALUE,
			(long)msgnum
		);
		return;
	}

	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, partnum);
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_part,		/* callback function */
		NULL, NULL,
		(void *) &ird,			/* user data */
		0
	);

	CtdlFreeMessage(msg);

	if (ird.cal != NULL) {
		/* Save this in the user's calendar if necessary */
		if (!strcasecmp(action, "accept")) {
			ical_add(ird.cal, 0);
		}

		/* Send a reply if necessary */
		/* FIXME ... do this */

		/* Delete the message from the inbox */
		/* FIXME ... do this */

		icalcomponent_free(ird.cal);
		ird.cal = NULL;
		cprintf("%d ok\n", CIT_OK);
		return;
	}
	else {
		cprintf("%d No calendar object found\n", ERROR);
		return;
	}

	/* should never get here */
}


/*
 * Search for a property in both the top level and in a VEVENT subcomponent
 */
icalproperty *ical_ctdl_get_subprop(
		icalcomponent *cal,
		icalproperty_kind which_prop
) {
	icalproperty *p;
	icalcomponent *c;

	p = icalcomponent_get_first_property(cal, which_prop);
	if (p == NULL) {
		c = icalcomponent_get_first_component(cal,
							ICAL_VEVENT_COMPONENT);
		if (c != NULL) {
			p = icalcomponent_get_first_property(c, which_prop);
		}
	}
	return p;
}


/*
 * Check to see if two events overlap.  Returns nonzero if they do.
 */
int ical_ctdl_is_overlap(
			struct icaltimetype t1start,
			struct icaltimetype t1end,
			struct icaltimetype t2start,
			struct icaltimetype t2end
) {

	if (icaltime_is_null_time(t1start)) return(0);
	if (icaltime_is_null_time(t2start)) return(0);

	/* First, check for all-day events */
	if (t1start.is_date) {
		if (!icaltime_compare_date_only(t1start, t2start)) {
			return(1);
		}
		if (!icaltime_is_null_time(t2end)) {
			if (!icaltime_compare_date_only(t1start, t2end)) {
				return(1);
			}
		}
	}

	if (t2start.is_date) {
		if (!icaltime_compare_date_only(t2start, t1start)) {
			return(1);
		}
		if (!icaltime_is_null_time(t1end)) {
			if (!icaltime_compare_date_only(t2start, t1end)) {
				return(1);
			}
		}
	}

	/* Now check for overlaps using date *and* time. */

	/* First, bail out if either event 1 or event 2 is missing end time. */
	if (icaltime_is_null_time(t1end)) return(0);
	if (icaltime_is_null_time(t2end)) return(0);

	/* If event 1 ends before event 2 starts, we're in the clear. */
	if (icaltime_compare(t1end, t2start) <= 0) return(0);

	/* If event 2 ends before event 1 starts, we're also ok. */
	if (icaltime_compare(t2end, t1start) <= 0) return(0);

	/* Otherwise, they overlap. */
	return(1);
}



/*
 * Backend for ical_hunt_for_conflicts()
 */
void ical_hunt_for_conflicts_backend(long msgnum, void *data) {
	icalcomponent *cal;
	struct CtdlMessage *msg;
	struct ical_respond_data ird;
	struct icaltimetype t1start, t1end, t2start, t2end;
	icalproperty *p;
	char conflict_event_uid[SIZ];
	char conflict_event_summary[SIZ];

	cal = (icalcomponent *)data;
	strcpy(conflict_event_uid, "");
	strcpy(conflict_event_summary, "");

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "1");	/* hopefully it's always 1 */
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_part,		/* callback function */
		NULL, NULL,
		(void *) &ird,			/* user data */
		0
	);
	CtdlFreeMessage(msg);

	if (ird.cal == NULL) return;

	t1start = icaltime_null_time();
	t1end = icaltime_null_time();
	t2start = icaltime_null_time();
	t1end = icaltime_null_time();

	/* Now compare cal to ird.cal */
	p = ical_ctdl_get_subprop(ird.cal, ICAL_DTSTART_PROPERTY);
	if (p == NULL) return;
	if (p != NULL) t2start = icalproperty_get_dtstart(p);
	
	p = ical_ctdl_get_subprop(ird.cal, ICAL_DTEND_PROPERTY);
	if (p != NULL) t2end = icalproperty_get_dtend(p);

	p = ical_ctdl_get_subprop(cal, ICAL_DTSTART_PROPERTY);
	if (p == NULL) return;
	if (p != NULL) t1start = icalproperty_get_dtstart(p);
	
	p = ical_ctdl_get_subprop(cal, ICAL_DTEND_PROPERTY);
	if (p != NULL) t1end = icalproperty_get_dtend(p);
	
	p = ical_ctdl_get_subprop(ird.cal, ICAL_UID_PROPERTY);
	if (p != NULL) {
		strcpy(conflict_event_uid, icalproperty_get_comment(p));
	}
	p = ical_ctdl_get_subprop(ird.cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		strcpy(conflict_event_summary, icalproperty_get_comment(p));
	}


	icalcomponent_free(ird.cal);

	if (ical_ctdl_is_overlap(t1start, t1end, t2start, t2end)) {
		cprintf("%ld||%s|%s|\n",
			msgnum,
			conflict_event_uid,
			conflict_event_summary
		);
	}
}



/* 
 * Phase 2 of "hunt for conflicts" operation.
 * At this point we have a calendar object which represents the VEVENT that
 * we're considering adding to the calendar.  Now hunt through the user's
 * calendar room, and output zero or more existing VEVENTs which conflict
 * with this one.
 */
void ical_hunt_for_conflicts(icalcomponent *cal) {
        char hold_rm[ROOMNAMELEN];

        strcpy(hold_rm, CC->quickroom.QRname);	/* save current room */

        if (getroom(&CC->quickroom, USERCALENDARROOM) != 0) {
                getroom(&CC->quickroom, hold_rm);
		cprintf("%d You do not have a calendar.\n", ERROR);
		return;
        }

	cprintf("%d Conflicting events:\n", LISTING_FOLLOWS);

        CtdlForEachMessage(MSGS_ALL, 0, "text/calendar",
		NULL,
		ical_hunt_for_conflicts_backend,
		(void *) cal
	);

	cprintf("000\n");
        getroom(&CC->quickroom, hold_rm);	/* return to saved room */

}



/*
 * Hunt for conflicts (Phase 1 -- retrieve the object and call Phase 2)
 */
void ical_conflicts(long msgnum, char *partnum) {
	struct CtdlMessage *msg;
	struct ical_respond_data ird;

	msg = CtdlFetchMessage(msgnum);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n",
			ERROR+ILLEGAL_VALUE,
			(long)msgnum
		);
		return;
	}

	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, partnum);
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_part,		/* callback function */
		NULL, NULL,
		(void *) &ird,			/* user data */
		0
	);

	CtdlFreeMessage(msg);

	if (ird.cal != NULL) {
		ical_hunt_for_conflicts(ird.cal);
		icalcomponent_free(ird.cal);
		return;
	}
	else {
		cprintf("%d No calendar object found\n", ERROR);
		return;
	}

	/* should never get here */
}




/*
 * All Citadel calendar commands from the client come through here.
 */
void cmd_ical(char *argbuf)
{
	char subcmd[SIZ];
	long msgnum;
	char partnum[SIZ];
	char action[SIZ];

	if (CtdlAccessCheck(ac_logged_in)) return;

	extract(subcmd, argbuf, 0);

	if (!strcmp(subcmd, "test")) {
		cprintf("%d This server supports calendaring\n", CIT_OK);
		return;
	}

	else if (!strcmp(subcmd, "respond")) {
		msgnum = extract_long(argbuf, 1);
		extract(partnum, argbuf, 2);
		extract(action, argbuf, 3);
		ical_respond(msgnum, partnum, action);
	}

	else if (!strcmp(subcmd, "conflicts")) {
		msgnum = extract_long(argbuf, 1);
		extract(partnum, argbuf, 2);
		ical_conflicts(msgnum, partnum);
	}

	else {
		cprintf("%d Invalid subcommand\n", ERROR+CMD_NOT_SUPPORTED);
		return;
	}

	/* should never get here */
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



/*
 * Back end for ical_obj_beforesave()
 * This hunts for the UID of the calendar event.
 */
void ical_ctdl_set_extended_msgid(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, size_t length,
		char *encoding, void *cbuserdata)
{
	icalcomponent *cal;
	icalproperty *p;

	/* If this is a text/calendar object, hunt for the UID and drop it in
	 * the "user data" pointer for the MIME parser.  When
	 * ical_obj_beforesave() sees it there, it'll set the Extended msgid
	 * to that string.
	 */
	if (!strcasecmp(cbtype, "text/calendar")) {
		cal = icalcomponent_new_from_string(content);
		if (cal != NULL) {
			p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
			if (p != NULL) {
				strcpy((char *)cbuserdata,
					icalproperty_get_comment(p)
				);
			}
			icalcomponent_free(cal);
		}
	}
}





/*
 * See if we need to prevent the object from being saved (we don't allow
 * MIME types other than text/calendar in the Calendar> room).  Also, when
 * saving an event to the calendar, set the message's Citadel extended message
 * ID to the UID of the object.  This causes our replication checker to
 * automatically delete any existing instances of the same object.  (Isn't
 * that cool?)
 */
int ical_obj_beforesave(struct CtdlMessage *msg)
{
	char roomname[ROOMNAMELEN];
	char *p;
	int a;
	char eidbuf[SIZ];

	/*
	 * Only messages with content-type text/calendar
	 * may be saved to Calendar>.  If the message is bound for
	 * Calendar> but doesn't have this content-type, throw an error
	 * so that the message may not be posted.
	 */

	/* First determine if this is our room */
	MailboxName(roomname, sizeof roomname, &CC->usersupp, USERCALENDARROOM);
	if (strcasecmp(roomname, CC->quickroom.QRname)) {
		return 0;	/* It's not the Calendar room. */
	}

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
			if (!strncasecmp(p + 14, "text/calendar", 13)) {
				strcpy(eidbuf, "");
				mime_parser(msg->cm_fields['M'],
					NULL,
					*ical_ctdl_set_extended_msgid,
					NULL, NULL,
					(void *)eidbuf,
					0
				);
				if (strlen(eidbuf) > 0) {
					if (msg->cm_fields['E'] != NULL) {
						phree(msg->cm_fields['E']);
					}
					msg->cm_fields['E'] = strdoop(eidbuf);
				}
				return 0;
			}
			else {
				return 1;
			}
		}
		p++;
	}
	
	/* Oops!  No Content-Type in this message!  How'd that happen? */
	lprintf(7, "RFC822 message with no Content-Type header!\n");
	return 1;
}


#endif	/* HAVE_ICAL_H */

/*
 * Register this module with the Citadel server.
 */
char *Dynamic_Module_Init(void)
{
#ifdef HAVE_ICAL_H
	CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
	CtdlRegisterSessionHook(ical_create_room, EVT_LOGIN);
	CtdlRegisterProtoHook(cmd_ical, "ICAL", "Citadel iCal commands");
#endif
	return "$Id$";
}
