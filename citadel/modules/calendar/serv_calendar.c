/* 
 * This module implements iCalendar object processing and the Calendar>
 * room on a Citadel server.  It handles iCalendar objects using the
 * iTIP protocol.  See RFCs 2445 and 2446.
 *
 *
 * Copyright (c) 1987-2011 by the citadel.org team
 *
 *  This program is open source software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#define PRODID "-//Citadel//NONSGML Citadel Calendar//EN"

#include "ctdl_module.h"

#include <libical/ical.h>

#include "msgbase.h"
#include "internet_addressing.h"
#include "serv_calendar.h"
#include "room_ops.h"
#include "euidindex.h"
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
		syslog(LOG_CRIT, "ERROR: could not allocate component!\n");
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

	/* Return the object we just created. */
	return(encaps);
}




/*
 * Write a calendar object into the specified user's calendar room.
 * If the supplied user is NULL, this function writes the calendar object
 * to the currently selected room.
 */
void ical_write_to_cal(struct ctdluser *u, icalcomponent *cal) {
	char *ser = NULL;
	long serlen;
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

	ser = icalcomponent_as_ical_string_r(cal);
	if (ser == NULL) return;

	serlen = strlen(ser);

	/* If the caller supplied a user, write to that user's default calendar room */
	if (u) {
		/* This handy API function does all the work for us. */
		CtdlWriteObject(USERCALENDARROOM,	/* which room */
			"text/calendar",	/* MIME type */
			ser,			/* data */
			serlen + 1,		/* length */
			u,			/* which user */
			0,			/* not binary */
			0,			/* don't delete others of this type */
			0			/* no flags */
		);
	}

	/* If the caller did not supply a user, write to the currently selected room */
	if (!u) {
		struct CitContext *CCC = CC;
		StrBuf *MsgBody;

		msg = malloc(sizeof(struct CtdlMessage));
		memset(msg, 0, sizeof(struct CtdlMessage));
		msg->cm_magic = CTDLMESSAGE_MAGIC;
		msg->cm_anon_type = MES_NORMAL;
		msg->cm_format_type = 4;
		CM_SetField(msg, eAuthor, CCC->user.fullname, strlen(CCC->user.fullname));
		CM_SetField(msg, eOriginalRoom, CCC->room.QRname, strlen(CCC->room.QRname));
		CM_SetField(msg, eNodeName, config.c_nodename, strlen(config.c_nodename));
		CM_SetField(msg, eHumanNode, config.c_humannode, strlen(config.c_humannode));

		MsgBody = NewStrBufPlain(NULL, serlen + 100);
		StrBufAppendBufPlain(MsgBody, HKEY("Content-type: text/calendar\r\n\r\n"), 0);
		StrBufAppendBufPlain(MsgBody, ser, serlen, 0);

		CM_SetAsFieldSB(msg, eMesageText, &MsgBody);
	
		/* Now write the data */
		CtdlSubmitMsg(msg, NULL, "", QP_EADDR);
		CM_Free(msg);
	}

	/* In either case, now we can free the serialized calendar object */
	free(ser);
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
	recptypes *recp = NULL;
	icalparameter *partstat = NULL;
	char *serialized_reply = NULL;
	char *reply_message_text = NULL;
	const char *ch;
	struct CtdlMessage *msg = NULL;
	recptypes *valid = NULL;

	*organizer_string = '\0';
	strcpy(summary_string, "Calendar item");

	if (request == NULL) {
		syslog(LOG_ERR, "ERROR: trying to reply to NULL event?\n");
		return;
	}

	the_reply = icalcomponent_new_clone(request);
	if (the_reply == NULL) {
		syslog(LOG_ERR, "ERROR: cannot clone request\n");
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
			ch = icalproperty_get_attendee(attendee);
			if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {
				safestrncpy(attendee_string, ch + 7, sizeof (attendee_string));
				striplt(attendee_string);
				recp = validate_recipients(attendee_string, NULL, 0);
				if (recp != NULL) {
					if (!strcasecmp(recp->recp_local, CC->user.fullname)) {
						if (me_attend) icalproperty_free(me_attend);
						me_attend = icalproperty_new_clone(attendee);
					}
					free_recipients(recp);
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
	serialized_reply = icalcomponent_as_ical_string_r(the_reply);
	icalcomponent_free(the_reply);	/* don't need this anymore */
	if (serialized_reply == NULL) return;

	reply_message_text = malloc(strlen(serialized_reply) + SIZ);
	if (reply_message_text != NULL) {
		sprintf(reply_message_text,
			"Content-type: text/calendar; charset=\"utf-8\"\r\n\r\n%s\r\n",
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
			reply_message_text,
			NULL);
	
		if (msg != NULL) {
			valid = validate_recipients(organizer_string, NULL, 0);
			CtdlSubmitMsg(msg, valid, "", QP_EADDR);
			CM_Free(msg);
			free_recipients(valid);
		}
	}
	free(serialized_reply);
}



/*
 * Callback function for mime parser that hunts for calendar content types
 * and turns them into calendar objects.  If something is found, it is placed
 * in ird->cal, and the caller now owns that memory and is responsible for freeing it.
 */
void ical_locate_part(char *name, char *filename, char *partnum, char *disp,
		void *content, char *cbtype, char *cbcharset, size_t length, char *encoding,
		char *cbid, void *cbuserdata) {

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

	if (  (strcasecmp(cbtype, "text/calendar"))
	   && (strcasecmp(cbtype, "application/ics")) ) {
		return;
	}

	if (ird->cal != NULL) {
		icalcomponent_free(ird->cal);
		ird->cal = NULL;
	}

	ird->cal = icalcomponent_new_from_string(content);
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
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
	);

	/* We're done with the incoming message, because we now have a
	 * calendar object in memory.
	 */
	CM_Free(msg);

	/*
	 * Here is the real meat of this function.  Handle the event.
	 */
	if (ird.cal != NULL) {
		/* Save this in the user's calendar if necessary */
		if (!strcasecmp(action, "accept")) {
			ical_write_to_cal(&CC->user, ird.cal);
		}

		/* Send a reply if necessary */
		if (icalcomponent_get_method(ird.cal) == ICAL_METHOD_REQUEST) {
			ical_send_a_reply(ird.cal, action);
		}

		/* We used to delete the invitation after handling it.
		 * We don't do that anymore, but here is the code that handled it:
		 * CtdlDeleteMessages(CC->room.QRname, &msgnum, 1, "");
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
		char *cbid, void *cbuserdata) {

	struct original_event_container *oec = NULL;

	if (  (strcasecmp(cbtype, "text/calendar"))
	   && (strcasecmp(cbtype, "application/ics")) ) {
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
			const char *e, *r;
			e = icalproperty_get_attendee(e_attendee);
			r = icalproperty_get_attendee(r_attendee);

			if ((e != NULL) && 
			    (r != NULL) && 
			    !strcasecmp(e, r)) {
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
	syslog(LOG_DEBUG, "UID of event being replied to is <%s>\n", uid);

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (CtdlGetRoom(&CC->room, USERCALENDARROOM) != 0) {
		CtdlGetRoom(&CC->room, hold_rm);
		syslog(LOG_CRIT, "cannot get user calendar room\n");
		return(2);
	}

	/*
	 * Look in the EUID index for a message with
	 * the Citadel EUID set to the value we're looking for.  Since
	 * Citadel always sets the message EUID to the iCalendar UID of
	 * the event, this will work.
	 */
	msgnum_being_replaced = CtdlLocateMessageByEuid(uid, &CC->room);

	CtdlGetRoom(&CC->room, hold_rm);	/* return to saved room */

	syslog(LOG_DEBUG, "msgnum_being_replaced == %ld\n", msgnum_being_replaced);
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
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_original_event,	/* callback function */
		    NULL, NULL,
		    &oec,				/* user data */
		    0
	);
	CM_Free(msg);

	original_event = oec.c;
	if (original_event == NULL) {
		syslog(LOG_ERR, "ERROR: Original_component is NULL.\n");
		return(2);
	}

	/* Merge the attendee's updated status into the event */
	ical_merge_attendee_reply(original_event, cal);

	/* Serialize it */
	serialized_event = icalcomponent_as_ical_string_r(original_event);
	icalcomponent_free(original_event);	/* Don't need this anymore. */
	if (serialized_event == NULL) return(2);

	CtdlMailboxName(roomname, sizeof roomname, &CC->user, USERCALENDARROOM);

	message_text = malloc(strlen(serialized_event) + SIZ);
	if (message_text != NULL) {
		sprintf(message_text,
			"Content-type: text/calendar; charset=\"utf-8\"\r\n\r\n%s\r\n",
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
			message_text,
			NULL);
	
		if (msg != NULL) {
			CIT_ICAL->avoid_sending_invitations = 1;
			CtdlSubmitMsg(msg, NULL, roomname, QP_EADDR);
			CM_Free(msg);
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
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
		);

	/* We're done with the incoming message, because we now have a
	 * calendar object in memory.
	 */
	CM_Free(msg);

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

	/* if either event lacks end time, assume end = start */
	if (icaltime_is_null_time(t1end))
		memcpy(&t1end, &t1start, sizeof(struct icaltimetype));
	else {
		if (t1end.is_date && icaltime_compare(t1start, t1end)) {
                        /*
                         * the end date is non-inclusive so adjust it by one
                         * day because our test is inclusive, note that a day is
                         * not too much because we are talking about all day
                         * events
			 * if start = end we assume that nevertheless the whole
			 * day is meant
                         */
			icaltime_adjust(&t1end, -1, 0, 0, 0);	
		}
	}

	if (icaltime_is_null_time(t2end))
		memcpy(&t2end, &t2start, sizeof(struct icaltimetype));
	else {
		if (t2end.is_date && icaltime_compare(t2start, t2end)) {
			icaltime_adjust(&t2end, -1, 0, 0, 0);	
		}
	}

	/* First, check for all-day events */
	if (t1start.is_date || t2start.is_date) {
		/* If event 1 ends before event 2 starts, we're in the clear. */
		if (icaltime_compare_date_only(t1end, t2start) < 0) return(0);

		/* If event 2 ends before event 1 starts, we're also ok. */
		if (icaltime_compare_date_only(t2end, t1start) < 0) return(0);

		return(1);
	}

	/* syslog(LOG_DEBUG, "Comparing t1start %d:%d t1end %d:%d t2start %d:%d t2end %d:%d \n",
		t1start.hour, t1start.minute, t1end.hour, t1end.minute,
		t2start.hour, t2start.minute, t2end.hour, t2end.minute);
	*/

	/* Now check for overlaps using date *and* time. */

	/* If event 1 ends before event 2 starts, we're in the clear. */
	if (icaltime_compare(t1end, t2start) <= 0) return(0);
	/* syslog(LOG_DEBUG, "first passed\n"); */

	/* If event 2 ends before event 1 starts, we're also ok. */
	if (icaltime_compare(t2end, t1start) <= 0) return(0);
	/* syslog(LOG_DEBUG, "second passed\n"); */

	/* Otherwise, they overlap. */
	return(1);
}

/* 
 * Phase 6 of "hunt for conflicts"
 * called by ical_conflicts_phase5()
 *
 * Now both the proposed and existing events have been boiled down to start and end times.
 * Check for overlap and output any conflicts.
 *
 * Returns nonzero if a conflict was reported.  This allows the caller to stop iterating.
 */
int ical_conflicts_phase6(struct icaltimetype t1start,
			struct icaltimetype t1end,
			struct icaltimetype t2start,
			struct icaltimetype t2end,
			long existing_msgnum,
			char *conflict_event_uid,
			char *conflict_event_summary,
			char *compare_uid)
{
	int conflict_reported = 0;

	/* debugging cruft *
	time_t tt;
	tt = icaltime_as_timet_with_zone(t1start, t1start.zone);
	syslog(LOG_DEBUG, "PROPOSED START: %s", ctime(&tt));
	tt = icaltime_as_timet_with_zone(t1end, t1end.zone);
	syslog(LOG_DEBUG, "  PROPOSED END: %s", ctime(&tt));
	tt = icaltime_as_timet_with_zone(t2start, t2start.zone);
	syslog(LOG_DEBUG, "EXISTING START: %s", ctime(&tt));
	tt = icaltime_as_timet_with_zone(t2end, t2end.zone);
	syslog(LOG_DEBUG, "  EXISTING END: %s", ctime(&tt));
	* debugging cruft */

	/* compare and output */

	if (ical_ctdl_is_overlap(t1start, t1end, t2start, t2end)) {
		cprintf("%ld||%s|%s|%d|\n",
			existing_msgnum,
			conflict_event_uid,
			conflict_event_summary,
			(	((strlen(compare_uid)>0)
				&&(!strcasecmp(compare_uid,
				conflict_event_uid))) ? 1 : 0
			)
		);
		conflict_reported = 1;
	}

	return(conflict_reported);
}



/*
 * Phase 5 of "hunt for conflicts"
 * Called by ical_conflicts_phase4()
 *
 * We have the proposed event boiled down to start and end times.
 * Now check it against an existing event. 
 */
void ical_conflicts_phase5(struct icaltimetype t1start,
			struct icaltimetype t1end,
			icalcomponent *existing_event,
			long existing_msgnum,
			char *compare_uid)
{
	char conflict_event_uid[SIZ];
	char conflict_event_summary[SIZ];
	struct icaltimetype t2start, t2end;
	icalproperty *p;

	/* recur variables */
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;
	icalrecur_iterator *ritr = NULL;
	struct icaldurationtype dur;
	int num_recur = 0;

	/* initialization */
	strcpy(conflict_event_uid, "");
	strcpy(conflict_event_summary, "");
	t2start = icaltime_null_time();
	t2end = icaltime_null_time();

	/* existing event stuff */
	p = ical_ctdl_get_subprop(existing_event, ICAL_DTSTART_PROPERTY);
	if (p == NULL) return;
	if (p != NULL) t2start = icalproperty_get_dtstart(p);
	if (icaltime_is_utc(t2start)) {
		t2start.zone = icaltimezone_get_utc_timezone();
	}
	else {
		t2start.zone = icalcomponent_get_timezone(existing_event,
			icalparameter_get_tzid(
				icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)
			)
		);
		if (!t2start.zone) {
			t2start.zone = get_default_icaltimezone();
		}
	}

	p = ical_ctdl_get_subprop(existing_event, ICAL_DTEND_PROPERTY);
	if (p != NULL) {
		t2end = icalproperty_get_dtend(p);

		if (icaltime_is_utc(t2end)) {
			t2end.zone = icaltimezone_get_utc_timezone();
		}
		else {
			t2end.zone = icalcomponent_get_timezone(existing_event,
				icalparameter_get_tzid(
					icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)
				)
			);
			if (!t2end.zone) {
				t2end.zone = get_default_icaltimezone();
			}
		}
		dur = icaltime_subtract(t2end, t2start);
	}
	else {
		memset (&dur, 0, sizeof(struct icaldurationtype));
	}

	rrule = ical_ctdl_get_subprop(existing_event, ICAL_RRULE_PROPERTY);
	if (rrule) {
		recur = icalproperty_get_rrule(rrule);
		ritr = icalrecur_iterator_new(recur, t2start);
	}

	do {
		p = ical_ctdl_get_subprop(existing_event, ICAL_UID_PROPERTY);
		if (p != NULL) {
			strcpy(conflict_event_uid, icalproperty_get_comment(p));
		}
	
		p = ical_ctdl_get_subprop(existing_event, ICAL_SUMMARY_PROPERTY);
		if (p != NULL) {
			strcpy(conflict_event_summary, icalproperty_get_comment(p));
		}
	
		if (ical_conflicts_phase6(t1start, t1end, t2start, t2end,
		   existing_msgnum, conflict_event_uid, conflict_event_summary, compare_uid))
		{
			num_recur = MAX_RECUR + 1;	/* force it out of scope, no need to continue */
		}

		if (rrule) {
			t2start = icalrecur_iterator_next(ritr);
			if (!icaltime_is_null_time(t2end)) {
				const icaltimezone *hold_zone = t2end.zone;
				t2end = icaltime_add(t2start, dur);
				t2end.zone = hold_zone;
			}
			++num_recur;
		}

		if (icaltime_compare(t2start, t1end) < 0) {
			num_recur = MAX_RECUR + 1;	/* force it out of scope */
		}

	} while ( (rrule) && (!icaltime_is_null_time(t2start)) && (num_recur < MAX_RECUR) );
	icalrecur_iterator_free(ritr);
}




/*
 * Phase 4 of "hunt for conflicts"
 * Called by ical_hunt_for_conflicts_backend()
 *
 * At this point we've got it boiled down to two icalcomponent events in memory.
 * If they conflict, output something to the client.
 */
void ical_conflicts_phase4(icalcomponent *proposed_event,
		icalcomponent *existing_event,
		long existing_msgnum)
{
	struct icaltimetype t1start, t1end;
	icalproperty *p;
	char compare_uid[SIZ];

	/* recur variables */
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;
	icalrecur_iterator *ritr = NULL;
	struct icaldurationtype dur;
	int num_recur = 0;

	/* initialization */
	t1end = icaltime_null_time();
	*compare_uid = '\0';

	/* proposed event stuff */

	p = ical_ctdl_get_subprop(proposed_event, ICAL_DTSTART_PROPERTY);
	if (p == NULL)
		return;
	else
		t1start = icalproperty_get_dtstart(p);

	if (icaltime_is_utc(t1start)) {
		t1start.zone = icaltimezone_get_utc_timezone();
	}
	else {
		t1start.zone = icalcomponent_get_timezone(proposed_event,
			icalparameter_get_tzid(
				icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)
			)
		);
		if (!t1start.zone) {
			t1start.zone = get_default_icaltimezone();
		}
	}
	
	p = ical_ctdl_get_subprop(proposed_event, ICAL_DTEND_PROPERTY);
	if (p != NULL) {
		t1end = icalproperty_get_dtend(p);

		if (icaltime_is_utc(t1end)) {
			t1end.zone = icaltimezone_get_utc_timezone();
		}
		else {
			t1end.zone = icalcomponent_get_timezone(proposed_event,
				icalparameter_get_tzid(
					icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)
				)
			);
			if (!t1end.zone) {
				t1end.zone = get_default_icaltimezone();
			}
		}

		dur = icaltime_subtract(t1end, t1start);
	}
	else {
		memset (&dur, 0, sizeof(struct icaldurationtype));
	}

	rrule = ical_ctdl_get_subprop(proposed_event, ICAL_RRULE_PROPERTY);
	if (rrule) {
		recur = icalproperty_get_rrule(rrule);
		ritr = icalrecur_iterator_new(recur, t1start);
	}

	p = ical_ctdl_get_subprop(proposed_event, ICAL_UID_PROPERTY);
	if (p != NULL) {
		strcpy(compare_uid, icalproperty_get_comment(p));
	}

	do {
		ical_conflicts_phase5(t1start, t1end, existing_event, existing_msgnum, compare_uid);

		if (rrule) {
			t1start = icalrecur_iterator_next(ritr);
			if (!icaltime_is_null_time(t1end)) {
				const icaltimezone *hold_zone = t1end.zone;
				t1end = icaltime_add(t1start, dur);
				t1end.zone = hold_zone;
			}
			++num_recur;
		}

	} while ( (rrule) && (!icaltime_is_null_time(t1start)) && (num_recur < MAX_RECUR) );
	icalrecur_iterator_free(ritr);
}



