/* 
 * $Id$ 
 *
 * This module implements iCalendar object processing and the Calendar>
 * room on a Citadel server.  It handles iCalendar objects using the
 * iTIP protocol.  See RFCs 2445 and 2446.
 *
 */

#define PRODID "-//Citadel//NONSGML Citadel Calendar//EN"

#include "sysdep.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif
#include <libcitadel.h>
#include "citadel.h"
#include "server.h"
#include "citserver.h"
#include "support.h"
#include "config.h"
#include "user_ops.h"
#include "room_ops.h"
#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_calendar.h"
#include "euidindex.h"
#include "ctdl_module.h"

#ifdef CITADEL_WITH_CALENDAR_SERVICE

#include <ical.h>
#include "ical_dezonify.h"



struct ical_respond_data {
	char desired_partnum[SIZ];
	icalcomponent *cal;
};


/*
 * Utility function to create a new VCALENDAR component with some of the
 * required fields already set the way we like them.
 */
icalcomponent *icalcomponent_new_citadel_vcalendar(void) {
	icalcomponent *encaps;

	encaps = icalcomponent_new_vcalendar();
	if (encaps == NULL) {
		lprintf(CTDL_CRIT, "Error at %s:%d - could not allocate component!\n",
			__FILE__, __LINE__);
		return NULL;
	}

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	return(encaps);
}


/*
 * Utility function to encapsulate a subcomponent into a full VCALENDAR
 */
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp) {
	icalcomponent *encaps;

	/* If we're already looking at a full VCALENDAR component,
	 * don't bother ... just return itself.
	 */
	if (icalcomponent_isa(subcomp) == ICAL_VCALENDAR_COMPONENT) {
		return subcomp;
	}

	/* Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new_citadel_vcalendar();
	if (encaps == NULL) return NULL;

	/* Encapsulate the subcomponent inside */
	icalcomponent_add_component(encaps, subcomp);

	/* Convert all timestamps to UTC so we don't have to deal with
	 * stupid VTIMEZONE crap.
	 */
	ical_dezonify(encaps);

	/* Return the object we just created. */
	return(encaps);
}




/*
 * Write a calendar object into the specified user's calendar room.
 * If the supplied user is NULL, this function writes the calendar object
 * to the currently selected room.
 */
void ical_write_to_cal(struct ctdluser *u, icalcomponent *cal) {
	char temp[PATH_MAX];
	FILE *fp = NULL;
	char *ser = NULL;
	icalcomponent *encaps = NULL;
	struct CtdlMessage *msg = NULL;
	icalcomponent *tmp=NULL;

	if (cal == NULL) return;

	/* If the supplied object is a subcomponent, encapsulate it in
	 * a full VCALENDAR component, and save that instead.
	 */
	if (icalcomponent_isa(cal) != ICAL_VCALENDAR_COMPONENT) {
		tmp = icalcomponent_new_clone(cal);
		encaps = ical_encapsulate_subcomponent(tmp);
		ical_write_to_cal(u, encaps);
		icalcomponent_free(tmp);
		icalcomponent_free(encaps);
		return;
	}

	ser = icalcomponent_as_ical_string(cal);
	if (ser == NULL) return;

	/* If the caller supplied a user, write to that user's default calendar room */
	if (u) {
		/* Make a temp file out of it */
		CtdlMakeTempFileName(temp, sizeof temp);
		fp = fopen(temp, "w");
		if (fp != NULL) {
			fwrite(ser, strlen(ser), 1, fp);
			fclose(fp);
		
			/* This handy API function does all the work for us. */
			CtdlWriteObject(USERCALENDARROOM,	/* which room */
				"text/calendar",	/* MIME type */
				temp,			/* temp file */
				u,			/* which user */
				0,			/* not binary */
				0,			/* don't delete others of this type */
				0			/* no flags */
			);
			unlink(temp);
		}
	}

	/* If the caller did not supply a user, write to the currently selected room */
	if (!u) {
		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = 4;
		msg->cm_fields['A'] = strdup(CC->user.fullname);
		msg->cm_fields['O'] = strdup(CC->room.QRname);
		msg->cm_fields['N'] = strdup(config.c_nodename);
		msg->cm_fields['H'] = strdup(config.c_humannode);
		msg->cm_fields['M'] = malloc(strlen(ser) + 40);
		strcpy(msg->cm_fields['M'], "Content-type: text/calendar\r\n\r\n");
		strcat(msg->cm_fields['M'], ser);
	
		/* Now write the data */
		CtdlSubmitMsg(msg, NULL, "");
		CtdlFreeMessage(msg);
	}

	/* In either case, now we can free the serialized calendar object */
//	free(ser);
}


/*
 * Add a calendar object to the user's calendar
 * 
 * ok because it uses ical_write_to_cal()
 */
void ical_add(icalcomponent *cal, int recursion_level) {
	icalcomponent *c;

	/*
 	 * The VEVENT subcomponent is the one we're interested in saving.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {
	
		ical_write_to_cal(&CC->user, cal);

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
 * 'action' is the string "accept" or "decline" or "tentative".
 *
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
		lprintf(CTDL_ERR, "ERROR: trying to reply to NULL event?\n");
		return;
	}

	the_reply = icalcomponent_new_clone(request);
	if (the_reply == NULL) {
		lprintf(CTDL_ERR, "ERROR: cannot clone request\n");
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
						if (!strcasecmp(recp->recp_local, CC->user.fullname)) {
							if (me_attend) icalproperty_free(me_attend);
							me_attend = icalproperty_new_clone(attendee);
						}
						free_recipients(recp);
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
	serialized_reply = strdup(icalcomponent_as_ical_string(the_reply));
	icalcomponent_free(the_reply);	/* don't need this anymore */
	if (serialized_reply == NULL) return;

	reply_message_text = malloc(strlen(serialized_reply) + SIZ);
	if (reply_message_text != NULL) {
		sprintf(reply_message_text,
			"Content-type: text/calendar charset=\"utf-8\"\r\n\r\n%s\r\n",
			serialized_reply
		);

		msg = CtdlMakeMessage(&CC->user,
			organizer_string,	/* to */
			"",			/* cc */
			CC->room.QRname, 0, FMT_RFC822,
			"",
			"",
			summary_string,		/* Use summary for subject */
			NULL,
			reply_message_text);
	
		if (msg != NULL) {
			valid = validate_recipients(organizer_string);
			CtdlSubmitMsg(msg, valid, "");
			CtdlFreeMessage(msg);
			free_recipients(valid);
		}
	}
	free(serialized_reply);
}



