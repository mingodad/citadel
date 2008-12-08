/*
 * $Id$
 *
 * Miscellaneous functions which handle calendar components.
 */

#include "webcit.h"
#include "webserver.h"
#include "time.h"

/** Hour strings */
char *hourname[] = {
	"12am", "1am", "2am", "3am", "4am", "5am", "6am",
	"7am", "8am", "9am", "10am", "11am", "12pm",
	"1pm", "2pm", "3pm", "4pm", "5pm", "6pm",
	"7pm", "8pm", "9pm", "10pm", "11pm"
};

/**
 * \brief display and edit date/time
 * The display_icaltimetype_as_webform() and icaltime_from_webform() functions
 * handle the display and editing of date/time properties in web pages.  The
 * first one converts an icaltimetype into valid HTML markup -- a series of form
 * fields for editing the date and time.  When the user submits the form, the
 * results can be fed back into the second function, which turns it back into
 * an icaltimetype.  The "prefix" string required by both functions is prepended
 * to all field names.  This allows a form to contain more than one date/time
 * property (for example, a start and end time) by ensuring the field names are
 * unique within the form.
 *
 * \todo NOTE: These functions assume that the icaltimetype being edited is in UTC, and
 * will convert to/from local time for editing.  "local" in this case is assumed
 * to be the time zone in which the WebCit server is running.  A future improvement
 * might be to allow the user to specify his/her timezone.
 * \param t the time we want to parse
 * \param prefix ???? \todo
 */


void display_icaltimetype_as_webform(struct icaltimetype *t, char *prefix) {
	int i;
	time_t now;
	struct tm tm_now;
	int this_year;
	time_t tt;
	struct tm tm;
	int all_day_event = 0;
	int time_format;
	char timebuf[32];
	
	time_format = get_time_format_cached ();

	now = time(NULL);
	localtime_r(&now, &tm_now);
	this_year = tm_now.tm_year + 1900;

	if (t == NULL) return;
	if (t->is_date) all_day_event = 1;
	tt = icaltime_as_timet(*t);
	if (all_day_event) {
		gmtime_r(&tt, &tm);
	}
	else {
		localtime_r(&tt, &tm);
	}

	wprintf("<input type=\"text\" name=\"");
	wprintf(prefix);
	wprintf("\" id=\"");
	wprintf(prefix);
	wprintf("\" value=\"");
	wc_strftime(timebuf, 32, "%d/%m/%Y", &tm);
	wprintf(timebuf);
	wprintf("\">");
	wprintf("<script type=\"text/javascript\">");
	wprintf("attachDatePicker('");
	wprintf(prefix);
	wprintf("');\n");
	wprintf("</script>");
	wprintf(_("Hour: "));
	wprintf("<SELECT NAME=\"%s_hour\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=23; ++i) {

		if (time_format == WC_TIMEFORMAT_24) {
			wprintf("<OPTION %s VALUE=\"%d\">%d</OPTION>\n",
				((tm.tm_hour == i) ? "SELECTED" : ""),
				i, i
				);
		}
		else {
			wprintf("<OPTION %s VALUE=\"%d\">%s</OPTION>\n",
				((tm.tm_hour == i) ? "SELECTED" : ""),
				i, hourname[i]
				);
		}

	}
	wprintf("</SELECT>\n");

	wprintf(_("Minute: "));
	wprintf("<SELECT NAME=\"%s_minute\" SIZE=\"1\">\n", prefix);
	for (i=0; i<=59; ++i) {
		if ( (i % 5 == 0) || (tm.tm_min == i) ) {
			wprintf("<OPTION %s VALUE=\"%d\">:%02d</OPTION>\n",
				((tm.tm_min == i) ? "SELECTED" : ""),
				i, i
				);
		}
	}
	wprintf("</SELECT>\n");
}

/**
 *\brief Get time from form
 * get the time back from the user and convert it into internal structs.
 * \param t our time element
 * \param prefix whats that\todo ????
 */
