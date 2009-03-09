/*
 * $Id$
 *
 * Miscellaneous functions which handle calendar components.
 */

#include "webcit.h"
#include "webserver.h"
#include "time.h"

/* Hour strings */
char *hourname[] = {
	"12am", "1am", "2am", "3am", "4am", "5am", "6am",
	"7am", "8am", "9am", "10am", "11am", "12pm",
	"1pm", "2pm", "3pm", "4pm", "5pm", "6pm",
	"7pm", "8pm", "9pm", "10pm", "11pm"
};

/*
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
 * NOTE: These functions assume that the icaltimetype being edited is in UTC, and
 * will convert to/from local time for editing.  "local" in this case is assumed
 * to be the time zone in which the WebCit server is running.  A future improvement
 * might be to allow the user to specify his/her timezone.
 */

void display_icaltimetype_as_webform(struct icaltimetype *t, char *prefix, int date_only) {
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
	wprintf("\" size=\"10\" maxlength=\"10\" value=\"");
	wc_strftime(timebuf, 32, "%Y-%m-%d", &tm);
	wprintf(timebuf);
	wprintf("\">");

	StrBufAppendPrintf(WC->trailing_javascript, "attachDatePicker('");
	StrBufAppendPrintf(WC->trailing_javascript, prefix);
	StrBufAppendPrintf(WC->trailing_javascript, "', '%s');\n", get_selected_language());

	/* If we're editing a date only, we still generate the time boxes, but we hide them.
	 * This keeps the data model consistent.
	 */
	if (date_only) {
		wprintf("<div style=\"display:none\">");
	}

	wprintf("<span ID=\"");
	wprintf(prefix);
	wprintf("_time\">");
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
	wprintf("</SELECT></span>\n");

	if (date_only) {
		wprintf("</div>");
	}
}

/*
 * Get date/time from a web form and convert it into an icaltimetype struct.
 */
void icaltime_from_webform(struct icaltimetype *t, char *prefix) {
  	char vname[32];

	if (!t) return;

 	/* Stuff with zero values */
	memset(t, 0, sizeof(struct icaltimetype));

	/* Get the year/month/date all in one shot -- it will be in ISO YYYY-MM-DD format */
	sscanf((char*)BSTR(prefix), "%04d-%02d-%02d", &t->year, &t->month, &t->day);

	/* hour */
 	sprintf(vname, "%s_hour", prefix);
	t->hour = IBSTR(vname);

	/* minute */
	sprintf(vname, "%s_minute", prefix);
	t->minute = IBSTR(vname);

	/* time zone is set to the default zone for this server */
	t->is_utc = 0;
	t->is_date = 0;
	t->zone = get_default_icaltimezone();
}


/*
 * Get date (no time) from a web form and convert it into an icaltimetype struct.
 */
void icaltime_from_webform_dateonly(struct icaltimetype *t, char *prefix) {
	if (!t) return;

 	/* Stuff with zero values */
	memset(t, 0, sizeof(struct icaltimetype));

	/* Get the year/month/date all in one shot -- it will be in ISO YYYY-MM-DD format */
	sscanf((char*)BSTR(prefix), "%04d-%02d-%02d", &t->year, &t->month, &t->day);

	/* time zone is set to the default zone for this server */
	t->is_utc = 1;
	t->is_date = 1;
}


/*
 * Render a PARTSTAT parameter as a string (and put it in parentheses)
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

/*
 * Utility function to encapsulate a subcomponent into a full VCALENDAR.
 *
 * We also scan for any date/time properties that reference timezones, and attach
 * those timezones along with the supplied subcomponent.  (Increase the size of the array if you need to.)
 *
 * Note: if you change anything here, change it in Citadel server's ical_send_out_invitations() too.
 */
icalcomponent *ical_encapsulate_subcomponent(icalcomponent *subcomp) {
	icalcomponent *encaps;
	icalproperty *p;
	struct icaltimetype t;
	const icaltimezone *attached_zones[5] = { NULL, NULL, NULL, NULL, NULL };
	int i;
	const icaltimezone *z;
	int num_zones_attached = 0;
	int zone_already_attached;

	if (subcomp == NULL) {
		lprintf(3, "ERROR: ical_encapsulate_subcomponent() called with NULL argument\n");
		return NULL;
	}

	/*
	 * If we're already looking at a full VCALENDAR component, this is probably an error.
	 */
	if (icalcomponent_isa(subcomp) == ICAL_VCALENDAR_COMPONENT) {
		lprintf(3, "ERROR: component sent to ical_encapsulate_subcomponent() already top level\n");
		return subcomp;
	}

	/* search for... */
	for (p = icalcomponent_get_first_property(subcomp, ICAL_ANY_PROPERTY);
	     p != NULL;
	     p = icalcomponent_get_next_property(subcomp, ICAL_ANY_PROPERTY))
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
			t = icalproperty_get_dtstart(p);	/*/ it's safe to use dtstart for all of them */
			if ((icaltime_is_valid_time(t)) && (z=icaltime_get_timezone(t), z)) {
			
				zone_already_attached = 0;
				for (i=0; i<5; ++i) {
					if (z == attached_zones[i]) {
						++zone_already_attached;
						lprintf(9, "zone already attached!!\n");
					}
				}
				if ((!zone_already_attached) && (num_zones_attached < 5)) {
					lprintf(9, "attaching zone %d!\n", num_zones_attached);
					attached_zones[num_zones_attached++] = z;
				}

				icalproperty_set_parameter(p,
					icalparameter_new_tzid(icaltimezone_get_tzid((icaltimezone *)z))
				);
			}
		}
	}

	/* Encapsulate the VEVENT component into a complete VCALENDAR */
	encaps = icalcomponent_new(ICAL_VCALENDAR_COMPONENT);
	if (encaps == NULL) {
		lprintf(3, "ERROR: ical_encapsulate_subcomponent() could not allocate component\n");
		return NULL;
	}

	/* Set the Product ID */
	icalcomponent_add_property(encaps, icalproperty_new_prodid(PRODID));

	/* Set the Version Number */
	icalcomponent_add_property(encaps, icalproperty_new_version("2.0"));

	/* Attach any timezones we need */
	if (num_zones_attached > 0) for (i=0; i<num_zones_attached; ++i) {
		icalcomponent *zc;
		zc = icalcomponent_new_clone(icaltimezone_get_component((icaltimezone *)attached_zones[i]));
		icalcomponent_add_component(encaps, zc);
	}

	/* Encapsulate the subcomponent inside */
	icalcomponent_add_component(encaps, subcomp);

	/* Return the object we just created. */
	return(encaps);
}