/*
 * Callback function for mime parser that hunts for calendar content types
 * and turns them into calendar objects
 */
void ical_locate_part(char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		void *cbuserdata) {

	struct ical_respond_data *ird = NULL;

	ird = (struct ical_respond_data *) cbuserdata;

	/* desired_partnum can be set to "_HUNT_" to have it just look for
	 * the first part with a content type of text/calendar.  Otherwise
	 * we have to only process the right one.
	 */
	if (strcasecmp(ird->desired_partnum, "_HUNT_")) {
		if (strcasecmp(partnum, ird->desired_partnum)) {
			return;
		}
	}

	if (strcasecmp(cbtype, "text/calendar")) {
		return;
	}

	if (ird->cal != NULL) {
		icalcomponent_free(ird->cal);
		ird->cal = NULL;
	}

	ird->cal = icalcomponent_new_from_string(content);
	if (ird->cal != NULL) {
		ical_dezonify(ird->cal);
	}
}


/*
 * Respond to a meeting request.
 */
void ical_respond(long msgnum, char *partnum, char *action) {
	struct CtdlMessage *msg = NULL;
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

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n",
			ERROR + ILLEGAL_VALUE,
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
		 * anymore.  So delete it.  (NOTE we don't do this anymore.)
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
		 */

		/* Free the memory we allocated and return a response. */
		icalcomponent_free(ird.cal);
		ird.cal = NULL;
		cprintf("%d ok\n", CIT_OK);
		return;
	}
	else {
		cprintf("%d No calendar object found\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	/* should never get here */
}


/*
 * Figure out the UID of the calendar event being referred to in a
 * REPLY object.  This function is recursive.
 */
void ical_learn_uid_of_reply(char *uidbuf, icalcomponent *cal) {
	icalcomponent *subcomponent;
	icalproperty *p;

	/* If this object is a REPLY, then extract the UID. */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {
		p = icalcomponent_get_first_property(cal, ICAL_UID_PROPERTY);
		if (p != NULL) {
			strcpy(uidbuf, icalproperty_get_comment(p));
		}
	}

	/* Otherwise, recurse through any VEVENT subcomponents.  We do NOT want the
	 * UID of the reply; we want the UID of the invitation being replied to.
	 */
	for (subcomponent = icalcomponent_get_first_component(cal, ICAL_VEVENT_COMPONENT);
	    subcomponent != NULL;
	    subcomponent = icalcomponent_get_next_component(cal, ICAL_VEVENT_COMPONENT) ) {
		ical_learn_uid_of_reply(uidbuf, subcomponent);
	}
}


/*
 * ical_update_my_calendar_with_reply() refers to this callback function; when we
 * locate the message containing the calendar event we're replying to, this function
 * gets called.  It basically just sticks the message number in a supplied buffer.
 */
void ical_hunt_for_event_to_update(long msgnum, void *data) {
	long *msgnumptr;

	msgnumptr = (long *) data;
	*msgnumptr = msgnum;
}


struct original_event_container {
	icalcomponent *c;
};

/*
 * Callback function for mime parser that hunts for calendar content types
 * and turns them into calendar objects (called by ical_update_my_calendar_with_reply()
 * to fetch the object being updated)
 */
void ical_locate_original_event(char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		void *cbuserdata) {

	struct original_event_container *oec = NULL;

	if (strcasecmp(cbtype, "text/calendar")) {
		return;
	}
	oec = (struct original_event_container *) cbuserdata;
	if (oec->c != NULL) {
		icalcomponent_free(oec->c);
	}
	oec->c = icalcomponent_new_from_string(content);
}


/*
 * Merge updated attendee information from a REPLY into an existing event.
 */
void ical_merge_attendee_reply(icalcomponent *event, icalcomponent *reply) {
	icalcomponent *c;
	icalproperty *e_attendee, *r_attendee;

	/* First things first.  If we're not looking at a VEVENT component,
	 * recurse through subcomponents until we find one.
	 */
	if (icalcomponent_isa(event) != ICAL_VEVENT_COMPONENT) {
		for (c = icalcomponent_get_first_component(event, ICAL_VEVENT_COMPONENT);
		    c != NULL;
		    c = icalcomponent_get_next_component(event, ICAL_VEVENT_COMPONENT) ) {
			ical_merge_attendee_reply(c, reply);
		}
		return;
	}

	/* Now do the same thing with the reply.
	 */
	if (icalcomponent_isa(reply) != ICAL_VEVENT_COMPONENT) {
		for (c = icalcomponent_get_first_component(reply, ICAL_VEVENT_COMPONENT);
		    c != NULL;
		    c = icalcomponent_get_next_component(reply, ICAL_VEVENT_COMPONENT) ) {
			ical_merge_attendee_reply(event, c);
		}
		return;
	}

	/* Clone the reply, because we're going to rip its guts out. */
	reply = icalcomponent_new_clone(reply);

	/* At this point we're looking at the correct subcomponents.
	 * Iterate through the attendees looking for a match.
	 */
STARTOVER:
	for (e_attendee = icalcomponent_get_first_property(event, ICAL_ATTENDEE_PROPERTY);
	    e_attendee != NULL;
	    e_attendee = icalcomponent_get_next_property(event, ICAL_ATTENDEE_PROPERTY)) {

		for (r_attendee = icalcomponent_get_first_property(reply, ICAL_ATTENDEE_PROPERTY);
		    r_attendee != NULL;
		    r_attendee = icalcomponent_get_next_property(reply, ICAL_ATTENDEE_PROPERTY)) {

			/* Check to see if these two attendees match...
			 */
			if (!strcasecmp(
			   icalproperty_get_attendee(e_attendee),
			   icalproperty_get_attendee(r_attendee)
			)) {
				/* ...and if they do, remove the attendee from the event
				 * and replace it with the attendee from the reply.  (The
				 * reply's copy will have the same address, but an updated
				 * status.)
				 */
				icalcomponent_remove_property(event, e_attendee);
				icalproperty_free(e_attendee);
				icalcomponent_remove_property(reply, r_attendee);
				icalcomponent_add_property(event, r_attendee);

				/* Since we diddled both sets of attendees, we have to start
				 * the iteration over again.  This will not create an infinite
				 * loop because we removed the attendee from the reply.  (That's
				 * why we cloned the reply, and that's what we mean by "ripping
				 * its guts out.")
				 */
				goto STARTOVER;
			}
	
		}
	}

	/* Free the *clone* of the reply. */
	icalcomponent_free(reply);
}




/*
 * Handle an incoming RSVP (object with method==ICAL_METHOD_REPLY) for a
 * calendar event.  The object has already been deserialized for us; all
 * we have to do here is hunt for the event in our calendar, merge in the
 * updated attendee status, and save it again.
 *
 * This function returns 0 on success, 1 if the event was not found in the
 * user's calendar, or 2 if an internal error occurred.
 */
int ical_update_my_calendar_with_reply(icalcomponent *cal) {
	char uid[SIZ];
	char hold_rm[ROOMNAMELEN];
	long msgnum_being_replaced = 0;
	struct CtdlMessage *msg = NULL;
	struct original_event_container oec;
	icalcomponent *original_event;
	char *serialized_event = NULL;
	char roomname[ROOMNAMELEN];
	char *message_text = NULL;

	/* Figure out just what event it is we're dealing with */
	strcpy(uid, "--==<< InVaLiD uId >>==--");
	ical_learn_uid_of_reply(uid, cal);
	lprintf(CTDL_DEBUG, "UID of event being replied to is <%s>\n", uid);

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (getroom(&CC->room, USERCALENDARROOM) != 0) {
		getroom(&CC->room, hold_rm);
		lprintf(CTDL_CRIT, "cannot get user calendar room\n");
		return(2);
	}

	/*
	 * Look in the EUID index for a message with
	 * the Citadel EUID set to the value we're looking for.  Since
	 * Citadel always sets the message EUID to the vCalendar UID of
	 * the event, this will work.
	 */
	msgnum_being_replaced = locate_message_by_euid(uid, &CC->room);

	getroom(&CC->room, hold_rm);	/* return to saved room */

	lprintf(CTDL_DEBUG, "msgnum_being_replaced == %ld\n", msgnum_being_replaced);
	if (msgnum_being_replaced == 0) {
		return(1);			/* no calendar event found */
	}

	/* Now we know the ID of the message containing the event being updated.
	 * We don't actually have to delete it; that'll get taken care of by the
	 * server when we save another event with the same UID.  This just gives
	 * us the ability to load the event into memory so we can diddle the
	 * attendees.
	 */
	msg = CtdlFetchMessage(msgnum_being_replaced, 1);
	if (msg == NULL) {
		return(2);			/* internal error */
	}
	oec.c = NULL;
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_original_event,	/* callback function */
		NULL, NULL,
		&oec,				/* user data */
		0
	);
	CtdlFreeMessage(msg);

	original_event = oec.c;
	if (original_event == NULL) {
		lprintf(CTDL_ERR, "ERROR: Original_component is NULL.\n");
		return(2);
	}

	/* Merge the attendee's updated status into the event */
	ical_merge_attendee_reply(original_event, cal);

	/* Serialize it */
	serialized_event = strdup(icalcomponent_as_ical_string(original_event));
	icalcomponent_free(original_event);	/* Don't need this anymore. */
	if (serialized_event == NULL) return(2);

	MailboxName(roomname, sizeof roomname, &CC->user, USERCALENDARROOM);

	message_text = malloc(strlen(serialized_event) + SIZ);
	if (message_text != NULL) {
		sprintf(message_text,
			"Content-type: text/calendar charset=\"utf-8\"\r\n\r\n%s\r\n",
			serialized_event
		);

		msg = CtdlMakeMessage(&CC->user,
			"",			/* No recipient */
			"",			/* No recipient */
			roomname,
			0, FMT_RFC822,
			"",
			"",
			"",		/* no subject */
			NULL,
			message_text);
	
		if (msg != NULL) {
			CIT_ICAL->avoid_sending_invitations = 1;
			CtdlSubmitMsg(msg, NULL, roomname);
			CtdlFreeMessage(msg);
			CIT_ICAL->avoid_sending_invitations = 0;
		}
	}
	free(serialized_event);
	return(0);
}


/*
 * Handle an incoming RSVP for an event.  (This is the server subcommand part; it
 * simply extracts the calendar object from the message, deserializes it, and
 * passes it up to ical_update_my_calendar_with_reply() for processing.
 */
void ical_handle_rsvp(long msgnum, char *partnum, char *action) {
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;
	int ret;

	if (
	   (strcasecmp(action, "update"))
	   && (strcasecmp(action, "ignore"))
	) {
		cprintf("%d Action must be 'update' or 'ignore'\n",
			ERROR + ILLEGAL_VALUE
		);
		return;
	}

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n",
			ERROR + ILLEGAL_VALUE,
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
		/* Update the user's calendar if necessary */
		if (!strcasecmp(action, "update")) {
			ret = ical_update_my_calendar_with_reply(ird.cal);
			if (ret == 0) {
				cprintf("%d Your calendar has been updated with this reply.\n",
					CIT_OK);
			}
			else if (ret == 1) {
				cprintf("%d This event does not exist in your calendar.\n",
					ERROR + FILE_NOT_FOUND);
			}
			else {
				cprintf("%d An internal error occurred.\n",
					ERROR + INTERNAL_ERROR);
			}
		}
		else {
			cprintf("%d This reply has been ignored.\n", CIT_OK);
		}

		/* Now that we've processed this message, we don't need it
		 * anymore.  So delete it.  (Don't do this anymore.)
		CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
		 */

		/* Free the memory we allocated and return a response. */
		icalcomponent_free(ird.cal);
		ird.cal = NULL;
		return;
	}
	else {
		cprintf("%d No calendar object found\n", ERROR + ROOM_NOT_FOUND);
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
 * (This function is used in both Citadel and WebCit.  If you change it in
 * one place, change it in the other.  Better yet, put it in a library.)
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
	struct CtdlMessage *msg = NULL;
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

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "_HUNT_");
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

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (getroom(&CC->room, USERCALENDARROOM) != 0) {
		getroom(&CC->room, hold_rm);
		cprintf("%d You do not have a calendar.\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	cprintf("%d Conflicting events:\n", LISTING_FOLLOWS);

	CtdlForEachMessage(MSGS_ALL, 0, NULL,
		NULL,
		NULL,
		ical_hunt_for_conflicts_backend,
		(void *) cal
	);

	cprintf("000\n");
	getroom(&CC->room, hold_rm);	/* return to saved room */

}



/*
 * Hunt for conflicts (Phase 1 -- retrieve the object and call Phase 2)
 */
void ical_conflicts(long msgnum, char *partnum) {
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found.\n",
			ERROR + ILLEGAL_VALUE,
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
		cprintf("%d No calendar object found\n", ERROR + ROOM_NOT_FOUND);
		return;
	}

	/* should never get here */
}



/*
 * Look for busy time in a VEVENT and add it to the supplied VFREEBUSY.
 */
void ical_add_to_freebusy(icalcomponent *fb, icalcomponent *cal) {
	icalproperty *p;
	icalvalue *v;
	struct icalperiodtype my_period;

	if (cal == NULL) return;
	my_period = icalperiodtype_null_period();

	if (icalcomponent_isa(cal) != ICAL_VEVENT_COMPONENT) {
		ical_add_to_freebusy(fb,
			icalcomponent_get_first_component(
				cal, ICAL_VEVENT_COMPONENT
			)
		);
		return;
	}

	ical_dezonify(cal);

	/* If this event is not opaque, the user isn't publishing it as
	 * busy time, so don't bother doing anything else.
	 */
	p = icalcomponent_get_first_property(cal, ICAL_TRANSP_PROPERTY);
	if (p != NULL) {
		v = icalproperty_get_value(p);
		if (v != NULL) {
			if (icalvalue_get_transp(v) != ICAL_TRANSP_OPAQUE) {
				return;
			}
		}
	}

	/* Convert the DTSTART and DTEND properties to an icalperiod. */
	p = icalcomponent_get_first_property(cal, ICAL_DTSTART_PROPERTY);
	if (p != NULL) {
		my_period.start = icalproperty_get_dtstart(p);
	}

	p = icalcomponent_get_first_property(cal, ICAL_DTEND_PROPERTY);
	if (p != NULL) {
		my_period.end = icalproperty_get_dtstart(p);
	}

	/* Now add it. */
	icalcomponent_add_property(fb,
		icalproperty_new_freebusy(my_period)
	);

	/* Make sure the DTSTART property of the freebusy *list* is set to
	 * the DTSTART property of the *earliest event*.
	 */
	p = icalcomponent_get_first_property(fb, ICAL_DTSTART_PROPERTY);
	if (p == NULL) {
		icalcomponent_set_dtstart(fb,
			icalcomponent_get_dtstart(cal) );
	}
	else {
		if (icaltime_compare(
			icalcomponent_get_dtstart(cal),
			icalcomponent_get_dtstart(fb)
		   ) < 0) {
			icalcomponent_set_dtstart(fb,
				icalcomponent_get_dtstart(cal) );
		}
	}

	/* Make sure the DTEND property of the freebusy *list* is set to
	 * the DTEND property of the *latest event*.
	 */
	p = icalcomponent_get_first_property(fb, ICAL_DTEND_PROPERTY);
	if (p == NULL) {
		icalcomponent_set_dtend(fb,
			icalcomponent_get_dtend(cal) );
	}
	else {
		if (icaltime_compare(
			icalcomponent_get_dtend(cal),
			icalcomponent_get_dtend(fb)
		   ) > 0) {
			icalcomponent_set_dtend(fb,
				icalcomponent_get_dtend(cal) );
		}
	}

}



/*
 * Backend for ical_freebusy()
 *
 * This function simply loads the messages in the user's calendar room,
 * which contain VEVENTs, then strips them of all non-freebusy data, and
 * adds them to the supplied VCALENDAR.
 *
 */
void ical_freebusy_backend(long msgnum, void *data) {
	icalcomponent *cal;
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	cal = (icalcomponent *)data;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "_HUNT_");
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_part,		/* callback function */
		NULL, NULL,
		(void *) &ird,			/* user data */
		0
	);
	CtdlFreeMessage(msg);

	if (ird.cal == NULL) return;

	ical_add_to_freebusy(cal, ird.cal);

	/* Now free the memory. */
	icalcomponent_free(ird.cal);
}



