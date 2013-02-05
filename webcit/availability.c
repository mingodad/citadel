/*
 * Copyright (c) 1996-2012 by the citadel.org team
 *
 * This program is open source software.  You can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#include "webcit.h"
#include "webserver.h"
#include "calendar.h"

/*
 * Utility function to fetch a VFREEBUSY type of thing for any specified user.
 */
icalcomponent *get_freebusy_for_user(char *who) {
	long nLines;
	char buf[SIZ];
	StrBuf *serialized_fb = NewStrBuf();
	icalcomponent *fb = NULL;

	serv_printf("ICAL freebusy|%s", who);
	serv_getln(buf, sizeof buf);
	if (buf[0] == '1') {
		read_server_text(serialized_fb, &nLines);
	}

	if (serialized_fb == NULL) {
		return NULL;
	}
	
	fb = icalcomponent_new_from_string(ChrPtr(serialized_fb));
	FreeStrBuf(&serialized_fb);
	if (fb == NULL) {
		return NULL;
	}

	return(fb);
}


/*
 * Check to see if two events overlap.  
 * (This function is used in both Citadel and WebCit.  If you change it in
 * one place, change it in the other.  We should seriously consider moving
 * this function upstream into libical.)
 *
 * Returns nonzero if they do overlap.
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
 * Back end function for check_attendee_availability()
 * This one checks an individual attendee against a supplied
 * event start and end time.  All these fields have already been
 * broken out.  
 *
 * attendee_string	name of the attendee
 * event_start		start time of the event to check
 * event_end		end time of the event to check
 *
 * The result is placed in 'annotation'.
 */
void check_individual_attendee(char *attendee_string,
				struct icaltimetype event_start,
				struct icaltimetype event_end,
				char *annotation) {

	icalcomponent *fbc = NULL;
	icalcomponent *fb = NULL;
	icalproperty *thisfb = NULL;
	struct icalperiodtype period;

	/*
	 * Set to 'unknown' right from the beginning.  Unless we learn
	 * something else, that's what we'll go with.
	 */
	strcpy(annotation, _("availability unknown"));

	fbc = get_freebusy_for_user(attendee_string);
	if (fbc == NULL) {
		return;
	}

	/*
	 * Make sure we're looking at a VFREEBUSY by itself.  What we're probably
	 * looking at initially is a VFREEBUSY encapsulated in a VCALENDAR.
	 */
	if (icalcomponent_isa(fbc) == ICAL_VCALENDAR_COMPONENT) {
		fb = icalcomponent_get_first_component(fbc, ICAL_VFREEBUSY_COMPONENT);
	}
	else if (icalcomponent_isa(fbc) == ICAL_VFREEBUSY_COMPONENT) {
		fb = fbc;
	}

	/* Iterate through all FREEBUSY's looking for conflicts. */
	if (fb != NULL) {

		strcpy(annotation, _("free"));

		for (thisfb = icalcomponent_get_first_property(fb, ICAL_FREEBUSY_PROPERTY);
		    thisfb != NULL;
		    thisfb = icalcomponent_get_next_property(fb, ICAL_FREEBUSY_PROPERTY) ) {

			/** Do the check */
			period = icalproperty_get_freebusy(thisfb);
			if (ical_ctdl_is_overlap(period.start, period.end,
			   event_start, event_end)) {
				strcpy(annotation, _("BUSY"));
			}

		}
	}

	icalcomponent_free(fbc);
}




/*
 * Check the availability of all attendees for an event (when possible)
 * and annotate accordingly.
 *
 * vevent	the event which should be compared with attendees calendar
 */
void check_attendee_availability(icalcomponent *vevent) {
	icalproperty *attendee = NULL;
	icalproperty *dtstart_p = NULL;
	icalproperty *dtend_p = NULL;
	struct icaltimetype dtstart_t;
	struct icaltimetype dtend_t;
	char attendee_string[SIZ];
	char annotated_attendee_string[SIZ];
	char annotation[SIZ];
	const char *ch;

	if (vevent == NULL) {
		return;
	}

	/*
	 * If we're looking at a fully encapsulated VCALENDAR
	 * rather than a VEVENT component, attempt to use the first
	 * relevant VEVENT subcomponent.  If there is none, the
	 * NULL returned by icalcomponent_get_first_component() will
	 * tell the next iteration of this function to create a
	 * new one.
	 */
	if (icalcomponent_isa(vevent) == ICAL_VCALENDAR_COMPONENT) {
		check_attendee_availability(
			icalcomponent_get_first_component(
				vevent, ICAL_VEVENT_COMPONENT
			)
		);
		return;
	}

	ical_dezonify(vevent);		/**< Convert everything to UTC */

	/*
	 * Learn the start and end times.
	 */
	dtstart_p = icalcomponent_get_first_property(vevent, ICAL_DTSTART_PROPERTY);
	if (dtstart_p != NULL) dtstart_t = icalproperty_get_dtstart(dtstart_p);

	dtend_p = icalcomponent_get_first_property(vevent, ICAL_DTEND_PROPERTY);
	if (dtend_p != NULL) dtend_t = icalproperty_get_dtend(dtend_p);

	/*
	 * Iterate through attendees.
	 */
	for (attendee = icalcomponent_get_first_property(vevent, ICAL_ATTENDEE_PROPERTY);
	    attendee != NULL;
	    attendee = icalcomponent_get_next_property(vevent, ICAL_ATTENDEE_PROPERTY)) {
		ch = icalproperty_get_attendee(attendee);
		if ((ch != NULL) && !strncasecmp(ch, "MAILTO:", 7)) {

			/** screen name or email address */
			safestrncpy(attendee_string, ch + 7, sizeof(attendee_string));
			striplt(attendee_string);

			check_individual_attendee(attendee_string,
						dtstart_t, dtend_t,
						annotation);

			/** Replace the attendee name with an annotated one. */
			snprintf(annotated_attendee_string, sizeof annotated_attendee_string,
				"MAILTO:%s (%s)", attendee_string, annotation);
			icalproperty_set_attendee(attendee, annotated_attendee_string);

		}
	}

}