/*
 * Phase 3 of "hunt for conflicts"
 * Called by ical_hunt_for_conflicts()
 */
void ical_hunt_for_conflicts_backend(long msgnum, void *data) {
	icalcomponent *proposed_event;
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	proposed_event = (icalcomponent *)data;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "_HUNT_");
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
	);
	CM_Free(msg);

	if (ird.cal == NULL) return;

	ical_conflicts_phase4(proposed_event, ird.cal, msgnum);
	icalcomponent_free(ird.cal);
}



/* 
 * Phase 2 of "hunt for conflicts" operation.
 * At this point we have a calendar object which represents the VEVENT that
 * is proposed for addition to the calendar.  Now hunt through the user's
 * calendar room, and output zero or more existing VEVENTs which conflict
 * with this one.
 */
void ical_hunt_for_conflicts(icalcomponent *cal) {
	char hold_rm[ROOMNAMELEN];

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (CtdlGetRoom(&CC->room, USERCALENDARROOM) != 0) {
		CtdlGetRoom(&CC->room, hold_rm);
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
	CtdlGetRoom(&CC->room, hold_rm);	/* return to saved room */

}



/*
 * Hunt for conflicts (Phase 1 -- retrieve the object and call Phase 2)
 */
void ical_conflicts(long msgnum, char *partnum) {
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) {
		cprintf("%d Message %ld not found\n",
			ERROR + ILLEGAL_VALUE,
			(long)msgnum
		);
		return;
	}

	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, partnum);
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
		);

	CM_Free(msg);

	if (ird.cal != NULL) {
		ical_hunt_for_conflicts(ird.cal);
		icalcomponent_free(ird.cal);
		return;
	}

	cprintf("%d No calendar object found\n", ERROR + ROOM_NOT_FOUND);
}