/*
 * Grab another user's free/busy times
 */
void ical_freebusy(char *who) {
	struct ctdluser usbuf;
	char calendar_room_name[ROOMNAMELEN];
	char hold_rm[ROOMNAMELEN];
	char *serialized_request = NULL;
	icalcomponent *encaps = NULL;
	icalcomponent *fb = NULL;
	int found_user = (-1);
	struct recptypes *recp = NULL;
	char buf[256];
	char host[256];
	char type[256];
	int i = 0;
	int config_lines = 0;

	/* First try an exact match. */
	found_user = getuser(&usbuf, who);

	/* If not found, try it as an unqualified email address. */
	if (found_user != 0) {
		strcpy(buf, who);
		recp = validate_recipients(buf);
		lprintf(CTDL_DEBUG, "Trying <%s>\n", buf);
		if (recp != NULL) {
			if (recp->num_local == 1) {
				found_user = getuser(&usbuf, recp->recp_local);
			}
			free_recipients(recp);
		}
	}

	/* If still not found, try it as an address qualified with the
	 * primary FQDN of this Citadel node.
	 */
	if (found_user != 0) {
		snprintf(buf, sizeof buf, "%s@%s", who, config.c_fqdn);
		lprintf(CTDL_DEBUG, "Trying <%s>\n", buf);
		recp = validate_recipients(buf);
		if (recp != NULL) {
			if (recp->num_local == 1) {
				found_user = getuser(&usbuf, recp->recp_local);
			}
			free_recipients(recp);
		}
	}

	/* Still not found?  Try qualifying it with every domain we
	 * might have addresses in.
	 */
	if (found_user != 0) {
		config_lines = num_tokens(inetcfg, '\n');
		for (i=0; ((i < config_lines) && (found_user != 0)); ++i) {
			extract_token(buf, inetcfg, i, '\n', sizeof buf);
			extract_token(host, buf, 0, '|', sizeof host);
			extract_token(type, buf, 1, '|', sizeof type);

			if ( (!strcasecmp(type, "localhost"))
			   || (!strcasecmp(type, "directory")) ) {
				snprintf(buf, sizeof buf, "%s@%s", who, host);
				lprintf(CTDL_DEBUG, "Trying <%s>\n", buf);
				recp = validate_recipients(buf);
				if (recp != NULL) {
					if (recp->num_local == 1) {
						found_user = getuser(&usbuf, recp->recp_local);
					}
					free_recipients(recp);
				}
			}
		}
	}

	if (found_user != 0) {
		cprintf("%d No such user.\n", ERROR + NO_SUCH_USER);
		return;
	}

	MailboxName(calendar_room_name, sizeof calendar_room_name,
		&usbuf, USERCALENDARROOM);

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (getroom(&CC->room, calendar_room_name) != 0) {
		cprintf("%d Cannot open calendar\n", ERROR + ROOM_NOT_FOUND);
		getroom(&CC->room, hold_rm);
		return;
	}

	/* Create a VFREEBUSY subcomponent */
	lprintf(CTDL_DEBUG, "Creating VFREEBUSY component\n");
	fb = icalcomponent_new_vfreebusy();
	if (fb == NULL) {
		cprintf("%d Internal error: cannot allocate memory.\n",
			ERROR + INTERNAL_ERROR);
		getroom(&CC->room, hold_rm);
		return;
	}

	/* Set the method to PUBLISH */
	icalcomponent_set_method(fb, ICAL_METHOD_PUBLISH);

	/* Set the DTSTAMP to right now. */
	icalcomponent_set_dtstamp(fb, icaltime_from_timet(time(NULL), 0));

	/* Add the user's email address as ORGANIZER */
	sprintf(buf, "MAILTO:%s", who);
	if (strchr(buf, '@') == NULL) {
		strcat(buf, "@");
		strcat(buf, config.c_fqdn);
	}
	for (i=0; buf[i]; ++i) {
		if (buf[i]==' ') buf[i] = '_';
	}
	icalcomponent_add_property(fb, icalproperty_new_organizer(buf));

	/* Add busy time from events */
	lprintf(CTDL_DEBUG, "Adding busy time from events\n");
	CtdlForEachMessage(MSGS_ALL, 0, NULL, NULL, NULL, ical_freebusy_backend, (void *)fb );

	/* If values for DTSTART and DTEND are still not present, set them
	 * to yesterday and tomorrow as default values.
	 */
	if (icalcomponent_get_first_property(fb, ICAL_DTSTART_PROPERTY) == NULL) {
		icalcomponent_set_dtstart(fb, icaltime_from_timet(time(NULL)-86400L, 0));
	}
	if (icalcomponent_get_first_property(fb, ICAL_DTEND_PROPERTY) == NULL) {
		icalcomponent_set_dtend(fb, icaltime_from_timet(time(NULL)+86400L, 0));
	}

	/* Put the freebusy component into the calendar component */
	lprintf(CTDL_DEBUG, "Encapsulating\n");
	encaps = ical_encapsulate_subcomponent(fb);
	if (encaps == NULL) {
		icalcomponent_free(fb);
		cprintf("%d Internal error: cannot allocate memory.\n",
			ERROR + INTERNAL_ERROR);
		getroom(&CC->room, hold_rm);
		return;
	}

	/* Set the method to PUBLISH */
	lprintf(CTDL_DEBUG, "Setting method\n");
	icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

	/* Serialize it */
	lprintf(CTDL_DEBUG, "Serializing\n");
	serialized_request = strdup(icalcomponent_as_ical_string(encaps));
	icalcomponent_free(encaps);	/* Don't need this anymore. */

	cprintf("%d Here is the free/busy data:\n", LISTING_FOLLOWS);
	if (serialized_request != NULL) {
		client_write(serialized_request, strlen(serialized_request));
		free(serialized_request);
	}
	cprintf("\n000\n");

	/* Go back to the room from which we came... */
	getroom(&CC->room, hold_rm);
}



