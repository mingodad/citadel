/* 
 * $Id$ 
 *
 * Function to go through an ical component set and convert all non-UTC
 * DTSTART and DTEND properties to UTC.  It also strips out any VTIMEZONE
 * subcomponents afterwards, because they're irrelevant.
 *
 */

#ifdef HAVE_ICAL_H

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <fcntl.h>
#include <sys/types.h>
#include <ical.h>

/*
 * Back end function for ical_dezonify()
 *
 * We supply this with the master component and the property (which will
 * be a DTSTART or DTEND) which we want to convert to UTC.
 */
void ical_dezonify_backend(icalcomponent *cal, icalproperty *prop) {
	icaltimezone *t;
	icalparameter *param;
	const char *tzid;
	struct icaltimetype TheTime;

	/* Give me nothing and I will give you nothing in return. */
	if (cal == NULL) return;

	/* Hunt for a TZID parameter in this property. */
	param = icalproperty_get_first_parameter(prop, ICAL_TZID_PARAMETER);
	if (param == NULL) return;

	/* Get the stringish name of this TZID. */
	tzid = icalparameter_get_tzid(param);
	if (tzid == NULL) return;

	/* Convert it to an icaltimezone type. */
	t = icalcomponent_get_timezone(cal, tzid);
	if (t == NULL) return;

	/* Now we know the timezone.  Convert to UTC. */

	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		TheTime = icalproperty_get_dtstart(prop);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		TheTime = icalproperty_get_dtend(prop);
	}

	/* Do the conversion.
	 */
	icaltimezone_convert_time(&TheTime,
				t,
				icaltimezone_get_utc_timezone()
	);

	/* Now strip the TZID parameter, because it's incorrect now. */
	icalproperty_remove_parameter(prop, ICAL_TZID_PARAMETER);

	if (icalproperty_isa(prop) == ICAL_DTSTART_PROPERTY) {
		icalproperty_set_dtstart(prop, TheTime);
	}
	else if (icalproperty_isa(prop) == ICAL_DTEND_PROPERTY) {
		icalproperty_set_dtend(prop, TheTime);
	}

}


/*
 * Recursive portion of ical_dezonify()
 */
void ical_dezonify_recur(icalcomponent *cal, icalcomponent *rcal) {
	icalcomponent *c;
	icalproperty *p;

	/*
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

	/*
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
		   ) {
			ical_dezonify_backend(cal, p);
		}
	}
}


/*
 * Convert all DTSTART and DTEND properties in all subcomponents to UTC.
 * This function will search any VTIMEZONE subcomponents to learn the
 * relevant timezone information.
 */
void ical_dezonify(icalcomponent *cal) {
	icalcomponent *vt = NULL;

	/* Convert all times to UTC */
	ical_dezonify_recur(cal, cal);

	/* Strip out VTIMEZONE subcomponents -- we don't need them anymore */
	while (vt = icalcomponent_get_first_component(
			cal, ICAL_VTIMEZONE_COMPONENT), vt != NULL) {
		icalcomponent_remove_component(cal, vt);
		icalcomponent_free(vt);
	}
}

#endif /* HAVE_ICAL_H */
