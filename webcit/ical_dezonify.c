/* 
 * $Id$ 
 */
/**
 * \defgroup IcalDezonify normalize ical dates to UTC
 * Function to go through an ical component set and convert all non-UTC
 * date/time properties to UTC.  It also strips out any VTIMEZONE
 * subcomponents afterwards, because they're irrelevant.
 *
 * Everything here will work on both a fully encapsulated VCALENDAR component
 * or any type of subcomponent.
 *
 */
/*@{*/

#include "webcit.h"
#include "webserver.h"


#ifdef WEBCIT_WITH_CALENDAR_SERVICE


/**
 * \brief Back end function for ical_dezonify()
 *
 * We supply this with the master component, the relevant component,
 * and the property (which will be a DTSTART, DTEND, etc.)
 * which we want to convert to UTC.
 * \param cal dunno ???
 * \param rcal dunno ???
 * \param prop dunno ???
 */
void ical_dezonify_backend(icalcomponent *cal,
			icalcomponent *rcal,
			icalproperty *prop) {

	icaltimezone *t = NULL;
	icalparameter *param;
	const char *tzid;
	struct icaltimetype TheTime;

	/** Give me nothing and I will give you nothing in return. */
	if (cal == NULL) return;

	/** Hunt for a TZID parameter in this property. */
	param = icalproperty_get_first_parameter(prop, ICAL_TZID_PARAMETER);

	/** Get the stringish name of this TZID. */
	if (param != NULL) {
		tzid = icalparameter_get_tzid(param);

		/** Convert it to an icaltimezone type. */
		if (tzid != NULL) {
			t = icalcomponent_get_timezone(cal, tzid);
		}

	}

	/** Now we know the timezone.  Convert to UTC. */

	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		TheTime = icalproperty_get_dtstart(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		TheTime = icalproperty_get_dtend(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_DUE_PROPERTY) {
		TheTime = icalproperty_get_due(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_EXDATE_PROPERTY) {
		TheTime = icalproperty_get_exdate(prop);
	}
	else {
		return;
	}

	/** Do the conversion. */
	if (t != NULL) {
		icaltimezone_convert_time(&TheTime,
					t,
					icaltimezone_get_utc_timezone()
		);
	}
	TheTime.is_utc = 1;
	icalproperty_remove_parameter_by_kind(prop, ICAL_TZID_PARAMETER);

	/** Now add the converted property back in. */
	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		icalproperty_set_dtstart(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		icalproperty_set_dtend(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_DUE_PROPERTY) {
		icalproperty_set_due(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_EXDATE_PROPERTY) {
		icalproperty_set_exdate(prop, TheTime);
	}
}


/**
 * \brief Recursive portion of ical_dezonify()
 * \param cal dunno ???
 * \param rcal dunno ???
 */
void ical_dezonify_recur(icalcomponent *cal, icalcomponent *rcal) {
	icalcomponent *c;
	icalproperty *p;

	/**
	 * Recurse through all subcomponents *except* VTIMEZONE ones.
	 */
	for (c=icalcomponent_get_first_component(
					rcal, ICAL_ANY_COMPONENT);
		c != NULL;
		c = icalcomponent_get_next_component(
					rcal, ICAL_ANY_COMPONENT)
	) {
		if (icalcomponent_isa(c) != ICAL_VTIMEZONE_COMPONENT) {
			ical_dezonify_recur(cal, c);
		}
	}

	/**
	 * Now look for DTSTART and DTEND properties
	 */
	for (p=icalcomponent_get_first_property(
				rcal, ICAL_ANY_PROPERTY);
		p != NULL;
		p = icalcomponent_get_next_property(
				rcal, ICAL_ANY_PROPERTY)
	) {
		if (
			(icalproperty_isa(p) == ICAL_DTSTART_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_DTEND_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_DUE_PROPERTY)
			|| (icalproperty_isa(p) == ICAL_EXDATE_PROPERTY)
		   ) {
			ical_dezonify_backend(cal, rcal, p);
		}
	}
}


/**
 * \brief Convert all DTSTART and DTEND properties in all subcomponents to UTC.
 * This function will search any VTIMEZONE subcomponents to learn the
 * relevant timezone information.
 * \param cal item to process
 */
void ical_dezonify(icalcomponent *cal) {
	icalcomponent *vt = NULL;

	/** Convert all times to UTC */
	ical_dezonify_recur(cal, cal);

	/** Strip out VTIMEZONE subcomponents -- we don't need them anymore */
	while (vt = icalcomponent_get_first_component(
			cal, ICAL_VTIMEZONE_COMPONENT), vt != NULL) {
		icalcomponent_remove_component(cal, vt);
		icalcomponent_free(vt);
	}

}


#endif /* WEBCIT_WITH_CALENDAR_SERVICE */
/*@}*/