/*
 * Backend for ical_getics()
 * 
 * This is a ForEachMessage() callback function that searches the current room
 * for calendar events and adds them each into one big calendar component.
 */
void ical_getics_backend(long msgnum, void *data) {
	icalcomponent *encaps, *c;
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	encaps = (icalcomponent *)data;
	if (encaps == NULL) return;

	/* Look for the calendar event... */

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "_HUNT_");
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_locate_part,		/* callback function */
		NULL, NULL,
		(void *) &ird,			/* user data */
		0
	);
	CtdlFreeMessage(msg);

	if (ird.cal == NULL) return;

	/* Here we go: put the VEVENT into the VCALENDAR.  We now no longer
	 * are responsible for "the_request"'s memory -- it will be freed
	 * when we free "encaps".
	 */

	/* If the top-level component is *not* a VCALENDAR, we can drop it right
	 * in.  This will almost never happen.
	 */
	if (icalcomponent_isa(ird.cal) != ICAL_VCALENDAR_COMPONENT) {
		icalcomponent_add_component(encaps, ird.cal);
	}
	/*
	 * In the more likely event that we're looking at a VCALENDAR with the VEVENT
	 * and other components encapsulated inside, we have to extract them.
	 */
	else {
		for (c = icalcomponent_get_first_component(ird.cal, ICAL_ANY_COMPONENT);
		    (c != NULL);
		    c = icalcomponent_get_next_component(ird.cal, ICAL_ANY_COMPONENT)) {
			icalcomponent_add_component(encaps, icalcomponent_new_clone(c));
		}
		icalcomponent_free(ird.cal);
	}
}