/*
 * Look for busy time in a VEVENT and add it to the supplied VFREEBUSY.
 *
 * fb			The VFREEBUSY component to which we are appending
 * top_level_cal	The top-level VCALENDAR component which contains a VEVENT to be added
 */
void ical_add_to_freebusy(icalcomponent *fb, icalcomponent *top_level_cal) {
	icalcomponent *cal;
	icalproperty *p;
	icalvalue *v;
	struct icalperiodtype this_event_period = icalperiodtype_null_period();
	icaltimetype dtstart;
	icaltimetype dtend;

	/* recur variables */
	icalproperty *rrule = NULL;
	struct icalrecurrencetype recur;
	icalrecur_iterator *ritr = NULL;
	struct icaldurationtype dur;
	int num_recur = 0;

	if (!top_level_cal) return;

	/* Find the VEVENT component containing an event */
	cal = icalcomponent_get_first_component(top_level_cal, ICAL_VEVENT_COMPONENT);
	if (!cal) return;

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

	/*
	 * Now begin calculating the event start and end times.
	 */
	p = icalcomponent_get_first_property(cal, ICAL_DTSTART_PROPERTY);
	if (!p) return;
	dtstart = icalproperty_get_dtstart(p);

	if (icaltime_is_utc(dtstart)) {
		dtstart.zone = icaltimezone_get_utc_timezone();
	}
	else {
		dtstart.zone = icalcomponent_get_timezone(top_level_cal,
			icalparameter_get_tzid(
				icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER)
			)
		);
		if (!dtstart.zone) {
			dtstart.zone = get_default_icaltimezone();
		}
	}

	dtend = icalcomponent_get_dtend(cal);
	if (!icaltime_is_null_time(dtend)) {
		dur = icaltime_subtract(dtend, dtstart);
	}
	else {
		memset (&dur, 0, sizeof(struct icaldurationtype));
	}

	/* Is a recurrence specified?  If so, get ready to process it... */
	rrule = ical_ctdl_get_subprop(cal, ICAL_RRULE_PROPERTY);
	if (rrule) {
		recur = icalproperty_get_rrule(rrule);
		ritr = icalrecur_iterator_new(recur, dtstart);
	}

	do {
		/* Convert the DTSTART and DTEND properties to an icalperiod. */
		this_event_period.start = dtstart;
	
		if (!icaltime_is_null_time(dtend)) {
			this_event_period.end = dtend;
		}

		/* Convert the timestamps to UTC.  It's ok to do this because we've already expanded
		 * recurrences and this data is never going to get used again.
		 */
		this_event_period.start = icaltime_convert_to_zone(
			this_event_period.start,
			icaltimezone_get_utc_timezone()
		);
		this_event_period.end = icaltime_convert_to_zone(
			this_event_period.end,
			icaltimezone_get_utc_timezone()
		);
	
		/* Now add it. */
		icalcomponent_add_property(fb, icalproperty_new_freebusy(this_event_period));

		/* Make sure the DTSTART property of the freebusy *list* is set to
		 * the DTSTART property of the *earliest event*.
		 */
		p = icalcomponent_get_first_property(fb, ICAL_DTSTART_PROPERTY);
		if (p == NULL) {
			icalcomponent_set_dtstart(fb, this_event_period.start);
		}
		else {
			if (icaltime_compare(this_event_period.start, icalcomponent_get_dtstart(fb)) < 0) {
				icalcomponent_set_dtstart(fb, this_event_period.start);
			}
		}
	
		/* Make sure the DTEND property of the freebusy *list* is set to
		 * the DTEND property of the *latest event*.
		 */
		p = icalcomponent_get_first_property(fb, ICAL_DTEND_PROPERTY);
		if (p == NULL) {
			icalcomponent_set_dtend(fb, this_event_period.end);
		}
		else {
			if (icaltime_compare(this_event_period.end, icalcomponent_get_dtend(fb)) > 0) {
				icalcomponent_set_dtend(fb, this_event_period.end);
			}
		}

		if (rrule) {
			dtstart = icalrecur_iterator_next(ritr);
			if (!icaltime_is_null_time(dtend)) {
				dtend = icaltime_add(dtstart, dur);
				dtend.zone = dtstart.zone;
				dtend.is_utc = dtstart.is_utc;
			}
			++num_recur;
		}

	} while ( (rrule) && (!icaltime_is_null_time(dtstart)) && (num_recur < MAX_RECUR) ) ;
	icalrecur_iterator_free(ritr);
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
	icalcomponent *fb;
	struct CtdlMessage *msg = NULL;
	struct ical_respond_data ird;

	fb = (icalcomponent *)data;		/* User-supplied data will be the VFREEBUSY component */

	msg = CtdlFetchMessage(msgnum, 1);
	if (msg == NULL) return;
	memset(&ird, 0, sizeof ird);
	strcpy(ird.desired_partnum, "_HUNT_");
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
		);
	CM_Free(msg);

	if (ird.cal) {
		ical_add_to_freebusy(fb, ird.cal);		/* Add VEVENT times to VFREEBUSY */
		icalcomponent_free(ird.cal);
	}
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
	recptypes *recp = NULL;
	char buf[256];
	char host[256];
	char type[256];
	int i = 0;
	int config_lines = 0;

	/* First try an exact match. */
	found_user = CtdlGetUser(&usbuf, who);

	/* If not found, try it as an unqualified email address. */
	if (found_user != 0) {
		strcpy(buf, who);
		recp = validate_recipients(buf, NULL, 0);
		syslog(LOG_DEBUG, "Trying <%s>\n", buf);
		if (recp != NULL) {
			if (recp->num_local == 1) {
				found_user = CtdlGetUser(&usbuf, recp->recp_local);
			}
			free_recipients(recp);
		}
	}

	/* If still not found, try it as an address qualified with the
	 * primary FQDN of this Citadel node.
	 */
	if (found_user != 0) {
		snprintf(buf, sizeof buf, "%s@%s", who, config.c_fqdn);
		syslog(LOG_DEBUG, "Trying <%s>\n", buf);
		recp = validate_recipients(buf, NULL, 0);
		if (recp != NULL) {
			if (recp->num_local == 1) {
				found_user = CtdlGetUser(&usbuf, recp->recp_local);
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
				syslog(LOG_DEBUG, "Trying <%s>\n", buf);
				recp = validate_recipients(buf, NULL, 0);
				if (recp != NULL) {
					if (recp->num_local == 1) {
						found_user = CtdlGetUser(&usbuf, recp->recp_local);
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

	CtdlMailboxName(calendar_room_name, sizeof calendar_room_name,
		&usbuf, USERCALENDARROOM);

	strcpy(hold_rm, CC->room.QRname);	/* save current room */

	if (CtdlGetRoom(&CC->room, calendar_room_name) != 0) {
		cprintf("%d Cannot open calendar\n", ERROR + ROOM_NOT_FOUND);
		CtdlGetRoom(&CC->room, hold_rm);
		return;
	}

	/* Create a VFREEBUSY subcomponent */
	syslog(LOG_DEBUG, "Creating VFREEBUSY component\n");
	fb = icalcomponent_new_vfreebusy();
	if (fb == NULL) {
		cprintf("%d Internal error: cannot allocate memory.\n",
			ERROR + INTERNAL_ERROR);
		CtdlGetRoom(&CC->room, hold_rm);
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
	syslog(LOG_DEBUG, "Adding busy time from events\n");
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
	syslog(LOG_DEBUG, "Encapsulating\n");
	encaps = ical_encapsulate_subcomponent(fb);
	if (encaps == NULL) {
		icalcomponent_free(fb);
		cprintf("%d Internal error: cannot allocate memory.\n",
			ERROR + INTERNAL_ERROR);
		CtdlGetRoom(&CC->room, hold_rm);
		return;
	}

	/* Set the method to PUBLISH */
	syslog(LOG_DEBUG, "Setting method\n");
	icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

	/* Serialize it */
	syslog(LOG_DEBUG, "Serializing\n");
	serialized_request = icalcomponent_as_ical_string_r(encaps);
	icalcomponent_free(encaps);	/* Don't need this anymore. */

	cprintf("%d Free/busy for %s\n", LISTING_FOLLOWS, usbuf.fullname);
	if (serialized_request != NULL) {
		client_write(serialized_request, strlen(serialized_request));
		free(serialized_request);
	}
	cprintf("\n000\n");

	/* Go back to the room from which we came... */
	CtdlGetRoom(&CC->room, hold_rm);
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
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_locate_part,		/* callback function */
		    NULL, NULL,
		    (void *) &ird,			/* user data */
		    0
	);
	CM_Free(msg);

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

			/* For VTIMEZONE components, suppress duplicates of the same tzid */

			if (icalcomponent_isa(c) == ICAL_VTIMEZONE_COMPONENT) {
				icalproperty *p = icalcomponent_get_first_property(c, ICAL_TZID_PROPERTY);
				if (p) {
					const char *tzid = icalproperty_get_tzid(p);
					if (!icalcomponent_get_timezone(encaps, tzid)) {
						icalcomponent_add_component(encaps,
									icalcomponent_new_clone(c));
					}
				}
			}

			/* All other types of components can go in verbatim */
			else {
				icalcomponent_add_component(encaps, icalcomponent_new_clone(c));
			}
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
		return;		/* Not an iCalendar-centric room */
	}

	encaps = icalcomponent_new_vcalendar();
	if (encaps == NULL) {
		syslog(LOG_ALERT, "ERROR: could not allocate component!\n");
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

	ser = icalcomponent_as_ical_string_r(encaps);
	icalcomponent_free(encaps);			/* Don't need this anymore. */
	client_write(ser, strlen(ser));
	free(ser);
	cprintf("\n000\n");
}


/*
 * Helper callback function for ical_putics() to discover which TZID's we need.
 * Simply put the tzid name string into a hash table.  After the callbacks are
 * done we'll go through them and attach the ones that we have.
 */
void ical_putics_grabtzids(icalparameter *param, void *data)
{
	const char *tzid = icalparameter_get_tzid(param);
	HashList *keys = (HashList *) data;
	
	if ( (keys) && (tzid) && (!IsEmptyStr(tzid)) ) {
		Put(keys, tzid, strlen(tzid), strdup(tzid), NULL);
	}
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
	icalcomponent *encaps = NULL;
	HashList *tzidlist = NULL;
	HashPos *HashPos;
	void *Value;
	const char *Key;
	long len;

	/* Only allow this operation if we're in a room containing a calendar or tasks view */
	if ( (CC->room.QRdefaultview != VIEW_CALENDAR)
	   &&(CC->room.QRdefaultview != VIEW_TASKS) ) {
		cprintf("%d Not a calendar room\n", ERROR+NOT_HERE);
		return;
	}

	/* Only allow this operation if we have permission to overwrite the existing calendar */
	if (!CtdlDoIHavePermissionToDeleteMessagesFromThisRoom()) {
		cprintf("%d Permission denied.\n", ERROR+HIGHER_ACCESS_REQUIRED);
		return;
	}

	cprintf("%d Transmit data now\n", SEND_LISTING);
	calstream = CtdlReadMessageBody(HKEY("000"), config.c_maxmsglen, NULL, 0, 0);
	if (calstream == NULL) {
		return;
	}

	cal = icalcomponent_new_from_string(calstream);
	free(calstream);

	/* We got our data stream -- now do something with it. */

	/* Delete the existing messages in the room, because we are overwriting
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

			/* Non-VTIMEZONE components each get written as individual messages.
			 * But we also need to attach the relevant VTIMEZONE components to them.
			 */
			if ( (icalcomponent_isa(c) != ICAL_VTIMEZONE_COMPONENT)
			   && (encaps = icalcomponent_new_vcalendar()) ) {
				icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));
				icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));
				icalcomponent_set_method(encaps, ICAL_METHOD_PUBLISH);

				/* Attach any needed timezones here */
				tzidlist = NewHash(1, NULL);
				if (tzidlist) {
					icalcomponent_foreach_tzid(c, ical_putics_grabtzids, tzidlist);
				}
				HashPos = GetNewHashPos(tzidlist, 0);

				while (GetNextHashPos(tzidlist, HashPos, &len, &Key, &Value)) {
					syslog(LOG_DEBUG, "Attaching timezone '%s'\n", (char*) Value);
					icaltimezone *t = NULL;

					/* First look for a timezone attached to the original calendar */
					t = icalcomponent_get_timezone(cal, Value);

					/* Try built-in tzdata if the right one wasn't attached */
					if (!t) {
						t = icaltimezone_get_builtin_timezone(Value);
					}

					/* I've got a valid timezone to attach. */
					if (t) {
						icalcomponent_add_component(encaps,
							icalcomponent_new_clone(
								icaltimezone_get_component(t)
							)
						);
					}

				}
				DeleteHashPos(&HashPos);
				DeleteHash(&tzidlist);

				/* Now attach the component itself (usually a VEVENT or VTODO) */
				icalcomponent_add_component(encaps, icalcomponent_new_clone(c));

				/* Write it to the message store */
				ical_write_to_cal(NULL, encaps);
				icalcomponent_free(encaps);
			}
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
void ical_CtdlCreateRoom(void)
{
	struct ctdlroom qr;
	visit vbuf;

	/* Create the calendar room if it doesn't already exist */
	CtdlCreateRoom(USERCALENDARROOM, 4, "", 0, 1, 0, VIEW_CALENDAR);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (CtdlGetRoomLock(&qr, USERCALENDARROOM)) {
		syslog(LOG_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_CALENDAR;	/* 3 = calendar view */
	CtdlPutRoomLock(&qr);

	/* Set the view to a calendar view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_CALENDAR;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	/* Create the tasks list room if it doesn't already exist */
	CtdlCreateRoom(USERTASKSROOM, 4, "", 0, 1, 0, VIEW_TASKS);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (CtdlGetRoomLock(&qr, USERTASKSROOM)) {
		syslog(LOG_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_TASKS;
	CtdlPutRoomLock(&qr);

	/* Set the view to a task list view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_TASKS;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	/* Create the notes room if it doesn't already exist */
	CtdlCreateRoom(USERNOTESROOM, 4, "", 0, 1, 0, VIEW_NOTES);

	/* Set expiration policy to manual; otherwise objects will be lost! */
	if (CtdlGetRoomLock(&qr, USERNOTESROOM)) {
		syslog(LOG_CRIT, "Couldn't get the user calendar room!\n");
		return;
	}
	qr.QRep.expire_mode = EXPIRE_MANUAL;
	qr.QRdefaultview = VIEW_NOTES;
	CtdlPutRoomLock(&qr);

	/* Set the view to a notes view */
	CtdlGetRelationship(&vbuf, &CC->user, &qr);
	vbuf.v_view = VIEW_NOTES;
	CtdlSetRelationship(&vbuf, &CC->user, &qr);

	return;
}


/*
 * ical_send_out_invitations() is called by ical_saving_vevent() when it finds a VEVENT.
 *
 * top_level_cal is the highest available level calendar object.
 * cal is the subcomponent containing the VEVENT.
 *
 * Note: if you change the encapsulation code here, change it in WebCit's ical_encapsulate_subcomponent()
 */
void ical_send_out_invitations(icalcomponent *top_level_cal, icalcomponent *cal) {
	icalcomponent *the_request = NULL;
	char *serialized_request = NULL;
	icalcomponent *encaps = NULL;
	char *request_message_text = NULL;
	struct CtdlMessage *msg = NULL;
	recptypes *valid = NULL;
	char attendees_string[SIZ];
	int num_attendees = 0;
	char this_attendee[256];
	icalproperty *attendee = NULL;
	char summary_string[SIZ];
	icalproperty *summary = NULL;
	size_t reqsize;
	icalproperty *p;
	struct icaltimetype t;
	const icaltimezone *attached_zones[5] = { NULL, NULL, NULL, NULL, NULL };
	int i;
	const icaltimezone *z;
	int num_zones_attached = 0;
	int zone_already_attached;
	icalparameter *tzidp = NULL;
	const char *tzidc = NULL;

	if (cal == NULL) {
		syslog(LOG_ERR, "ERROR: trying to reply to NULL event?\n");
		return;
	}


	/* If this is a VCALENDAR component, look for a VEVENT subcomponent. */
	if (icalcomponent_isa(cal) == ICAL_VCALENDAR_COMPONENT) {
		ical_send_out_invitations(top_level_cal,
			icalcomponent_get_first_component(
				cal, ICAL_VEVENT_COMPONENT
			)
		);
		return;
	}

	/* Clone the event */
	the_request = icalcomponent_new_clone(cal);
	if (the_request == NULL) {
		syslog(LOG_ERR, "ERROR: cannot clone calendar object\n");
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
		const char *ch = icalproperty_get_attendee(attendee);
		if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {
			safestrncpy(this_attendee, ch + 7, sizeof(this_attendee));
			
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

	syslog(LOG_DEBUG, "<%d> attendees: <%s>\n", num_attendees, attendees_string);

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
		syslog(LOG_ALERT, "ERROR: could not allocate component!\n");
		icalcomponent_free(the_request);
		return;
	}

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/* Set the method to REQUEST */
	icalcomponent_set_method(encaps, ICAL_METHOD_REQUEST);

	/* Look for properties containing timezone parameters, to see if we need to attach VTIMEZONEs */
	for (p = icalcomponent_get_first_property(the_request, ICAL_ANY_PROPERTY);
	     p != NULL;
	     p = icalcomponent_get_next_property(the_request, ICAL_ANY_PROPERTY))
	{
		if ( (icalproperty_isa(p) == ICAL_COMPLETED_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_CREATED_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DATEMAX_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DATEMIN_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DTEND_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DTSTAMP_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DTSTART_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_DUE_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_EXDATE_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_LASTMODIFIED_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_MAXDATE_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_MINDATE_PROPERTY)
		  || (icalproperty_isa(p) == ICAL_RECURRENCEID_PROPERTY)
		) {
			t = icalproperty_get_dtstart(p);	// it's safe to use dtstart for all of them

			/* Determine the tzid in order for some of the conditions below to work */
			tzidp = icalproperty_get_first_parameter(p, ICAL_TZID_PARAMETER);
			if (tzidp) {
				tzidc = icalparameter_get_tzid(tzidp);
			}
			else {
				tzidc = NULL;
			}

			/* First see if there's a timezone attached to the data structure itself */
			if (icaltime_is_utc(t)) {
				z = icaltimezone_get_utc_timezone();
			}
			else {
				z = icaltime_get_timezone(t);
			}

			/* If not, try to determine the tzid from the parameter using attached zones */
			if ((!z) && (tzidc)) {
				z = icalcomponent_get_timezone(top_level_cal, tzidc);
			}

			/* Still no good?  Try our internal database */
			if ((!z) && (tzidc)) {
				z = icaltimezone_get_builtin_timezone_from_tzid(tzidc);
			}

			if (z) {
				/* We have a valid timezone.  Good.  Now we need to attach it. */

				zone_already_attached = 0;
				for (i=0; i<5; ++i) {
					if (z == attached_zones[i]) {
						/* We've already got this one, no need to attach another. */
						++zone_already_attached;
					}
				}
				if ((!zone_already_attached) && (num_zones_attached < 5)) {
					/* This is a new one, so attach it. */
					attached_zones[num_zones_attached++] = z;
				}

				icalproperty_set_parameter(p,
					icalparameter_new_tzid(icaltimezone_get_tzid(z))
				);
			}
		}
	}

	/* Encapsulate any timezones we need */
	if (num_zones_attached > 0) for (i=0; i<num_zones_attached; ++i) {
		icalcomponent *zc;
		zc = icalcomponent_new_clone(icaltimezone_get_component(attached_zones[i]));
		icalcomponent_add_component(encaps, zc);
	}

	/* Here we go: encapsulate the VEVENT into the VCALENDAR.  We now no longer
	 * are responsible for "the_request"'s memory -- it will be freed
	 * when we free "encaps".
	 */
	icalcomponent_add_component(encaps, the_request);

	/* Serialize it */
	serialized_request = icalcomponent_as_ical_string_r(encaps);
	icalcomponent_free(encaps);	/* Don't need this anymore. */
	if (serialized_request == NULL) return;

	reqsize = strlen(serialized_request) + SIZ;
	request_message_text = malloc(reqsize);
	if (request_message_text != NULL) {
		snprintf(request_message_text, reqsize,
			"Content-type: text/calendar\r\n\r\n%s\r\n",
			serialized_request
		);

		msg = CtdlMakeMessage(
			&CC->user,
			NULL,			/* No single recipient here */
			NULL,			/* No single recipient here */
			CC->room.QRname,
			0,
			FMT_RFC822,
			NULL,
			NULL,
			summary_string,		/* Use summary for subject */
			NULL,
			request_message_text,
			NULL
		);
	
		if (msg != NULL) {
			valid = validate_recipients(attendees_string, NULL, 0);
			CtdlSubmitMsg(msg, valid, "", QP_EADDR);
			CM_Free(msg);
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
 * This function is recursive.  The caller can simply supply the same object
 * as both arguments.  When it recurses it will alter the second argument
 * while holding on to the top level object.  This allows us to go back and
 * grab things like time zones which might be attached.
 *
 */
void ical_saving_vevent(icalcomponent *top_level_cal, icalcomponent *cal) {
	icalcomponent *c;
	icalproperty *organizer = NULL;
	char organizer_string[SIZ];

	syslog(LOG_DEBUG, "ical_saving_vevent() has been called!\n");

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
				ical_send_out_invitations(top_level_cal, cal);
			}
		}
	}

	/* If the component has subcomponents, recurse through them. */
	for (c = icalcomponent_get_first_component(cal, ICAL_ANY_COMPONENT);
	    (c != NULL);
	    c = icalcomponent_get_next_component(cal, ICAL_ANY_COMPONENT)) {
		/* Recursively process subcomponent */
		ical_saving_vevent(top_level_cal, c);
	}

}



/*
 * Back end for ical_obj_beforesave()
 * This hunts for the UID of the calendar event (becomes Citadel msg EUID),
 * the summary of the event (becomes message subject),
 * and the start time (becomes message date/time).
 */
void ical_obj_beforesave_backend(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	const char* pch;
	icalcomponent *cal, *nested_event, *nested_todo, *whole_cal;
	icalproperty *p;
	char new_uid[256] = "";
	struct CtdlMessage *msg = (struct CtdlMessage *) cbuserdata;

	if (!msg) return;

	/* We're only interested in calendar data. */
	if (  (strcasecmp(cbtype, "text/calendar"))
	   && (strcasecmp(cbtype, "application/ics")) ) {
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

			/* Set the message EUID to the iCalendar UID */

			p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
			if (p == NULL) {
				/* If there's no uid we must generate one */
				generate_uuid(new_uid);
				icalcomponent_add_property(cal, icalproperty_new_uid(new_uid));
				p = ical_ctdl_get_subprop(cal, ICAL_UID_PROPERTY);
			}
			if (p != NULL) {
				pch = icalproperty_get_comment(p);
				if (!IsEmptyStr(pch)) {
					CM_SetField(msg, eExclusiveID, pch, strlen(pch));
					syslog(LOG_DEBUG, "Saving calendar UID <%s>\n", pch);
				}
			}

			/* Set the message subject to the iCalendar summary */

			p = ical_ctdl_get_subprop(cal, ICAL_SUMMARY_PROPERTY);
			if (p != NULL) {
				pch = icalproperty_get_comment(p);
				if (!IsEmptyStr(pch)) {
					char *subj;

					subj = rfc2047encode(pch, strlen(pch));
					CM_SetAsField(msg, eMsgSubject, &subj, strlen(subj));
				}
			}

			/* Set the message date/time to the iCalendar start time */

			p = ical_ctdl_get_subprop(cal, ICAL_DTSTART_PROPERTY);
			if (p != NULL) {
				time_t idtstart;
				idtstart = icaltime_as_timet(icalproperty_get_dtstart(p));
				if (idtstart > 0) {
					CM_SetFieldLONG(msg, eTimestamp, idtstart);
				}
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
 * MIME types other than text/calendar in "calendar" or "tasks" rooms).
 *
 * If the message is being saved, we also set various message header fields
 * using data found in the iCalendar object.
 */
int ical_obj_beforesave(struct CtdlMessage *msg, recptypes *recp)
{
	/* First determine if this is a calendar or tasks room */
	if (  (CC->room.QRdefaultview != VIEW_CALENDAR)
	   && (CC->room.QRdefaultview != VIEW_TASKS)
	) {
		return(0);		/* Not an iCalendar-centric room */
	}

	/* It must be an RFC822 message! */
	if (msg->cm_format_type != 4) {
		syslog(LOG_DEBUG, "Rejecting non-RFC822 message\n");
		return(1);		/* You tried to save a non-RFC822 message! */
	}

	if (CM_IsEmpty(msg, eMesageText)) {
		return(1);		/* You tried to save a null message! */
	}

	/* Do all of our lovely back-end parsing */
	mime_parser(CM_RANGE(msg, eMesageText),
		    *ical_obj_beforesave_backend,
		    NULL, NULL,
		    (void *)msg,
		    0
		);

	return(0);
}


/*
 * Things we need to do after saving a calendar event.
 */
void ical_obj_aftersave_backend(char *name, char *filename, char *partnum,
		char *disp, void *content, char *cbtype, char *cbcharset, size_t length,
		char *encoding, char *cbid, void *cbuserdata)
{
	icalcomponent *cal;

	/* We're only interested in calendar items here. */
	if (  (strcasecmp(cbtype, "text/calendar"))
	   && (strcasecmp(cbtype, "application/ics")) ) {
		return;
	}

	/* Hunt for the UID and drop it in
	 * the "user data" pointer for the MIME parser.  When
	 * ical_obj_beforesave() sees it there, it'll set the Exclusive msgid
	 * to that string.
	 */
	if (  (!strcasecmp(cbtype, "text/calendar"))
	   || (!strcasecmp(cbtype, "application/ics")) ) {
		cal = icalcomponent_new_from_string(content);
		if (cal != NULL) {
			ical_saving_vevent(cal, cal);
			icalcomponent_free(cal);
		}
	}
}


/* 
 * Things we need to do after saving a calendar event.
 * (This will start back end tasks such as automatic generation of invitations,
 * if such actions are appropriate.)
 */
int ical_obj_aftersave(struct CtdlMessage *msg, recptypes *recp)
{
	char roomname[ROOMNAMELEN];

	/*
	 * If this isn't the Calendar> room, no further action is necessary.
	 */

	/* First determine if this is our room */
	CtdlMailboxName(roomname, sizeof roomname, &CC->user, USERCALENDARROOM);
	if (strcasecmp(roomname, CC->room.QRname)) {
		return(0);	/* Not the Calendar room -- don't do anything. */
	}

	/* It must be an RFC822 message! */
	if (msg->cm_format_type != 4) return(1);

	/* Reject null messages */
	if (CM_IsEmpty(msg, eMesageText)) return(1);
	
	/* Now recurse through it looking for our icalendar data */
	mime_parser(CM_RANGE(msg, eMesageText),
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
	const char *ch;

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
		ch =  icalproperty_get_attendee(p);
		if ((ch != NULL) && 
		    !strncasecmp(ch, "MAILTO:", 7)) {

			/* screen name or email address */
			safestrncpy(buf, ch + 7, sizeof(buf));
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
 * Function to output iCalendar data as plain text.  Nobody uses MSG0
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

	ical_fixed_output_backend(cal, 0);

	/* Free the memory we obtained from libical's constructor */
	icalcomponent_free(cal);
}



void serv_calendar_destroy(void)
{
	icaltimezone_free_builtin_timezones();
}

/*
 * Register this module with the Citadel server.
 */
CTDL_MODULE_INIT(calendar)
{
	if (!threading)
	{

		/* Tell libical to return errors instead of aborting if it gets bad data */
		icalerror_errors_are_fatal = 0;

		/* Use our own application prefix in tzid's generated from system tzdata */
		icaltimezone_set_tzid_prefix("/citadel.org/");

		/* Initialize our hook functions */
		CtdlRegisterMessageHook(ical_obj_beforesave, EVT_BEFORESAVE);
		CtdlRegisterMessageHook(ical_obj_aftersave, EVT_AFTERSAVE);
		CtdlRegisterSessionHook(ical_CtdlCreateRoom, EVT_LOGIN, PRIO_LOGIN + 1);
		CtdlRegisterProtoHook(cmd_ical, "ICAL", "Citadel iCal commands");
		CtdlRegisterSessionHook(ical_session_startup, EVT_START, PRIO_START + 1);
		CtdlRegisterSessionHook(ical_session_shutdown, EVT_STOP, PRIO_STOP + 80);
		CtdlRegisterFixedOutputHook("text/calendar", ical_fixed_output);
		CtdlRegisterFixedOutputHook("application/ics", ical_fixed_output);
		CtdlRegisterCleanupHook(serv_calendar_destroy);
	}
	
	/* return our module name for the log */
	return "calendar";
}
