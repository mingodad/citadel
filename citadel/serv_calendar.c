/* 
 * $Id$ 
 *
 * This module implements iCalendar object processing and the Calendar>
 * room on a Citadel/UX server.  It handles iCalendar objects using the
 * iTIP protocol.  See RFCs 2445 and 2446.
 *
 */

#define PRODID "-//Citadel//NONSGML Citadel Calendar//EN"

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
 * Write a calendar object into the specified user's calendar room.
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
 * Send a reply to a meeting invitation.
 *
 * 'request' is the invitation to reply to.
 * 'action' is the string "accept" or "decline".
 *
 * (Sorry about this being more than 80 columns ... there was just
 * no easy way to break it down sensibly.)
 */
void ical_send_a_reply(icalcomponent *request, char *action) {
	icalcomponent *the_reply = NULL;
	icalcomponent *vevent = NULL;
	icalproperty *attendee = NULL;
	char attendee_string[SIZ];
	icalproperty *organizer = NULL;
	char organizer_string[SIZ];
	icalproperty *summary = NULL;
	char summary_string[SIZ];
	icalproperty *me_attend = NULL;
	struct recptypes *recp = NULL;
	icalparameter *partstat = NULL;
	char *serialized_reply = NULL;
	char *reply_message_text = NULL;
	struct CtdlMessage *msg = NULL;
	struct recptypes *valid = NULL;

	strcpy(organizer_string, "");
	strcpy(summary_string, "Calendar item");

	if (request == NULL) {
		lprintf(3, "ERROR: trying to reply to NULL event?\n");
		return;
	}

	the_reply = icalcomponent_new_clone(request);
	if (the_reply == NULL) {
		lprintf(3, "ERROR: cannot clone request\n");
		return;
	}

	/* Change the method from REQUEST to REPLY */
	icalcomponent_set_method(the_reply, ICAL_METHOD_REPLY);

	vevent = icalcomponent_get_first_component(the_reply, ICAL_VEVENT_COMPONENT);
	if (vevent != NULL) {
		/* Hunt for attendees, removing ones that aren't us.
		 * (Actually, remove them all, cloning our own one so we can
		 * re-insert it later)
		 */
		while (attendee = icalcomponent_get_first_property(vevent,
		    ICAL_ATTENDEE_PROPERTY), (attendee != NULL)
		) {
			if (icalproperty_get_attendee(attendee)) {
				strcpy(attendee_string,
					icalproperty_get_attendee(attendee) );
				if (!strncasecmp(attendee_string, "MAILTO:", 7)) {
					strcpy(attendee_string, &attendee_string[7]);
					striplt(attendee_string);
					recp = validate_recipients(attendee_string);
					if (recp != NULL) {
						if (!strcasecmp(recp->recp_local, CC->usersupp.fullname)) {
							if (me_attend) icalproperty_free(me_attend);
							me_attend = icalproperty_new_clone(attendee);
						}
						phree(recp);
					}
				}
			}
			/* Remove it... */
			icalcomponent_remove_property(vevent, attendee);
			icalproperty_free(attendee);
		}

		/* We found our own address in the attendee list. */
		if (me_attend) {
			/* Change the partstat from NEEDS-ACTION to ACCEPT or DECLINE */
			icalproperty_remove_parameter(me_attend, ICAL_PARTSTAT_PARAMETER);

			if (!strcasecmp(action, "accept")) {
				partstat = icalparameter_new_partstat(ICAL_PARTSTAT_ACCEPTED);
			}
			else if (!strcasecmp(action, "decline")) {
				partstat = icalparameter_new_partstat(ICAL_PARTSTAT_DECLINED);
			}
			else if (!strcasecmp(action, "tentative")) {
				partstat = icalparameter_new_partstat(ICAL_PARTSTAT_TENTATIVE);
			}

			if (partstat) icalproperty_add_parameter(me_attend, partstat);

			/* Now insert it back into the vevent. */
			icalcomponent_add_property(vevent, me_attend);
		}

		/* Figure out who to send this thing to */
		organizer = icalcomponent_get_first_property(vevent, ICAL_ORGANIZER_PROPERTY);
		if (organizer != NULL) {
			if (icalproperty_get_organizer(organizer)) {
				strcpy(organizer_string,
					icalproperty_get_organizer(organizer) );
			}
		}
		if (!strncasecmp(organizer_string, "MAILTO:", 7)) {
			strcpy(organizer_string, &organizer_string[7]);
			striplt(organizer_string);
		} else {
			strcpy(organizer_string, "");
		}

		/* Extract the summary string -- we'll use it as the
		 * message subject for the reply
		 */
		summary = icalcomponent_get_first_property(vevent, ICAL_SUMMARY_PROPERTY);
		if (summary != NULL) {
			if (icalproperty_get_summary(summary)) {
				strcpy(summary_string,
					icalproperty_get_summary(summary) );
			}
		}

	}

	/* Now generate the reply message and send it out. */
	serialized_reply = strdoop(icalcomponent_as_ical_string(the_reply));
	icalcomponent_free(the_reply);	/* don't need this anymore */
	if (serialized_reply == NULL) return;

	reply_message_text = mallok(strlen(serialized_reply) + SIZ);
	if (reply_message_text != NULL) {
		sprintf(reply_message_text,
			"Content-type: text/calendar\r\n\r\n%s\r\n",
			serialized_reply
		);

		msg = CtdlMakeMessage(&CC->usersupp, organizer_string,
			CC->quickroom.QRname, 0, FMT_RFC822,
			"",
			summary_string,		/* Use summary for subject */
			reply_message_text);
	
		if (msg != NULL) {
			valid = validate_recipients(organizer_string);
			CtdlSubmitMsg(msg, valid, "");
			CtdlFreeMessage(msg);
		}
	}
	phree(serialized_reply);
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

	/* We're done with the incoming message, because we now have a
	 * calendar object in memory.
	 */
	CtdlFreeMessage(msg);

	/*
	 * Here is the real meat of this function.  Handle the event.
	 */
	if (ird.cal != NULL) {
		/* Save this in the user's calendar if necessary */
		if (!strcasecmp(action, "accept")) {
			ical_add(ird.cal, 0);
		}

		/* Send a reply if necessary */
		if (icalcomponent_get_method(ird.cal) == ICAL_METHOD_REQUEST) {
			ical_send_a_reply(ird.cal, action);
		}

		/* Now that we've processed this message, we don't need it
		 * anymore.  So delete it.
		 */
		CtdlDeleteMessages(CC->quickroom.QRname, msgnum, "");

		/* Free the memory we allocated and return a response. */
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
	char compare_uid[SIZ];

	cal = (icalcomponent *)data;
	strcpy(compare_uid, "");
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
	
	p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
	if (p != NULL) {
		strcpy(compare_uid, icalproperty_get_comment(p));
	}

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
		cprintf("%ld||%s|%s|%d|\n",
			msgnum,
			conflict_event_uid,
			conflict_event_summary,
			(	((strlen(compare_uid)>0)
				&&(!strcasecmp(compare_uid,
				conflict_event_uid))) ? 1 : 0
			)
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
 * ical_send_out_invitations() is called by ical_saving_vevent() when it
 * finds a VEVENT.   FIXME ... finish implementing.
 */
void ical_send_out_invitations(icalcomponent *cal) {
	icalcomponent *the_request = NULL;
	char *serialized_request = NULL;
	char *request_message_text = NULL;
	struct CtdlMessage *msg = NULL;
	struct recptypes *valid = NULL;
	char attendees_string[SIZ];
	char this_attendee[SIZ];
	icalproperty *attendee = NULL;
	char summary_string[SIZ];
	icalproperty *summary = NULL;
	icalcomponent *encaps = NULL;

	if (cal == NULL) {
		lprintf(3, "ERROR: trying to reply to NULL event?\n");
		return;
	}

	/* Clone the event */
	the_request = icalcomponent_new_clone(cal);
	if (the_request == NULL) {
		lprintf(3, "ERROR: cannot clone calendar object\n");
		return;
	}

	/* Extract the summary string -- we'll use it as the
	 * message subject for the request
	 */
	strcpy(summary_string, "Meeting request");
	summary = icalcomponent_get_first_property(the_request, ICAL_SUMMARY_PROPERTY);
	if (summary != NULL) {
		if (icalproperty_get_summary(summary)) {
			strcpy(summary_string,
				icalproperty_get_summary(summary) );
		}
	}

	/* Determine who the recipients of this message are (the attendees) */
	strcpy(attendees_string, "");
	for (attendee = icalcomponent_get_first_property(the_request, ICAL_ATTENDEE_PROPERTY); attendee != NULL; attendee = icalcomponent_get_next_property(the_request, ICAL_ATTENDEE_PROPERTY)) {
		if (icalproperty_get_attendee(attendee)) {
			strcpy(this_attendee, icalproperty_get_attendee(attendee) );
			if (!strncasecmp(this_attendee, "MAILTO:", 7)) {
				strcpy(this_attendee, &this_attendee[7]);
				snprintf(&attendees_string[strlen(attendees_string)],
					sizeof(attendees_string) - strlen(attendees_string),
					"%s, ",
					this_attendee
				);
			}
		}
	}

	lprintf(9, "attendees_string: <%s>\n", attendees_string);

	/* Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
	if (encaps == NULL) {
		lprintf(3, "Error at %s:%d - could not allocate component!\n",
			__FILE__, __LINE__);
		icalcomponent_free(the_request);
		return;
	}

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/* Set the method to REQUEST */
	icalcomponent_set_method(encaps, ICAL_METHOD_REQUEST);

	/* FIXME: here we need to insert a VTIMEZONE object. */

	/* Here we go: put the VEVENT into the VCALENDAR.  We now no longer
	 * are responsible for "the_request"'s memory -- it will be freed
	 * when we free "encaps".
	 */
	icalcomponent_add_component(encaps, the_request);

	/* Serialize it */
	serialized_request = strdoop(icalcomponent_as_ical_string(encaps));
	icalcomponent_free(encaps);	/* Don't need this anymore. */
	if (serialized_request == NULL) return;

	request_message_text = mallok(strlen(serialized_request) + SIZ);
	if (request_message_text != NULL) {
		sprintf(request_message_text,
			"Content-type: text/calendar\r\n\r\n%s\r\n",
			serialized_request
		);

		msg = CtdlMakeMessage(&CC->usersupp,
			"",			/* No single recipient here */
			CC->quickroom.QRname, 0, FMT_RFC822,
			"",
			summary_string,		/* Use summary for subject */
			request_message_text);
	
		if (msg != NULL) {
			valid = validate_recipients(attendees_string);
			CtdlSubmitMsg(msg, valid, "");
			CtdlFreeMessage(msg);
		}
	}
	phree(serialized_request);
}


/*
 * When a calendar object is being saved, determine whether it's a VEVENT
 * and the user saving it is the organizer.  If so, send out invitations
 * to any listed attendees.
 *
 */
void ical_saving_vevent(icalcomponent *cal) {
	icalcomponent *c;
	icalproperty *organizer = NULL;
	char organizer_string[SIZ];

	strcpy(organizer_string, "");
	/*
 	 * The VEVENT subcomponent is the one we're interested in.
	 * Send out invitations if, and only if, this user is the Organizer.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {
		organizer = icalcomponent_get_first_property(cal,
						ICAL_ORGANIZER_PROPERTY);
		if (organizer != NULL) {
			if (icalproperty_get_organizer(organizer)) {
				strcpy(organizer_string,
					icalproperty_get_organizer(organizer));
			}
		}
		if (!strncasecmp(organizer_string, "MAILTO:", 7)) {
			strcpy(organizer_string, &organizer_string[7]);
			striplt(organizer_string);
			/*
			 * If the user saving the event is listed as the
			 * organizer, then send out invitations.
			 */
			if (CtdlIsMe(organizer_string)) {
				ical_send_out_invitations(cal);
			}
		}
	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != NULL);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		ical_saving_vevent(c);
	}

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


/*
 * Things we need to do after saving a calendar event.
 */
void ical_obj_aftersave_backend(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, size_t length,
		char *encoding, void *cbuserdata)
{
	icalcomponent *cal;

	/* If this is a text/calendar object, hunt for the UID and drop it in
	 * the "user data" pointer for the MIME parser.  When
	 * ical_obj_beforesave() sees it there, it'll set the Extended msgid
	 * to that string.
	 */
	if (!strcasecmp(cbtype, "text/calendar")) {
		cal = icalcomponent_new_from_string(content);
		if (cal != NULL) {
			ical_saving_vevent(cal);
			icalcomponent_free(cal);
		}
	}
}


/* 
 * Things we need to do after saving a calendar event.
 */
int ical_obj_aftersave(struct CtdlMessage *msg)
{
	char roomname[ROOMNAMELEN];
	char *p;
	int a;

	/*
	 * If this isn't the Calendar> room, no further action is necessary.
	 */

	/* First determine if this is our room */
	MailboxName(roomname, sizeof roomname, &CC->usersupp, USERCALENDARROOM);
	if (strcasecmp(roomname, CC->quickroom.QRname)) {
		return 0;	/* It's not the Calendar room. */
	}

	/* Then determine content-type of the message */
	
	/* It must be an RFC822 message! */
	/* FIXME: Not handling MIME multipart messages; implement with IMIP */
	if (msg->cm_format_type != 4) return(1);
	
	/* Find the Content-Type: header */
	p = msg->cm_fields['M'];
	a = strlen(p);
	while (--a > 0) {
		if (!strncasecmp(p, "Content-Type: ", 14)) {	/* Found it */
			if (!strncasecmp(p + 14, "text/calendar", 13)) {
				mime_parser(msg->cm_fields['M'],
					NULL,
					*ical_obj_aftersave_backend,
					NULL, NULL,
					NULL,
					0
				);
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
	CtdlRegisterMessageHook(ical_obj_aftersave, EVT_AFTERSAVE);
	CtdlRegisterSessionHook(ical_create_room, EVT_LOGIN);
	CtdlRegisterProtoHook(cmd_ical, "ICAL", "Citadel iCal commands");
#endif
	return "$Id$";
}