/*
 * Retrieve all of the calendar items in the current room, and output them
 * as a single icalendar object.
 */
void ical_getics(void)
{
	icalcomponent *encaps = NULL;
	char *ser = NULL;

	if ( (CC->room.QRdefaultview != VIEW_CALENDAR)
	   &&(CC->room.QRdefaultview != VIEW_TASKS) ) {
		cprintf("%d Not a calendar room\n", ERROR+NOT_HERE);
		return;		/* Not a vCalendar-centric room */
	}

	encaps = icalcomponent_new_vcalendar();
	if (encaps == NULL) {
		lprintf(CTDL_DEBUG, "Error at %s:%d - could not allocate component!\n",
			__FILE__, __LINE__);
		cprintf("%d Could not allocate memory\n", ERROR+INTERNAL_ERROR);
		return;
	}

	cprintf("%d one big calendar\n", LISTING_FOLLOWS);

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/* Set the method to PUBLISH */
	icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

	/* Now go through the room encapsulating all calendar items. */
	CtdlForEachMessage(MSGS_ALL, 0, NULL,
		NULL,
		NULL,
		ical_getics_backend,
		(void *) encaps
	);

	ser = strdup(icalcomponent_as_ical_string(encaps));
	client_write(ser, strlen(ser));
	free(ser);
	cprintf("\n000\n");
	icalcomponent_free(encaps);	/* Don't need this anymore. */

}


/*
 * Delete all of the calendar items in the current room, and replace them
 * with calendar items from a client-supplied data stream.
 */