void icaltime_from_webform(struct icaltimetype *t, char *prefix) {
 	char datebuf[32];
  	char vname[32];
 	struct tm tm;
 	/* Stuff tm with some zero values */
	tm.tm_year = 0;
 	tm.tm_sec = 0;
 	tm.tm_min = 0;
 	tm.tm_hour = 0;
 	tm.tm_mday = 0;
 	tm.tm_mon = 0;
 	int hour = 0;
 	int minute = 0;
  	struct icaltimetype t2;
 	
	
 	strptime((char*)BSTR(prefix), "%d/%m/%Y", &tm);
 	sprintf(vname, "%s_hour", prefix);      hour = IBSTR(vname);
	sprintf(vname, "%s_minute", prefix);    minute = IBSTR(vname);
 	tm.tm_hour = hour;
 	tm.tm_min = minute;
	strftime(&datebuf[0], 32, "%Y%m%dT%H%M%S", &tm);
 	t2 = icaltime_from_string(datebuf);
  	memcpy(t, &t2, sizeof(struct icaltimetype));
}


/**
 *\brief Get time from form
 * get the time back from the user and convert it into internal structs.
 * \param t our time element
 * \param prefix whats that\todo ????
 */

void icaltime_from_webform_dateonly(struct icaltimetype *t, char *prefix) {
 	struct tm tm;
 	/* Stuff tm with some zero values */
 	tm.tm_sec = 0;
 	tm.tm_min = 0;
	tm.tm_hour = 0;
 	tm.tm_mday = 0;
 	tm.tm_mon = 0;
 	time_t tm_t;
 	struct icaltimetype t2; 	
 	strptime((char *)BSTR(prefix), "%d/%m/%Y", &tm);
 	tm_t = mktime(&tm);
 	t2 = icaltime_from_timet(tm_t, 1);
 	memcpy(t, &t2, sizeof(struct icaltimetype));
}


/**
 * \brief Render PAPSTAT
 * Render a PARTSTAT parameter as a string (and put it in parentheses)
 * \param buf the string to put it to
 * \param attendee the attendee to textify
 */
void partstat_as_string(char *buf, icalproperty *attendee) {
	icalparameter *partstat_param;
	icalparameter_partstat partstat;

	strcpy(buf, _("(status unknown)"));

	partstat_param = icalproperty_get_first_parameter(
		attendee,
		ICAL_PARTSTAT_PARAMETER
		);
	if (partstat_param == NULL) {
		return;
	}

	partstat = icalparameter_get_partstat(partstat_param);
	switch(partstat) {
	case ICAL_PARTSTAT_X:
		strcpy(buf, "(x)");
		break;
	case ICAL_PARTSTAT_NEEDSACTION:
		strcpy(buf, _("(needs action)"));
		break;
	case ICAL_PARTSTAT_ACCEPTED:
		strcpy(buf, _("(accepted)"));
		break;
	case ICAL_PARTSTAT_DECLINED:
		strcpy(buf, _("(declined)"));
		break;
	case ICAL_PARTSTAT_TENTATIVE:
		strcpy(buf, _("(tenative)"));
		break;
	case ICAL_PARTSTAT_DELEGATED:
		strcpy(buf, _("(delegated)"));
		break;
	case ICAL_PARTSTAT_COMPLETED:
		strcpy(buf, _("(completed)"));
		break;
	case ICAL_PARTSTAT_INPROCESS:
		strcpy(buf, _("(in process)"));
		break;
	case ICAL_PARTSTAT_NONE:
		strcpy(buf, _("(none)"));
		break;
	}
}


/**
 * \brief embedd
 * Utility function to encapsulate a subcomponent into a full VCALENDAR
 * \param subcomp the component to encapsulate
 * \returns the meta object ???
 */
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp) {
	icalcomponent *encaps;

	/* lprintf(9, "ical_encapsulate_subcomponent() called\n"); */

	if (subcomp == NULL) {
		lprintf(3, "ERROR: called with NULL argument!\n");
		return NULL;
	}

	/**
	 * If we're already looking at a full VCALENDAR component,
	 * don't bother ... just return itself.
	 */
	if (icalcomponent_isa(subcomp) == ICAL_VCALENDAR_COMPONENT) {
		return subcomp;
	}

	/** Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
	if (encaps == NULL) {
		lprintf(3, "%s:%d: Error - could not allocate component!\n",
			__FILE__, __LINE__);
		return NULL;
	}

	/** Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/** Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/** Encapsulate the subcomponent inside */
	/* lprintf(9, "Doing the encapsulation\n"); */
	icalcomponent_add_component(encaps, subcomp);

	/** Convert all timestamps to UTC so we don't have to deal with
	 * stupid VTIMEZONE crap.
	 */
	ical_dezonify(encaps);

	/** Return the object we just created. */
	return(encaps);
}