void ical_putics(void)
{
	char *calstream = NULL;
	icalcomponent *cal;
	icalcomponent *c;

	if ( (CC->room.QRdefaultview != VIEW_CALENDAR)
	   &&(CC->room.QRdefaultview != VIEW_TASKS) ) {
		cprintf("%d Not a calendar room\n", ERROR+NOT_HERE);
		return;		/* Not a vCalendar-centric room */
	}

	if (!CtdlDoIHavePermissionToDeleteMessagesFromThisRoom()) {
		cprintf("%d Permission denied.\n", ERROR+HIGHER_ACCESS_REQUIRED);
		return;
	}

	cprintf("%d Transmit data now\n", SEND_LISTING);
        calstream = CtdlReadMessageBody("000", config.c_maxmsglen, NULL, 0, 0);
	if (calstream == NULL) {
		return;
	}

	cal = icalcomponent_new_from_string(calstream);
	free(calstream);
	ical_dezonify(cal);

	/* We got our data stream -- now do something with it. */

	/* Delete the existing messages in the room, because we are replacing
	 * the entire calendar with an entire new (or updated) calendar.
	 * (Careful: this opens an S_ROOMS critical section!)
	 */
	CtdlDeleteMessages(CC->room.QRname, NULL, 0, "");

	/* If the top-level component is *not* a VCALENDAR, we can drop it right
	 * in.  This will almost never happen.
	 */
	if (icalcomponent_isa(cal) != ICAL_VCALENDAR_COMPONENT) {
		ical_write_to_cal(NULL, cal);
	}
	/*
	 * In the more likely event that we're looking at a VCALENDAR with the VEVENT
	 * and other components encapsulated inside, we have to extract them.
	 */
	else {
		for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
		    (c != NULL);
		    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
			ical_write_to_cal(NULL, c);
		}
	}

	icalcomponent_free(cal);
}


/*
 * All Citadel calendar commands from the client come through here.
 */
void cmd_ical(char *argbuf)
{
	char subcmd[64];
	long msgnum;
	char partnum[256];
	char action[256];
	char who[256];

	extract_token(subcmd, argbuf, 0, '|', sizeof subcmd);

	/* Allow "test" and "freebusy" subcommands without logging in. */

	if (!strcasecmp(subcmd, "test")) {
		cprintf("%d This server supports calendaring\n", CIT_OK);
		return;
	}

	if (!strcasecmp(subcmd, "freebusy")) {
		extract_token(who, argbuf, 1, '|', sizeof who);
		ical_freebusy(who);
		return;
	}

	if (!strcasecmp(subcmd, "sgi")) {
		CIT_ICAL->server_generated_invitations =
			(extract_int(argbuf, 1) ? 1 : 0) ;
		cprintf("%d %d\n",
			CIT_OK, CIT_ICAL->server_generated_invitations);
		return;
	}

	if (CtdlAccessCheck(ac_logged_in)) return;

	if (!strcasecmp(subcmd, "respond")) {
		msgnum = extract_long(argbuf, 1);
		extract_token(partnum, argbuf, 2, '|', sizeof partnum);
		extract_token(action, argbuf, 3, '|', sizeof action);
		ical_respond(msgnum, partnum, action);
		return;
	}

	if (!strcasecmp(subcmd, "handle_rsvp")) {
		msgnum = extract_long(argbuf, 1);
		extract_token(partnum, argbuf, 2, '|', sizeof partnum);
		extract_token(action, argbuf, 3, '|', sizeof action);
		ical_handle_rsvp(msgnum, partnum, action);
		return;
	}

	if (!strcasecmp(subcmd, "conflicts")) {
		msgnum = extract_long(argbuf, 1);
		extract_token(partnum, argbuf, 2, '|', sizeof partnum);
		ical_conflicts(msgnum, partnum);
		return;
	}

	if (!strcasecmp(subcmd, "getics")) {
		ical_getics();
		return;
	}

	if (!strcasecmp(subcmd, "putics")) {
		ical_putics();
		return;
	}

	cprintf("%d Invalid subcommand\n", ERROR + CMD_NOT_SUPPORTED);
}



/*
 * We don't know if the calendar room exists so we just create it at login
 */
void ical_create_room(void)
{
	struct ctdlroom qr;
	struct visit vbuf;

	/* Create the calendar room if it doesn't already exist */
	create_room(USERCALENDARROOM, 4, "", 0, 1, 0, VIEW_CALENDAR);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERCALENDARROOM)) {
		lprintf(CTDL_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_CALENDAR;	/* 3 = calendar view */
	lputroom(&qr);

	/* Set the view to a calendar view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_CALENDAR;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	/* Create the tasks list room if it doesn't already exist */
	create_room(USERTASKSROOM, 4, "", 0, 1, 0, VIEW_TASKS);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERTASKSROOM)) {
		lprintf(CTDL_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_TASKS;
	lputroom(&qr);

	/* Set the view to a task list view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_TASKS;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	/* Create the notes room if it doesn't already exist */
	create_room(USERNOTESROOM, 4, "", 0, 1, 0, VIEW_NOTES);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (lgetroom(&qr, USERNOTESROOM)) {
		lprintf(CTDL_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_NOTES;
	lputroom(&qr);

	/* Set the view to a notes view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_NOTES;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	return;
}


/*
 * ical_send_out_invitations() is called by ical_saving_vevent() when it
 * finds a VEVENT.
 */
void ical_send_out_invitations(icalcomponent *cal) {
	icalcomponent *the_request = NULL;
	char *serialized_request = NULL;
	icalcomponent *encaps = NULL;
	char *request_message_text = NULL;
	struct CtdlMessage *msg = NULL;
	struct recptypes *valid = NULL;
	char attendees_string[SIZ];
	int num_attendees = 0;
	char this_attendee[256];
	icalproperty *attendee = NULL;
	char summary_string[SIZ];
	icalproperty *summary = NULL;

	if (cal == NULL) {
		lprintf(CTDL_ERR, "ERROR: trying to reply to NULL event?\n");
		return;
	}


	/* If this is a VCALENDAR component, look for a VEVENT subcomponent. */
	if (icalcomponent_isa(cal) == ICAL_VCALENDAR_COMPONENT) {
		ical_send_out_invitations(
			icalcomponent_get_first_component(
				cal, ICAL_VEVENT_COMPONENT
			)
		);
		return;
	}

	/* Clone the event */
	the_request = icalcomponent_new_clone(cal);
	if (the_request == NULL) {
		lprintf(CTDL_ERR, "ERROR: cannot clone calendar object\n");
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
			safestrncpy(this_attendee, icalproperty_get_attendee(attendee), sizeof this_attendee);
			if (!strncasecmp(this_attendee, "MAILTO:", 7)) {
				strcpy(this_attendee, &this_attendee[7]);

				if (!CtdlIsMe(this_attendee, sizeof this_attendee)) {	/* don't send an invitation to myself! */
					snprintf(&attendees_string[strlen(attendees_string)],
						sizeof(attendees_string) - strlen(attendees_string),
						"%s, ",
						this_attendee
					);
					++num_attendees;
				}
			}
		}
	}

	lprintf(CTDL_DEBUG, "<%d> attendees: <%s>\n", num_attendees, attendees_string);

	/* If there are no attendees, there are no invitations to send, so...
	 * don't bother putting one together!  Punch out, Maverick!
	 */
	if (num_attendees == 0) {
		icalcomponent_free(the_request);
		return;
	}

	/* Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new_vcalendar();
	if (encaps == NULL) {
		lprintf(CTDL_DEBUG, "Error at %s:%d - could not allocate component!\n",
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

	/* Now make sure all of the DTSTART and DTEND properties are UTC. */
	ical_dezonify(the_request);

	/* Here we go: put the VEVENT into the VCALENDAR.  We now no longer
	 * are responsible for "the_request"'s memory -- it will be freed
	 * when we free "encaps".
	 */
	icalcomponent_add_component(encaps, the_request);

	/* Serialize it */
	serialized_request = strdup(icalcomponent_as_ical_string(encaps));
	icalcomponent_free(encaps);	/* Don't need this anymore. */
	if (serialized_request == NULL) return;

	request_message_text = malloc(strlen(serialized_request) + SIZ);
	if (request_message_text != NULL) {
		sprintf(request_message_text,
			"Content-type: text/calendar\r\n\r\n%s\r\n",
			serialized_request
		);

		msg = CtdlMakeMessage(&CC->user,
			"",			/* No single recipient here */
			"",			/* No single recipient here */
			CC->room.QRname, 0, FMT_RFC822,
			"",
			"",
			summary_string,		/* Use summary for subject */
			NULL,
			request_message_text);
	
		if (msg != NULL) {
			valid = validate_recipients(attendees_string);
			CtdlSubmitMsg(msg, valid, "");
			CtdlFreeMessage(msg);
			free_recipients(valid);
		}
	}
	free(serialized_request);
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

	lprintf(CTDL_DEBUG, "ical_saving_vevent() has been called!\n");

	/* Don't send out invitations unless the client wants us to. */
	if (CIT_ICAL->server_generated_invitations == 0) {
		return;
	}

	/* Don't send out invitations if we've been asked not to. */
	if (CIT_ICAL->avoid_sending_invitations > 0) {
		return;
	}

	strcpy(organizer_string, "");
	/*
 	 * The VEVENT subcomponent is the one we're interested in.
	 * Send out invitations if, and only if, this user is the Organizer.
	 */
	if (icalcomponent_isa(cal) == ICAL_VEVENT_COMPONENT) {
		organizer = icalcomponent_get_first_property(cal, ICAL_ORGANIZER_PROPERTY);
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
			if (CtdlIsMe(organizer_string, sizeof organizer_string)) {
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
 * This hunts for the UID of the calendar event (becomes Citadel msg EUID),
 * the summary of the event (becomes message subject),
 * and the start time (becomes message date/time).
 */
void ical_ctdl_set_exclusive_msgid(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, void *cbuserdata)
{
	icalcomponent *cal, *nested_event, *nested_todo, *whole_cal;
	icalproperty *p;
	struct icalmessagemod *imm;
	char new_uid[SIZ];

	imm = (struct icalmessagemod *)cbuserdata;

	/* We're only interested in calendar data. */
	if (strcasecmp(cbtype, "text/calendar")) {
		return;
	}

	/* Hunt for the UID and drop it in
	 * the "user data" pointer for the MIME parser.  When
	 * ical_obj_beforesave() sees it there, it'll set the Exclusive msgid
	 * to that string.
	 */
	whole_cal = icalcomponent_new_from_string(content);
	cal = whole_cal;
	if (cal != NULL) {
		if (icalcomponent_isa(cal) == ICAL_VCALENDAR_COMPONENT) {
			nested_event = icalcomponent_get_first_component(
				cal, ICAL_VEVENT_COMPONENT);
			if (nested_event != NULL) {
				cal = nested_event;
			}
			else {
				nested_todo = icalcomponent_get_first_component(
					cal, ICAL_VTODO_COMPONENT);
				if (nested_todo != NULL) {
					cal = nested_todo;
				}
			}
		}
		
		if (cal != NULL) {
			p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
			if (p == NULL) {
				/* If there's no uid we must generate one */
				generate_uuid(new_uid);
				icalcomponent_add_property(cal, icalproperty_new_uid(new_uid));
				p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
			}
			if (p != NULL) {
				strcpy(imm->uid, icalproperty_get_comment(p));
			}
			p = ical_ctdl_get_subprop(cal, ICAL_SUMMARY_PROPERTY);
			if (p != NULL) {
				strcpy(imm->subject, icalproperty_get_comment(p));
			}
			p = ical_ctdl_get_subprop(cal, ICAL_DTSTART_PROPERTY);
			if (p != NULL) {
				imm->dtstart = icaltime_as_timet(icalproperty_get_dtstart(p));
			}
		}
		icalcomponent_free(cal);
		if (whole_cal != cal) {
			icalcomponent_free(whole_cal);
		}
	}
}




/*
 * See if we need to prevent the object from being saved (we don't allow
 * MIME types other than text/calendar in "calendar" or "tasks"  rooms).  Also,
 * when saving an event to the calendar, set the message's Citadel exclusive
 * message ID to the UID of the object.  This causes our replication checker to
 * automatically delete any existing instances of the same object.  (Isn't
 * that cool?)
 *
 * We also set the message's Subject to the event summary, and the Date/time to
 * the event start time.
 */
int ical_obj_beforesave(struct CtdlMessage *msg)
{
	struct icalmessagemod imm;

	/* First determine if this is a calendar or tasks room */
	if (  (CC->room.QRdefaultview != VIEW_CALENDAR)
	   && (CC->room.QRdefaultview != VIEW_TASKS)
	) {
		return(0);		/* Not a vCalendar-centric room */
	}

	/* It must be an RFC822 message! */
	if (msg->cm_format_type != 4) {
		lprintf(CTDL_DEBUG, "Rejecting non-RFC822 message\n");
		return(1);		/* You tried to save a non-RFC822 message! */
	}

	if (msg->cm_fields['M'] == NULL) {
		return(1);		/* You tried to save a null message! */
	}

	memset(&imm, 0, sizeof(struct icalmessagemod));
	
	/* Do all of our lovely back-end parsing */
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_ctdl_set_exclusive_msgid,
		NULL, NULL,
		(void *)&imm,
		0
	);

	if (!IsEmptyStr(imm.uid)) {
		if (msg->cm_fields['E'] != NULL) {
			free(msg->cm_fields['E']);
		}
		msg->cm_fields['E'] = strdup(imm.uid);
		lprintf(CTDL_DEBUG, "Saving calendar UID <%s>\n", msg->cm_fields['E']);
	}
	if (!IsEmptyStr(imm.subject)) {
		if (msg->cm_fields['U'] != NULL) {
			free(msg->cm_fields['U']);
		}
		msg->cm_fields['U'] = strdup(imm.subject);
	}
	if (imm.dtstart > 0) {
		if (msg->cm_fields['T'] != NULL) {
			free(msg->cm_fields['T']);
		}
		msg->cm_fields['T'] = strdup("000000000000000000");
		sprintf(msg->cm_fields['T'], "%ld", imm.dtstart);
	}

	return(0);
}


/*
 * Things we need to do after saving a calendar event.
 */
void ical_obj_aftersave_backend(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, void *cbuserdata)
{
	icalcomponent *cal;

	/* We're only interested in calendar items here. */
	if (strcasecmp(cbtype, "text/calendar")) {
		return;
	}

	/* Hunt for the UID and drop it in
	 * the "user data" pointer for the MIME parser.  When
	 * ical_obj_beforesave() sees it there, it'll set the Exclusive msgid
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
 * (This will start back end tasks such as automatic generation of invitations,
 * if such actions are appropriate.)
 */
int ical_obj_aftersave(struct CtdlMessage *msg)
{
	char roomname[ROOMNAMELEN];

	/*
	 * If this isn't the Calendar> room, no further action is necessary.
	 */

	/* First determine if this is our room */
	MailboxName(roomname, sizeof roomname, &CC->user, USERCALENDARROOM);
	if (strcasecmp(roomname, CC->room.QRname)) {
		return(0);	/* Not the Calendar room -- don't do anything. */
	}

	/* It must be an RFC822 message! */
	if (msg->cm_format_type != 4) return(1);

	/* Reject null messages */
	if (msg->cm_fields['M'] == NULL) return(1);
	
	/* Now recurse through it looking for our icalendar data */
	mime_parser(msg->cm_fields['M'],
		NULL,
		*ical_obj_aftersave_backend,
		NULL, NULL,
		NULL,
		0
	);

	return(0);
}


void ical_session_startup(void) {
	CIT_ICAL = malloc(sizeof(struct cit_ical));
	memset(CIT_ICAL, 0, sizeof(struct cit_ical));
}

void ical_session_shutdown(void) {
	free(CIT_ICAL);
}


/*
 * Back end for ical_fixed_output()
 */
void ical_fixed_output_backend(icalcomponent *cal,
			int recursion_level
) {
	icalcomponent *c;
	icalproperty *p;
	char buf[256];

      	p = icalcomponent_get_first_property(cal, ICAL_SUMMARY_PROPERTY);
	if (p != NULL) {
		cprintf("%s\n", (const char *)icalproperty_get_comment(p));
	}

      	p = icalcomponent_get_first_property(cal, ICAL_LOCATION_PROPERTY);
	if (p != NULL) {
		cprintf("%s\n", (const char *)icalproperty_get_comment(p));
	}

      	p = icalcomponent_get_first_property(cal, ICAL_DESCRIPTION_PROPERTY);
	if (p != NULL) {
		cprintf("%s\n", (const char *)icalproperty_get_comment(p));
	}

	/* If the component has attendees, iterate through them. */
	for (p = icalcomponent_get_first_property(cal, ICAL_ATTENDEE_PROPERTY); (p != NULL); p = icalcomponent_get_next_property(cal, ICAL_ATTENDEE_PROPERTY)) {
		safestrncpy(buf, icalproperty_get_attendee(p), sizeof buf);
		if (!strncasecmp(buf, "MAILTO:", 7)) {

			/* screen name or email address */
			strcpy(buf, &buf[7]);
			striplt(buf);
			cprintf("%s ", buf);
		}
		cprintf("\n");
	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != 0);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		ical_fixed_output_backend(c, recursion_level+1);
	}
}



/*
 * Function to output vcalendar data as plain text.  Nobody uses MSG0
 * anymore, so really this is just so we expose the vCard data to the full
 * text indexer.
 */
void ical_fixed_output(char *ptr, int len) {
	icalcomponent *cal;
	char *stringy_cal;

	stringy_cal = malloc(len + 1);
	safestrncpy(stringy_cal, ptr, len + 1);
	cal = icalcomponent_new_from_string(stringy_cal);
	free(stringy_cal);

	if (cal == NULL) {
		return;
	}

	ical_dezonify(cal);
	ical_fixed_output_backend(cal, 0);

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}


#endif	/* CITADEL_WITH_CALENDAR_SERVICE */

/*
 * Register this module with the Citadel server.
 */
CTDL_MODULE_INIT(calendar)
{
	if (!threading)
	{
#ifdef CITADEL_WITH_CALENDAR_SERVICE
		CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
		CtdlRegisterMessageHook(ical_obj_aftersave, EVT_AFTERSAVE);
		CtdlRegisterSessionHook(ical_create_room, EVT_LOGIN);
		CtdlRegisterProtoHook(cmd_ical, "ICAL", "Citadel iCal commands");
		CtdlRegisterSessionHook(ical_session_startup, EVT_START);
		CtdlRegisterSessionHook(ical_session_shutdown, EVT_STOP);
		CtdlRegisterFixedOutputHook("text/calendar", ical_fixed_output);
#endif
	}
	
	/* return our Subversion id for the Log */
	return "$Id$";
}



void serv_calendar_destroy(void)
{
#ifdef CITADEL_WITH_CALENDAR_SERVICE
	icaltimezone_free_builtin_timezones();
#endif
}
